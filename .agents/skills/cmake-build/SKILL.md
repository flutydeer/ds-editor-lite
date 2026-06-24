---
name: CMake Build
description: 需要在 ds-editor-lite 中运行 CMake 构建或完整构建验证时使用。默认使用项目提供的 VS DevShell + CMake preset 脚本；不要使用 CLion MCP run configuration 代替构建。
---

# CMake Build

用于在 Windows + Visual Studio 环境下构建 C++/CMake 项目。

## 使用原则

- 不要使用 CLion MCP run configuration 做构建验证。run configuration 会启动 `DsEditorLite`，GUI 程序不退出时会持续阻塞 Agent。
- 不要使用 `execute_run_configuration("DsEditorLite")` 代替 `cmake --build`。
- 不要手写 Visual Studio 安装路径，也不要使用带 `*` 的 VS DevShell 示例路径。
- 使用项目脚本初始化 VS DevShell、Qt 环境变量，并执行 CMake preset。

## 推荐命令

完整构建验证（先 configure，再 build）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset debug
```

仅增量 build（确认本轮已经 configure 过时使用）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Build -Preset debug
```

构建单个目标：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Build -Preset debug -Target DsEditorLite
```

## 脚本行为

- 通过 `vswhere.exe` 优先查找 Visual Studio 2026 / 18.x 的 C++ 工具链。
- 如果 `vswhere` 找不到 VS 18.x，会先回退到安装了 C++ 工具链的最新 VS，再枚举 `C:\Program Files\Microsoft Visual Studio\18|17\*\VC\Auxiliary\Build\vcvarsall.bat`。
- 使用 `vcvarsall.bat x64` 初始化编译环境。
- 若 `QT_DIR` 未设置，会尝试自动选择 `C:\Qt\*\msvc2022_64` 中版本号最高的目录。
- 在同一个 `cmd.exe` 命令链里执行环境初始化和 CMake，避免 VS DevShell 环境丢失。

## 失败处理

- 找不到 VS、Qt 或 preset 失败时，直接报告脚本输出中的具体错误。
- 不要改用 CLion run configuration 兜底。
- 不要改用硬编码的个人路径兜底。
