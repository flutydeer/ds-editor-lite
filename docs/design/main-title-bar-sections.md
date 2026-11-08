# 主标题栏的分段布局契约

主标题栏 `MainTitleBar` 的内容按固定顺序排列：应用图标位、主菜单、项目名下拉、文档操作、弹性空间、播放控制、系统按钮。分隔线只出现在前导簇内相邻的两段之间，即菜单与项目名之间、项目名与文档操作之间。播放控制和系统按钮不参与前导簇，不带分隔线。

## 问题

macOS 由平台持有菜单栏。`QMenuBarPrivate::init()` 一旦拿到 `platformMenuBar` 就直接 `q->hide()`（`qtbase/src/widgets/widgets/qmenubar.cpp`），控件本身不再显示在窗口里。隐藏控件在 `QLayout` 里是 empty item，宽度按 0 计算，于是菜单段塌成 0 宽，原本插在"菜单"和"项目名"之间的分隔线成了标题栏的第一个可见元素，在窗口最左侧留下一条孤立的竖线。

用 `#ifdef Q_OS_MAC` 单独跳过这条线能修掉这一个症状，但平台差异会继续散落在构造函数的 `if` 链里。任何一次"某段在某个环境不存在"的改动都会重现同一类残骸。

## 布局计划

`TitleBarLayoutPlan`（`src/app/UI/Views/MainTitleBar/TitleBarLayoutPlan.h`）是纯数据加纯工厂，字段描述有哪些段：

| 字段 | 含义 |
| --- | --- |
| `appIconInset` | 无边框窗口在左端留出应用图标/拖拽位 |
| `inWindowMenu` | 菜单栏画在本窗口内 |
| `projectSelector` | 项目名下拉 |
| `documentActions` | 保存等文档操作按钮 |
| `playback` | 播放控制 |
| `systemButtons` | 自绘的最小化/最大化/关闭 |
| `leftMargin` | 左内边距 |

`forChrome(useNativeFrame, menuInWindow)` 是纯函数，测试可以在任意平台直接断言，不需要模拟 macOS。

## 构建规则

`buildSections()` 顺序遍历计划。前导簇用 `addLeadingSection()` 追加，规则是"已有段时才先插一条分隔线"。于是"标题栏最左侧不会出现分隔线"成为结构上的不变量，而不是某一处判断的结果。摘掉菜单段只需把 `inWindowMenu` 置为 false，分隔线自动随之消失。

`inWindowMenu` 为 false 时菜单控件不进入布局，但仍以主窗口为父对象。原生菜单栏挂在 `parentWidget()` 的 `windowHandle()` 上，与布局无关，因此两者互不影响。

## 运行时切换

`setLayoutPlan()` 清空布局项、重建分段。清空只删除布局项与分隔线，widget 本身保留（`takeAt()` 返回的 `QLayoutItem` 析构不会析构 widget）。计划和当前一致时直接返回，避免无意义的 relayout。

计划在 `MainWindow::titleBarLayoutPlan()` 里计算：

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

`TabPanelTitleBar`（底部面板独立成窗）仍是旧路径，独立窗口上还会看到原生标题栏与自绘三键，属于后续待办。

## 双击标题栏

系统边框模式下没有 window agent，平台自己的标题栏是这一行之上的另一条窄带，双击这一行的空白处原本什么都不发生，与 macOS 上"整个标题栏都能铺满/还原"的预期不符。

`MainTitleBar` 现在自己上报这个手势：`mouseDoubleClickEvent()` 只在落点没有子控件时发 `doubleClicked()`，因为忽略鼠标事件的子控件会把按下事件冒泡上来。`MainWindow` 只在系统边框模式下连接它（无边框时 QWK 自己的 `MouseButtonDblClick` 分支已经在 `isInTitleBarDraggableArea()` 里处理，两处都连会导致来回切换两次），行为是最大化与还原互切，全屏时忽略。

## 与自适应溢出的关系

计划里的 `inWindowMenu` 同时下发给 `MainMenuView::setInWindowMenu()`。原生菜单栏下自适应溢出整体不成立：`removeAction()` 会经过 `QMenuBar::actionEvent` 走到 `QPlatformMenuBar::removeMenu()`，把菜单从系统菜单栏里摘掉，窗口里省不下任何宽度，系统顶栏反而少几个菜单。该状态下的划分恒为"全部内联"，`sizeHint()` 退回 `QMenuBar` 自身的空提示。详见 `adaptive-main-menu-bar.md`。

## 回归入口

`TestApplicationGui` 的 `tst_title_bar_layout.cpp` 覆盖以下内容。

- 三组 `forChrome()` 组合的字段与间距契约：Windows 无边框、macOS 无边框、两种原生边框。
- 有菜单段时两条分隔线，且首段是菜单控件。
- 摘掉菜单段后只剩一条分隔线，首段是项目名下拉，下拉左侧没有任何分隔线。
- 切回原计划后分段与分隔线数量完全复现。
- 菜单栏离开窗口后划分保持全部内联，`overflowedMenuActions()` 为空。
- 双击标题栏空白处最大化，再次双击还原；落点在子控件上时不触发。
