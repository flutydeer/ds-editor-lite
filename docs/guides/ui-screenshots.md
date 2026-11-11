# 跨平台 UI 截图画册

Windows、macOS 和 Linux 的 CI 在本平台单元测试通过后，运行独立截图步骤。截图直接使用本次构建的程序和 GUI 测试夹具，分别启动浅色、深色进程，不改变普通测试的主题或用户的持久化设置。

## 布局与范围

总览按项目与工作区、轨道与时间线、音符与音高、歌词与发音、音色与混音、音频与工程导入导出、软件包与资源、偏好设置、任务与诊断九个模块分类。每个标题独占一行，下方每行最多两张宽图，每张图配图注，不加横线或分类说明。小窗口不强制放大，原图保留原始分辨率。

画册包含 49 张实际截图，包括两种主窗边框、编辑器面板、弹层、设置页和独立窗体。系统文件选择器、全局菜单及异常情况下才出现的系统提示不在范围内。空任务列表和尚未实现的 Help 页如实标注。

## 系统窗体捕获

- macOS 14.4+ 使用 ScreenCaptureKit 的本进程窗口接口和 SCScreenshotManager，保留系统合成的真实圆角、阴影和透明背景。只枚举本进程窗口，不申请屏幕录制权限。服务不可用或 3 秒超时时回退 AppKit frame 缓存并明确记录，不能保证回退图含真实阴影。
- Windows 使用 QScreen 读取目标窗体周围的实际桌面像素。CI 的同进程背景窗覆盖截图区域，目标置前，预留阴影空间并检查屏幕边界和窗口堆叠。GitHub hosted runner 为 Windows Server，虚拟机可能没有 Windows 11 的圆角效果，截图不补画系统没有提供的效果。
- Linux 使用独立 Xvfb、Openbox 和 picom XRender 合成器。浅色使用 Clearlooks 装饰，深色使用 Onyx 装饰。截图包含该环境真实绘制的边框和阴影，不代表所有 Linux 桌面。Wayland 不走无提示桌面读取路径。
- 自绘面板和弹层使用 QWidget::grab，直接保存内容，不拼接重复的完整宿主。

Windows/X11 桌面读取只在 LITE_SCREENSHOT_DESKTOP_CAPTURE=1 时启用，面向独立 CI 桌面。未开启、窗口无法完整容纳、发生遮挡或读取失败时明确回退 Qt 客户区截图。不要在含个人信息或其他应用窗口的桌面上开启该选项。

captureMethod 记录实际来源：screencapturekit-window、windows-desktop-window、x11-desktop-window、appkit-frame-fallback、qt-client-fallback 或 qt-widget-content。验证脚本对回退输出 warning，不能仅凭任务成功宣称全部窗口具有系统阴影。

## 产物包

Tests 工作流上传 ui-screenshots-arm64-osx、ui-screenshots-x64-windows 和 ui-screenshots-x64-linux 三个包。每个平台目录都有 capture.json，包含操作系统版本、已完成主题和完整性状态。其 light/、dark/ 子目录包含：

- ui-review.png：全部模块的合成总览。
- ui-review.json：实际主题、模块、图注、原图路径、捕获来源、原始尺寸与排版坐标。
- modules/01-workspace/ 至 modules/09-diagnostics/：按模块收纳每张原图及该模块的 review.png。
- modules/01-workspace/title-bar-*.json：主窗主题、边框和几何报告。
- diagnostics/：运行、画册验证及 Linux 窗管与合成器日志。

截图步骤失败也会上传已产生的文件和诊断。capture.json 的 complete=false 或缺少主题表示产物不完整。

## 本地复现

先完成项目的标准构建和单元测试，再在独立桌面执行：

```bash
python scripts/ci/capture-ui-gallery.py --build-dir build/Tests --output build/ui-screenshots --qt-dir /path/to/Qt
```

Linux 需要安装 Xvfb、xauth、Openbox、picom，运行：

```bash
xvfb-run -a -s '-screen 0 2560x1600x24' python scripts/ci/capture-ui-gallery.py --build-dir build/Coverage --output build/ui-screenshots --qt-dir /path/to/Qt
```

脚本为每个主题启动两次主程序，分别使用系统边框和无边框，再运行 TestApplicationGui 的 captureUiGallery。正常单元测试不执行实际截图，但包含画册分组排版回归测试。

环境变量：LITE_CAPTURE_THEME 为 light/dark，主程序复用 DS_EDITOR_THEME=lite-light/lite-dark。LITE_CHROME_CAPTURE 指定主窗产物目录，LITE_FORCE_NATIVE_FRAME=1/0 仅覆盖当次边框，LITE_UI_GALLERY 指定画册目录。LITE_CHROME_CAPTURE_DELAY_MS 默认 6000 ms。

验证单套产物：

```bash
python scripts/ci/verify-ui-gallery.py build/ui-screenshots/macos/light --theme light
```

验证覆盖实际 PNG、49 张原图、9 个模块、主题、分类归档、独占行标题、双列布局、图注位置和图块不重叠。仍需目检真实产物，确认系统效果、状态及文字可读性。macOS 无边框主窗的 contentsRect、titleBarRect 和 frameMargins 顶边为 0，系统截图中的透明像素或真实阴影不属于额外布局留白。
