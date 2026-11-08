# 在 CI 里给 macOS 窗口截图

贡献者手里不一定有 Mac，但标题栏、红绿灯、系统按钮这些属于窗口 chrome 的东西只能在 macOS 上看。这个流程让 GitHub 的 macOS runner 跑一遍主程序，把两种边框模式各截一张图作为 artifact 带回来。

## 为什么不用 screencapture

`screencapture` 是另一个进程去读我们的窗口像素，macOS 从 10.15 起要求"屏幕录制"权限，没有授权时它不报错，直接返回黑图或只剩壁纸。到了 macOS 15，同一个 job 里第二次调用还会弹出"bypass the system private window picker"的授权框，无人可点，步骤直接挂住。GitHub 的 `macos-latest` 现在是 macOS 26，社区里已经有人把这条路测死了（`qte77/gha-runner-gui-kit` 的 README 记录得很清楚，Apple 开发者论坛 806451 是同一个问题）。

所以截图在**应用进程内部**完成。进程读自己的窗口像素不需要任何授权，而且 macOS 上可以直接取 AppKit 的 theme frame，红绿灯和原生标题栏本来就在那里面，`QWidget::grab()` 是看不到的。

## 环境变量

| 变量 | 作用 |
| --- | --- |
| `LITE_CHROME_CAPTURE` | 截图输出目录，设置后启动即进入抓图模式 |
| `LITE_FORCE_NATIVE_FRAME` | `1` 系统边框，`0` 无边框，只覆盖本次运行，不写配置 |
| `LITE_CHROME_CAPTURE_DELAY_MS` | 抓图前等待时间，默认 6000 |

抓到后写 `<目录>/title-bar-native-frame.png` 或 `title-bar-frameless.png`，并写出同名 `.json` 几何报告，然后退出。退出码 0 表示成功，2 表示截图或报告写入失败，3 表示 macOS 非全屏无边框窗口仍在自定义标题栏上方保留空间。失败时保留截图和报告，便于检查 artifact。

等待时间是给首帧绘制、主题样式和初始文档留的余量，冷启动的 runner 上必要时可以调大。抓图定时器只观察窗口，不修改 style mask 或触发布局，系统边框模式也不会在快门前被改成 FullSizeContentView。

本地任意平台都能跑：

```bash
LITE_CHROME_CAPTURE=/tmp/shots LITE_FORCE_NATIVE_FRAME=0 ./DsEditorLite
```

## CI

`.github/workflows/chrome-screenshots.yml` 只在改动标题栏、主窗口或 `WindowFrameUtils` 时随 PR 触发，也可以手动 `workflow_dispatch`。它复用 `tests.yml` 的依赖与缓存步骤，只构建 `DsEditorLite` 与 `DsConnectorLite`，然后连续跑两次进程，各截一张图。

取图：

```bash
gh run download <run-id> -n chrome-screenshots -D ./shots
```

## 检查顶部留白

几何报告里的矩形数组为 `[x, y, width, height]`，边距数组为 `[left, top, right, bottom]`。macOS 非全屏无边框截图必须满足：

- `contentsRect[1] == 0`，根窗口没有额外的内容区顶部缩进。
- `titleBarRect[1] == 0`，自定义标题栏在根窗口坐标中从顶部开始。
- `frameMargins[1] == 0`，Qt 客户区外没有额外的原生标题栏高度。

日志中的 `Chrome native geometry` 另记录 Qt 原生 view 相对 AppKit frame view 的顶部间距，以及 view、screen 两种 safe-area 顶边距。验收时还需确认无边框的 `top inset` 为 0，并查看实际截图。Qt 的 `frameMargins` 不完全等同于原生 view 的真实位置，前面的自动检查不能代替这一步。这样可以区分原生 view 没有扩展与 Qt 布局主动留边，不再仅凭 PNG 文件非空判定修复成功。

### Qt safe area 与 FullSizeContentView

Qt 6.11.2 中存在两层独立机制：

1. [Cocoa 平台窗口](https://github.com/qt/qtbase/blob/v6.11.2/src/plugins/platforms/cocoa/qcocoawindow.mm#L313-L355)在 FullSizeContentView 模式下仍把原生标题栏区域计入 safe area。
2. [QWidget](https://github.com/qt/qtbase/blob/v6.11.2/src/widgets/kernel/qwidget.cpp#L1217-L1227)默认让顶层窗口遵守 safe area，[contentsMargins()](https://github.com/qt/qtbase/blob/v6.11.2/src/widgets/kernel/qwidget.cpp#L7746-L7752)将其合入内容边距，根布局再按 contentsRect 排版。

所以仅设置 `ExpandedClientAreaHint` 或反复设置 `NSWindowStyleMaskFullSizeContentView` 不足以消除布局留边。macOS 无边框主窗口在创建原生窗口前显式关闭 `WA_ContentsMarginsRespectsSafeArea`，让自定义标题栏及其 traffic-light 区域使用整个客户区，系统边框模式保持默认行为。不使用固定 32px 或负边距补偿。

旧日志中的 `styleMask 0x800f -> 0x800f` 只能证明扩展标志早已生效。`contentLayoutRect` 表示未遮挡区域，不能把它与 frame 的高度差直接当作 content view 没有扩展的证据。下面的新一轮 Cocoa 运行确认了 safe-area 修正后的实际效果。

## Safe-area 修正的 CI 验证

修复提交 [`cf0dca71`](https://github.com/flutydeer/ds-editor-lite/commit/cf0dca710dc0b9ff0fc8b375c49d7336f6372114) 对应的 [CI 42487888425](https://github.com/flutydeer/ds-editor-lite/actions/runs/42487888425) 构建、两种模式截图和产物上传均成功，总耗时 14 分 2 秒。验证环境为 macOS arm64 runner、Qt 6.11.2、Cocoa，窗口处于非全屏最大化状态，DPR 为 1。

已下载原始 artifact，检查两张完整截图、几何报告及原生日志。无边框模式的结果为：

| 观测项 | 实测值 |
| --- | --- |
| `respectsSafeArea` | `false` |
| `safeAreaMargins` | `[0, 32, 0, 0]` |
| `contentsMargins` | `[0, 0, 0, 0]` |
| `contentsRect` | `[0, 0, 1024, 674]` |
| `titleBarRect` | `[0, 0, 1024, 40]` |
| `frameMargins` | `[0, 0, 0, 0]` |
| 原生 frame / Qt view 尺寸 | 均为 `1024x674` |
| 原生 `top inset` | `0` |
| 原生 `styleMask` | `0x800f` |

对比父提交 `51ecebc9` 的 [CI 42485229296](https://github.com/flutydeer/ds-editor-lite/actions/runs/42485229296) 原始无边框截图，红色交通灯的像素包围盒从 `[6, 44, 19, 57]` 变成 `[6, 12, 19, 25]`，恰好上移 32px，自定义标题栏上方的空白条已消失。这里的包围盒为 `[minX, minY, maxX, maxY]`，坐标均取自未经裁剪的原始截图。

safe-area 顶边距仍为 32，而根布局顶部边距和原生 view 顶部间距均为 0，与“扩展客户区已经生效，但 QWidget 默认把 safe area 再次加入布局边距”的根因链一致。系统边框对照保留 `styleMask 0xf`、原生标题栏和 `top inset 32`，不再被抓图过程改成 FullSizeContentView。

## 已知限制

非 macOS 平台退回 `QWidget::grab()`，只有客户区，看不到系统边框。macOS 上拿到的是 theme frame 的位图，含圆角与标题栏，但不含窗口阴影。Cocoa 捕获缺少原生窗口、frame view 或 bitmap 时会报错退出，不静默降级为缺少原生 chrome 的客户区截图。它是 AppKit 视图重绘捕获，不等同于 WindowServer 的屏幕合成结果。

关闭根窗口的自动 safe-area 留边也会取消 Qt 对屏幕刘海区域的自动避让。当前几何验收针对非全屏窗口，全屏和刘海屏上的交互控件避让仍需单独实机验证，不能把标题栏 safe-area 顶边距整体加回，否则普通窗口的顶部留白会重新出现。

截图里如果出现模态对话框，说明启动路径上有东西需要人工确认，本身就是有价值的信号：抓图定时器跑在同一个事件循环里，模态不会拦住它。

## 首次运行记录

PR #202 触发的一次完整流程：依赖与配置约 6 分钟，构建加两次抓图共 14 分 52 秒，`macos-latest` 当时是 macOS 26。产物 `chrome-screenshots` 里两张图分别是 63 KB 与 64 KB。无边框那张可以看到红绿灯与项目名下拉同行、右侧没有自绘按钮，但这不足以证明顶部留白已消除。早期抓图路径还会修改系统边框窗口的 style mask，旧的系统边框截图不应视为未经扰动的对照。
