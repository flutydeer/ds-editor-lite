---
name: windows-installer
description: 为 DS Editor Lite 构建、检查或排查 Windows x64 DirectML/CUDA Inno Setup 7 安装包。用户要求 Windows 打包、生成安装器、检查打包依赖、复用 staging 重打包、构建 CUDA flavor 或诊断 build-installer.ps1 失败时使用。
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
7. 用户要求 CUDA 版安装包时追加 `-EnableCuda`（见下文「CUDA flavor」），不要手动拼 vcpkg feature 或 CMake 参数。

## 约束

- **默认构建 DML flavor，不主动构建 CUDA 安装包**——仅当用户明确要求 CUDA 版时
  才追加 `-EnableCuda`，不要自行决定打 CUDA 包。
- 默认 DML 不追加任何 vcpkg feature；`cuda12` 只能通过 `-EnableCuda` 开关
  联动（脚本负责 vcpkg feature + `LITE_ENABLE_CUDA=ON` + staging 断言）；
  `cuda11` 已不存在（上游只有 CUDA 12 资产）。
- 不从 build 输出目录直接打包；必须经过 CMake install staging。
- 不提交 VC Redist、`build/`、`dist/` 或生成的 `.iss`。
- 产品元数据只修改 `cmake/ProductMetadata.cmake`。AppId 一经发布不得更换（CUDA 包与 DML 包共用 AppId）。
- 不安装生成的安装包，除非用户明确授权修改本机安装状态。

## CUDA flavor（-EnableCuda）

仅当用户明确要求 CUDA 包时使用；默认（无参数）一律是 DML 包。

```powershell
.\packaging\windows\build-installer.ps1 -VcRedistPath <路径> -EnableCuda
```

- 脚本联动：vcpkg install 追加 `--x-feature=cuda12`，configure 追加
  `-D LITE_ENABLE_CUDA=ON`，`Assert-StagingLayout` 断言 staging 与 flavor 一致
  （cuda 目录缺失或多出都会 throw，报错含修复指引）。
- 产物：`DsEditorLite-<version>-win-x64-cuda-internal.exe`（DML 包仍为
  `-dml-internal`）。AppId/产品元数据与 DML 包一致。
- 体积：cuda 运行时约 290MB（providers_cuda.dll 275MB）；运行机器需 NVIDIA
  GPU + CUDA 12 驱动 + 系统 CUDA 12/cuDNN 9 运行库。

## 便携版（zip）

用户要求"绿色版/便携版/zip 解压即用/带 PDB 可追踪"时，使用
`packaging\windows\build-portable.ps1`（**不使用** build-installer.ps1）：
- 构建 `package-dml-portable` preset（`CMAKE_BUILD_TYPE=RelWithDebInfo`，
  `LITE_ENABLE_CUDA=OFF`，`LITE_ENABLE_FILE_LOG=ON`，独立目录
  `build\PortableDmlRelease`）。
- 必须经 CMake install 到 staging（**不从 build 输出目录直接打包**），再压缩
  staging 的 `bin` 树成 zip。脚本随后补齐 vcpkg 依赖的 PDB；Qt 与 app 的 PDB
  由 install 自带（windeployqt `--pdb` + `LITE_INSTALL_PDB`）。
- 文件名 `DsEditorLite-<yyyyMMdd-HHmm>-win-x64-dml-portable.zip`，时间戳命名，
  **不含版本号**。
- 用 **pwsh**（PowerShell 7）运行；git-bash 调 Windows PowerShell 5.1 会报
  `Get-FileHash is not recognized`。
- 细节见 `packaging/windows/README.md`「便携版（zip）」一节。
