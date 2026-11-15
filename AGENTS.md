# AGENTS.md

## Project overview

DS Editor Lite is a C++20 / Qt 6 Widgets desktop application (singing voice editor). The build target is `DsEditorLite`. Docs are written in Chinese; code comments and commit messages are in English.

## Build system

- **CMake** with **vcpkg** for dependency management.
- Clone with `--recursive` (git submodules: `src/3rdparty/qtmediate`, `scripts/vcpkg`).
- vcpkg manifest lives at `scripts/vcpkg-manifest/vcpkg.json`, not the repo root.
- Toolchain file: `vcpkg/scripts/buildsystems/vcpkg.cmake`.

### 前置条件

- **Visual Studio 2026**：安装"使用 C++ 的桌面开发"工作负荷，包含 MSVC v145 生成工具和 Windows 11 SDK。
- **Qt 6**：安装 Qt 6.11.1+，选择 MSVC 2022 64-bit、Qt State Machines 和 Qt HTTP Server 组件。
- **vcpkg**：克隆到项目同级目录或全局安装，详见下方 vcpkg 安装步骤。

### 配置和构建（Windows presets 主线）

Agent 做 CMake 配置/构建验证时，优先加载 `.agents/skills/cmake-configure` 与 `.agents/skills/cmake-build` skill，不要使用 CLion MCP run configuration 代替构建。

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

可选 CUDA 支持：在 vcpkg install 后追加 `--x-feature=cuda12`。

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

#### Testing principles

Tests protect observable behavior and explicit contracts. Before adding a test, identify the functional failure it would catch. Normal extensions or refactors should not require changing tests whose contracts remain intact.

- Assert meaningful outcomes rather than freezing component inventories, incidental counts or ordering, private helper names, internal field layouts, or exact call sequences, unless they are part of a required contract.
- Test supported behavior directly, not merely the absence of obsolete fields or implementations. Do not derive expected results from the implementation under test, copy its algorithm, or mock away meaningful behavior and only verify arranged calls.
- Cover shared rules in the layer that owns them. Consumer tests should verify integration and consumer-specific behavior instead of duplicating the same cases.
- Add cases for credible failure modes and explicitly supported boundaries, not unreachable states, exhaustive combinations, or exotic inputs solely to raise coverage. Retain real regression coverage and name it after the behavior protected, not conversations, patches, or debugging history.
- Keep ordinary tests offline and deterministic. Integration tests requiring externally supplied models or other real resources should be opt-in and use supplied resources rather than downloading them on each run. Coordinate asynchronous work with explicit synchronization and bounded, event-aware waits rather than fixed sleeps.

### Windows installer

- `cmake/ProductMetadata.cmake` is the single source of truth for the product name, version, publisher, copyright, URL, executable name, and installer AppId. Do not duplicate these values in C++, PowerShell, or Inno Setup templates.
- Build the Windows x64 DirectML installer only through `packaging/windows/build-installer.ps1`. The script initializes the VS x64 and Qt environments, runs vcpkg and the CMake configure/build/install stages, and invokes Inno Setup 7.
- Supply the VC++ Redistributable through `-VcRedistPath` or the `VC_REDIST_X64` environment variable. Do not commit it to the repository.
- To check packaging prerequisites only:

```powershell
.\packaging\windows\build-installer.ps1 -CheckPrerequisites
```

- To run the complete packaging workflow:

```powershell
.\packaging\windows\build-installer.ps1 -VcRedistPath D:\SDK\VC_redist\VC_redist.x64.exe
```

- When asked to package a Windows installer, agents must use `.agents/skills/windows-installer/SKILL.md` instead of assembling a separate build or Inno Setup command sequence.

## Repository structure

```
src/
  app/          # Main application (DsEditorLite target)
    Automation/ Bootstrap/ Controller/ Global/ Interface/ Model/ Modules/
    Resources/ UI/ Utils/
  libs/         # Internal libraries (ADT, Core, GUI, History, Language,
                #   MusicBase, PackageManager, ProjectConverters, ProjectModel,
                #   Support, SynthrtEngine, Tasking)
  3rdparty/     # curve-util; qtmediate (Git submodule - UI framework)
  tests/        # Test targets (opt-in via LITE_BUILD_TESTS)
  tools/        # Auxiliary tools
scripts/
  cmake/        # CMake utilities (LiteBuildApi, OrtRuntimeGate, winrc, etc.)
  vcpkg/        # Git submodule - vcpkg overlay ports/triplets
  vcpkg-manifest/ # vcpkg.json
docs/           # Chinese-language dev docs (design/ = 已完成的设计契约, plans/ = 进行中方案, guides/ = 开发指南, theme/ = 主题文档, archive/ = 历史归档)
```

## Code style

- `.clang-format` at repo root: LLVM-based, 4-space indent, 100-column limit, `PointerAlignment: Right`, `NamespaceIndentation: All`, `SortIncludes: Never`.
- C++20 standard, MSVC on Windows.
- `SortIncludes: Never` - do not reorder `#include` directives.

## Implementation principles

Reuse existing behavior and logic first. When direct reuse is not suitable, consider extending or refactoring the existing implementation before introducing another one. Do not invent replacement behavior or maintain parallel implementations unless necessary and supported by a clear rationale.

## Comments and documentation

Write for readers who have no knowledge of the conversation or implementation process. Describe the current behavior as a coherent whole. Favor stable purposes, contracts, and constraints over changing implementation inventories, without discarding concrete details readers need.

- Keep conversation context, review replies, debugging notes, obsolete workarounds, and task-specific validation logs out of code comments and product documentation. Put detailed change history in PR descriptions or a dedicated changelog when needed.
- Integrate changes into the relevant existing explanation. Rewrite or remove outdated passages instead of appending isolated notes about the latest change. Keep examples, parameter descriptions, and resource instructions consistent with the implementation.
- Keep comments short and focused on non-obvious reasoning, invariants, constraints, or tradeoffs. Avoid lengthy explanations, restating the code, narrating edits, or comparing against an obsolete implementation.
- All code comments must use ASCII characters unless a special quotation requires preserving non-ASCII text. Use ASCII punctuation such as `->`, `-`, and `'`.
- Keep user documentation focused on usage, supported behavior, prerequisites, and meaningful limitations. Include internal details only when they help the intended reader understand or use the system.
- Use concise, factual language. Avoid promotional claims, exaggerated guarantees, and assertions of complete support or equivalence that the implementation does not establish.
- In overview prose and cross-module descriptions, avoid inventories of current callers, backends, or dependencies. Keep changing facts in their authoritative source and link to package metadata, workflow definitions, or the relevant reference rather than repeating dependency lists, CI matrices, test inventories, or release numbers. Prefer version-independent examples.
- Preserve accurate comments explaining nearby code, including index ranges, data shapes, algorithm steps, and local helper relationships. These can be maintained with the code they describe. Do not generalize or delete them solely because implementation details may change. Retain concrete API/resource requirements, protocol versions, and measurements where readers need them.
- Preserve required attribution and licensing notices, along with explanations of constraints that still apply. Cleanup should remove noise without discarding useful technical context.

## Writing style

- 中文里不要用分号断句。该断就用句号，该并列就用逗号或顿号。
- 这条对提交信息、代码注释、文档、回复一律适用。

## Git commits

- Commit messages must be concise and contain exactly one English line, with no body or footer. Use Conventional Commits in the format `<type>(<scope>): <description>` (e.g. `feat(editor): ...`, `fix(automation): ...`).
- Never add AI attribution lines to commit messages (e.g. `Co-Authored-By`, `Claude-Session`).

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
