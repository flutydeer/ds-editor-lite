# Windows 安装包

本目录提供 DS Editor Lite Windows x64 DirectML 内测安装包的唯一构建入口。

## 前置条件

- 安装带 x64 C++ 工具链的 Visual Studio 和 Qt 6 MSVC 64-bit。脚本会自动探测并初始化环境。
- 安装 Inno Setup 7，或通过 `-InnoSetupPath` 传入 `ISCC.exe` 的完整路径。
- 安装和卸载界面使用 Inno Setup 7 的动态外观，自动跟随 Windows 深色或浅色主题。
- 从微软官方获取 Visual C++ Redistributable x64 安装程序，保存在仓库外。

DML 安装包使用默认 vcpkg manifest，不要启用 `cuda11` 或 `cuda12` feature。

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

## 绿色版（便携 zip）

`package-dml-green` preset（RelWithDebInfo）配合
`packaging\windows\build-green-zip.ps1` 产出解压即用的便携 zip，内置 PDB
符号，崩溃可追踪。

```powershell
.\packaging\windows\build-green-zip.ps1
```

或复用已有构建产物、只重新打包：

```powershell
.\packaging\windows\build-green-zip.ps1 -NoBuild
```

特性：

- RelWithDebInfo，优化 + 调试符号；MSVC `/Zi` 的 PDB 直接产出在 `out\bin`。
- 直接压缩 `build\GreenDmlRelease\out\bin`（不走 CMake install，保住 PDB）。
- zip 会校验 DsEditorLite.exe 与至少一个 PDB 存在，否则报错退出。
- 文件名为 `DsEditorLite-<yyyyMMdd-HHmm>-win-x64-dml-green.zip`（时间戳，**不含版本号**）。
- 建议用 **pwsh**（PowerShell 7）运行；从 git-bash 调 Windows PowerShell 5.1
  会出现 `Get-FileHash is not recognized` 环境问题。
- 产物目录：`dist\green\`，脚本输出路径、大小与 SHA-256。

## 产品元数据

产品名称、版本、发布者、版权、URL、可执行文件名、安装器 AppId 和 macOS BundleId
统一在 `cmake\ProductMetadata.cmake` 中维护。CMake 会据此生成应用头文件和打包 JSON；
不要在 C++、PowerShell 或 Inno 模板中重复定义。

安装器 AppId 发布后必须保持稳定，否则 Windows 会将新版本视为另一个应用。

## 签名与当前范围

签名默认关闭。脚本预留 `-SignToolPath`、`-SignCertThumbprint` 和 `-TimestampUrl`。

- 当前不配置自定义应用或安装器图标。
- 当前不注册 `.dspx` 文件关联。
- 当前不构建 CUDA 安装包。
