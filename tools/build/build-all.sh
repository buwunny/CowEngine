#!/usr/bin/env bash
# Build all versions of the engine and game (native and web).
# Requires: cmake, emcmake/emcc for web builds when building web.
cmake --build --preset editor-native -j
cmake --build --preset editor-web -j
cmake --build --preset game-native -j
cmake --build --preset game-web -j