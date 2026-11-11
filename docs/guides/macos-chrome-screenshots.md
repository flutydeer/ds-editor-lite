# macOS UI 截图画册

GitHub 的 macOS runner 启动真实程序和 GUI 测试夹具，生成按 UI 模块分类的界面总览。截图来自实际控件，不绘制示意界面，不补画圆角或阴影。

## 画册布局

`ui-review.png` 按以下模块排列。分类标题独占一整行，下方每行最多两张宽图，每张图片下方只有图注。分类之间不画横线，也不添加说明段落。

- 项目与工作区
- 轨道与时间轴
- 音符与音高编辑
- 歌词与发音
- 音色与混音
- 音频与工程导入导出
- 声库包与资源
- 偏好设置
- 任务与诊断

分类由 UI 模块决定，同一模块可以包含面板、弹层和独立窗口。拍摄顺序不影响归类。小窗口不强行放大，原图保持原始分辨率。

## 窗体与控件的取图方式

macOS 14.4 及以上优先通过 ScreenCaptureKit 的 `getCurrentProcessShareableContentWithCompletionHandler` 获取本进程窗口，再对指定窗口调用 `SCScreenshotManager`。捕获包含系统合成的真实圆角和阴影，使用透明背景，不包含鼠标、其他应用或子窗口。只枚举本进程窗口，不请求屏幕录制权限，不调用反复触发授权弹窗的 `screencapture` 命令。

系统捕获最多等待 3 秒。当前系统不支持、服务不可用或超时时，回退到 AppKit frame 缓存，并在日志和 JSON 中明确记录。该回退能包含原生标题栏与红绿灯，但不保证桌面合成的圆角和阴影。同一进程在服务失败后不再为每张图重复等待。

轨道、钢琴卷帘、参数、波形、混音等自绘面板，以及项目切换弹层，直接截取实际控件内容。画册不再为每个弹层拼接完整宿主窗口。原生窗体使用独立窗口截图，所属模块决定它在画册中的位置。

`captureMethod` 的取值如下。

| 值 | 实际来源 |
| --- | --- |
| `screencapturekit-window` | macOS 系统窗口合成捕获，保留真实阴影与圆角 |
| `appkit-frame-fallback` | AppKit frame 缓存，不保证系统阴影与圆角 |
| `qt-client-fallback` | Windows、Linux 或非 Cocoa 环境下的 Qt 客户区 |
| `qt-widget-content` | 指定自绘面板或弹层的内容 |

系统文件选择器、全局菜单以及需要异常才能出现的系统提示，不在当前画册范围内。

## 环境变量

| 变量 | 作用 |
| --- | --- |
| `LITE_CHROME_CAPTURE` | 主窗口截图输出目录，设置后启动即进入抓图模式 |
| `LITE_FORCE_NATIVE_FRAME` | `1` 系统边框，`0` 无边框，仅覆盖本次运行，不写配置 |
| `LITE_CHROME_CAPTURE_DELAY_MS` | 主窗口抓图前等待时间，默认 6000 ms |
| `LITE_UI_GALLERY` | GUI 测试夹具的画册输出目录 |

主程序写出 `title-bar-native-frame.png` 或 `title-bar-frameless.png`，以及同名 JSON 几何报告后退出。JSON 包含实际 `captureMethod`。退出码 2 表示图片或报告写入失败，3 表示 macOS 非全屏无边框窗口仍在自定义标题栏上方保留空间。

截图临时边框覆盖单独保存在运行期字段，配置序列化始终使用用户保存的偏好。未设置 `LITE_UI_GALLERY` 时，常规测试跳过真实截图场景，但仍运行画册布局回归测试。

## CI 与产物

`.github/workflows/chrome-screenshots.yml` 支持 `workflow_dispatch`，也随相关 PR 改动触发。工作流先运行主程序捕获两种边框，再运行：

```bash
LITE_UI_GALLERY=/path/to/shots ./TestApplicationGui uiGalleryLayoutGroupsModules captureUiGallery -v1
python scripts/ci/verify-ui-gallery.py /path/to/shots
```

`chrome-screenshots` artifact 包含：

- `ui-review.png`：完整分类总览。
- `ui-review.json`：分类、图注、原图路径、捕获方式、原始尺寸和排版坐标。
- `gallery/modules/`：每个模块的独立总览，顺序与 JSON 一致。
- `gallery/NN.png`：未经排版缩放的原始截图。
- `title-bar-*.png` 和同名 JSON：两种边框的主程序参考图及几何报告。
- 构建、运行与画册日志。

验证脚本检查实际 PNG、模块完整性、标题独占一行、两列限制、图注位置和图片无重叠，并汇总实际捕获方式。发生回退时输出 CI warning。验收还需查看原图，确认系统阴影和圆角、内容完整性及文字可读性，不能只看文件非空或捕获方法名称。

```bash
gh run download <run-id> -n chrome-screenshots -D ./shots
```

## 无边框几何检查

几何报告中的矩形为 `[x, y, width, height]`，边距为 `[left, top, right, bottom]`。macOS 非全屏无边框参考图要求 `contentsRect[1]`、`titleBarRect[1]` 和 `frameMargins[1]` 均为 0。

这些值描述 Qt 布局，不包含系统截图外侧的真实阴影。系统截图画布比 Qt 窗体略大是正常现象，不能把透明外沿或阴影当成标题栏留白。
