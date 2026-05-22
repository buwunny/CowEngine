# CowEngine 🐮

CowEngine is a C++ game engine with a native editor and a standalone game build that can target both desktop and WebAssembly.

## Requirements

- CMake 3.21+
- A C++17 compiler toolchain (gcc/clang or MSVC)
- Python 3 (for the local web server helper)
- vcpkg (for native dependencies)
- Emscripten SDK (optional, required for web builds)

## Dependency setup (native)

The CMake presets expect `VCPKG_ROOT` to be set and will enable vcpkg by default.

```bash
export VCPKG_ROOT=/path/to/vcpkg
$VCPKG_ROOT/vcpkg install
```

This project uses the dependencies listed in `vcpkg.json` (GLFW, GLM, nlohmann-json, Bullet, ImGui, glad).

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
- Binary: `build/game-native/CowEngineGame`
- Installed assets: `dist/game-native/`

## Build: game (web / WebAssembly)

Activate Emscripten first:

```bash
source /path/to/emsdk/emsdk_env.sh
```

Then build:

```bash
emcmake cmake --preset game-web
cmake --build --preset game-web -j
cmake --install build/game-web
```

Output: `dist/game-web/` (self-contained static site)

## Full build (native + web game outputs)

The internal build system provides a helper script and a CMake target to build both game outputs.

Script:

```bash
./tools/build/build-all-games.sh
```

CMake target (from any configured build tree):

```bash
cmake --build <build-dir> --target build_all_games
```

Outputs:
- `dist/game-native/`
- `dist/game-web/` (if Emscripten is available)

## Run

### Editor (native)

```bash
./build/editor-native/CowEngine
```

### Game (native)

```bash
./build/game-native/CowEngineGame
```

### Game (web)

```bash
./tools/build/serve-web.sh 8080
```

Then open http://localhost:8080/

## Notes

- If `emcmake` is not available, web builds are skipped by the helper script.
- The GitHub Pages workflow builds the web target via Emscripten.
