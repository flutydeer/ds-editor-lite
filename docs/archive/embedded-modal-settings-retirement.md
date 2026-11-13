# 内嵌模态设置窗口：移除记录

> 状态：❌ 已移除（实验功能废弃，2026-11-09）
> 本文是该功能唯一保留的文档。原设计说明不再维护，代码与配置入口已全部删除。
>
> 2026-11 注记：分离面板后来改成了无 native owner 的独立顶层窗口，本文第 1 条关于
> 「owned 顶层窗口」的前提不再成立（见 `docs/design/detached-panel-window.md`）。移除内嵌模态
> 设置窗口的结论不受影响。

## 决策

彻底移除「内嵌模态设置窗口」（Embedded Modal Settings）实验功能。设置窗口一律使用
`AppOptionsDialog::showStandaloneDialog()` 打开的独立模态对话框，不再保留开发者选项开关、
内嵌浮层宿主、背景交互挂起逻辑，以及配套的主题样式、翻译条目和自动化字段。

## 背景

该功能把设置窗口从独立顶层对话框改成铺满主窗口的浮层宿主（`EmbeddedModalHost` +
`EmbeddedModalPanel`），面板内嵌设置页，背后的编辑器保持可见。原目标是摆脱独立窗口的层级与
失焦问题，并用手工方式保留模态语义。它始终是默认关闭的实验开关（Developer Options →
Embedded options dialog），切换后需要重启应用才生效。

## 移除原因：与多窗口机制冲突

**核心原因是内嵌式对话框与面板分离后的多窗口机制不兼容。**

1. 内嵌浮层是主窗口的**子控件**，绘制范围仅限主窗口客户区，覆盖不到任何其它顶层窗口。
   `DetachedPanelWindow` 是 owned 顶层窗口，在 Windows 原生 z-order 中始终位于 owner 之上，
   并且带有自己的 `MainMenuView`（含 Options 菜单）。从分离窗口的菜单打开设置时，内嵌遮罩
   会被该窗口完全挡住，用户看不到任何设置界面。
2. 为绕开这一点，`MainWindow::openAppOptions()` 只能按 `requestingWindow` 分流：从分离窗口
   发起时强制退化成独立对话框，从主窗口发起时才按开关走内嵌路径。同一个「设置」入口因发起
   窗口不同而有两套表现，这恰好是内嵌方案原本要消除的层级不一致，等于把问题换个形式留了下来。
3. 多窗口还带来焦点与激活状态的分叉。内嵌模态不是 Qt 原生模态窗口，需要手工挂起与恢复背景
   的 `QAction` 和 `QShortcut`，并在关闭时归还焦点。挂起列表曾被模态期间重建的背景 action
   打穿（裸指针悬垂导致闪退），改用 `QPointer` 才修好。每增加一个顶层窗口或动态创建的
   action，都要重新核对这套模拟模态是否仍然成立。
4. Windows 的滚轮跟随键盘焦点而不是光标下的控件，Direct Manipulation 又会在外观选项变更时
   重新注册并劫持主窗口的 `WM_MOUSEWHEEL`。内嵌面板是主窗口子级，只能靠延迟转移焦点和
   `isOpen()` 守卫逐项绕过。这类平台特例在独立模态对话框里并不存在。

这个开关默认关闭且从未转正，为它长期维护一套与多窗口机制对抗的模拟模态并不划算。保留 Qt
原生模态对话框可以同时满足层级、焦点、快捷键与滚轮语义，并且天然兼容任意数量的顶层窗口。

## 移除范围

- 删除 `EmbeddedModalHost.h/.cpp` 浮层宿主，以及 `MainWindow` 中的 `m_modalHost`、
  `m_appOptionsDialog`、背景交互挂起与恢复、焦点暂存、`resizeEvent` 浮层同步。
  `openAppOptions()` 收敛为一次 `showStandaloneDialog()` 调用，`closeAppOptions()` 一并删除。
- 删除开发者选项 `DeveloperOption::enableEmbeddedOptionsDialog` 及其序列化、DeveloperPage
  开关 UI 与重启提示翻译。
- 删除自动化 DTO `DeveloperSettingsDto::enableEmbeddedOptionsDialog` 与捕获/恢复映射。
- `AppOptionsDialog` 不再区分内嵌与独立两种宿主模式，去掉 `standalone` 构造参数、
  `m_standalone` 成员和 sidebar 的 `standalone` QSS 属性。
- 删除两套主题的 `#EmbeddedModalPanel` 样式与 `overlay.backdrop` 语义色 token。
- 删除或改写相关回归用例，并从 GUI 回归矩阵中移除对 Embedded Options 的重启验证步骤。

## 兼容影响

- 旧配置文件里残留的 `enableEmbeddedOptionsDialog` 键会被忽略，不影响其它选项加载。
- 自动化接口 `settings.get_settings` 与 `settings.update_developer` 的 developer 对象不再包含
  `enableEmbeddedOptionsDialog` 字段，调用方显式传该字段时会被忽略。
- 用户可感知的变化只有一个：Developer Options 里少了一个实验开关。设置窗口恢复为统一的独立
  模态对话框，从主窗口和分离面板窗口打开的行为一致。

## 结论

不做功能开关式保留，也不保留原设计说明。若将来重新考虑内嵌方案，必须先解决「模态层要覆盖
所有顶层窗口」这一前提，否则会重演本次冲突。
