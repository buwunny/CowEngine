#!/usr/bin/env bash
# Build the standalone WebAssembly game.
#
# Output: dist/game-web/  (CowEngine.html/.js/.wasm/.data + index.html, self-contained — host this folder on any static web server)
#
# Requires:
#   - Emscripten SDK activated (source <path-to-emsdk>/emsdk_env.sh)
#   - CMake >= 3.21

set -euo pipefail
cd "$(dirname "$0")/../.."

if ! command -v emcmake >/dev/null 2>&1; then
    echo "ERROR: emcmake not found. Activate the Emscripten SDK first:"
    echo "    source /path/to/emsdk/emsdk_env.sh"
    exit 1
fi

emcmake cmake --preset game-web
cmake --build --preset game-web -j
rm -rf dist/game-web
cmake --install build/game-web

echo
echo "=== Game built ==="
echo "Self-contained folder: $(pwd)/dist/game-web"
echo "Try locally with:      python3 -m http.server --directory dist/game-web 8080"
echo "Then open:             http://localhost:8080/"
