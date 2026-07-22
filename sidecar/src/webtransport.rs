// WebTransport (HTTP/3 over QUIC) listener. Bridges each browser session to the
// C++ server through the shared Relay, exactly like the WS path.
//
// Browser<->sidecar mapping over WebTransport:
//   unreliable  <->  QUIC datagrams
//   reliable    <->  one unidirectional stream per message (stream end delimits
//                    the message, so no length prefix is needed)

use std::net::SocketAddr;
use std::sync::Arc;
use std::time::Duration;

use anyhow::Result;
use base64::Engine;
use tokio::sync::mpsc;
use wtransport::endpoint::{IncomingSession, SessionRequest};
use wtransport::tls::Sha256DigestFmt;
use wtransport::{Endpoint, Identity, ServerConfig};

use crate::{
    Relay, CH_RELIABLE, KIND_CONNECT, KIND_DISCONNECT, KIND_RELIABLE, KIND_UNRELIABLE,
};

pub async fn run(bind_addr: String, relay: Arc<Relay>) -> Result<()> {
    let addr: SocketAddr = bind_addr.parse()?;

    // Production: load the same CA-signed PEM the wss listener uses (COW_TLS_CERT
    // fullchain + COW_TLS_KEY). A publicly-trusted cert means the browser needs no
    // serverCertificateHashes — it validates normally, so no ?certhash= is needed.
    // Dev fallback: an in-memory self-signed cert whose SHA-256 the client pins.
    let identity = match (std::env::var("COW_TLS_CERT").ok(), std::env::var("COW_TLS_KEY").ok()) {
        (Some(cert), Some(key)) => {
            let id = Identity::load_pemfiles(&cert, &key)
                .await
                .map_err(|e| anyhow::anyhow!("WT: load cert '{cert}' / key '{key}': {e}"))?;
            println!("webtransport: https://{addr} (CA cert from {cert})");
            id
        }
        _ => {
            let id = Identity::self_signed(["localhost", "127.0.0.1", "::1"])?;
            let hash = id.certificate_chain().as_slice()[0].hash();
            let b64 = base64::engine::general_purpose::STANDARD.encode(hash.as_ref());
            println!("webtransport: https://{addr} (self-signed dev cert)");
            println!("webtransport: cert sha-256 base64 = {b64}");
            println!("webtransport: cert sha-256 hex    = {}", hash.fmt(Sha256DigestFmt::DottedHex));
            println!("  (pass ?certhash=<base64> to the client for the self-signed dev cert)");
            id
        }
    };

    let config = ServerConfig::builder()
        .with_bind_address(addr)
        .with_identity(identity)
        .keep_alive_interval(Some(Duration::from_secs(3)))
        .build();

    let server = Endpoint::server(config)?;
    loop {
        let incoming = server.accept().await;
        let relay = relay.clone();
        tokio::spawn(async move {
            if let Err(e) = handle(incoming, relay).await {
                eprintln!("wt session error: {e:?}");
            }
        });
    }
}

async fn handle(incoming: IncomingSession, relay: Arc<Relay>) -> Result<()> {
    let session_request = incoming.await?;
    let ip = session_request.remote_address().ip();

    // Same access gate as the WS path: Origin allow-list, join key, per-IP cap.
    if !relay.cfg.origin_allowed(session_request.origin()) {
        println!("wt refused {ip}: origin not allowed ({:?})", session_request.origin());
        session_request.forbidden().await;
        return Ok(());
    }
    if !relay.cfg.key_ok(Some(session_request.path())) {
        println!("wt refused {ip}: missing/invalid join key");
        session_request.forbidden().await;
        return Ok(());
    }
    if !relay.acquire_ip(ip).await {
        println!("wt refused {ip}: per-IP connection cap reached");
        session_request.too_many_requests().await;
        return Ok(());
    }

    let result = handle_accepted(session_request, &relay).await;
    relay.release_ip(ip).await;
    result
}

async fn handle_accepted(session_request: SessionRequest, relay: &Arc<Relay>) -> Result<()> {
    let connection = Arc::new(session_request.accept().await?);

    let (out_tx, mut out_rx) = mpsc::unbounded_channel::<(u8, Vec<u8>)>();
    let session = relay.register(out_tx).await;
    relay.to_server(session, KIND_CONNECT, &[]).await;
    println!("wt connect session={session}");

    // server -> browser
    let conn_out = connection.clone();
    let out_task = tokio::spawn(async move {
        while let Some((channel, payload)) = out_rx.recv().await {
            if channel == CH_RELIABLE {
                // One uni stream per reliable message; finish() delimits it.
                match conn_out.open_uni().await {
                    Ok(opening) => match opening.await {
                        Ok(mut s) => {
                            let _ = s.write_all(&payload).await;
                            let _ = s.finish().await;
                        }
                        Err(_) => break,
                    },
                    Err(_) => break,
                }
            } else if conn_out.send_datagram(&payload).is_err() {
                // Datagram too large or connection gone; drop (unreliable).
            }
        }
    });

    // browser -> server
    let result = pump_incoming(&connection, relay, session).await;

    relay.to_server(session, KIND_DISCONNECT, &[]).await;
    relay.deregister(session).await;
    out_task.abort();
    println!("wt disconnect session={session}");
    result
}

async fn pump_incoming(
    connection: &wtransport::Connection,
    relay: &Arc<Relay>,
    session: u32,
) -> Result<()> {
    let mut tmp = vec![0u8; 8192];
    loop {
        tokio::select! {
            dgram = connection.receive_datagram() => {
                let dgram = dgram?;
                relay.to_server(session, KIND_UNRELIABLE, &dgram).await;
            }
            uni = connection.accept_uni() => {
                let mut recv = uni?;
                let mut buf = Vec::new();
                while let Some(n) = recv.read(&mut tmp).await? {
                    buf.extend_from_slice(&tmp[..n]);
                }
                relay.to_server(session, KIND_RELIABLE, &buf).await;
            }
        }
    }
}
