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
use wtransport::endpoint::IncomingSession;
use wtransport::tls::Sha256DigestFmt;
use wtransport::{Endpoint, Identity, ServerConfig};

use crate::{
    Relay, CH_RELIABLE, KIND_CONNECT, KIND_DISCONNECT, KIND_RELIABLE, KIND_UNRELIABLE,
};

pub async fn run(bind_addr: String, relay: Arc<Relay>) -> Result<()> {
    let addr: SocketAddr = bind_addr.parse()?;

    // Dev certificate. In production, load a real CA-signed cert instead so the
    // browser needs no serverCertificateHashes. For self-signed dev, print the
    // SHA-256 the client must pin.
    let identity = Identity::self_signed(["localhost", "127.0.0.1", "::1"])?;
    let hash = identity.certificate_chain().as_slice()[0].hash();
    let b64 = base64::engine::general_purpose::STANDARD.encode(hash.as_ref());
    println!("webtransport: https://{addr}");
    println!("webtransport: cert sha-256 base64 = {b64}");
    println!("webtransport: cert sha-256 hex    = {}", hash.fmt(Sha256DigestFmt::DottedHex));
    println!("  (pass ?certhash=<base64> to the client for the self-signed dev cert)");

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
    let result = pump_incoming(&connection, &relay, session).await;

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
