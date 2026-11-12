# 跨平台 UI 截图画册

Windows、macOS 和 Linux 的 CI 在本平台单元测试通过后，运行独立截图步骤。截图直接使用本次构建的程序和 GUI 测试夹具，分别启动浅色、深色进程，不改变普通测试的主题或用户的持久化设置。

## 布局与范围

总览按项目与工作区、轨道与时间线、音符与音高、歌词与发音、音色与混音、音频与工程导入导出、软件包与资源、偏好设置、任务与诊断九个模块分类。每个标题独占一行，各分类独立在 5120 像素最大宽度内按截图实际显示宽度自然换行，不限制每行张数，也不以画布高度或宽高比决定排布。同排截图在图片区域内纵向居中，每张图的图注紧跟该图底部。分类向下叠加，最终宽度由实际内容最右边界与边距决定，高度自然增长。每张图配图注，不加横线或分类说明。小窗口不强制放大，原图保留原始分辨率。

画册包含 52 张实际截图，包括两种主窗边框、编辑器面板、弹层、设置页和独立窗体。系统文件选择器、全局菜单及异常情况下才出现的系统提示不在范围内。尚无内容的 Help 页只使用页面名称作图注。

截图夹具展示轨道静音与独奏、混音增益与声像、起伏的原始生成层和手绘参数、Linear/Hermite/None 锚点插值、搜索匹配结果、歌词输入与规则测试结果，以及任务列表和导出进度。参数与任务状态使用确定性测试数据，通过实际模型和控件渲染，不依赖外部推理服务。截图按不同视觉样式和有意义的状态组织，不逐个枚举编辑工具。钢琴窗保留音符编辑、手绘音高和锚点音高三种显示样式。参数面板在同一张图中展示原始生成层、手绘曲线及 Linear/Hermite/None 锚点插值，不重复仅按钮高亮不同的画面。搜索和量化对话框也不因父窗口不同而重复截图。浅色汇总画布使用冷灰底色，深色使用较亮的蓝灰，保留原始截图及系统阴影，不额外绘制窗框。

生成参数在音高修改触发的推理完成后填入，并在截图前校验其实际曲线值，避免被异步默认结果覆盖。钢琴窗和参数面板同图展示，另有音高与参数变换工具的范围边界、过渡区域和百分比手柄。不支持参数提示、启用动态音色融合提示和融合曲线使用真实界面状态。轨道同时展示 L/R 声像、自然顺序配色和其他轨道的背景音符。

## 系统窗体捕获

- macOS 14.4+ 使用 ScreenCaptureKit 的本进程窗口接口和 SCScreenshotManager，保留系统合成的真实圆角、阴影和透明背景。只枚举本进程窗口，不申请屏幕录制权限。服务不可用或 3 秒超时时回退 AppKit frame 缓存并明确记录，不能保证回退图含真实阴影。
- Windows 优先使用 QScreen 读取目标窗体周围的实际桌面像素。CI 的同进程背景窗覆盖截图区域，目标置前，预留阴影空间并检查屏幕边界和窗口堆叠。屏幕尺寸或遮挡不允许时，使用 PrintWindow 保留真实系统非客户区，以同一窗体的 QWidget::grab 完整像素替换客户区，并校验像素尺寸及窗体几何保持一致。该混合来源在清单中单独标注，不保证 DWM 阴影。GitHub hosted runner 为 Windows Server，虚拟机可能没有 Windows 11 的圆角效果，截图不补画系统没有提供的效果。
- Linux 使用独立 Xvfb 和 Openbox，不启动额外合成器。浅色使用 Clearlooks 装饰，深色使用 Onyx 装饰。截图包含该环境真实绘制的标题栏和边框，不添加该环境没有的阴影或圆角，不代表所有 Linux 桌面。Wayland 不走无提示桌面读取路径。
- 自绘面板和弹层使用 QWidget::grab，直接保存内容，不拼接重复的完整宿主。

Windows/X11 桌面读取只在 LITE_SCREENSHOT_DESKTOP_CAPTURE=1 时启用，面向独立 CI 桌面。桌面捕获及可用的原生窗框捕获均不可用时，明确回退 Qt 客户区截图。不要在含个人信息或其他应用窗口的桌面上开启该选项。

captureMethod 记录实际来源：screencapturekit-window、windows-desktop-window、windows-printwindow-frame-qt-client、x11-window-without-compositor、appkit-frame-fallback、qt-client-fallback 或 qt-widget-content。验证脚本说明 Windows 混合来源与原生窗框方式的阴影限制，并对客户区或 AppKit 回退输出 warning，不能仅凭任务成功宣称全部窗口具有系统阴影。

## 产物包

Tests 工作流沿用测试诊断产物的命名风格，上传 `<triplet>-ui-screenshots-<run_id>-<run_attempt>`，其中 triplet 为 arm64-osx、x64-windows 或 x64-linux。每个平台目录都有 capture.json，包含操作系统版本、已完成主题和完整性状态。其 light/、dark/ 子目录包含：

- ui-review.png：全部模块的合成总览。
- ui-review.json：实际主题、模块、图注、原图路径、捕获来源、原始尺寸与排版坐标。
- modules/01-workspace/ 至 modules/09-diagnostics/：按模块收纳每张原图及该模块的 review.png。
- modules/01-workspace/title-bar-*.json：主窗主题、边框和几何报告。
- diagnostics/：运行、画册验证及 Linux 窗管日志。

截图步骤失败也会上传已产生的文件和诊断。capture.json 的 complete=false 或缺少主题表示产物不完整。

## 本地复现

先完成项目的标准构建和单元测试，再在独立桌面执行：

```bash
python scripts/ci/capture-ui-gallery.py --build-dir build/Tests --output build/ui-screenshots --qt-dir /path/to/Qt
```

Linux 需要安装 Xvfb、xauth、Openbox，运行：

```bash
xvfb-run -a -s '-screen 0 2560x1600x24' python scripts/ci/capture-ui-gallery.py --build-dir build/Coverage --output build/ui-screenshots --qt-dir /path/to/Qt
```

脚本为每个主题启动两次主程序，分别使用系统边框和无边框，再运行 TestApplicationGui 的 captureUiGallery。正常单元测试不执行实际截图，但包含画册分组排版回归测试。

环境变量：LITE_CAPTURE_THEME 为 light/dark，主程序复用 DS_EDITOR_THEME=lite-light/lite-dark。LITE_CHROME_CAPTURE 指定主窗产物目录，LITE_FORCE_NATIVE_FRAME=1/0 仅覆盖当次边框，LITE_UI_GALLERY 指定画册目录。LITE_CHROME_CAPTURE_DELAY_MS 默认 6000 ms。

验证单套产物：

```bash
python scripts/ci/verify-ui-gallery.py build/ui-screenshots/macos/light --theme light
```

截图、文件写入或合成失败时立即终止本次捕获，进程返回失败；不会继续截图或用不完整内容生成成功清单。分类与合成只处理本次实际添加的截图，不维护预期张数、图注清单或分类顺序断言。通用验证只检查实际产物的 PNG、主题、分类归档、独占行标题、居中流式布局、图注位置和图块不重叠；新增、移除、改名或调整截图顺序不需要同步修改验证器。仍需目检真实产物，确认系统效果、状态及文字可读性。macOS 无边框主窗的 contentsRect、titleBarRect 和 frameMargins 顶边为 0，系统截图中的透明像素或真实阴影不属于额外布局留白。
