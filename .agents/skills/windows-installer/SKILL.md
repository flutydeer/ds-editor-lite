---
name: windows-installer
description: 为 DS Editor Lite 构建、检查或排查 Windows x64 DirectML/CUDA 安装包，也用于构建带 PDB 的便携 zip。
---

# DS Editor Lite Windows 打包

参数、依赖和产物命名见 [Windows 打包文档](../../../packaging/windows/README.md)。默认构建
DirectML 变体。用户明确要求 CUDA 时传入 `-EnableCuda`。

## 安装包

从仓库根目录运行 [build-installer.ps1](../../../packaging/windows/build-installer.ps1)。

1. 用用户给出的 `-VcRedistPath` 或已设置的 `VC_REDIST_X64` 定位 VC Redist。两者都缺失时索取本机安装程序路径。
2. 用同一变体、VC Redist 路径和 Inno Setup 配置运行 `-CheckPrerequisites`。
3. 移除 `-CheckPrerequisites` 后重新运行，等待脚本完成。
4. 成功后报告安装包的绝对路径、文件大小和 SHA-256。失败时报告实际阻塞构建的错误及处理结果。

`-NoBuild` 只用于源码和构建配置相符的暂存目录。`-SkipVcpkgInstall` 只用于已经同步的依赖树。

## 便携版

从仓库根目录运行 [build-portable.ps1](../../../packaging/windows/build-portable.ps1)。先按打包文档同步
所选变体的依赖。脚本输出 zip 的路径、大小和 SHA-256。

提交时排除 VC Redist、`build/`、`dist/` 和生成的 `.iss`。只有用户明确要求在本机安装时才打开生成的安装程序。
