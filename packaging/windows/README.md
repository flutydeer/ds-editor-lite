# Windows 安装包

使用本目录的脚本构建 DS Editor Lite 的 Windows x64 安装包或便携版。默认构建 DirectML 变体。

## 前置条件

- 安装带 x64 C++ 工具链的 Visual Studio 和 Qt 6 MSVC 64-bit。脚本会探测并初始化环境。
- 构建安装包需要 Inno Setup 7。可通过 `-InnoSetupPath` 传入 `ISCC.exe` 的完整路径。
- 构建安装包需要微软提供的 Visual C++ Redistributable x64 安装程序，保存在仓库外。

## 安装包

从仓库根目录运行，可用环境变量或 `-VcRedistPath` 指定 VC Redist 路径。

```powershell
$env:VC_REDIST_X64 = "D:\SDK\VC_redist\VC_redist.x64.exe"
.\packaging\windows\build-installer.ps1
```

用 `-CheckPrerequisites` 仅检查打包环境。用 `-StageDir` 和 `-OutputDir` 自定义暂存和产物目录。用
`-NoBuild` 复用已有的暂存目录。使用 `-SkipVcpkgInstall` 前，需将本地依赖同步至 manifest 所选配置。

默认产物位于 `dist\installer`，文件名为 `DsEditorLite-<version>-win-x64-dml-internal.exe`。
脚本输出路径和 SHA-256。

安装需要管理员权限，默认安装到系统 Program Files 下的产品目录。安装和卸载界面跟随
Windows 主题。安装器根据系统运行库的安装状态决定是否运行随包提供的 VC Redist。
安装器提供开始菜单和可选的桌面快捷方式，`.dspx` 文件关联需在 Windows 中自行设置。

签名默认关闭。启用签名时同时传入 `-SignToolPath` 和 `-SignCertThumbprint`，可用
`-TimestampUrl` 指定时间戳服务器。

## 便携版

便携版使用带调试符号的构建配置，压缩包包含用于分析崩溃的 PDB。按[构建说明](../../AGENTS.md#配置和构建windows-presets-主线)准备 vcpkg 依赖后，从仓库根目录运行。

```powershell
.\packaging\windows\build-portable.ps1
```

脚本将 CMake 安装到暂存目录的 `bin` 树打包，并检查其中的应用可执行文件和 PDB。用 `-NoBuild`
复用已有构建产物。默认产物位于 `dist\portable`，文件名为
`DsEditorLite-<yyyyMMdd-HHmm>-win-x64-dml-portable.zip`。脚本输出路径、大小和 SHA-256。

## CUDA 变体

在打包脚本上添加 `-EnableCuda`。

```powershell
.\packaging\windows\build-installer.ps1 -VcRedistPath "D:\SDK\VC_redist\VC_redist.x64.exe" -EnableCuda
.\packaging\windows\build-portable.ps1 -EnableCuda
```

- 安装包脚本会用 `cuda12` 特性同步依赖。打包便携版以前，在[依赖更新脚本](../../docs/dev-scripts/update-vcpkg-win.bat)中设置本机的 `QT_DIR`，从仓库根目录运行 `docs\dev-scripts\update-vcpkg-win.bat cuda12`。
- 两个脚本都会检查暂存目录中的 CUDA 运行时是否符合所选变体。CUDA 产物文件名用 `cuda` 标识。
- 运行 CUDA 变体需要 NVIDIA GPU、支持 CUDA 12 的驱动以及系统级 CUDA 12 和 cuDNN 9 运行库。

## 产品元数据

修改产品或安装器标识时，在[产品元数据](../../cmake/ProductMetadata.cmake)中维护。DirectML 和
CUDA 安装包共用同一 AppId。发布后保持 AppId 稳定，Windows 才会将后续版本识别为同一应用。
构建选项以 [CMake presets](../../CMakePresets.json) 为准。
