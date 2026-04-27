# AGENTS.md

## Project overview

DS Editor Lite is a C++20 / Qt 6 Widgets desktop application (singing voice editor). The build target is `DsEditorLite`. Primary language in docs and comments is Chinese.

## Build system

- **CMake** with **vcpkg** for dependency management.
- Clone with `--recursive` (git submodules: `src/libs/qtmediate`, `scripts/vcpkg`).
- vcpkg manifest lives at `scripts/vcpkg-manifest/vcpkg.json`, not the repo root.
- Toolchain file: `vcpkg/scripts/buildsystems/vcpkg.cmake`.

### Configure and build (Windows)

```sh
# 1. Install vcpkg deps (set QT_DIR first)
set QT_DIR=C:\Qt\6.10.1\msvc2022_64
set Qt6_DIR=%QT_DIR%
set CMAKE_PREFIX_PATH=%QT_DIR%
set VCPKG_KEEP_ENV_VARS=QT_DIR;Qt6_DIR;Qt6GuiTools_DIR;CMAKE_PREFIX_PATH

cd vcpkg
vcpkg install --x-manifest-root=../scripts/vcpkg-manifest --x-install-root=./installed --triplet=x64-windows

# 2. CMake configure
cmake -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_INSTALL_PREFIX=build/install

# 3. Build
cmake --build build --target DsEditorLite
```

Optional CUDA support: add `--x-feature=cuda12` or `--x-feature=cuda11` to vcpkg install.

### Tests

Tests are gated behind `-DLITE_BUILD_TESTS=ON`. They live in `src/tests/`.

## Repository structure

```
src/
  app/          # Main application (DsEditorLite target)
    Controller/ Model/ UI/ Interface/ Modules/ Global/ Utils/
  libs/         # Internal libraries
    qtmediate/       # Git submodule - UI framework
    rmvpe-infer/     # Pitch inference
    game-infer/      # Game inference
    audio-util/      # Audio utilities
    curve-util/      # Curve utilities
  tests/        # Test targets (opt-in)
  tools/        # Auxiliary tools
scripts/
  cmake/        # CMake utilities (winrc, etc.)
  vcpkg/        # Git submodule - vcpkg overlay ports/triplets
  vcpkg-manifest/ # vcpkg.json
docs/           # Chinese-language dev docs
```

## Code style

- `.clang-format` at repo root: LLVM-based, 4-space indent, 100-column limit, `PointerAlignment: Right`, `NamespaceIndentation: All`, `SortIncludes: Never`.
- C++20 standard, MSVC on Windows.
- `SortIncludes: Never` - do not reorder `#include` directives.

## Key dependencies

Qt 6 (Widgets, Core5Compat, Concurrent, StateMachine, OpenGLWidgets), talcs (audio), opendspx (file format), QWindowKit (frameless window), dsinfer (DNN inference), SDL2, libsndfile, yaml-cpp, language-manager/LangCore (G2p/linguistics).

## Gotchas

- Windows build requires MSVC (not MinGW). Needs `dwmapi.lib` and a recent Windows SDK for DWM APIs.
- `CMAKE_AUTOMOC/AUTORCC/AUTOUIC` are ON for the app target - Qt meta-object files are generated at build time.
- `NOMINMAX` is defined project-wide - do not rely on Windows `min`/`max` macros.
- vcpkg overlay ports are in `scripts/vcpkg/ports` (submodule), not upstream vcpkg.
