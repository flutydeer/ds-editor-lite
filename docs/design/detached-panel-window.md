# 脱离面板窗口：独立顶层窗口契约

启用开发者选项「Enable panel detach」后，底部面板可以分离为独立顶层窗口。Windows 可为它创建独立任务栏按钮，并允许独立最小化、还原或贴靠。

Qt 顶层窗口仍可有父控件。Windows 会用 `QWindow::transientParent()` 为该窗口设置 native owner，使其叠在 owner 上方并随之分组最小化。独立面板的契约要求同时避免 QWidget 父级和 native owner，仅检查 `isWindow()` 不足以区分这两种窗口。

## 设计

### 无 QWidget parent，所有权显式化

`DetachedPanelWindow` 以 `Window(nullptr, Qt::Window)` 构造。主窗口用 `std::unique_ptr` 持有该窗口，在析构时先嵌回面板，再销毁窗口。

反向引用用 `QPointer<MainWindow>`，只用于窗口标题与菜单 wiring，不参与窗口生命周期。主窗口先于分离窗口销毁是不可能的，`QPointer` 只是防御性兜底。

### 生命周期

| 动作 | 结果 |
|---|---|
| 点击面板标题栏的分离按钮 | 懒创建窗口，恢复几何，面板 reparent 进窗口，显示 |
| 再次分离 | 复用同一个窗口对象，不再恢复几何 |
| 关闭分离窗口（标题栏、任务栏、Alt+F4） | `closeEvent` 忽略关闭，发出 `reattachRequested`，面板嵌回主窗口 |
| 关闭主窗口 | 先嵌回面板并保存几何，再走原有退出流程 |
| 主窗口被销毁但仍处于分离状态 | 析构函数先嵌回面板，再销毁窗口与 `WindowPlacement` |
| 开发者选项在分离状态下关闭 | 与关闭主窗口相同，嵌回并保存几何 |

分离窗口不是 `WA_DeleteOnClose`。跨 detach/attach 复用它，关闭请求因此不会删除发信号的窗口，编辑状态也不必重建。

### 几何

`WindowOption` 增加 `detachedPanelGeometry`，格式与 `mainWindowGeometry` 相同：`QWidget::saveGeometry()` 的 base64 字符串。读取时要求长度合法且 base64 往返一致，否则视为无效。

首次分离由 `WindowPlacement` 完成默认尺寸、居中、屏幕裁剪与多显示器纠正。默认尺寸仍是 960 宽、面板当前高度（下限 480）。之后每次嵌回都把几何写回 `WindowOption`，跨重启恢复。恢复包含最大化状态，但会清掉最小化状态。显示器被移除时，`WindowPlacement` 把窗口钳回最近的可见区域。

`WindowPlacement` 在分离窗口生命周期内常驻，监听屏幕增删与 `Show`，窗口隐藏期间也能纠正下一次显示的位置。

### 窗口标识

任务栏按窗口标题区分窗口。分离窗口标题是「主窗口标题 + 当前面板页名」，例如 `● 我的工程 — 钢琴卷帘`。主窗口标题变化、面板页切换、界面语言切换后都会刷新。

### 打开剪辑时还原并激活编辑窗口

在轨道窗里双击剪辑会打开 `ClipEditor` 页。如果该页在分离窗口里，而分离窗口已经最小化或落在主窗口后面，编辑动作不应该只改页面内容。`EditorViewController::bringPanelWindowToFront()` 按面板类型找到宿主窗口：

- 最小化时先按最小化前的状态还原，最大化保持最大化，全屏保持全屏。
- 然后 `raise()` 并 `activateWindow()`，让编辑窗口获得焦点。
- 面板嵌在主窗口里时窗口绑定为空，方法是空操作，不抢主窗口焦点。

RHI 与 Legacy 两个轨道画布的双击入口都走这一条路径。

### 共享的 chrome

标题栏、菜单栏、播放控制、QWindowKit agent、主题注册与边框效果全部复用主窗口的那一套，只有 `TitleBarSections::forDetachedPanel()` 的配置差异。分离窗口同时保留主窗口的右缘死区补偿：最大化后关闭按钮贴住屏幕右缘时，最后一列鼠标事件被钳位一列后重发。

## 多窗口契约

后续新增编辑器窗口沿用以下规则，不再引入第二套机制：

1. 主窗口是应用生命周期锚点，只有它能发起退出。
2. 二级编辑器窗口没有 QWidget parent，没有 native owner，没有 `WS_EX_TOOLWINDOW`。
3. 所有权在主窗口，反向引用用 `QPointer`。
4. 关闭二级窗口只归还内容，不退出应用。
5. 窗口激活驱动活动面板，沿用 `EditorViewController` 的 WindowActivate 路由。
6. 每个窗口自己保存几何。
7. 辅助工具窗口（如 `LogWindow`）保留 owned 工具窗口语义，不进入任务栏，与编辑器窗口分开。

## 覆盖范围与已知边界

- 分离作用于整个底部面板。窗口遵从系统堆叠顺序，启动时面板嵌在主窗口内。
- 从任一窗口的菜单打开设置时，设置对话框以发起窗口为父窗口。
- `detachedPanelGeometry` 是宿主本地窗口状态，由 `WindowOption` 保存。

## 验证

相关回归用例见[主窗口测试](../../src/tests/TestApplicationGui/tst_main_window.cpp)。在 Windows 桌面检查任务栏和窗口管理行为时，分离底部面板，通过任务栏分别最小化与还原两个窗口，使用 Win+方向键贴靠分离窗口，通过 Alt+Tab 切换窗口，并确认主窗口可以覆盖分离窗口。
