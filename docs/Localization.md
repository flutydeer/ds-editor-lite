# Localization

本文介绍 DS Editor Lite 的 UI 本地化机制，以及新增界面组件、长期状态文本或新语言时应遵循的接入方式。

## 当前策略

- 英文是源语言，源代码中的 UI 文本使用英文，不维护 `en_US.ts/qm`。
- 当前支持 `system`、`en_US`、`zh_CN` 三种语言偏好；配置保存在 `general.uiLanguage`。
- “自动检测”使用 `QLocale::system().language()`：任意中文系统映射到简体中文，其他系统回退英文。
- 自动检测在应用启动和用户重新选择“自动检测”时执行，不监听运行期间的系统语言变化。Windows 显示语言通常在注销后才生效，应用下次启动时会重新检测。
- 用户显式选择 English 或简体中文后立即热切换，不需要重启。
- UI 语言与歌声/G2P 语言、数字和日期区域格式相互独立。不要调用 `QLocale::setDefault()`；需要按 UI 语言选择本地化显示名时使用 `UiLanguageManager::effectiveLocale()`。
- 轨道名、剪辑名、歌词和文件名等项目数据不会随 UI 语言变化。`New Project` 这类未命名工程占位文本在显示时翻译，不作为项目数据保存。

## 核心架构

`UiLanguageManager` 是应用级语言入口，负责：

- 规范化并保存语言偏好；
- 解析当前有效语言；
- 持有应用、`qtbase` 和 `qt` translator；
- 安装或卸载 translator；
- 提供 `preference()`、`effectiveLanguageId()` 和 `effectiveLocale()`；
- 在有效语言切换成功后发送 `languageChanged(QString)`。

启动顺序必须保持为：

```text
QApplication
  → AppOptions（读取 uiLanguage）
  → UiLanguageManager（安装 translator）
  → AppContext（构造业务单例）
  → 其他业务对象和 MainWindow
```

这样可保证所有在构造函数中调用 `tr()` 的对象第一次创建时就使用正确语言。不要把语言初始化移到 `AppContext` 或主窗口之后。

英文模式不加载应用 QM。中文模式从 Qt 翻译目录加载 `qtbase_zh_CN`、`qt_zh_CN`，并从资源路径 `:/i18n/translation_zh_CN.qm` 加载应用翻译。应用翻译或 Qt 翻译加载失败时会记录警告、卸载已加载的 translator，并安全回退英文。

语言设置位于“设置 → 常规 → 应用”。组合框的 `itemData` 保存稳定 ID，显示文本不参与配置持久化或业务判断。缺失或非法配置统一按 `system` 处理。

## 在组件中添加可翻译文本

### 创建时翻译

QObject 子类优先使用 `tr()`：

```cpp
m_titleLabel = new QLabel(tr("Track Properties"), this);
```

不适合使用类 `tr()` 的位置应给出稳定 context：

```cpp
const auto text = QCoreApplication::translate("ProjectLoader", "Opening Project");
```

动态菜单、临时对话框和 Toast 每次创建时都会读取当前 translator，直接在创建处调用 `tr()` 即可。如果对象可能跨越语言切换继续存在，则仍需按下一节处理。

不要提前翻译后保存为全局常量、静态 `QString` 或长期业务状态，也不要通过比较翻译后的显示文本判断业务状态。

### 常驻 Widget 热切换

常驻 Widget 应集中实现 `retranslateUi()`，并处理标准 `QEvent::LanguageChange`：

```cpp
void TrackPropertiesView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void TrackPropertiesView::retranslateUi() {
    m_titleLabel->setText(tr("Track Properties"));
    m_deleteAction->setText(tr("Delete"));
    m_nameEditor->setToolTip(tr("Track Name"));
}
```

需要热切换的对象必须保存为成员，常见对象包括：

- Label、Button、Action、Menu 和 Tab 标题；
- Tooltip、StatusTip、PlaceholderText；
- GroupBox、Card 和设置项标题；
- 空状态、错误状态以及当前状态对应的动态提示。

调用基类 `changeEvent()` 后再更新本组件。不要新增一套 Widget 翻译订阅机制；Qt 的 `LanguageChange` 是常驻界面的统一入口。

Qt Designer 生成的界面可在 `LanguageChange` 中调用 `ui->retranslateUi(this)`，手工创建的控件则使用本项目的成员加 `retranslateUi()` 模式。

### 组合框和列表

组合框必须使用稳定数据标识选项，不能依赖 `currentText()`：

```cpp
const auto selectedId = m_modeComboBox->currentData();
const QSignalBlocker blocker(m_modeComboBox);

m_modeComboBox->clear();
m_modeComboBox->addItem(tr("Automatic"), Mode::Automatic);
m_modeComboBox->addItem(tr("Manual"), Mode::Manual);
m_modeComboBox->setCurrentIndex(m_modeComboBox->findData(selectedId));
```

重建显示项时必须使用 `QSignalBlocker`，并通过 `itemData` 恢复选择，避免语言切换触发设备切换、配置保存或其他业务回调。真实设备名、文件名和用户输入保持原样，只更新应用提供的占位项，例如 “Default device” 或 “(Not working)”。

### 设置页

普通 `IOptionPage` 默认会在 `LanguageChange` 后保存当前选项、重建内容并恢复滚动位置，适合构造过程没有副作用的简单页面。

如果页面构造时会枚举设备、连接外部回调或修改运行时状态，应像 `AudioPage` 和 `MidiPage` 一样覆盖 `changeEvent()`，原地执行 `retranslateUi()`，不要重建页面。此类页面的组合框必须遵守稳定 `itemData` 和 `QSignalBlocker` 规则。

### Tooltip

更新控件的 `toolTip` 即可让 `ToolTipFilter` 在下次显示时读取新标题。自定义 Tooltip 持有的快捷键和消息也应通过其 setter 更新。不要假设语言切换时 Tooltip 实例一定存在；隐藏动画期间它可能暂时为空。

## 长期状态文本

如果一个 UI 文本的生命周期长于 translator，例如撤销历史、未命名工程状态或其他跨语言切换的显示状态，不要保存 `tr()` 的返回值。应保存语义状态，在显示时翻译。

撤销/重做动作使用 `ActionSequence::setTranslatableName()` 保存 context 和源文本：

```cpp
setTranslatableName("NoteActions",
                    QT_TRANSLATE_NOOP("NoteActions", "Insert note(s)"));
```

`QT_TRANSLATE_NOOP` 让 `lupdate` 提取字符串，但不会在动作创建时翻译；`ActionSequence::name()` 在菜单或 Tooltip 刷新时调用 `QCoreApplication::translate()`。如果名称本身是用户数据或无需翻译，可继续使用 `setName(QString)`。

新增类似状态时优先采用以下顺序：

1. 保存枚举、ID 或空值等语义状态；
2. 保存显示所需的未翻译参数；
3. 在 getter 或 View 渲染时调用 `tr()`；
4. 在 `LanguageChange` 时重新读取并显示。

不要通过字符串匹配识别默认名称，因为用户可能真的输入同样的文本。

## 翻译文件和构建

应用翻译源文件位于：

```text
src/app/Resources/translate/translation_zh_CN.ts
```

项目通过 Qt `LinguistTools` 和 `qt_add_translations()` 管理翻译。完成 Debug preset 配置后，从源码更新 TS：

```sh
cmake --build --preset debug --target DsEditorLite_lupdate
```

使用 Qt Linguist 编辑并保存 TS 后，可单独生成 QM：

```sh
cmake --build --preset debug --target DsEditorLite_lrelease
```

普通应用构建也会生成 `translation_zh_CN.qm`，并通过 `/i18n` 资源前缀嵌入可执行文件。生成型 `.qm` 不应提交到仓库，也不需要添加手工 qrc 条目。

Windows 部署由 `windeployqt --translations zh_CN` 携带 Qt 标准控件所需的中文翻译。安装包必须在没有开发机 Qt 目录的环境中验证标准文件对话框、消息框等 Qt UI 的中文显示。

提交翻译更新前应确认：

- 没有 `unfinished`；
- 没有 `vanished` 或 `obsolete`；
- 没有空翻译；
- context 与代码实际调用一致；
- `DsEditorLite_lupdate` 再次执行不会产生意外删除或重复条目。

## 新增语言

新增语言时需要同时完成以下工作：

1. 在 `UiLanguageManager` 注册稳定语言 ID，并扩展偏好规范化和有效语言解析；
2. 在设置页组合框中添加显示名称，`itemData` 使用语言 ID；
3. 新增对应 TS，并加入 `qt_add_translations(TS_FILES ...)`；
4. 在 `UiLanguageManager` 中加载应用 QM 和该语言的 Qt translator；
5. 更新 `windeployqt --translations` 的部署语言列表；
6. 扩展 `TestUiLanguage`，覆盖显式选择、自动检测、非法配置和回退；
7. 验证缺少应用或 Qt 翻译时不会崩溃，并产生可定位日志；
8. 执行目标语言与英文之间的连续往返热切换测试。

语言 ID 是配置协议的一部分，发布后不要随意重命名。若第三方组件使用独立翻译资源，应由 `UiLanguageManager` 持有独立 translator，并和应用 translator 一起安装、卸载及验证；不要恢复 qtmediate 的另一套 Widget 翻译订阅路径。

## 新增组件检查清单

新增需要本地化的组件时逐项确认：

- 所有应用提供的 UI 文本都由 `tr()`、`QCoreApplication::translate()` 或 `QT_TRANSLATE_NOOP` 标记；
- 临时对象在创建时翻译，常驻对象实现 `LanguageChange` 和 `retranslateUi()`；
- 构造函数中的局部可见控件已改为成员，能够在切换时更新；
- 组合框用稳定 `itemData` 保存语义，并在重译时阻断信号；
- 用户内容、文件名、设备名、轨道名和剪辑名没有被误译；
- 长期状态保存语义而不是翻译快照；
- UI 语言没有改变 G2P 语言或数字、日期格式；
- 中英往返后没有重复菜单、重复连接、对象泄漏或业务回调；
- 更新 TS 后所有活动文本都有完成的翻译；
- Debug 全量构建和 CTest 通过。

## 相关实现

- [`src/app/Utils/UiLanguageManager.*`](../src/app/Utils/UiLanguageManager.cpp)：语言解析和 translator 生命周期；
- [`src/app/Model/AppOptions/Options/GeneralOption.*`](../src/app/Model/AppOptions/Options/GeneralOption.cpp)：语言偏好持久化；
- [`src/app/UI/Dialogs/Options/Pages/GeneralPage.*`](../src/app/UI/Dialogs/Options/Pages/GeneralPage.cpp)：语言设置入口；
- [`src/app/Modules/History/ActionSequence.*`](../src/app/Modules/History/ActionSequence.cpp)：长期撤销历史名称的动态翻译；
- [`src/app/Controller/DocumentWorkflow/DocumentWorkflowController.cpp`](../src/app/Controller/DocumentWorkflow/DocumentWorkflowController.cpp)：未命名工程的语义状态与动态翻译；
- [`src/app/Resources/translate/translation_zh_CN.ts`](../src/app/Resources/translate/translation_zh_CN.ts)：简体中文翻译源；
- [`src/app/CMakeLists.txt`](../src/app/CMakeLists.txt)：TS/QM 生成、资源嵌入和 Windows 部署规则；
- [`src/tests/TestUiLanguage/main.cpp`](../src/tests/TestUiLanguage/main.cpp)：语言解析和配置测试；
- [`src/tests/TestDocumentWorkflow/main.cpp`](../src/tests/TestDocumentWorkflow/main.cpp)：未命名工程和撤销历史的动态翻译测试。
