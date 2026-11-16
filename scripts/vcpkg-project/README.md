# 项目覆盖端口

## 1. 范围与优先级

这个目录存放应用依赖的补丁。[项目 manifest](../vcpkg-manifest/vcpkg.json) 将项目覆盖端口放在共享覆盖端口之前。vcpkg 从优先匹配的目录读取完整配方，因此覆盖端口也要维护依赖和构建选项。

## 2. talcs

[端口配方](ports/talcs/portfile.cmake)应用以下补丁。

| 补丁 | 修复行为 |
|---|---|
| [fix-loop-position.patch](ports/talcs/fix-loop-position.patch) | transport 跨循环回绕点后按剩余读取长度推进位置，避免下一块跳过采样 |
| [fix-buffering-mode-position.patch](ports/talcs/fix-buffering-mode-position.patch) | 切换预读模式时清理预读并恢复当前位置，直接读取同步推进位置，避免离线导出起点偏移或重复已读音频 |
| [fix-clip-series-read-serialization.patch](ports/talcs/fix-clip-series-read-serialization.patch) | 片段读取与起点/范围修改使用同一互斥，覆盖普通及 Future 片段源 |
| [fix-sdl-driver-startup-lifetime.patch](ports/talcs/fix-sdl-driver-startup-lifetime.patch) | SDL 延迟启动绑定驱动生命周期，停止或销毁后不启动旧线程，线程启动不覆盖已发出的停止请求 |
| [fix-asio-com-lifetime.patch](ports/talcs/fix-asio-com-lifetime.patch) | ASIO 只配对释放已成功初始化的 COM，处理初始化失败和重复调用 |
| [fix-output-device-release.patch](ports/talcs/fix-output-device-release.patch) | 释放设备前清空公开设备引用，切换驱动失败不留下悬空指针 |

## 3. RtMidi

[CoreMIDI 补丁](ports/rtmidi/fix-coremidi-exception-specification.patch)允许客户端创建失败时向调用方抛出异常，并在检查结果前释放临时客户端名称。

应用 manifest 在 Linux 显式请求 `alsa` 特性，Windows 和 macOS 使用各自原生后端。

## 4. 验证与移除条件

补丁随端口配方参与 vcpkg ABI 和缓存判断。调整后须实际准备依赖、构建产品并运行对应音频、驱动或进程回归，不能只重新配置 CMake。

共享或上游端口包含等价修复并经应用验证后，移除对应项目补丁或覆盖配方。移除最后一个覆盖端口时，同时更新 manifest 中的项目覆盖路径。
