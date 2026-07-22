// CowEngine sidecar: terminates browser transports (WebTransport primary,
// WebSocket fallback) and relays each connection to the headless C++ server
// over one UDP socket, multiplexed by a per-connection session id.
//
// Server<->sidecar UDP framing:   [u32 session][u8 kind][payload]
//   kind 0=connect 1=disconnect 2=unreliable 3=reliable
//
// Browser<->sidecar:
//   WebSocket:     each binary message is [u8 channel][payload]  (0=unrel,1=rel)
//   WebTransport:  unreliable = datagram; reliable = u32-length-prefixed frames
//                  on one bidi stream.
//
// Env: COW_SERVER (default 127.0.0.1:4433), COW_WS (default 0.0.0.0:8080),
//      COW_WT (default 0.0.0.0:4443, WebTransport; requires --features webtransport).
//      COW_TLS_CERT + COW_TLS_KEY (PEM paths; requires --features tls) upgrade the
//      WS listener to wss:// — needed to connect from an https origin.
// Access gate (both transports): COW_ALLOWED_ORIGINS (comma list, empty=any),
//      COW_JOIN_KEY (require ?key=<val>; empty=off), COW_MAX_CONN_PER_IP (default 8).

use std::collections::HashMap;
use std::net::{IpAddr, SocketAddr};
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Arc;

use futures_util::{SinkExt, StreamExt};
use tokio::net::{TcpListener, UdpSocket};
use tokio::sync::{mpsc, Mutex};
use tokio_tungstenite::tungstenite::handshake::server::{ErrorResponse, Request, Response};
use tokio_tungstenite::tungstenite::http::{Response as HttpResponse, StatusCode};
use tokio_tungstenite::tungstenite::Message as WsMessage;

const KIND_CONNECT: u8 = 0;
const KIND_DISCONNECT: u8 = 1;
const KIND_UNRELIABLE: u8 = 2;
const KIND_RELIABLE: u8 = 3;

const CH_UNRELIABLE: u8 = 0;
const CH_RELIABLE: u8 = 1;

/// Outgoing message queued toward one browser connection: (channel, payload).
type OutTx = mpsc::UnboundedSender<(u8, Vec<u8>)>;

/// Access-control gate applied to every browser connection (WS + WebTransport).
/// None of this is strong auth — the web client is public — but it keeps casual
/// abuse and resource exhaustion out.
struct GateConfig {
    /// Allowed `Origin` header values (exact match). Empty = allow any origin.
    allowed_origins: Vec<String>,
    /// If set, connections must carry `?key=<value>` matching this. Keep it out
    /// of the published page for a friends-only gate; leave unset for public.
    join_key: Option<String>,
    /// Max concurrent connections from one client IP (0 = unlimited).
    max_conn_per_ip: u32,
}

impl GateConfig {
    fn from_env() -> Self {
        let allowed_origins = std::env::var("COW_ALLOWED_ORIGINS")
            .unwrap_or_default()
            .split(',')
            .map(|s| s.trim().to_string())
            .filter(|s| !s.is_empty())
            .collect();
        let join_key = std::env::var("COW_JOIN_KEY").ok().filter(|s| !s.is_empty());
        let max_conn_per_ip = std::env::var("COW_MAX_CONN_PER_IP")
            .ok()
            .and_then(|s| s.parse().ok())
            .unwrap_or(8);
        GateConfig { allowed_origins, join_key, max_conn_per_ip }
    }

    fn origin_allowed(&self, origin: Option<&str>) -> bool {
        if self.allowed_origins.is_empty() {
            return true; // no allow-list configured
        }
        origin.map_or(false, |o| self.allowed_origins.iter().any(|a| a == o))
    }

    fn key_ok(&self, query: Option<&str>) -> bool {
        match &self.join_key {
            None => true,
            Some(k) => query_has_key(query, k),
        }
    }
}

/// True if `query` (a URL query string, or a path containing one) has `key=<want>`.
fn query_has_key(query: Option<&str>, want: &str) -> bool {
    let q = match query {
        Some(q) => q,
        None => return false,
    };
    q.split(['?', '&'])
        .filter_map(|kv| kv.split_once('='))
        .any(|(k, v)| k == "key" && v == want)
}

/// Build a 403 handshake rejection for the WS gate callback.
fn reject_403(msg: String) -> ErrorResponse {
    HttpResponse::builder()
        .status(StatusCode::FORBIDDEN)
        .body(Some(msg))
        .expect("valid 403 response")
}

/// Shared relay state: the single UDP socket to the C++ server plus the live
/// session table used to route server replies back to the right browser.
struct Relay {
    udp: UdpSocket,
    sessions: Mutex<HashMap<u32, OutTx>>,
    conns_per_ip: Mutex<HashMap<IpAddr, u32>>,
    next_id: AtomicU32,
    cfg: GateConfig,
}

impl Relay {
    async fn new(server_addr: SocketAddr, cfg: GateConfig) -> std::io::Result<Arc<Self>> {
        let udp = UdpSocket::bind(("0.0.0.0", 0)).await?;
        udp.connect(server_addr).await?;
        Ok(Arc::new(Relay {
            udp,
            sessions: Mutex::new(HashMap::new()),
            conns_per_ip: Mutex::new(HashMap::new()),
            next_id: AtomicU32::new(1),
            cfg,
        }))
    }

    /// Reserve a connection slot for `ip`. Returns false if the per-IP cap is hit.
    async fn acquire_ip(&self, ip: IpAddr) -> bool {
        if self.cfg.max_conn_per_ip == 0 {
            return true;
        }
        let mut map = self.conns_per_ip.lock().await;
        let n = map.entry(ip).or_insert(0);
        if *n >= self.cfg.max_conn_per_ip {
            return false;
        }
        *n += 1;
        true
    }

    async fn release_ip(&self, ip: IpAddr) {
        if self.cfg.max_conn_per_ip == 0 {
            return;
        }
        let mut map = self.conns_per_ip.lock().await;
        if let Some(n) = map.get_mut(&ip) {
            *n = n.saturating_sub(1);
            if *n == 0 {
                map.remove(&ip);
            }
        }
    }

    fn frame(session: u32, kind: u8, payload: &[u8]) -> Vec<u8> {
        let mut v = Vec::with_capacity(5 + payload.len());
        v.extend_from_slice(&session.to_le_bytes());
        v.push(kind);
        v.extend_from_slice(payload);
        v
    }

    async fn to_server(&self, session: u32, kind: u8, payload: &[u8]) {
        let _ = self.udp.send(&Relay::frame(session, kind, payload)).await;
    }

    async fn register(&self, tx: OutTx) -> u32 {
        let id = self.next_id.fetch_add(1, Ordering::Relaxed);
        self.sessions.lock().await.insert(id, tx);
        id
    }

    async fn deregister(&self, id: u32) {
        self.sessions.lock().await.remove(&id);
    }

    /// Read datagrams from the C++ server forever and fan each out to the
    /// browser connection named by its session id.
    async fn run_udp_reader(self: Arc<Self>) {
        let mut buf = vec![0u8; 4096];
        loop {
            let n = match self.udp.recv(&mut buf).await {
                Ok(n) => n,
                Err(_) => continue,
            };
            if n < 5 {
                continue;
            }
            let session = u32::from_le_bytes([buf[0], buf[1], buf[2], buf[3]]);
            let kind = buf[4];
            let payload = buf[5..n].to_vec();
            let channel = if kind == KIND_RELIABLE {
                CH_RELIABLE
            } else {
                CH_UNRELIABLE
            };
            if let Some(tx) = self.sessions.lock().await.get(&session) {
                let _ = tx.send((channel, payload));
            }
        }
    }
}

/// Per-IP admission + WS handshake gate, then bridge. Applies the connection cap
/// before the handshake and the Origin/key checks during it (a rejected handshake
/// returns 403 to the browser). Generic over the stream so it serves both plain
/// TCP (ws://) and a TLS-wrapped stream (wss://).
async fn handle_ws<S>(stream: S, relay: Arc<Relay>, peer: Option<SocketAddr>)
where
    S: tokio::io::AsyncRead + tokio::io::AsyncWrite + Unpin + Send + 'static,
{
    let ip = peer.map(|p| p.ip());
    if let Some(ip) = ip {
        if !relay.acquire_ip(ip).await {
            eprintln!("ws refused {ip}: per-IP connection cap reached");
            return;
        }
    }
    bridge_ws(stream, &relay, peer).await;
    if let Some(ip) = ip {
        relay.release_ip(ip).await;
    }
}

async fn bridge_ws<S>(stream: S, relay: &Arc<Relay>, peer: Option<SocketAddr>)
where
    S: tokio::io::AsyncRead + tokio::io::AsyncWrite + Unpin + Send + 'static,
{
    // Check Origin + join key during the handshake; reject with 403 otherwise.
    let gate = relay.clone();
    let ws = match tokio_tungstenite::accept_hdr_async(stream, move |req: &Request, resp: Response| {
        let origin = req.headers().get("origin").and_then(|v| v.to_str().ok());
        if !gate.cfg.origin_allowed(origin) {
            return Err(reject_403(format!("origin not allowed: {origin:?}")));
        }
        if !gate.cfg.key_ok(req.uri().query()) {
            return Err(reject_403("missing or invalid join key".into()));
        }
        Ok(resp)
    })
    .await
    {
        Ok(w) => w,
        Err(e) => {
            eprintln!("ws handshake/gate rejected {peer:?}: {e}");
            return;
        }
    };
    let (mut sink, mut source) = ws.split();

    let (out_tx, mut out_rx) = mpsc::unbounded_channel::<(u8, Vec<u8>)>();
    let session = relay.register(out_tx).await;
    relay.to_server(session, KIND_CONNECT, &[]).await;
    println!("ws connect session={session} peer={peer:?}");

    // server -> browser: prefix each payload with its channel byte.
    let out_task = tokio::spawn(async move {
        while let Some((channel, payload)) = out_rx.recv().await {
            let mut msg = Vec::with_capacity(1 + payload.len());
            msg.push(channel);
            msg.extend_from_slice(&payload);
            if sink.send(WsMessage::Binary(msg.into())).await.is_err() {
                break;
            }
        }
    });

    // browser -> server: first byte selects the channel.
    while let Some(item) = source.next().await {
        match item {
            Ok(WsMessage::Binary(data)) => {
                if data.is_empty() {
                    continue;
                }
                let kind = if data[0] == CH_RELIABLE {
                    KIND_RELIABLE
                } else {
                    KIND_UNRELIABLE
                };
                relay.to_server(session, kind, &data[1..]).await;
            }
            Ok(WsMessage::Close(_)) | Err(_) => break,
            _ => {}
        }
    }

    relay.to_server(session, KIND_DISCONNECT, &[]).await;
    relay.deregister(session).await;
    out_task.abort();
    println!("ws disconnect session={session}");
}

fn env_or(key: &str, default: &str) -> String {
    std::env::var(key).unwrap_or_else(|_| default.to_string())
}

/// Resolve COW_SERVER, which is documented as `host:port` — so it has to accept a
/// DNS name, not just an IP literal. `"server:4433".parse::<SocketAddr>()` fails,
/// and that is exactly what docker compose passes (the service name on the
/// bridge network), so the name must go through a resolver.
///
/// Retries briefly: compose's `depends_on` only waits for the peer container to
/// *start*, so on a cold `up` the embedded DNS record can lag us by a moment.
async fn resolve_server(spec: &str) -> SocketAddr {
    if let Ok(addr) = spec.parse::<SocketAddr>() {
        return addr;
    }
    for attempt in 0..30u32 {
        match tokio::net::lookup_host(spec).await {
            Ok(addrs) => {
                let mut addrs: Vec<SocketAddr> = addrs.collect();
                // The relay socket binds 0.0.0.0, so an IPv6 answer would be
                // unusable — take IPv4 first.
                addrs.sort_by_key(|a| !a.is_ipv4());
                if let Some(addr) = addrs.first() {
                    if attempt > 0 {
                        println!("resolved COW_SERVER {spec} -> {addr} after {attempt}s");
                    }
                    return *addr;
                }
            }
            Err(e) if attempt == 29 => {
                panic!("COW_SERVER {spec:?} did not resolve after 30s: {e}")
            }
            Err(_) => {}
        }
        tokio::time::sleep(std::time::Duration::from_secs(1)).await;
    }
    panic!("COW_SERVER {spec:?} resolved to no usable address");
}

#[tokio::main]
async fn main() {
    let server_spec = env_or("COW_SERVER", "127.0.0.1:4433");
    let server_addr = resolve_server(&server_spec).await;
    let ws_addr = env_or("COW_WS", "0.0.0.0:8080");

    let cfg = GateConfig::from_env();
    println!(
        "access gate: origins={}  join_key={}  max_conn_per_ip={}",
        if cfg.allowed_origins.is_empty() { "any".into() } else { cfg.allowed_origins.join(",") },
        if cfg.join_key.is_some() { "required" } else { "none" },
        if cfg.max_conn_per_ip == 0 { "unlimited".into() } else { cfg.max_conn_per_ip.to_string() },
    );

    let relay = Relay::new(server_addr, cfg)
        .await
        .expect("failed to open UDP socket to server");
    tokio::spawn(relay.clone().run_udp_reader());

    #[cfg(feature = "webtransport")]
    {
        let wt_addr = env_or("COW_WT", "0.0.0.0:4443");
        let relay_wt = relay.clone();
        tokio::spawn(async move {
            if let Err(e) = webtransport::run(wt_addr, relay_wt).await {
                eprintln!("webtransport listener error: {e}");
            }
        });
    }

    let ws_listener = TcpListener::bind(&ws_addr)
        .await
        .expect("failed to bind WS port");
    #[cfg(not(feature = "webtransport"))]
    println!("(build with --features webtransport to enable the WebTransport listener)");

    // With COW_TLS_CERT + COW_TLS_KEY (and the `tls` feature), serve wss:// so an
    // https origin like cowengine.com can connect. Otherwise plain ws:// (dev).
    #[cfg(feature = "tls")]
    if let Some(acceptor) = tls::acceptor_from_env() {
        println!("cowengine-sidecar: WSS wss://{ws_addr}  ->  udp {server_addr}");
        loop {
            match ws_listener.accept().await {
                Ok((stream, peer)) => {
                    let relay = relay.clone();
                    let acceptor = acceptor.clone();
                    tokio::spawn(async move {
                        match acceptor.accept(stream).await {
                            Ok(tls_stream) => handle_ws(tls_stream, relay, Some(peer)).await,
                            Err(e) => eprintln!("tls handshake failed from {peer}: {e}"),
                        }
                    });
                }
                Err(e) => eprintln!("ws accept error: {e}"),
            }
        }
    }

    println!("cowengine-sidecar: WS ws://{ws_addr}  ->  udp {server_addr}");
    #[cfg(feature = "tls")]
    println!("(set COW_TLS_CERT + COW_TLS_KEY to serve wss:// instead)");
    #[cfg(not(feature = "tls"))]
    println!("(build with --features tls + set COW_TLS_CERT/COW_TLS_KEY for wss://)");
    loop {
        match ws_listener.accept().await {
            Ok((stream, peer)) => {
                let relay = relay.clone();
                tokio::spawn(handle_ws(stream, relay, Some(peer)));
            }
            Err(e) => eprintln!("ws accept error: {e}"),
        }
    }
}

/// TLS acceptor construction for the wss:// listener (ring crypto provider).
#[cfg(feature = "tls")]
mod tls {
    use std::fs::File;
    use std::io::BufReader;
    use std::sync::Arc;
    use tokio_rustls::rustls::pki_types::{CertificateDer, PrivateKeyDer};
    use tokio_rustls::rustls::ServerConfig;
    use tokio_rustls::TlsAcceptor;

    /// Build a TlsAcceptor from COW_TLS_CERT (fullchain PEM) + COW_TLS_KEY (PEM).
    /// Returns None if either is unset; logs and returns None on any load error
    /// so the caller cleanly falls back to plain ws://.
    pub fn acceptor_from_env() -> Option<TlsAcceptor> {
        let cert_path = std::env::var("COW_TLS_CERT").ok()?;
        let key_path = std::env::var("COW_TLS_KEY").ok()?;
        match build(&cert_path, &key_path) {
            Ok(a) => Some(a),
            Err(e) => {
                eprintln!("TLS requested but disabled: {e}");
                None
            }
        }
    }

    fn build(cert_path: &str, key_path: &str) -> Result<TlsAcceptor, String> {
        let certs = load_certs(cert_path).map_err(|e| format!("cert {cert_path}: {e}"))?;
        if certs.is_empty() {
            return Err(format!("cert {cert_path}: no certificates found"));
        }
        let key = load_key(key_path).map_err(|e| format!("key {key_path}: {e}"))?;

        let provider = Arc::new(tokio_rustls::rustls::crypto::ring::default_provider());
        let config = ServerConfig::builder_with_provider(provider)
            .with_safe_default_protocol_versions()
            .map_err(|e| e.to_string())?
            .with_no_client_auth()
            .with_single_cert(certs, key)
            .map_err(|e| e.to_string())?;
        Ok(TlsAcceptor::from(Arc::new(config)))
    }

    fn load_certs(path: &str) -> std::io::Result<Vec<CertificateDer<'static>>> {
        let mut r = BufReader::new(File::open(path)?);
        rustls_pemfile::certs(&mut r).collect()
    }

    fn load_key(path: &str) -> std::io::Result<PrivateKeyDer<'static>> {
        let mut r = BufReader::new(File::open(path)?);
        rustls_pemfile::private_key(&mut r)?
            .ok_or_else(|| std::io::Error::new(std::io::ErrorKind::InvalidData, "no private key found"))
    }
}

#[cfg(feature = "webtransport")]
mod webtransport;
