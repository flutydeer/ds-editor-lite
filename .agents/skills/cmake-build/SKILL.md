---
name: CMake Build
description: 需要构建项目时使用
---

# CMake Build

用于在 Windows + Visual Studio 环境下配置和构建 C++/CMake 项目。

## 使用原则

- 优先使用 CLion MCP 进行构建，它能自动处理 VS DevShell 和 CMake 路径，且保证构建目录与 IDE 一致。
- 如果 CLion 不可用，再回退到手动 CMake presets 命令（见下方备选流程）。
- 不要写死任何个人路径（如 CLion 绑定的 CMake/Ninja 路径）。

## 主要流程：CLion MCP

```json
clion_execute_run_configuration({
    "configurationName": "DsEditorLite",
    "projectPath": "<项目根目录>"
})
```

CLion 会自动处理 VS 开发者命令行初始化、CMake 配置和构建。无需手动干预。

## 备选流程：手动 CMake Presets

如果 CLion MCP 不可用，依次执行以下步骤（在**同一个** PowerShell 会话中）：

### 1. 初始化 VS 2026 x64 开发者命令行环境

方式一（推荐，纯 PowerShell）：
```powershell
Import-Module "C:\Program Files\Microsoft Visual Studio\18\*\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "C:\Program Files\Microsoft Visual Studio\18\*" -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
```
`*` 根据实际安装的 VS 版本（Community / Professional / Enterprise）替换。

方式二（cmd 兼容）：
```cmd
call "C:\Program Files\Microsoft Visual Studio\18\*\VC\Auxiliary\Build\vcvarsall.bat" x64
```

### 2. 配置 CMake

```powershell
cmake --preset debug
```

### 3. 构建

```powershell
cmake --build --preset debug
```

如果项目尚未安装 vcpkg 依赖和配置 Qt 环境变量，需要先参考 AGENTS.md 中的"Configure and build (Windows)"完成前置步骤。CMake 配置时会自动调用 vcpkg 工具链安装依赖（前提是 vcpkg 已经初始化）。
