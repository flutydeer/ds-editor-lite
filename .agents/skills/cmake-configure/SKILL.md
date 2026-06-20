---
name: CMake Configure
description: 需要为 C++/CMake 项目运行 CMake Configure 时使用
---

# CMake Configure

用于在 Windows + Visual Studio 环境下配置 C++/CMake 项目。

## 使用原则

- CMake configure（`cmake --preset debug`）比较轻量，用于验证 Qt/vcpkg 等依赖是否安装正确、检测新增/移除的源文件。
- 如果只需要快速验证依赖，跑 configure 就够了。
- 如需完整构建（包含 configure），使用 `cmake-build` skill（支持 CLion MCP 或手动 presets）。

## 手动流程

在**同一个** PowerShell 会话中执行：

### 1. 初始化 VS 2026 x64 开发者命令行环境

方式一（PowerShell）：
```powershell
Import-Module "C:\Program Files\Microsoft Visual Studio\18\*\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "C:\Program Files\Microsoft Visual Studio\18\*" -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
```
`*` 根据实际安装的 VS 版本（Community / Professional / Enterprise / Insiders）替换。

方式二（cmd）：
```cmd
call "C:\Program Files\Microsoft Visual Studio\18\*\VC\Auxiliary\Build\vcvarsall.bat" x64
```

### 2. 运行 configure

```powershell
cmake --preset debug
```

如果项目尚未安装 vcpkg 依赖和配置 Qt 环境变量，需要先参考 AGENTS.md 中的"配置和构建（Windows presets 主线）"完成前置步骤。

## 关于 CLion MCP

`clion_execute_run_configuration("DsEditorLite")` 会自动包含 configure 步骤，无需单独运行 configure。如果正在使用 CLion MCP 构建，直接使用 `cmake-build` skill 即可。
