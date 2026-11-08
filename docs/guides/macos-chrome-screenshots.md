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

抓到后写 `<目录>/title-bar-native-frame.png` 或 `title-bar-frameless.png`，然后退出，退出码 0 表示成功、2 表示抓图失败。等待时间是给首帧绘制、主题样式和初始文档留的余量，冷启动的 runner 上必要时可以调大。

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

## 已知限制

非 macOS 平台退回 `QWidget::grab()`，只有客户区，看不到系统边框。macOS 上拿到的是 theme frame 的位图，含圆角与标题栏，但不含窗口阴影。

截图里如果出现模态对话框，说明启动路径上有东西需要人工确认，本身就是有价值的信号：抓图定时器跑在同一个事件循环里，模态不会拦住它。
