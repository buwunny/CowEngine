# Building CowEngine

CowEngine builds four targets from one C++17 codebase: the native **editor**, the
native **game**, the **web** game (WebAssembly), and the **web editor**. A fifth,
headless **server** target powers multiplayer (see [DEPLOYMENT.md](DEPLOYMENT.md)).

## Requirements

- CMake 3.21+
- A C++17 compiler toolchain (gcc/clang or MSVC)
- Python 3 (for the local web-server helper)
- vcpkg (for native dependencies)
- Emscripten SDK (optional, required for web builds)

## Dependency setup (native)

The CMake presets expect `VCPKG_ROOT` to be set and enable vcpkg by default.

```bash
export VCPKG_ROOT=/path/to/vcpkg
$VCPKG_ROOT/vcpkg install
```

Dependencies are declared in [`vcpkg.json`](vcpkg.json): the core sim needs GLM,
nlohmann-json, Bullet, and EnTT; the `gui` feature (default) adds GLFW, glad, and
ImGui for the editor/game. The headless server build skips the `gui` feature.

## Build: editor (native)

```bash
cmake --preset editor-native
cmake --build --preset editor-native -j
```

Output: `build/editor-native/CowEngine`

## Build: game (native)

```bash
cmake --preset game-native
cmake --build --preset game-native -j
cmake --install build/game-native
```

Output:
- Binary: `build/game-native/CowEngine` (the executable target is `CowEngine`
  either way; `BUILD_GAME=ON` just strips the editor UI)
- Installed assets: `dist/game-native/`

## Build: game (web / WebAssembly)

Activate Emscripten first, then build:

```bash
source /path/to/emsdk/emsdk_env.sh
emcmake cmake --preset game-web
cmake --build --preset game-web -j
cmake --install build/game-web
```

Output: `dist/game-web/` (self-contained static site)

## Build: editor (web / WebAssembly)

```bash
source /path/to/emsdk/emsdk_env.sh
emcmake cmake --preset editor-web
cmake --build --preset editor-web -j
cmake --install build/editor-web
```

Output: `dist/editor-web/` (self-contained static site)

## Build: server (headless, native)

The authoritative multiplayer server reuses the engine sim with no rendering, so
it needs only the core vcpkg deps (`VCPKG_MANIFEST_NO_DEFAULT_FEATURES` is baked
into the preset).

```bash
cmake --preset server-native
cmake --build --preset server-native -j
```

Output: `build/server-native/CowEngineServer` (and the `cowengine_netclient_test`
headless netcode test). See [DEPLOYMENT.md](DEPLOYMENT.md) for running it with the
transport sidecar.

## Full build (native + web game outputs)

Helper script:

```bash
./tools/build/build-all-games.sh
```

CMake target (from any configured build tree):

```bash
cmake --build <build-dir> --target build_all_games
```

Outputs: `dist/game-native/` and `dist/game-web/` (if Emscripten is available).

## Run

### Editor (native)
```bash
./build/editor-native/CowEngine
```

### Game (native)
```bash
./build/game-native/CowEngine
```

### Game (web)
```bash
./tools/build/serve-web.sh 8080
# open http://localhost:8080/
```

### Editor (web)
`serve-web.sh` only serves `dist/game-web`, so serve `dist/editor-web` directly:
```bash
python3 -m http.server --directory dist/editor-web 8080
# open http://localhost:8080/
```

## Tests

```bash
cmake --build --preset server-native
./build/server-native/cowengine_netclient_test   # headless client-netcode test
```

## Notes

- If `emcmake` is not available, web builds are skipped by the helper script.
- The GitHub Pages workflow builds the game + editor web targets and publishes a
  landing page at `/`, the game at `/play`, and the editor at `/edit`. See
  [DEPLOYMENT.md](DEPLOYMENT.md).
