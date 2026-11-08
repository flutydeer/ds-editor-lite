# 主菜单栏的自适应溢出契约

主菜单栏 `MainMenuView` 是 `QMenuBar`。四个顶层菜单由 `addMenu()` 产生，界面上的每一项是 `menuAction()` 返回的 `QAction`，不是独立的 `QWidget`。剪辑编辑工具栏的 `AdaptiveToolBar` 在原控件容器之间搬迁 `QWidget`，这套机制不适用于主菜单栏。两者只共享行为契约，不共享代码，对应关系见 `adaptive-clip-toolbar.md`。

## 不使用 Qt 私有扩展按钮

Qt 内建的 `QMenuBarExtension` 会把放不下的 `menuAction` 平铺进一个 `>>` 弹层，功能上接近需求。但它把扩展按钮固定在菜单栏右端，依据 `menuRect.right()` 定位（`qmenubar.cpp` 的 `updateGeometries()`），收起菜单后省下的空间全部落在最后一个可见菜单项与按钮之间，按钮无法贴着内容。

依赖这条路径还需要触达多处私有行为，包括私有 objectName `qt_menubar_ext_button`、`PM_ToolBarExtensionExtent` 的宽度、提前 `setMenu()` 让 Qt 复用弹层的隐式契约、事后用事件过滤器搬动按钮、`minimumSizeHint()` 的阈值抖动修正、隐藏菜单项的 `QEvent::Shortcut` 补偿，以及按私有 objectName 写的样式表规则。Qt 升级改动其中任何一处，图标、主题和左对齐会一起消失，且不会报错。

实现改为在 `QMenuBar` 上持有一个带三点图标的普通 `QAction`。它由 Qt 按常规 action 布局测量和绘制，天然紧跟最后一个可见菜单项。`QMenuBar::item` 的既有样式直接生效，深浅主题的 `controls.qss` 不需要为溢出按钮增加规则。

## 尺寸与顺序

四个顶层菜单在 `m_topLevelMenus` 中保持固定的左到右顺序，即文件、编辑、选项、帮助。三点 action 只在发生溢出时加入菜单栏，排在最后一个保留菜单之后。它与前一个菜单项的间距等于任意两个普通菜单项之间的间距，收起菜单省下的空间留在右侧。右侧留白是预期行为，不压缩，也不居中。

划分从右向左收起菜单。每一轮都从完整菜单集重新计算，不读取上一轮的划分结果，所以拉宽后按相同阈值还原，不产生滞回。被收起的项按原始左到右顺序挂进三点菜单，每一项都是原来的 `menuAction()` 对象，仍带着自己的 `QMenu`，弹层因此是子菜单列表。不复制 action，也不建立代理。

每个 action 的宽度用 `initStyleOption()` 加 `style()->sizeFromContents(CT_MenuBarItem, ...)` 测量。带图标的项按 `PM_SmallIconSize` 作为内容宽度，因为 `QMenuBar` 对这类项只画图标，不带图标的项按 `fontMetrics().size(Qt::TextShowMnemonic, text)` 测量。三点 action 的文字保留给助记键，但不绘制。起始偏移和项间距取 `PM_MenuBarPanelWidth`、`PM_MenuBarHMargin`、`PM_MenuBarItemSpacing`，判定右边界与 Qt 的 `menuRect` 一致，即菜单栏宽度减去面板宽度。4 像素安全余量加在需求侧，吸收纯测量与 Qt 实际 `actionGeometry()` 之间的舍入差，保证 Qt 私有的 `>>` 扩展按钮不会被触发。

安全余量必须同时计入 `sizeHint()` 和 `minimumSizeHint()`。只从可用宽度里扣而不加进尺寸提示，会让标题栏按 `sizeHint()` 给足宽度时，划分判定反而认为最后一个菜单放不下。这个错误在 Windows 原生样式下必现，因为 `PM_MenuBarHMargin` 与 `PM_MenuBarItemSpacing` 都是 0，没有多余的间距可以抵消。离屏 fusion 的 6 像素项间距会把它完全掩盖，所以回归用例必须显式模拟零间距。

`sizeHint()` 取四个菜单全部内联的宽度加安全余量，`minimumSizeHint()` 取首个菜单加三点按钮加安全余量的宽度。两者都从完整菜单集计算，不随当前划分变化，外层标题栏布局不会在阈值附近震荡。尺寸策略是 `Preferred, Fixed`，让标题栏优先压缩菜单栏。宽度低于 `minimumSizeHint()` 时连三点按钮都放不下，Qt 会启用它自己的扩展按钮，真实布局达不到这个宽度。

## 刷新时机与所有权

`event()` 在 `QMenuBar::event()` 之后处理 `Resize`、`ActionChanged`、`FontChange`、`StyleChange`、`DevicePixelRatioChange`。`LanguageChange` 在 `changeEvent()` 里先刷新文案再重算。`m_updatingOverflow` 防止同一轮刷新重入。

`syncOverflowActions()` 每次清空菜单栏和三点菜单的 action 列表，再按计划重建。`removeAction()` 只改变归属，不销毁 action，也不改变 `QMenu` 的父子关系。析构时先把两边的 action 全部摘掉，避免 `QMenuBar` 析构时遍历已归属其它菜单的对象。

三点图标来自 `more_horizontal_24_regular.svg`，用主题属性 `iconColor` 与 `iconDisabledColor` 着色。`rebuildIcons()` 会一并刷新它，`retranslateUi()` 刷新 `&More Menus` 文案和提示文字。实现不使用 `QProxyStyle`，不查找私有 objectName，不拦截快捷键事件，也不事后 `move()` 按钮。

## 键盘行为与边界

三点 action 保留 `&More Menus`，Alt+M 由 `QMenuBar` 的常规助记键处理，弹层里可以继续用键盘选子菜单。被收起的顶层菜单离开菜单栏后，它们各自的 Alt 字母键不再由菜单栏响应。这是与全部内联状态的行为差异，不是缺陷，也不需要为隐藏 action 做越界 `actionRect` 的弹层补偿。

测量按左到右累加，局部布局方向翻转（RTL）未单独验证。几何由 `QMenuBar` 自身管理，方向变化时以 Qt 的实际排布为准。

## 回归入口

`TestApplicationGui` 的 `tst_main_menu_overflow.cpp` 覆盖以下内容。

- 从完整宽度到 `minimumSizeHint()` 逐像素扫描，断言收起顺序、溢出项仍是带 `QMenu` 的原对象、三点按钮与前一个菜单项的间距等于普通项间距、Qt 私有扩展按钮始终隐藏。
- 收起与拉宽使用同一阈值，两个尺寸提示在溢出前后保持恒定。
- 菜单标题变宽后重新划分，文案刷新后三点 action 的文本仍在。
- 用 `QProxyStyle` 把项间距和水平边距置 0、面板宽度置 2，模拟 Windows 原生样式的 metric，断言 `sizeHint()` 宽度下四个菜单全部内联且不触发 Qt 私有扩展，缩窄后仍按后缀顺序收起。

设置 `LITE_MENU_SCREENSHOT_DIR` 可导出完整与溢出两种状态的离屏对比图。离屏测试只验证几何与划分，Windows 原生平台还需人工缩窄窗口，确认按钮垂直对齐、弹层焦点交接、Alt+M 与主题图标着色，不能把离屏成功等同于桌面交互成功。
