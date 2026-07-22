# Deploying CowEngine multiplayer

There are two independent deployables:

1. **The site** — static WASM builds published to **GitHub Pages** (the landing
   page at `/`, the game at `/play`, the editor at `/edit`).
2. **The backend** — the headless C++ **server** + the transport **sidecar**,
   hosted on any box that allows inbound `443/tcp` (and `4443/udp` for
   WebTransport). The static site connects to it over `wss://`.

```
Browser (https://cowengine.com/play)
        │  wss://game.cowengine.com   (WebTransport primary → WebSocket fallback)
        ▼
  Sidecar  ──local UDP──►  C++ server        (both on your host, via docker compose)
 (TLS term)   4433/udp     (authoritative sim)
```

The browser page is served from `https://`, so it can only open a **secure**
socket (`wss://`) — that's why the sidecar must terminate TLS. `ws://` works only
for local dev from `http://localhost`.

---

## 1. Backend: server + sidecar

### Prerequisites
- A host (VPS/Fly.io/etc.) with a public IP and Docker + Docker Compose.
- Firewall/security group open for **inbound `443/tcp`** and **`4443/udp`**.
- A DNS record: `game.cowengine.com` → your host's IP.
- A TLS certificate for `game.cowengine.com` (Let's Encrypt below).

### Get a certificate (Let's Encrypt)
```bash
sudo certbot certonly --standalone -d game.cowengine.com
# → /etc/letsencrypt/live/game.cowengine.com/{fullchain.pem,privkey.pem}
```

### Run the stack
From the repo root:
```bash
TLS_DIR=/etc/letsencrypt/live/game.cowengine.com \
  docker compose -f deploy/docker-compose.yml up --build -d
```
- `server` — internal only, no published ports; the sidecar reaches it over the
  compose network at `server:4433`.
- `sidecar` — publishes `443:8080` (wss) and `4443:4443/udp` (WebTransport), and
  mounts `TLS_DIR` read-only at `/tls` for `COW_TLS_CERT`/`COW_TLS_KEY`.

Check it:
```bash
docker compose -f deploy/docker-compose.yml logs -f sidecar
# expect: "cowengine-sidecar: WSS wss://0.0.0.0:8080  ->  udp 127.0.0.1:4433"
```

> **Cert renewal:** Let's Encrypt certs last 90 days. After `certbot renew`,
> restart the sidecar (`docker compose ... restart sidecar`) so it reloads the
> PEM files. (Automate via a renew hook if you like.)

---

## 2. Frontend: GitHub Pages

The [`Deploy static content to Pages`](.github/workflows/static.yml) workflow
builds `game-web` + `editor-web`, assembles the site, bakes in the server URL,
and publishes. It runs on **manual dispatch** (Actions → run workflow).

### One-time setup
In **Settings → Secrets and variables → Actions → Variables**, add:

| Variable | Example | Purpose |
| --- | --- | --- |
| `COWENGINE_SERVER_WS` | `wss://game.cowengine.com` | Baked into `/play` as the server. **Required for multiplayer.** |
| `COWENGINE_SERVER_WT` | `https://game.cowengine.com:4443` | Optional WebTransport URL (primary; WS is the fallback). |
| `PAGES_CNAME` | `cowengine.com` | Optional custom domain (writes `site/CNAME`). |

In **Settings → Pages**: set **Source = GitHub Actions**, and (if using a custom
domain) set the custom domain to match `PAGES_CNAME`.

### Publish
Run the workflow (Actions → *Deploy static content to Pages* → *Run workflow*).
Result:
- `https://cowengine.com/` — landing page
- `https://cowengine.com/play/` — networked game (auto-connects to the baked server)
- `https://cowengine.com/edit/` — editor

If `COWENGINE_SERVER_WS` is unset, `/play` is published single-player; players can
still opt in with `?ws=wss://host` / `?wt=https://host:4443` query params.

---

## 3. Verification checklist
- [ ] `dig game.cowengine.com` resolves to the host IP.
- [ ] `openssl s_client -connect game.cowengine.com:443` shows the real cert.
- [ ] Sidecar log shows `WSS wss://...`; server log shows `listening ... (scene: ...)`.
- [ ] Open `https://cowengine.com/play` in **two** browsers → each sees the other
      move, and shot cows / scene objects stay in sync.
- [ ] Reload a tab → it reconnects cleanly and no ghost avatar lingers.
- [ ] DevTools console shows `[CowNet] WebSocket connected wss://…` (or
      `WebTransport connected`).

---

## Local dev (no TLS)
Plain `ws://` from `http://localhost` needs no certificate:
```bash
# 1) server
cmake --preset server-native && cmake --build --preset server-native
./build/server-native/CowEngineServer            # binds udp 4433

# 2) sidecar (WS only, fast build)
cargo build --manifest-path sidecar/Cargo.toml
./sidecar/target/debug/cowengine-sidecar         # ws://0.0.0.0:8080

# 3) game, pointed at the local sidecar
cmake --preset game-web && cmake --build --preset game-web && cmake --install build/game-web
python3 -m http.server -d dist/game-web 5500
# open http://localhost:5500/?ws=ws://localhost:8080 in two tabs
```
Build the sidecar with `--features tls` (+ `COW_TLS_CERT`/`COW_TLS_KEY`) for
`wss://`, and add `--features webtransport` for the HTTP/3 path.

> **Tip:** if connections silently fail, kill stale processes first —
> `pkill -f CowEngineServer; pkill -f cowengine-sidecar` — a leftover server
> holds the ports and absorbs connections.

---

## Known caveats
- **WebTransport cert:** when `COW_TLS_CERT`/`COW_TLS_KEY` are set (they are, in
  `docker-compose.yml`), the WebTransport listener loads the **same CA-signed PEM**
  as `wss://`, so the browser validates it normally — no `serverCertificateHashes`
  needed. Without those env vars it falls back to a self-signed dev cert and prints
  a `?certhash=` to pin. If UDP/4443 is blocked, clients still connect over `wss://`.
- **Single room:** one shared world, capacity `maxPlayers_ = 16` (server-side).
- **UDP for HTTP/3:** WebTransport needs inbound **UDP** 4443 open; some hosts
  block UDP by default — if so, the game still works over `wss://`.
