---
name: CMake Configure
description: 通过项目的 Visual Studio 开发环境和 CMake preset 脚本配置 ds-editor-lite，适用于更新构建树或检查 Qt 和 vcpkg 配置。
---

# CMake Configure

用于在 Windows + Visual Studio 环境下配置 C++/CMake 项目。

## 使用原则

- CMake configure（`cmake --preset debug`）比较轻量，用于验证 Qt/vcpkg 等依赖是否安装正确、检测新增/移除的源文件。
- 如果只需要快速验证依赖，跑 configure 就够了。
- 不要使用 CLion MCP run configuration 做 configure。run configuration 会启动 `DsEditorLite`，GUI 程序不退出时会持续阻塞 Agent。
- 不要手写 Visual Studio 安装路径，也不要使用带 `*` 的 VS DevShell 示例路径。
- 如需完整构建（包含 configure），使用 `cmake-build` skill。

## 推荐命令

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Configure -Preset debug
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
