# 无边框标题栏右缘死区修复：Qt childAt 浮点缝隙补偿

## 现象

窗口最大化（或窗口右缘贴住屏幕右缘）后，鼠标顶到屏幕最右侧时无法触发标题栏关闭按钮的 hover，点击也没有任何反应；往左一两像素才能正常交互。原生边框模式（`appearance.useNativeFrame`）和系统标题栏应用都没有这个问题。

实测环境（150% 缩放、1920×1280 物理）：死区只有客户区最后一物理列（x=1919），x=1918 及以左全部正常。不同缩放下死区宽度不同（0～2 物理列），100% 缩放同样存在。

## 根因

问题不在 QWindowKit，在 Qt widget 层：

1. Qt 6.8 起 `QWidgetWindow::handleMouseEvent` 用浮点版 `m_widget->childAt(event->position())` 定位鼠标下的控件；`QWidgetPrivate::pointInsideRectAndMask`（`qwidget_p.h`）的有效右边界是 `QRect::right() - 1`，即窗口右缘最后 1 逻辑像素 `[width-1, width)` 永远不属于任何子控件，`childAt` 返回 nullptr，事件兜底派给顶层窗口本身。
2. 非整数 DPR 下客户区最后一物理列恰好映射进这条缝隙：150% 缩放时鼠标停在物理 x=1919，逻辑坐标为 1919/1.5 = 1279.33，落在 `[1279, 1280)` 内，而有效右界是 1279，于是关闭按钮（`[1232, 1280)`）的最后一列永远命中不了，hover 和点击都丢失。

排查中排除的环节（均已实测确认无问题）：QWK 最大化几何（窗口矩形每边外扩一个 resize border，客户区与工作区精确对齐）、`WM_NCHITTEST`（最后一列也正确返回 `HTCLOSE`）、QWK 的非客户区消息到客户区消息转换、按钮渲染（铺满到最后一列）、QWK 的整型 `QRect::contains` 判定（命中最右列）。原生应用无此问题是因为系统标题栏的 hit test 不经过 Qt 的 `childAt`。

窗口底部同样存在一条 1 逻辑像素的水平缝隙（`[height-1, height)`），但那里没有交互控件，感知不到，也不处理。

## 修复方案

在应用侧把"QWK 已认定命中按钮、但 Qt `childAt` 会 miss"的缝隙列事件钳位一列后重发，让事件重新走常规的 Qt 事件路径。

### `MainTitleBar::systemButtonAt(const QPointF &windowPos)`

判断窗口级坐标是否落在系统按钮命中区。从最右的关闭按钮往左遍历，命中区向右扩展到窗口右缘（不设 x 上界，所以缝隙列 `x >= close.left` 也归关闭按钮）；y 限定在按钮行 `[top, bottom)`；隐藏按钮跳过。

### `MainWindow` 事件过滤器

- `MainWindow::event()` 在 `QEvent::WinIdChange` 时给 `windowHandle()` 安装事件过滤器。选这个时机而不是构造函数，是因为 `setWindowFlag` 等调用会重建原生窗口，只装一次会失效；`installEventFilter` 幂等，重复触发无害。
- 过滤器拦截发往本窗口的 `MouseMove / MouseButtonPress / MouseButtonRelease / MouseButtonDblClick`，当 `position().x() >= width() - 1.0`（缝隙含整数列，所以用 `>=`）且 `systemButtonAt` 命中时，构造一个 `x = width() - 1.5` 的新 `QMouseEvent`（原样携带按键、修饰键、时间戳、输入设备与事件类型），`QCoreApplication::sendEvent` 回 `windowHandle()` 并吃掉原事件。
- 克隆事件重新进入 `QWidgetWindow::handleMouseEvent`，`childAt` 命中按钮后由 Qt 原生机制维护 hover、按下、松开与 Enter/Leave，不需要手搓按钮状态机。不会死循环：钳位后的 `x = width()-1.5` 不再满足 `>= width()-1` 的拦截条件。
- `width() - 1.5` 的取值对 100%/125%/150%/200% 都落在按钮内（按钮宽 48，钳位后按钮本地坐标 46.5，有效右界 47），修复不依赖具体缩放比例；窗口未最大化但右缘贴屏幕边缘的场景同样覆盖。

### 为什么不用其它方案

- 改按钮布局（加宽、加间距、右移）都无法解决：缝隙出在顶层窗口自身的 `childAt` 判定上，顶层 miss 后无论按钮怎么摆都收不到事件。
- QWK 层补丁（命中 ChromeButton 时对转发坐标做 `min(x, clientWidth-2)` 钳位）也可行，但需要维护 vcpkg 上游 patch，改动面和升级成本都更高。
- 修改 Qt 报告的窗口几何（右侧 custom margin +1 逻辑像素）会让布局与 `mapToGlobal` 全局错位，影响拖拽与菜单定位，风险大。

## 覆盖范围与已知边界

- 只覆盖主窗口（`MainWindow`）。QWK 管理的 `Dialog` 不可最大化，仅在用户把对话框拖到屏幕右缘时才会遇到同样的缝隙，如需覆盖可将同样的过滤器逻辑复制到 `Dialog` 基类。
- 缝隙列内只有按钮行被重定向；其余位置（如钢琴卷帘滚动条右缘 1px）维持原有行为。
- 触摸合成的鼠标事件（`Qt::MouseEventSynthesizedBySystem`）同样经过过滤器并按同一规则钳位，触摸盲点一并修复。

## 验证记录

150% 缩放、窗口最大化下实测：`SendInput` 将光标置于 x=1919 的 y=1/10/30 三行，关闭按钮 hover 全部点亮（此前全部失效）；在该列真实按下并抬起，应用正常退出。回归确认 x≤1918 的 hover、按钮正常区域点击不受影响。
