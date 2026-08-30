# Windows 安装包

本目录提供 DS Editor Lite Windows x64 DirectML 内测安装包的唯一构建入口。

## 前置条件

- 安装带 x64 C++ 工具链的 Visual Studio 和 Qt 6 MSVC 64-bit。脚本会自动探测并初始化环境。
- 安装 Inno Setup 7，或通过 `-InnoSetupPath` 传入 `ISCC.exe` 的完整路径。
- 安装和卸载界面使用 Inno Setup 7 的动态外观，自动跟随 Windows 深色或浅色主题。
- 从微软官方获取 Visual C++ Redistributable x64 安装程序，保存在仓库外。

默认（DML）安装包使用默认 vcpkg manifest，不启用 `cuda12` feature；CUDA 包通过
`-EnableCuda` 由脚本统一追加该 feature（见下文「CUDA flavor」一节）。

## 构建

推荐使用环境变量保存 VC Redist 路径：

```powershell
$env:VC_REDIST_X64 = "D:\SDK\VC_redist\VC_redist.x64.exe"
.\packaging\windows\build-installer.ps1
```

只检查依赖，不执行构建：

```powershell
.\packaging\windows\build-installer.ps1 -CheckPrerequisites
```

也可以显式传入路径和输出目录：

```powershell
.\packaging\windows\build-installer.ps1 `
  -VcRedistPath "D:\SDK\VC_redist\VC_redist.x64.exe" `
  -StageDir dist\stage\dml `
  -OutputDir dist\installer `
  -InnoSetupPath "C:\Program Files\Inno Setup 7\ISCC.exe"
```

`-NoBuild` 复用已有 staging，仅重新生成安装包。`-SkipVcpkgInstall` 跳过 vcpkg
依赖同步，只有在本地依赖已经与 manifest 一致时才应使用。

产物位置：

```text
dist\installer\DsEditorLite-<version>-win-x64-dml-internal.exe
```

默认安装目录：

```text
C:\Program Files\OpenVPI\DS Editor Lite
```

## 构建流程

1. 自动初始化 Visual Studio x64 和 Qt 环境。
2. 从 `scripts\vcpkg-manifest` 安装默认 DML 依赖。
3. 配置并构建 `package-dml-release` CMake preset。
4. 通过 CMake install 写入 `dist\stage\dml`。
5. 校验 Qt 插件、应用资源、FillLyric 配置、synthrt 插件和 G2P 数据。
6. 仅将 staging 的 `bin` 目录交给 Inno Setup 7，排除头文件、导入库和 CMake 包文件。
7. 输出安装包路径和 SHA-256。

## 便携版（zip）

`package-dml-portable` preset（RelWithDebInfo）配合
`packaging\windows\build-portable.ps1` 产出解压即用的便携 zip，内置 PDB
符号，崩溃可追踪。

```powershell
.\packaging\windows\build-portable.ps1
```

或复用已有构建产物、只重新打包：

```powershell
.\packaging\windows\build-portable.ps1 -NoBuild
```

特性：

- RelWithDebInfo，优化 + 调试符号；MSVC `/Zi` 的 PDB 直接产出在 `out\bin`。
- 必须经 CMake install 到 staging（**不从 build 输出目录直接打包**），再压缩
  staging 的 `bin` 树。install 自带 app 与 Qt 的 PDB（`LITE_INSTALL_PDB` +
  windeployqt `--pdb`）、插件及其运行时的 PDB（vcpkg 侧 `vcpkg_copy_pdbs`
  覆盖插件目录；ort/DirectML 运行时的 PDB 由 install 层按配置从
  `share/onnxruntime-builds/pdb/<flavor>` 补齐，Release 安装包则不带任何
  PDB）；脚本随后从 `vcpkg\installed\x64-windows\bin` 按名补齐
  各 vcpkg 依赖的 PDB（此逻辑刻意留在打包脚本里，不进 CMake，保持构建系统
  不感知 vcpkg）。
- zip 会校验 DsEditorLite.exe 与至少一个 PDB 存在，否则报错退出。
- 文件名为 `DsEditorLite-<yyyyMMdd-HHmm>-win-x64-dml-portable.zip`（时间戳，**不含版本号**）。
- 建议用 **pwsh**（PowerShell 7）运行；从 git-bash 调 Windows PowerShell 5.1
  会出现 `Get-FileHash is not recognized` 环境问题。
- 产物目录：`dist\portable\`，脚本输出路径、大小与 SHA-256。

## 产品元数据

产品名称、版本、发布者、版权、URL、可执行文件名、安装器 AppId 和 macOS BundleId
统一在 `cmake\ProductMetadata.cmake` 中维护。CMake 会据此生成应用头文件和打包 JSON；
不要在 C++、PowerShell 或 Inno 模板中重复定义。

安装器 AppId 发布后必须保持稳定，否则 Windows 会将新版本视为另一个应用。

## 签名与当前范围

签名默认关闭。脚本预留 `-SignToolPath`、`-SignCertThumbprint` 和 `-TimestampUrl`。

- 当前不配置自定义应用或安装器图标。
- 当前不注册 `.dspx` 文件关联。
- 当前不构建 CUDA 安装包（默认不打包 cuda；仅在用户主动要求时用 `-EnableCuda`，见下文）。

## CUDA flavor（-EnableCuda，仅按需使用）

**默认不构建 CUDA 安装包**——日常打包一律走上面的 DML 流程。仅当用户明确要求
CUDA 包时，才在任一脚本上追加 `-EnableCuda`：

```powershell
.\packaging\windows\build-installer.ps1 -VcRedistPath "D:\SDK\VC_redist\VC_redist.x64.exe" -EnableCuda
.\packaging\windows\build-portable.ps1 -EnableCuda
```

- `-EnableCuda` 由脚本统一联动，不手动拼 vcpkg/CMake 参数：build-installer.ps1
  的 vcpkg install 会追加 `--x-feature=cuda12`（build-portable.ps1 不跑
  vcpkg，需先用 `update-vcpkg-win.bat cuda12` 把 installed 树同步成 cuda12）；
  configure 追加 `-D LITE_ENABLE_CUDA=ON`；staging 校验断言 staging 内容与
  flavor 一致（多出或缺失 `runtimes/onnx/cuda` 都会报错，带修复指引）。
- 产物命名区分 flavor：`DsEditorLite-<version>-win-x64-cuda-internal.exe`、
  `DsEditorLite-<timestamp>-win-x64-cuda-portable.zip`；AppId 与产品元数据
  与 DML 包共用，不随 flavor 变化。
- 体积代价：cuda 运行时约 290MB（`onnxruntime_providers_cuda.dll` 一项 275MB）。
- 运行机器需 NVIDIA GPU、支持 CUDA 12 的驱动，以及系统级 CUDA 12 / cuDNN 9
  运行库（ort 的 cuda 资产不自带）。
