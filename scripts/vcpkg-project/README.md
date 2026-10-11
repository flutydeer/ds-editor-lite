# 项目覆盖端口

## 1. 范围与优先级

此目录优先于共享 `scripts/vcpkg/ports`，仅承载当前应用必须使用、共享端口尚未包含的修复。vcpkg 覆盖端口需要完整配方，因此保留原依赖和构建选项。每个库仍只链接一份，不维护平行库实现。

## 2. talcs

保留共享端口的 `v0.1.0` 配方及 ASIO 路径辅助文件，加入以下补丁：

| 补丁 | 修复行为 |
|---|---|
| [fix-loop-position.patch](ports/talcs/fix-loop-position.patch) | transport 跨循环回绕点后按剩余读取长度推进位置，避免下一块跳过采样 |
| [fix-buffering-mode-position.patch](ports/talcs/fix-buffering-mode-position.patch) | 切换预读模式时清理预读并恢复当前位置，直接读取同步推进位置，避免离线导出起点偏移或重复已读音频 |
| [fix-clip-series-read-serialization.patch](ports/talcs/fix-clip-series-read-serialization.patch) | 片段读取与起点/范围修改使用同一互斥，覆盖普通及 Future 片段源 |
| [fix-sdl-driver-startup-lifetime.patch](ports/talcs/fix-sdl-driver-startup-lifetime.patch) | SDL 延迟启动绑定驱动生命周期，停止或销毁后不启动旧线程，线程启动不覆盖已发出的停止请求 |
| [fix-asio-com-lifetime.patch](ports/talcs/fix-asio-com-lifetime.patch) | ASIO 只配对释放已成功初始化的 COM，处理初始化失败和重复调用 |
| [fix-output-device-release.patch](ports/talcs/fix-output-device-release.patch) | 释放设备前清空公开设备引用，切换驱动失败不留下悬空指针 |

## 3. RtMidi

保留 `6.0.0` 配方，[fix-coremidi-exception-specification.patch](ports/rtmidi/fix-coremidi-exception-specification.patch) 移除 CoreMIDI 输入/输出客户端创建 helper 错误的 `throw()` 规格，使驱动初始化失败进入调用方异常处理而非 `std::terminate`。检查创建结果前释放临时客户端名称，避免失败路径泄漏。

应用 manifest 在 Linux 显式请求 `alsa` 特性，Windows 和 macOS 使用各自原生后端。

## 4. 验证与移除条件

补丁随端口配方参与 vcpkg ABI 和缓存判断。调整后须实际准备依赖、构建产品并运行对应音频、驱动或进程回归，不能只重新配置 CMake。

共享或上游端口包含等价修复并经应用验证后，移除对应项目补丁或覆盖配方。目录不再包含覆盖时，同时移除 manifest 中的项目覆盖路径。应用层 SynthrtEngine/PackageManager 的适配修改不属于 synthrt 源码补丁，已知未修复依赖问题见[四期测试报告](../../docs/automation/04-testing-and-ci/test-report.md#6-遗留问题与明确边界)。
