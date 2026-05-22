#!/usr/bin/env bash
# Build the standalone Linux game and stage a redistributable folder.
#
# Output: dist/game-native/  (CowEngine + assets, self-contained — copy/zip this folder to ship)
#
# Requires:
#   - vcpkg installed and VCPKG_ROOT exported, OR pass -DCMAKE_TOOLCHAIN_FILE=...
#   - a C++17 compiler (gcc/clang) and CMake >= 3.21

set -euo pipefail
cd "$(dirname "$0")/../.."

if [[ -z "${VCPKG_ROOT:-}" ]]; then
    echo "ERROR: VCPKG_ROOT is not set. Export it (e.g. export VCPKG_ROOT=$HOME/vcpkg) or edit this script."
    exit 1
fi

cmake --preset game-native
cmake --build --preset game-native -j
cmake --install build/game-native

echo
echo "=== Game built ==="
echo "Self-contained folder: $(pwd)/dist/game-native"
echo "Run with:              ./dist/game-native/CowEngine"
