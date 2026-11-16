---
name: CMake Build
description: 通过项目的 Visual Studio 开发环境和 CMake preset 脚本构建 ds-editor-lite 的 Editor、Connector 或测试。
---

# CMake Build

用于在 Windows + Visual Studio 环境下构建 C++/CMake 项目。

## 使用原则

- 不要使用 CLion MCP run configuration 做构建验证。它会启动 `DsEditorLite` 或 `DsConnectorLite`，进程不退出时会持续阻塞 Agent。
- 不要使用 `execute_run_configuration` 代替 `cmake --build`。
- 不要手写 Visual Studio 安装路径，也不要使用带 `*` 的 VS DevShell 示例路径。
- 使用项目脚本初始化 VS DevShell、Qt 环境变量，并执行 CMake preset。
- 根据改动范围选择目标。省略 `-Target` 时构建 preset 定义的目标。需要构建测试时使用 `-Preset tests`。
- Windows 无法覆盖运行中的可执行文件。如果其他会话的实例占用构建目标，先构建可用目标并报告占用。

## 推荐命令

完整构建验证（先 configure，再 build）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset debug
```

构建已配置的目录：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Build -Preset debug
```

仅构建 Editor：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Build -Preset debug -Target DsEditorLite
```

仅构建 Connector：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Build -Preset debug -Target DsConnectorLite
```

环境探测和配置错误的处理见 [CMake Configure](../cmake-configure/SKILL.md)。
