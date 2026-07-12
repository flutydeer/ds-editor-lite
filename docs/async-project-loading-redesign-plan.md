# 异步工程加载重设计方案

## 背景

这个问题只在“关闭程序后，带工程路径参数启动”时出现：轨道歌手变成 `No singer`，剪辑上可能还残留歌手显示，但无法触发推理。同一个工程在程序不关闭的情况下重新打开则正常。

根因不是 dspx 某个字段单点解析错误，而是启动异步时序存在缺口：`main.cpp` 在显示 `MainWindow` 后立刻打开命令行参数中的工程；此时 `PackageManager` 仍在后台扫描已安装歌手包。`DspxProjectConverter` 加载 dspx 时会同步调用 `PackageManager::findSingerByIdentifier()` 解析保存的 singer identifier。如果包扫描还没完成，locator 为空，converter 就会写入降级版 `SingerInfo`：有 identifier/name/defaultLanguage，但缺少完整 speakers/languages/default dict。后续 `packagesRefreshed` 只刷新 UI 下拉项，不会修复已加载到模型里的 singer，也不会重试失败的推理。

当前 `PackageManager` 之所以要等 `InferEngine`，不是因为扫描元数据需要模型或推理 runtime，而是因为它复用了 `InferEngine` 里初始化好的 `srt::SynthUnit`。这把两个本应独立的能力绑在了一起：包元数据扫描被 GPU 选择、ONNX driver 初始化、推理 runtime 初始化间接阻塞。

目标是重新设计这一段异步加载链路，而不是只给 argv 启动路径加一个延迟补丁：工程解码、模型 singer/speaker 状态、推理调度和 UI 选择器都必须显式理解“包 metadata 是否已就绪”。同时，Package metadata scanning 和 Inference runtime initialization 应该解耦：工程加载只等待包 metadata，就绪后即可完整恢复 singer/speaker；实际推理再等待推理 runtime 和语言模块。

## 对现有 AppStatus 设计的评价

`src/app/Model/AppStatus/AppStatus.h` 里已经有 `ModuleType` / `ModuleStatus` / `moduleStatusChanged`，这是合理的方向：应用确实由多个异步模块组成，推理引擎、语言模块、包扫描都应该通过统一状态让上层调度，而不是各自用隐式时序判断。

目前问题不是这个设计不好，而是状态粒度和模块边界还不够准确。现在只有：

- `languageModuleStatus`
- `inferEngineEnvStatus`

但 `PackageManager` 实际上也是一个异步模块，并且它的就绪条件不应该等同于 `InferEngine` 完整就绪。`Package Ready` 应表示“包 metadata cache / locator 可用”；`Inference Ready` 应表示“推理 runtime / driver / GPU 可用”。二者可以并行初始化，且 `.dspx` 工程打开只需要前者。

因此推荐扩展现有设计，把 `PackageManager` 接入 `AppStatus`，但不要把它建模成 `Inference Ready` 的后置阶段。依赖关系应该是能力级的：工程加载依赖 Package metadata，推理调度依赖 Package metadata + Inference runtime + Language module。

## 推荐方案

## 阶段拆分与当前进度

这项工作按可独立验证的能力拆分推进，避免一次性修改工程加载、UI、模型恢复和推理调度导致风险集中。

### 阶段 1：PackageManager 状态化并与 InferEngine 解耦（已完成）

已完成内容：

- `AppStatus` 增加 `ModuleType::Package` 和 `packageModuleStatus`。
- `PackageManager` 启动时直接异步扫描包，不再等待 `InferEngine::engineInitialized`。
- `PackageManager` 自建只用于 metadata scan 的轻量 `srt::SynthUnit`。
- metadata `SynthUnit` 只注册 singer provider plugin path，并复用 `appOptions->general()->packageSearchPaths`。
- `PackageManager::refreshInstalledPackages()` 不再依赖 `inferEngine->initialized()` / `inferEngine->synthUnit()`。
- `packagesRefreshed(...)` 改为释放 `m_resultRwLock` 后发出。
- `PackageManager` 扫描任务完成后维护 Package module 的 `Loading / Ready / Error` 状态。

已验证现象：带参启动工程时，包 metadata 能独立完成扫描并立即恢复歌手信息；推理引擎初始化完成后，现有推理链路会继续自动进入推理。

阶段 1 不包含：`.dspx` 打开等待 Package Ready、歌手下拉框 loading placeholder、fallback singer rehydrate、推理 gate 显式等待 Package Ready。这些保留到后续阶段。

### 阶段 2：DSPX 打开等待 Package Ready

目标是修复命令行带 `.dspx` 路径启动时的生命周期缺口。阶段 2 拆成两个小步骤，先解耦进度对话框 UI，再接入工程打开等待逻辑。

#### 阶段 2A：抽出通用 ProgressDialog

`TaskDialog` 当前同时承担进度 UI 和 `Task` 状态绑定。新增一个更通用的 `ProgressDialog`，只负责标题、消息、进度条、取消按钮、隐藏/关闭控制等 UI 能力，不依赖 `Task` / `TaskManager`。`TaskDialog` 改为继承 `ProgressDialog`，只保留把 `TaskStatus` 映射到通用进度 UI、取消时终止任务的适配逻辑。

这样 `.dspx` 等待 Package Ready 可以使用 `ProgressDialog`，语义上不再伪装成一个 task；现有 TaskDialog 调用点也能保持行为基本不变。

#### 阶段 2B：DSPX 请求打开等待 Package Ready

新增 `requestOpenFile(...)`，让 `.dspx` 在 Package `Unknown / Loading` 时显示 `ProgressDialog` 并等待 Package Ready；Package Error 时允许用户降级打开。

当前进度（2026-06-06）：

- 已新增 `ProgressDialog`，并让 `TaskDialog` 继承它，仅保留 Task 状态适配与取消终止任务逻辑。
- 已新增 `AppController::requestOpenFile(...)`，命令行启动路径已改为事件循环开始后 deferred request。
- 已修复 pending open 的两个紧急问题：等待连接建立后会立即复查当前 Package 状态，避免错过 Ready/Error；pending dialog 在完成、替换请求、控制器析构时会集中关闭并删除，避免泄漏。
- 已修复主窗口阻塞问题：根因是测试用的 `QThread::sleep(10)` 误放在 `PackageManager::initialize()` 的主线程路径中，阻塞了 `MainWindow` 创建和事件循环。改为在后台 `GetInstalledPackagesTask::runTask()` 中模拟慢速扫描，确保主窗口正常显示。
- `ProgressDialog` 新增取消能力：标题栏关闭按钮在 `cancellable && !canHide` 时可用，点击后通过 `closeEvent` → `onCanceled()` → `canceled()` 信号通知调用方。`AppController` 新增 `cancelPendingOpen()`，清空 pending 路径和 dialog，确保取消后不会在 Package Ready 时误打开工程。`TaskDialog::onCanceled()` 调用基类 `ProgressDialog::onCanceled()` 保持信号链路完整。
- 已运行 debug configure/build：`cmake --preset debug` 和 `cmake --build --preset debug` 通过。

阶段 2 已完成。

### 阶段 3：UI 表达 Package Loading

目标是默认空工程启动期间不阻塞主窗口，但歌手选择器不要呈现误导性的空列表。采用最简方案：Package 未 Ready 时禁用组合框，按钮文本显示 “(Scanning...)”，不修改下拉菜单内容。

#### 具体改动

**TwoLevelComboBox** 新增 `setLoadingText(const QString &text)`：
- 非空字符串时覆盖按钮显示文本（`currentText()` 优先返回 loadingText）
- `setItems()` 末尾清除 loadingText
- 不需要菜单 disabled action、不需要 pending 缓存逻辑

**TrackControlView** 构造时：
- Package 未 Ready：`cbSinger->setEnabled(false)` + `cbSinger->setLoadingText(tr(“(Scanning...)”))`
- Package 已 Ready：直接 `setItems(...)` 走现有路径
- 新增 `moduleStatusChanged(ModuleType::Package, Ready)` 监听：`setEnabled(true)`、清除 loadingText、`setItems(...)` + `setCurrentData(...)`

**ClipEditorToolBarView** 同理：
- 构造时同 TrackControlView 逻辑
- Ready 后恢复菜单并设置正确的选中项（考虑 Follow Track / independent singer 状态）

#### 不改动范围

- 不修改菜单结构
- 不引入 pending packages 缓存
- 不在 TwoLevelComboBox 中引入 AppStatus 依赖
- Package Error 的处理保留到阶段 4

#### 当前进度（2026-06-06）

已完成的功能改动：
- `TwoLevelComboBox` 新增 `setLoadingText()` 和 `m_loadingText` 成员；`currentText()` 优先返回 loadingText；`setItems()` 末尾自动清除 loadingText。
- `TwoLevelComboBox::paintEvent` 中 `drawText` 改为从 `opt.palette` 取 `QPalette::ButtonText` 颜色，使 QSS `color` 属性能影响自绘文本。
- `TwoLevelComboBox::paintEvent` 中箭头改为使用 `IconUtils::createTintedSvgIcon()` 绘制 `chevron_down_16_regular.svg`，并用 `QRectF` 计算箭头区域，避免高 DPI 下整数坐标带来的轻微偏移。
- `TrackControlView` 构造时检查 `packageModuleStatus`：未 Ready 则 `setEnabled(false)` + `setLoadingText(tr("(Scanning...)"))`；新增 `moduleStatusChanged` 监听，Ready 后恢复。
- `ClipEditorToolBarView` 同理，保留 Follow Track 语义。
- `track-editor.qss` 新增 `TrackControlView>TwoLevelComboBox:disabled` 样式：透明背景、无边框、半透明前景色。
- `clip-editor.qss` 新增 `TwoLevelComboBox#cbClipSinger:disabled` 样式：同上。
- debug build 通过；UI 样式已验收通过。

已修复的样式问题：
- `TwoLevelComboBox#cbClipSinger:disabled` 的 QSS `color` 未生效，根因不是选择器匹配失败，而是 `ClipEditorToolBarViewPrivate::setPianoRollToolsEnabled(true)` 在切到 SingingClip 状态时无条件重新启用了 `m_cbSinger`，导致控件不再处于 `:disabled` 状态。现已改为只有 Package Ready 时才启用 clip singer selector。
- 两处的下拉箭头颜色原本由 Qt style 绘制，不跟随 `QPalette::ButtonText`。现已改为 `TwoLevelComboBox::paintEvent()` 使用当前 palette 通过 `IconUtils` 自绘 SVG 箭头，使 QSS `color` 能同时影响文字和箭头。
- 箭头区域从整数 `QRect` 计算调整为 `QRectF` 计算，避免高 DPI 下视觉偏移；调试红框已移除。
- 阶段 3 已完成并验收通过。

### 阶段 4：模型 rehydrate 与推理 gate 完整化

目标是包刷新或降级打开后重解析当前工程的 singer/speaker，并让 `InferController` 显式等待 Package Ready、Inference Ready、Language Ready 后再调度推理。

#### 当前进度（2026-06-06）

阶段 4 采用收缩版范围，聚焦完成生命周期闭环，暂不处理运行期间卸载包后的反向失效和 runtime 资源清理。

已完成的代码改动：

- 新增 `ProjectPackageResolver`，监听 `AppModel::modelChanged`、`PackageManager::packagesRefreshed` 和 `AppStatus::moduleStatusChanged(ModuleType::Package, Ready)`，在 Package metadata Ready 后用当前 locator 重新解析工程内 track / singing clip 的 singer 和 speaker。
- `ProjectPackageResolver` 只做 fallback → resolved 恢复：identifier 能解析到完整 `SingerInfo` 时更新模型；解析不到时暂时保持现有信息，不在本阶段处理“包被卸载后降级当前工程”的路径。
- `ProjectPackageResolver` 通过现有 pair-level API 更新模型：`Track::setSingerAndSpeakerInfo(...)`、`SingingClip::setTrackSingerAndSpeakerInfo(...)`、`SingingClip::setOwnSingerAndSpeaker(...)`。
- `AppControllerPrivate::initializeModules()` 中在 `InferController` 前初始化 `ProjectPackageResolver`，让 Package Ready 后的 queued 推理重试尽量发生在模型重解析之后。
- `InferController` 新增统一 readiness gate：只有 `Package Ready + Inference Ready + Language Ready` 且 clip 有有效 singer identifier 时，才启动 pronunciation / phoneme / inference pipeline。
- `InferController` 在 Package / Inference / Language 任一模块进入 Ready 后通过 queued call 重试当前 singing clips。
- `InferEngine::loadInferencesForSinger()` 增加诊断：Inference runtime 未 Ready 时报告 `inference runtime is not ready`，Package 未 Ready 时报告 `package manager is not ready`，避免误报为缺包。
- `DspxProjectConverter` 增加诊断：Package metadata 未 Ready 时解析 singer 会输出 warning，保留同步 converter 和 fallback 行为不变。

已验证：

- `cmake --preset debug` 通过。
- `cmake --build --preset debug` 通过。

尚未验证：

- Phase 4 的手动功能用例尚未执行，包括 fallback → resolved 恢复、readiness 顺序和诊断日志验证。

### 1. 将 PackageManager 接入 AppStatus 模块状态体系，并与 InferEngine 解耦

修改 `src/app/Model/AppStatus/AppStatus.h/.cpp` 和 `src/app/Modules/PackageManager/PackageManager.h/.cpp`。

扩展模块类型：

```cpp
enum class ModuleType { Audio, Language, Inference, Package };
```

新增属性：

```cpp
Property<ModuleStatus> packageModuleStatus = ModuleStatus::Unknown;
```

在 `AppStatus.cpp` 中让它发出统一信号：

```cpp
packageModuleStatus.onChanged(
    [this](auto value) { emit moduleStatusChanged(ModuleType::Package, value); });
```

`PackageManager` 不需要自己再定义一套对外状态枚举；它可以保留内部刷新状态 `RefreshState` 处理并发扫描，但对应用层暴露时使用 `appStatus->packageModuleStatus`。

建议状态语义：

- `Unknown`：尚未开始初始化 metadata backend / 扫描。
- `Loading`：正在初始化 metadata backend 或扫描包。
- `Ready`：扫描成功完成，locator 可用。
- `Error`：metadata backend 初始化失败或包扫描失败。

这样查找语义会变清晰：

- `Package != Ready` 时查找为空：表示包系统还没准备好。
- `Package == Ready` 时查找为空：表示包或歌手确实未安装/无法解析。

同时保留 `packagesRefreshed(...)` 信号，但把它移到释放 `m_resultRwLock` 之后发出；目前它在写锁内部发出，后续如果有同步槽读取包列表会有重入/死锁风险。

#### PackageManager 自建轻量 metadata SynthUnit

当前 `PackageManager::refreshInstalledPackages()` 通过 `inferEngine->synthUnit()` 扫描包，因此被迫等待 `InferEngine::engineInitialized`。新的设计中，`PackageManager` 自己持有一个只用于 metadata scan 的 `srt::SynthUnit`，例如：

```cpp
srt::SynthUnit m_metadataSynthUnit;
```

`PackageManager::initialize()` 负责初始化这个 metadata backend，并启动扫描任务。它只配置包元数据扫描需要的内容：

- 从 `appOptions->general()->packageSearchPaths` 设置 package paths。
- 注册解析 package/singer metadata 必需的 singer provider plugin path。
- 不选择 GPU。
- 不初始化 ONNX driver。
- 不创建 inference driver。
- 不加载模型。

`refreshInstalledPackages()` 从：

```cpp
if (!inferEngine->initialized()) {
    return GetInstalledPackagesError{...};
}

srt::SynthUnit &su = inferEngine->synthUnit();
```

改成使用 `PackageManager` 自己的 metadata `SynthUnit`。扫描时仍使用 `su.open(packagePath, true)`，保持 no-load metadata scan 语义。

`InferEngine` 仍然持有自己的 `SynthUnit`，用于实际 package load、singer load、inference spec/model 创建。两个 `SynthUnit` 不共享 package object，只共享 package identifier/path 等元数据结果。

#### 启动顺序调整

`src/app/main.cpp` 不再连接：

```cpp
InferEngine::engineInitialized -> PackageManager::initialize
```

改为应用启动时直接初始化 `PackageManager`：

```cpp
PackageManager::instance();
packageManager->initialize();
```

`InferEngine` 和 `PackageManager` 可以并行初始化：

```text
PackageManager metadata scan ┐
InferEngine runtime init     ├─ 并行
LanguageEngine init          ┘
```

`.dspx` 打开只等待 `Package Ready`；推理调度才同时等待 `Package Ready + Inference Ready + Language Ready`。

### 2. DSPX 工程打开必须等待包 metadata 就绪，并用 ProgressDialog 呈现等待状态

修改 `src/app/Controller/AppController.h/.cpp`、`src/app/Controller/AppController_p.h`，并新增/复用 `src/app/UI/Dialogs/Base/ProgressDialog.h/.cpp`。`TaskDialog` 继承 `ProgressDialog`，继续只服务 Task 状态适配。

保留现有同步接口：

```cpp
bool openFile(const QString &filePath, QString &errorMessage);
```

新增异步请求接口，例如：

```cpp
void requestOpenFile(const QString &filePath);
```

行为：

- `.mid` / `.midi`：不依赖歌手包，立即走现有打开流程。
- `.dspx` 且 `appStatus->packageModuleStatus == Ready`：立即调用现有 `openFile(...)`。
- `.dspx` 且包正在 `Unknown` / `Loading`：显示不可隐藏或可取消的 `ProgressDialog`，提示正在扫描歌手包/初始化包管理器；包 ready 后关闭 dialog 并继续打开工程。
- 包扫描进入 `Error`：关闭/更新 dialog，提示初始化失败，但允许用户继续打开工程。此时进入降级打开模式，保留 fallback singer identity，而不是阻塞用户。

`.dspx` 工程打开不等待 `InferEngine Ready`。如果 `Package Ready` 早于 `Inference Ready`，工程也可以先完整恢复 singer/speaker；后续由 `InferController` 在推理条件满足后触发推理。

`ProgressDialog` 第一版只展示 indeterminate progress，不绑定具体 `Task`：

- 创建 `ProgressDialog(false, false, parent)` 或根据体验决定是否 cancellable。
- `setTitle(tr("Opening Project"))`
- `setMessage(tr("Scanning singer packages..."))`
- 等待 `AppStatus::moduleStatusChanged(ModuleType::Package, ...)`。
- Ready 后 `forceClose()` 并继续打开。
- Error 后提示用户可继续降级打开。

如果希望等待过程展示真实进度，后续可以把包扫描改成正式 `Task` 并持续更新 `TaskStatus`，再由 `TaskDialog` 复用同一个 `ProgressDialog` UI；第一版用 indeterminate progress 即可。

修改 `src/app/main.cpp`：命令行工程路径不再直接调用 `openFile(...)`，而是用 `QTimer::singleShot(0, ...)` 在事件循环开始后调用 `appController->requestOpenFile(filePath)`。

现有启动后自动选中第一条轨道第一个 clip 并切到 ClipEditor 的行为，需要移动到“延迟打开成功之后”的 helper 中执行，避免丢失当前用户体验。

### 3. 应用启动后允许先创建空工程，但歌手组合框要能表达“扫描中”

用户提出的 Web 应用式体验是合理的：启动速度优先，不因为包扫描阻塞整个主窗口和默认工程创建；但用户打开歌手选择器时，不能呈现一个误导性的空列表。

修改 `src/app/UI/Controls/TwoLevelComboBox.h/.cpp` 以及使用它的：

- `src/app/UI/Views/TrackEditor/TrackControlView.cpp`
- `src/app/UI/Views/ClipEditor/ToolBar/ClipEditorToolBarView.cpp`

建议给 `TwoLevelComboBox` 增加 loading/placeholder 能力，例如：

```cpp
void setLoading(bool loading);
void setPlaceholderText(const QString &text);
```

当 `appStatus->packageModuleStatus == Loading` 或 `Unknown` 时：

- 下拉菜单保留当前显示值。
- 菜单中显示一个 disabled action：`tr("(Scanning packages...)")` / `tr("(正在扫描...)")`。
- 禁止用户选择这个 action，不发 `currentDataChanged()`。
- 如果是 Clip editor 且当前是 Follow Track，仍保留按钮文本 `Follow Track (...)`；loading 只影响可选列表，不覆盖当前 effective singer/speaker 显示。

当 `packagesRefreshed` 或 `Package Ready` 到来后：

- `setItems(packageManager->installedPackages().successfulPackages)` 重建真实菜单。
- 调用现有 `setCurrentData(...)` 恢复当前 track/clip 状态。

这样可以兼顾启动速度和用户体验：默认工程可以立即出现，但歌手列表会明确告诉用户“还在扫描”，而不是让用户误以为没有任何歌手包。

### 4. DspxProjectConverter 保持同步，但 fallback 只作为降级路径

修改 `src/app/Modules/ProjectConverters/DspxProjectConverter.cpp`，主要用于诊断和语义收敛，不建议把 converter 本身改成异步。

正常情况下由 AppController 保证 `.dspx` 在包就绪后再加载，所以 converter 仍保持当前同步接口：

```cpp
bool load(const QString &path, AppModel *model, QString &errMsg, ImportMode mode);
```

保留 fallback `SingerInfo` 构造逻辑，用于：

- 用户在包扫描失败后选择继续打开
- 工程引用的包确实未安装
- 第三方 opendspx 文件信息不完整
- 剪贴板/导入数据降级恢复

但如果 converter 发现 `PackageManager` 尚未 `Ready` 就在解析 dspx singer，应记录日志或断言式警告，把它视为生命周期调用错误，而不是和“未安装歌手”混在一起。

### 5. 包刷新后增加模型级 singer/speaker 重解析

新增一个小型 resolver/controller，推荐命名为 `ProjectPackageResolver`；如果想做最小改动，也可以先放在现有 controller 中，但不建议继续塞进 `ValidationController`。

职责：

- 监听 `AppModel::modelChanged`
- 监听 `AppStatus::moduleStatusChanged(ModuleType::Package, Ready)` 或 `PackageManager::packagesRefreshed`
- 当包已就绪时，遍历所有 Track 和 SingingClip
- 根据已有 `SingerIdentifier` 调用 `PackageManager::findSingerByIdentifier()` 重新解析完整 `SingerInfo`
- 根据 speaker id 在 resolved singer 的 `speakers()` 中重新解析完整 `SpeakerInfo`
- 只通过现有成对 API 更新模型：
  - `Track::setSingerAndSpeakerInfo(...)`
  - `SingingClip::setTrackSingerAndSpeakerInfo(...)`
  - `SingingClip::setOwnSingerAndSpeaker(...)`
  - `SingingClip::useTrackSingerAndSpeaker()`

重要约束：不要重新拆出 singer-only / speaker-only setter 或细分信号。Singer 和 speaker 继续作为强关联 pair 处理，仍使用统一的 `singerOrSpeakerChanged`。

运行时机：

1. `AppModel::modelChanged` 后，如果包已经 ready，立即重解析。
2. 每次包刷新成功后，对当前工程重解析。
3. 后续如果支持修改包搜索路径，刷新完成后也走同一套重解析。

这一步能修复两类问题：

- 启动竞态或降级打开导致已加载模型里保存了 fallback singer。
- 运行中刷新/安装包后，当前工程里原本无法解析的 singer 可以恢复。

### 6. 推理调度必须同时等待语言模块、包 metadata 和推理 runtime 就绪

修改 `src/app/Modules/Inference/InferController.cpp` 和 `src/app/Modules/Inference/InferController_p.h`。

现有推理控制器的 pending 思路是对的：语言模块没 ready 时先不跑，ready 后再跑。这里要把 package ready 和 inference ready 都纳入同一个 gate。

增加统一判断，例如：

```cpp
bool canStartClipInference(const SingingClip &clip) {
    return appStatus->languageModuleStatus == AppStatus::ModuleStatus::Ready
        && appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready
        && appStatus->inferEngineEnvStatus == AppStatus::ModuleStatus::Ready
        && !clip.singerInfo().isEmpty()
        && !clip.singerIdentifier().isEmpty();
}
```

在这些入口统一使用：

- `handleSingingClipInserted()`
- `singerOrSpeakerChanged` 回调
- `handleNoteChanged()`
- `handleLanguageModuleStatusChanged()`
- `handleInferenceModuleStatusChanged()`
- 全量重建/重试推理任务的路径

同时让 `InferController` 监听：

- `moduleStatusChanged(ModuleType::Package, Ready)`
- `moduleStatusChanged(ModuleType::Inference, Ready)`
- `moduleStatusChanged(ModuleType::Language, Ready)`

包就绪后，等待模型重解析完成，再重试所有符合条件的 singing clips。必要时使用 queued call，例如 `QTimer::singleShot(0, ...)`，保证 resolver 的槽先执行完。

修改 `src/app/Modules/Inference/InferEngine.cpp` 中 `InferEngine::loadInferencesForSinger()` 的诊断：如果包管理器尚未 ready，不要报 “package not found”，而应报 “package manager not ready”。这能避免把时序问题误判成缺包问题，也能保护未来的直接调用者。

`InferEngine::loadInferencesForSinger()` 仍然可以依赖 `PackageManager::findPackageByIdentifier()` 查 package path；改变的是 `InferEngine` 不再给 `PackageManager` 提供扫描用 `SynthUnit`。

### 7. 本次暂不规划缺包/缺音频/非法 dspx 的完整降级体验

缺包只是“工程资源缺失/非法”的一种。后续还需要统一处理：

- singer package 不存在
- speaker id 不存在
- 音频文件找不到
- dspx 存在非法数据
- 参数/音符数据越界或不可恢复

这些属于工程资源诊断和降级打开策略，应该单独设计，不混入本次“包扫描异步加载”修复。

本次只要求：

- 包扫描未完成时等待。
- 包扫描失败时允许用户继续降级打开。
- 已降级加载的 singer 信息尽可能保留 identifier/name，后续包 ready 后可 rehydrate。

## 关键文件

- `src/app/Model/AppStatus/AppStatus.h/.cpp`：新增 Package module 状态。
- `src/app/main.cpp`：PackageManager 启动即初始化；命令行工程路径改为事件循环后的 deferred open。
- `src/app/Controller/AppController.h/.cpp`：新增异步打开请求接口、等待包扫描、ProgressDialog 呈现。
- `src/app/Controller/AppController_p.h`：保存 pending open 路径/状态，增加打开成功后的 helper。
- `src/app/UI/Dialogs/Base/ProgressDialog.h/.cpp`：提供不依赖 Task 的通用进度对话框。
- `src/app/UI/Dialogs/Base/TaskDialog.h/.cpp`：继承 ProgressDialog，仅适配 Task 状态和取消行为。
- `src/app/UI/Controls/TwoLevelComboBox.h/.cpp`：增加 scanning placeholder/disabled action 能力。
- `src/app/UI/Views/TrackEditor/TrackControlView.cpp`：包扫描中显示歌手列表 loading 状态，ready 后恢复 selection。
- `src/app/UI/Views/ClipEditor/ToolBar/ClipEditorToolBarView.cpp`：同上，但保留 Follow Track effective text。
- `src/app/Modules/PackageManager/PackageManager.h/.cpp`：自建 metadata `SynthUnit`，写入 AppStatus package 状态，调整刷新信号发出位置。
- `src/app/Modules/Inference/InferEngine.cpp`：不再作为 PackageManager 扫描前置；区分“包管理器未就绪”和“包确实不存在”。
- `src/app/Modules/ProjectConverters/DspxProjectConverter.cpp`：保持同步加载，补充 readiness 诊断，保留 fallback 作为降级路径。
- `src/app/Modules/Inference/InferController.cpp/.h`：推理调度增加 package-ready、inference-ready gate 和 ready 后重试。
- 新增或复用一个 resolver controller：包刷新后用现有 pair-level API 重解析模型 singer/speaker。
- 可选新增共享 helper：统一 PackageManager metadata `SynthUnit` 和 InferEngine runtime `SynthUnit` 的 package path / plugin path 配置来源。

## 最小可交付范围

第一版建议至少包含：

1. `AppStatus` 增加 Package module 状态。
2. `PackageManager` 自建 metadata `SynthUnit`，不再等待 `InferEngine::engineInitialized`。
3. `PackageManager` 启动即异步扫描包，并正确维护 Package 状态。
4. `.dspx` 打开等待 Package Ready，并用 `ProgressDialog` 显示扫描等待。
5. `main.cpp` 启动参数打开改成 deferred request。
6. 歌手组合框在包扫描中显示 disabled 的“正在扫描...”项。
7. `InferController` 推理调度等待 Package Ready、Inference Ready、Language Ready。
8. 包 ready 后重试符合条件的 clips。
9. 包刷新后对当前模型做 singer/speaker rehydrate。

这是一条完整生命周期：包 metadata 扫描 → 工程加载等待 → UI loading 状态 → 模型重解析 → 推理 runtime/language ready 后调度推理。

## 后续理想增强

可以后续再做：

- 用 `LookupResult<T>` 区分 `NotReady / Found / Missing`，减少调用方手动判断状态。
- 缺包/缺音频/非法 dspx 的统一资源诊断 UI。
- 缺包 UI 显示 packageId/packageVersion/singerId，而不是笼统 `No singer`。
- 包搜索路径变更后自动刷新并重解析当前工程。
- 打开工程时显示更详细的多阶段进度：扫描包、加载工程、校验资源、初始化推理引擎。
- 后续将菜单、最近文件、拖放等所有 UI 打开入口统一迁移到 `requestOpenFile(...)`，让运行中打开 `.dspx` 也复用 Package Ready 等待逻辑。
- 将 `ProgressDialog` 中仍暴露的 `TaskGlobal::Status` 替换为更通用的进度状态类型，进一步减少 Task 语义泄漏。
- 视需要清理 `requestOpenFile(...)` / `openFile(...)` 的重复文件检查和后缀判断。
- 视需要优化打开成功后激活首个 clip 的容器访问，避免不必要的列表拷贝。
- 为 fallback → resolved 的模型恢复过程增加自动测试。
- 将 package metadata backend 初始化和 inference runtime 初始化的共享路径配置抽成专门 helper，避免两个 `SynthUnit` 配置漂移。

## 风险与规避

### openFile 语义改变影响调用方

规避：保留现有同步 `openFile(...)`，新增 `requestOpenFile(...)`，逐步把 UI/启动路径迁移过去。

### 默认工程启动期间用户看到空 singer 列表

规避：不阻塞主窗口启动，但组合框菜单显示 disabled 的“正在扫描...”，ready 后自动刷新为真实包列表。

### metadata SynthUnit 和 inference SynthUnit 配置不一致

规避：抽出共享的 package path / singer provider path 配置 helper。PackageManager 和 InferEngine 都使用同一套 package search path 来源。PackageManager 只注册 metadata scan 必需的插件路径，不注册 inference driver/runtime。

### 同一个 package 被两个 SynthUnit 分别 open

规避：明确这是预期设计。PackageManager 使用 `noLoad=true` 只扫描 metadata；InferEngine 在真正推理时再完整 load package 和 singer。两个模块不共享 package object，只共享 identifier/path 等不可变元数据。

### 重解析触发大量 singerOrSpeakerChanged

规避：setter 前先比较旧/新 pair；继续使用现有 pair-level batching，只在 effective pair 真变化时发信号。

### 推理早于模型重解析启动

规避：包 ready 后的推理重试使用 queued call，或保证 resolver 的连接顺序先于 InferController 的重试逻辑。

### 包扫描失败时用户无法打开工程

规避：扫描失败只阻止“完整解析”，不阻止工程打开；给用户提示后允许降级打开。

### packagesRefreshed 信号线程/锁问题

规避：`packagesRefreshed` 不在写锁内发出；UI 和模型 controller 使用 Qt queued/default 连接，避免持锁回调读取包列表。

## 验证流程

1. 原始复现流程：
   - 新建工程。
   - 设置轨道歌手。
   - 绘制音符并确认能推理。
   - 保存工程。
   - 关闭程序。
   - 带工程路径参数启动。
   - 等待包扫描 ProgressDialog 自动结束。
   - 轨道歌手应正确恢复，不再是 `No singer`。
   - Clip toolbar / ClipView 应显示正确 inherited 或 independent singer/speaker。
   - 推理应在 Package、Inference、Language 都 ready 后正常触发。

2. 启动竞态验证：
   - 临时放慢包扫描或加日志。
   - 带 dspx 参数启动。
   - 确认工程打开等待 Package Ready，converter 解析时 locator 已填充。
   - 当前结果（2026-06-06）：阶段 2 已修复主窗口阻塞和 ProgressDialog 取消/清理问题；仍建议按本流程做回归验证。

3. Package / Inference 解耦验证：
   - 临时放慢 `InferEngine` 初始化或模拟 GPU/ONNX 初始化耗时。
   - 确认 PackageManager 不等待 `InferEngine::engineInitialized`，仍可先完成包扫描。
   - 确认 `.dspx` 可以在 Package Ready 后打开，即使 Inference 尚未 Ready。
   - 确认推理不会早于 Inference Ready 启动。

4. 默认工程启动体验：
   - 正常启动 app。
   - 在包扫描未完成时点开 track/clip 歌手组合框。
   - 菜单应显示“正在扫描...”而不是误导性的空包列表。
   - 扫描完成后下拉项自动刷新。

5. 运行中打开：
   - 正常启动 app。
   - 包 ready 后打开同一个 dspx。
   - 行为应保持和当前正常路径一致。

6. 包扫描失败：
   - 模拟 Package module Error。
   - 打开 dspx 时应提示用户初始化失败，但允许继续降级打开。

7. readiness 顺序：
   - 测 package 先 ready、language 后 ready、inference 最后 ready。
   - 测 language 先 ready、inference 后 ready、package 最后 ready。
   - 测 inference 先 ready、package 后 ready、language 最后 ready。
   - 只有三者都 ready 后才开始推理。

8. 包刷新：
   - 工程打开状态下刷新包。
   - 确认模型重解析执行。
   - 确认受影响 clips 可以重试推理。

9. Phase 4 快速验证：
   - Rehydrate：模拟 Package Error 后降级打开带 singer 的 dspx，再恢复 Package Ready / 触发包刷新；确认日志出现 `Resolved project singer/speaker pairs from package metadata`，track / clip 的 fallback singer/speaker 恢复为完整信息，并能继续推理。
   - 推理 gate：分别放慢 Package、Inference、Language 任一模块；确认三者未全部 Ready 前不启动推理 pipeline，不出现过早的 `loadInferencesForSinger` 失败；三者全部 Ready 后自动 retry singing clips。
   - 诊断：临时绕过 `requestOpenFile(...)` 或人为制造 Package / Inference 未 Ready 的直接调用；确认 converter 输出 `DspxProjectConverter is resolving singer before package metadata is ready`，InferEngine 分别输出 `inference runtime is not ready` 或 `package manager is not ready`。

10. 构建：
    - 按项目要求使用 CMake build skill/workflow 构建 debug preset。
