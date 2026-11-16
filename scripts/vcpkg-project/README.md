# 项目覆盖端口

## 1. 范围与优先级

此目录优先于共享 `scripts/vcpkg/ports`，仅承载当前应用必须使用、共享端口或 vcpkg 基线尚未包含的修复与配方。vcpkg 覆盖端口需要完整配方，因此保留原依赖和构建选项。每个库仍只链接一份，不维护平行库实现。

## 2. talcs

共享端口的 talcs 配方仍停在上游 `v0.1.0`。本项目使用上游 `2ad6028f`（2026-11-14，DiffScope 固定的同一提交），因此在此保留完整配方：`REF` 指向该提交，构建 `talcs::Device`（Windows WASAPI、macOS CoreAudio、Linux 由 `alsa` / `pulseaudio` 特性启用）、`talcs::Asio`、`Format`、`Midi`、`Dspx`、`Widgets`。

相比共享端口，配方还有三处差异：

- 移除 `lib/r8brain` 注入。新版本通过 `unofficial-r8brain-free-src` 使用重采样库，配方依赖本目录的 [r8brain-free-src](ports/r8brain-free-src)（从 `stdware/vcpkg-overlay` 的 `main` 复制，版本 6.5）。
- 删除 SDL 相关选项与 `asiosdk_dir_helper`。`talcs::Device` 不再链接 SDL，ASIO 拆为独立库并经 `find_path` 找到 `asiosdk` 头文件。
- 只保留两个上游尚未包含的补丁：

| 补丁 | 修复行为 |
|---|---|
| [fix-loop-position.patch](ports/talcs/fix-loop-position.patch) | transport 跨循环回绕点后按剩余读取长度推进位置，避免下一块跳过采样 |
| [fix-clip-series-read-serialization.patch](ports/talcs/fix-clip-series-read-serialization.patch) | Future 片段源的起点与范围修改补上与读取路径相同的互斥 |

随版本升级移除的补丁：`fix-setnextreadposition-before-open`（上游已按 `isOpen()` 守卫）、`fix-buffering-mode-position`（上游重写预读后行为等价）、`fix-sdl-driver-startup-lifetime`（SDL 后端移除）、`fix-asio-com-lifetime`（ASIO 重写后按初始化结果配对释放 COM）、`fix-output-device-release`（上游已清空公开设备引用）。

## 3. RtMidi

保留 `6.1.0` 配方，因为 talcs 的 MIDI 模块使用该版本引入的 `rt::midi` 命名空间。[fix-coremidi-exception-specification.patch](ports/rtmidi/fix-coremidi-exception-specification.patch) 移除 CoreMIDI 输入/输出客户端创建 helper 错误的 `throw()` 规格，使驱动初始化失败进入调用方异常处理而非 `std::terminate`。检查创建结果前释放临时客户端名称，避免失败路径泄漏。

应用 manifest 在 Linux 同时显式请求 RtMidi 的 `alsa` 特性和 talcs 的 `alsa`、`pulseaudio` 特性，Windows 和 macOS 使用各自原生后端。

## 4. 验证与移除条件

补丁随端口配方参与 vcpkg ABI 和缓存判断。调整后须实际准备依赖、构建产品并运行对应音频、驱动或进程回归，不能只重新配置 CMake。

共享或上游端口包含等价修复并经应用验证后，移除对应项目补丁或覆盖配方。目录不再包含覆盖时，同时移除 manifest 中的项目覆盖路径。应用层 SynthrtEngine/PackageManager 的适配修改不属于 synthrt 源码补丁，已知未修复依赖问题见[四期测试报告](../../docs/automation/04-testing-and-ci/test-report.md#6-遗留问题与明确边界)。
