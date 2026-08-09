# 内嵌模态设置窗口（Embedded Modal Settings）— 设计文档

> 状态：✅ 已完成（含滚轮/焦点兼容与崩溃修复）
> 本文是设计说明；涉及文件修改清单已移除。

## 概述

设置窗口由独立顶层对话框改为**内嵌模态（Embedded Modal）**：一个铺满主窗口的浮层宿主（backdrop + 居中面板），面板内嵌设置页，背后的编辑器保持可见（类 Web 模态风格）。改造目标：

- 摆脱独立窗口的层级/失焦问题（旧对话框是顶层窗口，切换窗口、任务栏表现与编辑器不一致）；
- 保留模态语义：打开期间背景菜单、快捷键被禁用，不能触发后台操作；
- 视觉上融入主题体系。

## 架构

```
MainWindow
└── EmbeddedModalHost (无布局浮层，子级 QWidget)
    ├── backdrop：paintEvent 自绘半透明遮罩
    └── EmbeddedModalPanel (objectName，QSS 样式化)
        └── AppOptionsPanel (复用现有设置页)
```

- `EmbeddedModalHost` 是**无布局浮层子窗口**（与 `OverlaySplitter` 的悬浮 grip 同一模式）：不参与任何父级布局，由 MainWindow 在 `resizeEvent` 中同步 `setGeometry(rect())`，打开时 `raise()`。
- `open(content, panelSize)` 将内容 reparent 到 `m_panel` 内，`closePanel()` 立即 `hide()` 并 `emit closed()`。
- 面板背景由 QSS 控制：`#EmbeddedModalPanel { background: ${surface.window} }`（跟随主题 token，两套主题统一）；左侧 tab 列表（`QListWidget#AppOptionsDialogTabListWidget`）使用独立语义 token **`surface.sidebar`**（dark：`neutral.800`，light：`neutral.0`），比 `surface.window` 亮一档，形成左栏与右侧内容区的层次。
- 布局无整体内边距（body `setContentsMargins({})`）：左侧栏为独立容器 `QWidget#AppOptionsSidebar`（`WA_StyledBackground`，背景用 `surface.sidebar`，`border-top/bottom-left-radius: 8px` 对齐面板圆角），其布局 `setContentsMargins(12, 9, 12, 9)` + item 3px 上下外边距补偿出 12px 内容缩进；列表本身透明、无任何内边距（滚动区上的 box-model padding 会引发多余的滚动条，故背景与内边距全部移到侧栏容器上）；侧栏宽度固定为「列表固定宽 + 左右边距」（列表原始 sizeHint 基于内容会超过固定宽度，须钳制否则背景比列表宽）。
- 右侧页面（`IOptionPage`，继承 `QScrollArea`）单独覆盖全局 `QScrollArea { padding: 6px }` 规则为 0（`QScrollArea.IOptionPage { padding: 0px }`，提高选择器特异性）；并安装 `OverlayScrollBar`（悬浮在 viewport 右缘、悬停淡入，原生滚动条 AlwaysOff 不预留空间），**滚动条显隐不影响卡片宽度**。

## 渲染（无动画）

- **不使用 QGraphicsEffect**：早期方案用 QGraphicsOpacityEffect 叠加透明度，在 Windows 上会破坏 QSS 样式控件的绘制（`QPainter::begin: a paint device can only be painted by one painter at a time`）。
- **也不使用 QVariantAnimation 渐入渐出**：曾用单动画同时驱动 backdrop 透明度与面板上浮（150ms OutCubic，12px），但半透明遮罩的逐帧重绘即使在 Release 下也明显卡顿，最终**彻底移除动画**——打开/关闭瞬时完成。
- 打开时 `show() + raise()`，`paintEvent` 以**全不透明度** `fillRect` backdrop；关闭即 `hide()`，不经过任何过渡。
- 窗口缩放/最大化还原时 `resizeEvent` 调 `anchorPanelToCenter()` 重算居中位并移动面板。
- backdrop 颜色取主题 token `overlay.backdrop`，无效时回退 `QColor(0, 0, 0, 96)`；`ThemeManager::themeChanged` 时刷新。

## 交互语义

- 面板**无标题栏**；关闭途径只有两种：**点击 backdrop** 或 **Esc**（宿主上的 `QShortcut`，仅在打开期间 `setEnabled(true)`）。
- **点击 backdrop 关闭**：`mousePressEvent` 仅在点击位置**落在面板矩形之外**时调 `closePanel()`；从面板空白区冒泡上来的按下事件被吞掉但不关闭（普通 `QWidget` 默认忽略按下事件，会沿父链冒泡到宿主，若不按位置区分会误关）。
- 打开期间后台的菜单/快捷键被禁用（见下）。

## 背景交互挂起与恢复

模态打开时，MainWindow 会把背景区的交互能力禁用，关闭时恢复：

```cpp
// suspendBackgroundInteraction()：在 host->open() 之后调用，
// 使面板（host 的子孙）被排除在过滤之外
for (auto *action : findChildren<QAction *>())
    if (!insideModal(action) && action->isEnabled()) {
        m_suspendedActions.append(action);   // QList<QPointer<QAction>>
        action->setEnabled(false);
    }
// QShortcut 同理挂到 m_suspendedShortcuts
```

- 过滤规则：父链上出现 `m_modalHost` 即视为模态内部，跳过。
- 恢复在 `EmbeddedModalHost::closed`（面板隐藏后）触发 `restoreBackgroundInteraction()`：逐个 `setEnabled(true)`、清空列表、归还焦点、恢复 Direct Manipulation（见下）。

### 挂起列表必须用 QPointer（崩溃修复记录）

**问题**：挂起列表最初存裸 `QAction*`。用户打开设置后修改了选项 → `AppOptions::optionsChanged` 发出 → 背景区的轨道头 `TrackControlView` 刷新歌手下拉框的**注入混合预设菜单项**（`populatePresetMenus → clearInjectedActions`）→ 旧的注入 action 被 `deleteLater()` 销毁——它们仍在挂起列表里，成了悬垂指针。关闭设置恢复时 `setEnabled(true)` 命中已释放内存（MSVC 堆填充 `0xdd`）→ 访问违例闪退。

**修复**：`m_suspendedActions` 改为 `QList<QPointer<QAction>>`，恢复时跳过已置空的项：

```cpp
for (const auto &action : std::as_const(m_suspendedActions))
    if (action)
        action->setEnabled(true);
```

QPointer 在对象销毁时自动置空，从结构上免疫"模态打开期间背景 action 被重建/删除"这类竞态。**任何存跨模态生命周期指针的类似清单都应使用 QPointer 而非裸指针。**

## Windows 滚轮兼容（Direct Manipulation + 焦点）

内嵌改造后曾出现两个问题，根因都在 Windows 平台特性：

1. **Direct Manipulation 吞滚轮**：主窗口启用了 `QtWin32DirectManipulateHelper`（`WITH_DIRECT_MANIPULATION`，由 `AppContext` 的 `DirectManipulationHolder` 在启动时注册）。DManip 在**首次滚轮事件后**接管主窗口的 `WM_MOUSEWHEEL`，把后续滚轮转换成编辑器平移手势——于是设置页再收不到 `QWheelEvent`（旧对话框是独立顶层窗口、未注册 DManip，所以以前正常）。修复：打开模态时 `unregisterDirectManipulation()`，关闭恢复时 `registerDirectManipulation()`。

2. **滚轮跟随键盘焦点**：Windows 上滚轮事件派发给**焦点控件**而非光标下控件。打开设置时焦点在标题栏/菜单上，滚轮会落到背景。修复：
   - 打开前保存 `m_focusBeforeModal = qApp->focusWidget()`；
   - 用 `QTimer::singleShot(0, ...)` **推迟到菜单弹层关闭之后**再把焦点放进面板（触发菜单的 action 执行完会把焦点还原到菜单前控件，立即 setFocus 会被覆盖）；
   - 关闭时若保存的控件仍可见则归还焦点。

## 关键文件

- `src/app/UI/Window/EmbeddedModalHost.h/.cpp` — 浮层宿主（backdrop 自绘、Esc、点击吞噬）
- `src/app/UI/Window/MainWindow.h/.cpp` — `openAppOptions`/`closeAppOptions`/`suspendBackgroundInteraction`/`restoreBackgroundInteraction`、`m_focusBeforeModal`
- `src/app/UI/Dialogs/Options/AppOptionsPanel.h/.cpp` — 设置面板（`selectOption`）
- `src/app/UI/Views/TrackEditor/TrackControlView.cpp` — 崩溃场景相关：`populatePresetMenus`/`clearInjectedActions`（模态期间销毁背景 action 的源头）
- `src/app/AppContext.cpp`、`src/app/CMakeLists.txt` — Direct Manipulation 注册与条件编译
- `src/app/Resources/theme/lite-dark|lite-light/controls.qss` — `#EmbeddedModalPanel` 背景 token
