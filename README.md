# CowEngine 🐮

A small C++17 game engine with a browser-based editor, a `.cow` scripting
language, Bullet physics, and authoritative multiplayer that runs in the browser.
One codebase builds a native editor, a native game, a WebAssembly game/editor, and
a headless multiplayer server.

**Live:** [cowengine.com](https://cowengine.com) — [▶ Play](https://cowengine.com/play) · [🛠 Editor](https://cowengine.com/edit)

## Features

- **`.cow` scripting** — a small embedded language; gameplay (movement, spawning,
  interactions) is written in `.cow` and bound to C++ builtins via `ScriptHost`.
- **Bullet physics** — rigid bodies are the authoritative world state, mirrored
  one-way into transforms each frame.
- **Browser editor** — build and script scenes, then export a standalone game
  (native or web) with the assets bundled in.
- **Runs everywhere** — native (GLFW/OpenGL) and WebAssembly (Emscripten) from the
  same sources.
- **Text rendering** — `TextRenderer` bakes a TrueType face into a glyph atlas and
  draws camera-facing world labels or HUD text, one draw call each. Players get a
  nametag over their head from it.
- **Multiplayer** — an authoritative headless C++ server (reusing the exact engine
  sim) with client-side prediction/reconciliation, entity interpolation, and shared
  physics, reached from the static web client over **WebTransport** (with a
  **WebSocket/`wss://`** fallback).

## How multiplayer fits together

```
Browser (WASM client)              Sidecar (Rust)            C++ server
 prediction + interpolation  ⟶  TLS termination, per-  ⟶  authoritative sim
 WebTransport / wss://           connection UDP session     (Bullet + .cow)
```

The static page is hosted on GitHub Pages; the server + sidecar run on any host
with `443/tcp` (and `4443/udp` for WebTransport) open. Full runbook in
[DEPLOYMENT.md](DEPLOYMENT.md).

## Quick start

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset editor-native
cmake --build --preset editor-native -j
./build/editor-native/CowEngine
```

Building the game, the web targets, and the server is covered in
[BUILDING.md](BUILDING.md).

## Documentation

- [BUILDING.md](BUILDING.md) — building and running every target, plus tests.
- [DEPLOYMENT.md](DEPLOYMENT.md) — hosting the multiplayer backend and publishing
  the site to GitHub Pages.

## Repository layout

| Path | What |
| --- | --- |
| `src/`, `include/` | Engine, ECS, scripting, rendering, networking |
| `src/server/`, `include/server/` | Headless authoritative server |
| `sidecar/` | Rust WebTransport/WebSocket → UDP relay |
| `scripts/`, `scenes/`, `models/` | Sample `.cow` scripts, scenes, and assets |
| `deploy/` | Dockerfiles, compose, landing page |
| `.github/workflows/` | GitHub Pages deploy |
