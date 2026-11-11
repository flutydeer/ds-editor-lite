# 主标题栏的分段布局契约

主标题栏 `MainTitleBar` 的内容按固定顺序排列：应用图标位、主菜单、项目名下拉、文档操作、弹性空间、播放控制、系统按钮。分隔线只出现在前导簇内相邻的两段之间，即菜单与项目名之间、项目名与文档操作之间。播放控制和系统按钮不参与前导簇，不带分隔线。

## 问题

macOS 由平台持有菜单栏。`QMenuBarPrivate::init()` 一旦拿到 `platformMenuBar` 就直接 `q->hide()`（`qtbase/src/widgets/widgets/qmenubar.cpp`），控件本身不再显示在窗口里。隐藏控件在 `QLayout` 里是 empty item，宽度按 0 计算，于是菜单段塌成 0 宽，原本插在"菜单"和"项目名"之间的分隔线成了标题栏的第一个可见元素，在窗口最左侧留下一条孤立的竖线。

用 `#ifdef Q_OS_MAC` 单独跳过这条线能修掉这一个症状，但平台差异会继续散落在构造函数的 `if` 链里。任何一次"某段在某个环境不存在"的改动都会重现同一类残骸。

## 布局计划

主窗口和分离面板窗口共用 `TitleBarSections`（`src/app/UI/Views/MainTitleBar/TitleBarSections.h`）。字段描述标题栏中的各段。

| 字段 | 含义 |
| --- | --- |
| `appIconInset` | 无边框窗口在左端留出应用图标/拖拽位 |
| `mainMenu` | 菜单栏画在本窗口内 |
| `trafficLightArea` | macOS 无边框窗口的原生红绿灯槽位 |
| `projectSelector` | 项目名下拉 |
| `documentActions` | 保存等文档操作按钮 |
| `playback` | 播放控制 |
| `systemButtons` | 自绘的最小化/最大化/关闭 |
| `leftMargin` | 左内边距 |

`forMainWindow(useNativeFrame, platform)` 计算主窗口的边框布局，测试可以在任意平台传入目标平台。分离面板通过 `forDetachedPanel(useNativeFrame)` 选择缩减后的布局。

## 构建规则

`buildSections()` 按顺序添加控件。追加前导段时，已有前导段才插入分隔线。调用 `setInWindowMenu(false)` 摘掉菜单段时，它与项目名之间的分隔线也会消失。

平台持有菜单栏时，菜单控件保留主窗口父对象，原生菜单栏继续关联主窗口的 `windowHandle()`。

## 运行时切换

`setInWindowMenu()` 在菜单位置变动时清空布局项并重建分段。清空只删除布局项与分隔线，保留各段的控件。

主窗口通过 `MainWindow::mainMenuInWindow()` 判断菜单是否放在窗口内。

```cpp
!m_mainMenu->isNativeMenuBar() && !appOptions->developer()->hideMainMenuInWindow
```

`isNativeMenuBar()` 覆盖所有由平台持有菜单栏的环境，包括 macOS 与使用 dbusmenu 的 Ubuntu Unity。开发者选项 `hideMainMenuInWindow` 把同一份计划扩展到 Windows/Linux，用于在没有 macOS 机器时预览那套外观。

## macOS 无边框：红绿灯进标题栏

macOS 开无边框时曾出现两个症状：红绿灯另起一行占掉整条原生标题栏，右侧却画着 Windows 风格的三个按钮。

红绿灯那一行来自 QWindowKit 的一次性动作。`NSWindowProxy::setSystemTitleBarVisible(false)` 负责把窗口切成 full-size content（`NSWindowStyleMaskFullSizeContentView`、透明标题栏、隐藏标题），而它只在 `CocoaWindowContext::winIdChanged()` 和 NSView 的 `window` 属性 KVO 回调里各跑一次，函数开头 `[nsview window]` 为空就直接返回。错过这一次，窗口就永远停在原生标题栏状态。

Windows 三键则是判断写成了“是否无边框”：`systemButtons` 只看 `useNativeFrame`，macOS 无边框也满足条件，于是自绘了三键。QWindowKit 官方示例明确用 `#ifndef Q_OS_MAC` 把自绘系统按钮排除掉，macOS 只调 `setSystemButtonArea*`。

现在的规则：

- 计划里 `trafficLightArea` 与 `systemButtons` 互斥。macOS 无边框只置 `trafficLightArea`，`leftMargin` 随之取 0，让红绿灯贴住窗口左边。
- 标题栏左端放一个 74 像素宽、不绘制、不参与 hit-test 的空 widget。它只提供几何，交给 `WidgetWindowAgent::setSystemButtonArea()`，QWK 会把三个标准按钮居中到该矩形内，并在窗口缩放、退出全屏动画后自行重算。不参与 hit-test 意味着这块区域仍然可以拖动窗口。
- `setSystemButtonArea()` 必须在 `setTitleBar()` 之后调用，后者在 macOS 上会把区域清空。
- 窗口 `Show` 时再调一次 `WindowFrameUtils::mergeTitleBarIntoContent()`，幂等地把 full-size content 补上。QWK 那一次能生效时这等于空操作。

对话框走同一套：`DialogTitleBar` 的 `trafficLightArea` 参数打开时只留红绿灯槽位，不再创建自绘关闭按钮。

独立面板使用 `MainTitleBar` 的缩减布局，直接复用主窗口的平台计划，只去掉项目名和文档操作。macOS 无边框分离窗口也使用同一套 74 像素红绿灯槽位、QWK 定位和 full-size content 补偿，并关闭 Qt 的安全区自动留白。移除自绘三键后，原有弹性空间让播放工具自然靠右，不增加占位。原生红色关闭按钮仍经 `closeEvent()` 请求重新嵌回主窗口，不销毁编辑状态。

## 双击标题栏

系统边框模式下没有 window agent，平台自己的标题栏是这一行之上的另一条窄带，双击这一行的空白处原本什么都不发生，与 macOS 上"整个标题栏都能铺满/还原"的预期不符。

`MainTitleBar` 现在自己上报这个手势：`mouseDoubleClickEvent()` 只在落点没有子控件时发 `doubleClicked()`，因为忽略鼠标事件的子控件会把按下事件冒泡上来。`MainWindow` 只在系统边框模式下连接它（无边框时 QWK 自己的 `MouseButtonDblClick` 分支已经在 `isInTitleBarDraggableArea()` 里处理，两处都连会导致来回切换两次），行为是最大化与还原互切，全屏时忽略。

## 与自适应溢出的关系

`MainTitleBar::setInWindowMenu()` 将菜单位置同步给 `MainMenuView`。在原生菜单栏上，`removeAction()` 会通过 `QMenuBar::actionEvent` 调用 `QPlatformMenuBar::removeMenu()`，从系统菜单栏里摘掉菜单。该状态下的划分恒为“全部内联”，`sizeHint()` 使用 `QMenuBar` 自身的提示。详见 `adaptive-main-menu-bar.md`。

## 回归入口

`TestApplicationGui` 的 `tst_title_bar_layout.cpp` 覆盖以下内容。

- `forMainWindow()` 在 Windows 和 macOS 上的无边框及原生边框布局和间距。
- 分离窗口在各平台、两种边框模式下复用主窗口的系统按钮计划，macOS 无边框没有顶部空白和自绘三键，播放工具靠右。重复分离与关闭重新嵌回后，编辑上下文保持不变。
- 有菜单段时两条分隔线，且首段是菜单控件。
- 摘掉菜单段后只剩一条分隔线，首段是项目名下拉，下拉左侧没有任何分隔线。
- 切回原计划后分段与分隔线数量完全复现。
- 菜单栏离开窗口后划分保持全部内联，`overflowedMenuActions()` 为空。
- 双击标题栏空白处最大化，再次双击还原；落点在子控件上时不触发。
