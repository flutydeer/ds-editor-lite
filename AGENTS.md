# AGENTS.md

## Project overview

DS Editor Lite is a C++20 / Qt 6 Widgets singing voice editor. The main build target is `DsEditorLite`. Write documentation in Chinese and code comments and commit messages in English.

## Build system

本项目使用 CMake 和 vcpkg。克隆仓库时使用 `--recursive` 初始化 [.gitmodules](.gitmodules) 中的子模块。
上游 vcpkg 工具放在仓库根目录下的 `vcpkg/`，依赖和覆盖端口在[项目清单](scripts/vcpkg-manifest/vcpkg.json)中维护。

### 前置条件

- Visual Studio 2026，安装“使用 C++ 的桌面开发”工作负荷、MSVC v145 工具链和 Windows 11 SDK。
- Qt 6.11.1 或更新版本，安装 MSVC 2022 64-bit 套件。所需 Qt 组件见[测试工作流](.github/workflows/tests.yml)。
- 首次配置时，在仓库根目录执行 `git clone https://github.com/microsoft/vcpkg.git vcpkg`，然后执行 `.\vcpkg\bootstrap-vcpkg.bat`。复现 CI 时使用 [bootstrap 脚本](scripts/ci/bootstrap-vcpkg.py)指定的版本。

### 配置和构建（Windows presets 主线）

在仓库根目录运行 [构建脚本](.agents/skills/scripts/run-cmake-preset.ps1)。脚本会初始化 Visual Studio x64 开发环境，
探测 Qt 安装目录，也可用 `-QtDir` 指定实际路径。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Dependencies -Preset debug
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset debug
```

首次构建或 manifest 更新后同步依赖。构建 Release 时使用 `-Preset release`。
需要单独配置或构建时分别使用 `-Mode Configure` 或 `-Mode Build`。可用的构建目标和配置见 [CMake presets](CMakePresets.json)。

Agent 执行 CMake 验证时，使用 [cmake-configure](.agents/skills/cmake-configure/SKILL.md)
或 [cmake-build](.agents/skills/cmake-build/SKILL.md) skill。单独配置使用 `cmake-configure`，构建使用 `cmake-build`。

### Tests

Tests live in [src/tests](src/tests) and require `-DLITE_BUILD_TESTS=ON`. See the [test guide](docs/automation/04-testing-and-ci/test-plan.md) for presets, resource requirements, and coverage.

#### Testing principles

Tests protect observable behavior and explicit contracts. Before adding a test, identify the functional failure it would catch. Normal extensions or refactors should not require changing tests whose contracts remain intact.

- Assert meaningful outcomes rather than freezing component inventories, incidental counts or ordering, private helper names, internal field layouts, or exact call sequences, unless they are part of a required contract.
- Test supported behavior directly, not merely the absence of obsolete fields or implementations. Do not derive expected results from the implementation under test, copy its algorithm, or mock away meaningful behavior and only verify arranged calls.
- Cover shared rules in the layer that owns them. Consumer tests should verify integration and consumer-specific behavior instead of duplicating the same cases.
- Add cases for credible failure modes and explicitly supported boundaries, not unreachable states, exhaustive combinations, or exotic inputs solely to raise coverage. Retain real regression coverage and name it after the behavior protected, not conversations, patches, or debugging history.
- Keep ordinary tests offline and deterministic. Integration tests requiring externally supplied models or other real resources should be opt-in and use supplied resources rather than downloading them on each run. Coordinate asynchronous work with explicit synchronization and bounded, event-aware waits rather than fixed sleeps.

### GUI tests

- Prefer running GUI tests inside Windows Sandbox. When `Containers-DisposableClientVM` is installed and `HypervisorPresent` is true, use `.agents/skills/windows-sandbox-gui-tests` for payload staging, launch, monitoring, evidence collection, and shutdown.
- If the sandbox is unavailable, fall back to the current desktop silently. Do not ask for confirmation and do not wait for approval: real windows will open and may take focus, which unattended automation must tolerate.
- The fallback uses the same skill: `Invoke-HostGuiTrial.ps1 -AllowHostGui` runs the built binary in place and records the same evidence under `build/SandboxGuiTrial/host-results`.
- End-to-end smoke flows (launch the app, load a voicebank, draw notes, verify synthesis and playback, save DSPX) belong to `gui-smoke-test` and prefer the sandbox for the same reason.

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
  app/          # Editor and automation service
  connector/    # MCP connector
  libs/         # Internal libraries
  3rdparty/     # Bundled libraries and UI framework submodule
  tests/        # Test suites and fixtures
  tools/        # Auxiliary tools
cmake/          # CMake utilities and product metadata
scripts/        # Build and test scripts, vcpkg manifest and overlays
docs/           # 设计、方案、使用指南、主题和阶段归档
```

## Code style

- `.clang-format` at repo root: LLVM-based, 4-space indent, 100-column limit, `PointerAlignment: Right`, `NamespaceIndentation: All`, `SortIncludes: Never`.
- C++20 standard, MSVC on Windows.
- `SortIncludes: Never` - do not reorder `#include` directives.

## Implementation principles

Reuse existing behavior and logic first. When direct reuse is not suitable, consider extending or refactoring the existing implementation before introducing another one. Do not invent replacement behavior or maintain parallel implementations unless necessary and supported by a clear rationale.

## Comments, documentation, and PR descriptions

Write for readers who have no knowledge of the conversation or implementation process. Describe the current behavior or the changes introduced by the PR as a coherent whole. Keep accounts of the implementation and revision process out of PR descriptions. Favor stable purposes, contracts, and constraints over changing implementation inventories, without discarding concrete details readers need.

- Keep conversation context, review replies, debugging notes, obsolete workarounds, and task-specific validation logs out of code comments, product documentation, and PR descriptions.
- Integrate changes into the relevant existing explanation. Rewrite or remove outdated passages instead of appending isolated notes about the latest change. Keep examples, parameter descriptions, and resource instructions consistent with the implementation.
- Keep comments short and focused on non-obvious reasoning, invariants, constraints, or tradeoffs. Avoid lengthy explanations, restating the code, narrating edits, or comparing against an obsolete implementation.
- Preserve brief warnings about failure modes that still apply. State the trigger and consequence beside the affected code, including platform or API behavior that makes an otherwise plausible change fail.
- All code comments must use ASCII characters unless a special quotation requires preserving non-ASCII text. Use ASCII punctuation such as `->`, `-`, and `'`.
- Keep user documentation focused on usage, supported behavior, prerequisites, and meaningful limitations. Include internal details only when they help the intended reader understand or use the system.
- Use concise, factual language. Avoid promotional claims, exaggerated guarantees, and assertions of complete support or equivalence that the implementation does not establish.
- In overview prose and cross-module descriptions, avoid inventories of current callers, backends, or dependencies. Keep changing facts in their authoritative source and link to package metadata, workflow definitions, or the relevant reference rather than repeating dependency lists, CI matrices, test inventories, or release numbers. Prefer version-independent examples. In PR descriptions, prefer references that stay current as commits are added.
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

## Gotchas

- Windows build requires MSVC (not MinGW). Needs `dwmapi.lib` and a recent Windows SDK for DWM APIs.
- `CMAKE_AUTOMOC/AUTORCC/AUTOUIC` are ON for the app target - Qt meta-object files are generated at build time.
- `NOMINMAX` is defined project-wide - do not rely on Windows `min`/`max` macros.
- The [manifest](scripts/vcpkg-manifest/vcpkg.json) defines overlay precedence. See the [local overlay guide](scripts/vcpkg-project/README.md) before updating a patch.
