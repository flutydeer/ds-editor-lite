---
name: windows-installer
description: 为 DS Editor Lite 构建、检查或排查 Windows x64 DirectML Inno Setup 7 安装包。用户要求 Windows 打包、生成安装器、检查打包依赖、复用 staging 重打包或诊断 build-installer.ps1 失败时使用。
---

# DS Editor Lite Windows Installer

使用项目的确定性打包脚本，不自行拼装 CMake、vcpkg、install 或 Inno 命令。

## 流程

1. 读取 `packaging/windows/README.md`，确认当前参数和产物位置。
2. 查找 VC Redist：优先使用用户提供的 `-VcRedistPath`，其次使用 `VC_REDIST_X64`。缺失时明确请求官方 `vc_redist.x64.exe` 的本机路径。
3. 在执行完整打包前运行依赖检查：

```powershell
.\packaging\windows\build-installer.ps1 -CheckPrerequisites
```

4. 完整打包执行：

```powershell
.\packaging\windows\build-installer.ps1
```

5. 仅在当前 staging 已由同一源码和配置生成时使用 `-NoBuild`。仅在确认 manifest 依赖已同步时使用 `-SkipVcpkgInstall`。
6. 等待脚本完全结束。成功后报告安装包绝对路径、大小和 SHA-256；失败时报告第一个实际错误及修复结果。

## 约束

- 仅构建默认 DML manifest，不追加 `cuda11` 或 `cuda12` feature。
- 不从 build 输出目录直接打包；必须经过 CMake install staging。
- 不提交 VC Redist、`build/`、`dist/` 或生成的 `.iss`。
- 产品元数据只修改 `cmake/ProductMetadata.cmake`。AppId 一经发布不得更换。
- 不安装生成的安装包，除非用户明确授权修改本机安装状态。

## 绿色版（便携 zip）

用户要求"绿色版/便携版/zip 解压即用/带 PDB 可追踪"时，使用
`packaging\windows\build-green-zip.ps1`（**不使用** build-installer.ps1）：
- 构建 `package-dml-green` preset（`CMAKE_BUILD_TYPE=RelWithDebInfo`，
  `LITE_ENABLE_CUDA=OFF`，`LITE_ENABLE_FILE_LOG=ON`，独立目录
  `build\GreenDmlRelease`）。
- 从 `build\GreenDmlRelease\out\bin` 直接打 zip（不走 CMake install，保住 PDB）。
- 文件名 `DsEditorLite-<yyyyMMdd-HHmm>-win-x64-dml-green.zip`，时间戳命名，
  **不含版本号**。
- 用 **pwsh**（PowerShell 7）运行；git-bash 调 Windows PowerShell 5.1 会报
  `Get-FileHash is not recognized`。
- 细节见 `packaging/windows/README.md`「绿色版（便携 zip）」一节。
