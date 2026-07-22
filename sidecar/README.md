# CowEngine sidecar

Terminates the browser's transport (**WebTransport** primary, **WebSocket**
fallback) and relays each connection to the headless C++ server over UDP,
multiplexed by a per-connection session id. The C++ server never speaks
QUIC/TLS — the sidecar owns all of that.

```
[browser WASM client]  --WebTransport / WSS-->  [sidecar]  --UDP-->  [CowEngineServer]
```

## Wire framing

Sidecar ⇄ C++ server (UDP): `[u32 session][u8 kind][payload]`
- kind `0`=connect `1`=disconnect `2`=unreliable `3`=reliable

Browser ⇄ sidecar:
- **WebSocket**: each binary frame is `[u8 channel][payload]` (channel `0`=unreliable, `1`=reliable)
- **WebTransport**: unreliable = QUIC datagram; reliable = one unidirectional stream per message

## Build & run

```bash
# WebSocket only (fast; dev/testing):
cargo build --release
# With WebTransport (pulls quinn/rustls):
cargo build --release --features webtransport

# Run (env-configured):
COW_SERVER=127.0.0.1:4433 \   # UDP addr of the C++ CowEngineServer
COW_WS=0.0.0.0:8080 \         # WebSocket listen (ws://)
COW_WT=0.0.0.0:4443 \         # WebTransport listen (feature-gated)
  ./target/release/cowengine-sidecar
```

On startup with `--features webtransport` it prints the self-signed cert's
SHA-256 (base64 + hex). Pass the base64 value to the client as `?certhash=…`
so the browser will accept the dev cert via `serverCertificateHashes`.

## Full local test

```bash
# 1. Headless game server
./build/server-native/CowEngineServer 4433 scenes/scene.json

# 2. Sidecar
cd sidecar && cargo run --release --features webtransport

# 3. Serve the web client and open it, pointing at the sidecar, e.g.:
#    dist/game-web/index.html?wt=https://localhost:4443&certhash=<base64>
#    (or ?ws=ws://localhost:8080 to force the WebSocket fallback)
# The browser console logs "[CowNet] WebSocket/WebTransport connected" then
# "[CowNet] recv ch=1 msgType=2 …" (ServerWelcome) and msgType=4 (Snapshot).
```

## Production notes

- Serve WebTransport/WebSocket over a real CA cert (Let's Encrypt) so no
  `serverCertificateHashes` pin is needed; WebTransport needs inbound **UDP/443**.
- The WS fallback must be **wss://** from an https page — put the sidecar behind
  a TLS terminator or extend it with `tokio-rustls` (Phase 7 hosting).
- Origin allow-listing and rate limiting are Phase 8 hardening.
