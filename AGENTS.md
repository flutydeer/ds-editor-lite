# AGENTS.md

## Project overview

DS Editor Lite is a C++20 / Qt 6 Widgets desktop application (singing voice editor). The build target is `DsEditorLite`. Primary language in docs and comments is Chinese.

## Build system

- **CMake** with **vcpkg** for dependency management.
- Clone with `--recursive` (git submodules: `src/libs/qtmediate`, `scripts/vcpkg`).
- vcpkg manifest lives at `scripts/vcpkg-manifest/vcpkg.json`, not the repo root.
- Toolchain file: `vcpkg/scripts/buildsystems/vcpkg.cmake`.

### 前置条件

- **Visual Studio 2026**：安装"使用 C++ 的桌面开发"工作负荷，包含 MSVC v145 生成工具和 Windows 11 SDK。
- **Qt 6**：安装 Qt 6.11.1+，选择 MSVC 2022 64-bit 组件和 Qt State Machines。
- **vcpkg**：克隆到项目同级目录或全局安装，详见下方 vcpkg 安装步骤。

### 配置和构建（Windows presets 主线）

本项目使用 CMake presets 作为标准构建入口。以下步骤在**同一个**命令行会话中执行：

#### 1. 初始化 VS 2026 x64 开发者环境

**PowerShell**：
```powershell
Import-Module "C:\Program Files\Microsoft Visual Studio\18\*\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "C:\Program Files\Microsoft Visual Studio\18\*" -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
```
（`*` 根据实际 VS 版本替换为 `Community` / `Professional` / `Enterprise`）

**cmd**：
```cmd
call "C:\Program Files\Microsoft Visual Studio\18\*\VC\Auxiliary\Build\vcvarsall.bat" x64
```

#### 2. 设置 Qt 环境变量（每次新会话都需要）

```cmd
set QT_DIR=C:\Qt\6.11.1\msvc2022_64
set Qt6_DIR=%QT_DIR%
set CMAKE_PREFIX_PATH=%QT_DIR%
set VCPKG_KEEP_ENV_VARS=QT_DIR;Qt6_DIR;Qt6GuiTools_DIR;CMAKE_PREFIX_PATH
```

#### 3. 安装/更新 vcpkg 依赖

```cmd
cd vcpkg
vcpkg install --x-manifest-root=../scripts/vcpkg-manifest --x-install-root=./installed --triplet=x64-windows
cd ..
```

可选 CUDA 支持：在 vcpkg install 后追加 `--x-feature=cuda12` 或 `--x-feature=cuda11`。

#### 4. CMake 配置

```cmd
cmake --preset debug
```

#### 5. 构建

```cmd
cmake --build --preset debug
```

也可以使用 release preset：
```cmd
cmake --preset release && cmake --build --preset release
```

#### 备选：手动 CMake 命令

如果 presets 不可用，可以用等价的命令行：
```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_INSTALL_PREFIX=build/install -G Ninja
cmake --build build --target DsEditorLite
```

#### 更新 vcpkg 依赖

当 `scripts/vcpkg-manifest/vcpkg.json` 发生变更（如新增依赖或版本升级）时，需要重新安装以使本地的 `vcpkg/installed` 与 manifest 同步。重新运行 vcpkg install 命令即可，vcpkg 会自动对比差异并增量更新：

```cmd
cd vcpkg
vcpkg install --x-manifest-root=../scripts/vcpkg-manifest --x-install-root=./installed --triplet=x64-windows
```

也可使用 `docs/dev-scripts/` 下的辅助脚本（复制到项目根目录并修改 Qt 路径后使用）。

### Tests

Tests are gated behind `-DLITE_BUILD_TESTS=ON` (enabled by default in the `debug` preset). They live in `src/tests/`.

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

## CodeGraph MCP

在 Agent 的 MCP 配置中添加以下内容：

```json
{
  "mcpServers": {
    "codegraph": {
      "type": "stdio",
      "command": "codegraph",
      "args": ["serve", "--mcp"]
    }
  }
}
```

需指定项目路径时：

```json
{
  "mcpServers": {
    "codegraph": {
      "type": "stdio",
      "command": "codegraph",
      "args": ["serve", "--mcp", "--path", "path/to/ds-editor-lite"]
    }
  }
}
```

## Key dependencies

Qt 6 (Widgets, Core5Compat, Concurrent, StateMachine, OpenGLWidgets), talcs (audio), opendspx (file format), QWindowKit (frameless window), dsinfer (DNN inference), SDL2, libsndfile, yaml-cpp, language-manager/LangCore (G2p/linguistics).

## Gotchas

- Windows build requires MSVC (not MinGW). Needs `dwmapi.lib` and a recent Windows SDK for DWM APIs.
- `CMAKE_AUTOMOC/AUTORCC/AUTOUIC` are ON for the app target - Qt meta-object files are generated at build time.
- `NOMINMAX` is defined project-wide - do not rely on Windows `min`/`max` macros.
- vcpkg overlay ports are in `scripts/vcpkg/ports` (submodule), not upstream vcpkg.
