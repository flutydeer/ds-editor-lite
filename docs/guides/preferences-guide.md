# 首选项页面开发指南

首选项由 [`AppOptions`](../../src/app/Model/AppOptions/AppOptions.cpp) 读写。对话框负责按需创建页面，页面通过设置服务提交修改。Windows 上的默认配置文件为 `%APPDATA%\OpenVPI\DS Editor Lite\appConfig.json`，实际路径可通过 `AppOptions::configPath()` 获取。

## 定义和保存选项

在 [`Model/AppOptions/Options`](../../src/app/Model/AppOptions/Options) 实现 `IOption`。基类构造参数指定配置文件中的分组键。`load()` 从对应 JSON 对象读值，缺省字段保留模型中的默认值。`save()` 将当前值写入 JSON。给枚举选项定义稳定的字符串值，使配置独立于 C++ 枚举的排列。

新增配置分组时，在 `AppOptions` 的构造函数和 `saveAndNotify()` 中注册它的读写，并提供读取该分组的访问方法。已有分组内的新选项可以直接扩展对应模型的读写。

## 创建页面

从 [`IOptionPage`](../../src/app/UI/Dialogs/Options/Pages/IOptionPage.h) 派生页面，在构造函数中调用 `initializePage()`。在 `createContentWidget()` 中用当前设置初始化控件，并为标签提供可翻译文本。`OptionListCard` 可以组合带标题、说明和右侧控件的 `OptionsCardItem`，页面主布局末尾以 `addStretch()` 保持卡片顶部对齐。

在控件结束编辑或切换时调用 `modifyOption()`，通过设置服务提交对应分组的值。服务集中的校验、持久化和变更通知同时适用于界面和自动化调用。已有分组可沿用 [`AppearancePage`](../../src/app/UI/Dialogs/Options/Pages/AppearancePage.cpp) 的做法，从 `settings().getSettings()` 取得快照，修改相应字段后调用该分组的更新方法。提交失败时，按快照恢复控件显示并按需报告错误。

## 注册页面与菜单入口

1. 在 [`AppOptionsGlobal::Option`](../../src/app/Global/AppOptionsGlobal.h) 中为页面增加枚举值。
2. 在 [`AppOptionsDialog::retranslateUi()`](../../src/app/UI/Dialogs/Options/AppOptionsDialog.cpp) 中加入页面标签和图标。可见页面的顺序对应枚举值，第一行从 `All` 之后开始。
3. 在 `AppOptionsDialog::ensurePage()` 中为枚举值添加页面构造分支。对话框在用户首次选中该页面时创建并保存实例。
4. 需要从菜单直达该页时，沿用主菜单的选项动作，将对应枚举值传给 [`MainWindow::openAppOptions()`](../../src/app/UI/Window/MainWindow.cpp)。由触发菜单所在的窗口作为模态对话框的父窗口。
