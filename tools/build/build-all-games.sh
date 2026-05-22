#!/usr/bin/env bash
# Build both native and web game versions (used by the embedding pipeline).
# Requires: cmake, emcmake/emcc for web builds when building web.

set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"

echo "Configuring and building native game..."
cmake --preset game-native
cmake --build --preset game-native -j
cmake --install build/game-native

echo "Configuring and building web game (requires emsdk)..."
if command -v emcmake >/dev/null 2>&1; then
    emcmake cmake --preset game-web
    cmake --build --preset game-web -j
    rm -rf dist/game-web
    cmake --install build/game-web
else
    echo "emcmake not found — skipping web build. Install Emscripten SDK and retry." >&2
fi

echo "Done. Built outputs: dist/game-native (if native) and dist/game-web (if web)."
