# 脱离面板窗口：独立顶层窗口契约

## 现象

开发者选项打开「Enable panel detach」后，底部面板可以分离成一个看起来独立的窗口，但它：

- 任务栏没有自己的按钮，最小化不进入任务栏，而是缩到工作区左下角变成一条小横条。
- 始终叠在主窗口之上，点击主窗口也不会沉下去。
- 无法按普通窗口的方式还原、分屏，Alt+Tab 与任务栏预览的行为都不对。

## 根因

问题不在 QWindowKit，也不在自绘标题栏，而在窗口的父子关系。

`DetachedPanelWindow` 之前以主窗口为 QWidget parent 构造：

```cpp
Window(mainWindow, Qt::Window)
```

Qt 文档对 `QWidget::isWindow()` 写明的语义是：窗口可以带 parent，但会与 parent 分组、随 parent 最小化、被 parent 删除，并且「如果窗口管理器支持，还会与 parent 共用一个任务栏项」。Windows 后端把这条语义实现为 native owner：`WindowCreationData::fromWindow()` 在顶层窗口有 `QWindow::transientParent()` 时，把这个父窗口的 HWND 作为 `CreateWindowEx` 的 owner 传入（owner 走 `GWL_HWNDPARENT`，不是 `SetParent`）。

owned top-level 的 Win32 规则正好逐条对应上面的现象：

- 任务栏不为 owned 窗口建按钮，最小化动画没有按钮可去，窗口被压到工作区角落。
- owned 窗口永远在 owner 之上。
- 最小化、显示、隐藏与 owner 生命周期绑定。

`isWindow()` 仍然返回 `true`，所以只检查 `isWindow()` 的测试发现不了这个问题。

## 设计

### 无 QWidget parent，所有权显式化

`DetachedPanelWindow` 以 `Window(nullptr, Qt::Window)` 构造，是一个没有 owner 的真顶层窗口。主窗口不再通过 QObject 父子树回收它，而是用 `std::unique_ptr` 显式持有，并在析构时先嵌回面板、再销毁窗口。

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

## 为什么不采用其它方案

- **保留 parent，事后清 `GWLP_HWNDPARENT`**：Qt 仍认为它是 secondary window，窗口重建、flag 变更、`updateTransientParent()` 都会重新施加 owner，形成两套互相矛盾的真相。
- **保留 parent，补 `WS_EX_APPWINDOW`**：任务栏按钮可能出现，但 owned 窗口的置顶与成组最小化仍然错误。
- **`QMdiArea` 或窗口内浮层**：没有独立任务栏按钮，没有系统 Snap，不是目标模型。
- **通用窗口注册表/基类**：当前只有一个二级窗口，显式所有权与本文契约足够。等真的有多个编辑器窗口时再抽，避免为空需求造框架。

## 覆盖范围与已知边界

- 本轮只改造底部面板这一个二级窗口，不新增钢琴卷帘、混音台等独立窗口。
- 不提供 Always on Top 开关。本轮消除的是 owner 带来的隐式置顶，显式置顶如需再加。
- 不恢复上次退出时的分离状态。`enablePanelDetach` 仍是开发者选项，启动即分裂会让人意外。
- 不改变设置、Toast、文档工作流对话框的主窗口归属。这些是多窗口交互体验的后续项。
- 自动化 `WindowSettingsDto` 不暴露 `detachedPanelGeometry`，它只是宿主本地窗口状态。

## 验证

自动化能证明的：分离窗口 `parentWidget() == nullptr`、`transientParent() == nullptr`、不在主窗口的 `findChild` 树里、在 `topLevelWidgets()` 里、Windows 下 `GW_OWNER == nullptr` 且没有 `WS_EX_TOOLWINDOW`，标题随面板页更新，几何跨主窗口实例恢复，主窗口销毁时窗口随之销毁。

自动化不能证明、必须真机检查的：任务栏出现独立按钮、点击按钮最小化/还原、Win+Left/Right Snap、窗口能被主窗口覆盖、Alt+Tab 列出两个窗口、最小化进入任务栏而不是左下角。

真机验收步骤见 PR 的 `## 验证` 一节。
