# 异步工程加载 — 设计文档

> 状态：✅ 实施完成（阶段 1–4 全部完成，2026-08-06 复核；阶段 4 手动功能用例尚未执行）
> 本文由原 `plans/` 实施计划整理而来；分阶段实施记录与提交清单已移除。

## 背景与根因

**现象**：关闭程序后带工程路径参数启动，轨道歌手变成 `No singer`，剪辑上可能还残留歌手显示，但无法触发推理。同一个工程在程序不关闭的情况下重新打开则正常。

**根因**不是 dspx 某个字段单点解析错误，而是启动异步时序存在缺口：`main.cpp` 在显示 `MainWindow` 后立刻打开命令行参数中的工程；此时 `PackageManager` 仍在后台扫描已安装歌手包。`DspxProjectConverter` 加载 dspx 时会同步调用 `PackageManager::findSingerByIdentifier()` 解析保存的 singer identifier——如果包扫描还没完成，locator 为空，converter 就会写入降级版 `SingerInfo`（有 identifier/name/defaultLanguage，缺完整 speakers/languages/default dict）。后续 `packagesRefreshed` 只刷新 UI 下拉项，不会修复已加载到模型里的 singer，也不会重试失败的推理。

**结构性根因**：`PackageManager` 之所以要等 `InferEngine`，不是因为它扫描元数据需要模型或推理 runtime，而是因为它复用了 `InferEngine` 里初始化好的 `srt::SynthUnit`。这把两个本应独立的能力绑在了一起：包元数据扫描被 GPU 选择、ONNX driver 初始化、推理 runtime 初始化间接阻塞。

**设计目标**：重新设计异步加载链路，而不是只给 argv 启动路径加延迟补丁——工程解码、模型 singer/speaker 状态、推理调度和 UI 选择器都必须显式理解"包 metadata 是否已就绪"；Package metadata scanning 和 Inference runtime initialization 解耦。

## 能力级依赖

`AppStatus` 已有的 `ModuleType` / `ModuleStatus` / `moduleStatusChanged` 是合理方向，但状态粒度和模块边界不够准确。核心区分：

| 状态 | 含义 | 依赖方 |
|---|---|---|
| **Package Ready** | 包 metadata cache / locator 可用 | `.dspx` 工程打开、singer/speaker 恢复 |
| **Inference Ready** | 推理 runtime / driver / GPU 可用 | 推理调度 |

二者可以并行初始化。`.dspx` 工程打开**只**依赖 Package metadata；推理调度依赖 Package metadata + Inference runtime + Language module。不要把 `PackageManager` 建模成 `Inference Ready` 的后置阶段。

## 模块状态体系（AppStatus 扩展）

- `enum class ModuleType { Audio, Language, Inference, Package }`，新增 `Property<ModuleStatus> packageModuleStatus = ModuleStatus::Unknown`，变化时发统一信号 `moduleStatusChanged(ModuleType::Package, value)`
- 状态语义：`Unknown`（尚未开始初始化/扫描）、`Loading`（正在初始化 metadata backend 或扫描）、`Ready`（扫描成功，locator 可用）、`Error`（backend 初始化失败或扫描失败）
- 查找语义因此清晰：`Package != Ready` 时查找为空 = 包系统未就绪；`Package == Ready` 时查找为空 = 包或歌手确实未安装
- `PackageManager` 保留内部 `RefreshState` 处理并发扫描，但对外暴露使用 `appStatus->packageModuleStatus`，不另设枚举
- `packagesRefreshed(...)` 信号**移到释放 `m_resultRwLock` 之后发出**（原在写锁内部，同步槽读取包列表有重入/死锁风险）

## PackageManager 与 InferEngine 解耦

`PackageManager` 自建一个**只用于 metadata scan 的轻量 `srt::SynthUnit`**（`m_metadataSynthUnit`），`initialize()` 只配置包元数据扫描需要的内容：

- 从 `appOptions->general()->packageSearchPaths` 设置 package paths
- 注册解析 package/singer metadata 必需的 singer provider plugin path
- **不**选择 GPU、**不**初始化 ONNX driver、**不**创建 inference driver、**不**加载模型

`refreshInstalledPackages()` 不再依赖 `inferEngine->initialized()` / `inferEngine->synthUnit()`，改用自有 SynthUnit，扫描时仍用 `su.open(packagePath, true)` 保持 no-load metadata scan 语义。

`InferEngine` 仍然持有自己的 `SynthUnit` 用于实际 package load、singer load、inference spec/model 创建。两个 `SynthUnit` **不共享 package object**，只共享 package identifier/path 等不可变元数据（这是预期设计，不是缺陷）。

**启动顺序**：`main.cpp` 不再连接 `InferEngine::engineInitialized -> PackageManager::initialize`，改为启动时直接 `PackageManager::instance()->initialize()`：

```text
PackageManager metadata scan ┐
InferEngine runtime init     ├─ 并行
LanguageEngine init          ┘
```

## 工程打开等待（requestOpenFile + ProgressDialog）

保留现有同步接口 `bool openFile(const QString &filePath, QString &errorMessage)`，新增异步请求接口 `void requestOpenFile(const QString &filePath)`：

- `.mid` / `.midi`：不依赖歌手包，立即走现有打开流程
- `.dspx` 且 `packageModuleStatus == Ready`：立即调用现有 `openFile(...)`
- `.dspx` 且包在 `Unknown` / `Loading`：显示 `ProgressDialog` 等待（indeterminate），包 ready 后关闭 dialog 并继续打开工程
- 包扫描进入 `Error`：提示初始化失败，但**允许用户继续降级打开**（保留 fallback singer identity，不阻塞用户）

`.dspx` 打开**不等待** `InferEngine Ready`。`main.cpp` 命令行工程路径改为 `QTimer::singleShot(0, ...)` 在事件循环开始后调用 `requestOpenFile`；"打开成功后自动选中第一条轨道第一个 clip 并切到 ClipEditor"的行为移到延迟打开成功之后的 helper 中执行。

**ProgressDialog** 从 `TaskDialog` 中抽出：只负责标题、消息、进度条、取消按钮、隐藏/关闭控制等 UI 能力，不依赖 `Task` / `TaskManager`；`TaskDialog` 改为继承 `ProgressDialog`，只保留 Task 状态适配与取消终止任务逻辑。等待连接建立后会立即复查当前 Package 状态（避免错过 Ready/Error）；pending dialog 在完成、替换请求、控制器析构时集中关闭并删除，避免泄漏；取消（`cancelPendingOpen()`）清空 pending 路径和 dialog，确保取消后不会在 Package Ready 时误打开工程。

## UI 表达 Package Loading

目标是默认空工程启动期间不阻塞主窗口，但歌手选择器不呈现误导性的空列表。最简方案：Package 未 Ready 时**禁用组合框**，按钮文本显示 "(Scanning...)"，不修改下拉菜单内容。

- `TwoLevelComboBox` 新增 `setLoadingText(const QString &text)`：非空字符串时覆盖按钮显示文本（`currentText()` 优先返回 loadingText）；`setItems()` 末尾清除 loadingText；不需要菜单 disabled action、不需要 pending 缓存逻辑；不引入 AppStatus 依赖
- `TrackControlView` / `ClipEditorToolBarView` 构造时检查 `packageModuleStatus`：未 Ready 则 `setEnabled(false)` + `setLoadingText(...)`；监听 `moduleStatusChanged(ModuleType::Package, Ready)` 后 `setEnabled(true)`、清除 loadingText、`setItems(...)` + `setCurrentData(...)`（ClipEditor 保留 Follow Track / independent singer 语义）
- QSS：`TrackControlView>TwoLevelComboBox:disabled` 与 `TwoLevelComboBox#cbClipSinger:disabled` 样式（透明背景、无边框、半透明前景色）；下拉箭头改为 `paintEvent()` 内用当前 palette 经 `IconUtils` 自绘 SVG，使 QSS `color` 能同时影响文字和箭头（原生 style 绘制不跟随 `QPalette::ButtonText`）
- 陷阱记录：`ClipEditorToolBarViewPrivate::setPianoRollToolsEnabled(true)` 在切到 SingingClip 状态时无条件重新启用 `m_cbSinger`，曾导致 disabled 样式失效——现改为只有 Package Ready 时才启用 clip singer selector

## 模型重解析（ProjectPackageResolver）

新增小型 resolver（`src/app/Controller/ProjectPackageResolver.h/.cpp`），职责：

- 监听 `AppModel::modelChanged`、`PackageManager::packagesRefreshed`、`AppStatus::moduleStatusChanged(ModuleType::Package, Ready)`
- 包就绪后用当前 locator 遍历所有 Track 和 SingingClip，根据已有 `SingerIdentifier` 重新解析完整 `SingerInfo`，根据 speaker id 在 resolved singer 的 `speakers()` 中重新解析完整 `SpeakerInfo`

**重要约束**：只通过现有成对 API 更新模型——`Track::setSingerAndSpeakerInfo(...)`、`SingingClip::setTrackSingerAndSpeakerInfo(...)`、`SingingClip::setOwnSingerAndSpeaker(...)`、`SingingClip::useTrackSingerAndSpeaker()`。不重新拆出 singer-only / speaker-only setter 或细分信号；singer 和 speaker 继续作为强关联 pair 处理，仍使用统一的 `singerOrSpeakerChanged`。

运行时机：① `AppModel::modelChanged` 后若包已 ready 立即重解析；② 每次包刷新成功后对当前工程重解析；③ 未来支持修改包搜索路径后，刷新完成也走同一套重解析。

这一步修复两类问题：启动竞态或降级打开导致已加载模型里保存了 fallback singer；运行中刷新/安装包后当前工程里原本无法解析的 singer 恢复。

## 推理 readiness gate

`InferController` 新增统一判断：

```cpp
bool canStartClipInference(const SingingClip &clip) {
    return appStatus->languageModuleStatus == AppStatus::ModuleStatus::Ready
        && appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready
        && appStatus->inferEngineEnvStatus == AppStatus::ModuleStatus::Ready
        && !clip.singerInfo().isEmpty()
        && !clip.singerIdentifier().isEmpty();
}
```

统一用于这些入口：`handleSingingClipInserted()`、`singerOrSpeakerChanged` 回调、`handleNoteChanged()`、`handleLanguageModuleStatusChanged()`、`handleInferenceModuleStatusChanged()`、全量重建/重试推理任务的路径。同时监听三个模块的 Ready 信号；包就绪后**使用 queued call**（`QTimer::singleShot(0, ...)`）重试所有符合条件的 singing clips，保证 resolver 的槽先执行完。

`InferEngine::loadInferencesForSinger()` 的诊断区分两种情况：包管理器尚未 ready 时报 "package manager not ready"，而不是 "package not found"——避免把时序问题误判成缺包问题。

## DspxProjectConverter 保持同步

converter 仍保持同步接口（正常情况下由 AppController 保证 `.dspx` 在包就绪后再加载）：

```cpp
bool load(const QString &path, AppModel *model, QString &errMsg, ImportMode mode);
```

fallback `SingerInfo` 构造逻辑保留，仅作为**降级路径**：包扫描失败后用户选择继续打开、工程引用的包确实未安装、第三方 opendspx 文件信息不完整、剪贴板/导入数据降级恢复。但如果 converter 发现 `PackageManager` 尚未 `Ready` 就在解析 dspx singer，应记录日志/断言式警告（`DspxProjectConverter is resolving singer before package metadata is ready`），把它视为**生命周期调用错误**，而不是和"未安装歌手"混在一起。

## 遗留事项

- 阶段 4 手动功能用例尚未执行：fallback → resolved 恢复、readiness 顺序（package/language/inference 三种先后次序）、诊断日志验证
- 本次未规划运行期间卸载包后的反向失效和 runtime 资源清理（收缩版范围，聚焦生命周期闭环）

## 后续增强方向

- `LookupResult<T>` 区分 `NotReady / Found / Missing`，减少调用方手动判断状态
- 缺包/缺音频/非法 dspx 的统一资源诊断 UI（与 `audio-asset-portability-plan` 的资源检查衔接）
- 缺包 UI 显示 `packageId/packageVersion/singerId`，而不是笼统 `No singer`
- 包搜索路径变更后自动刷新并重解析当前工程
- 打开工程时显示更详细的多阶段进度：扫描包、加载工程、校验资源、初始化推理引擎
- 将菜单、最近文件、拖放等所有 UI 打开入口统一迁移到 `requestOpenFile(...)`，让运行中打开 `.dspx` 也复用 Package Ready 等待逻辑
- `ProgressDialog` 中仍暴露的 `TaskGlobal::Status` 替换为更通用的进度状态类型，减少 Task 语义泄漏
- 清理 `requestOpenFile(...)` / `openFile(...)` 的重复文件检查和后缀判断；优化打开成功后激活首个 clip 的容器访问
- 为 fallback → resolved 的模型恢复过程增加自动测试
- 抽共享 helper 统一两个 `SynthUnit` 的 package path / plugin path 配置来源，避免配置漂移

## 风险与规避

| 风险 | 规避 |
|---|---|
| openFile 语义改变影响调用方 | 保留同步 `openFile(...)`，新增 `requestOpenFile(...)`，逐步迁移 |
| 默认工程启动期间用户看到空 singer 列表 | 不阻塞主窗口，组合框显示 disabled 的"(Scanning...)"，ready 后自动刷新 |
| metadata / inference 两个 SynthUnit 配置不一致 | 抽共享配置 helper；同一 package search path 来源 |
| 重解析触发大量 singerOrSpeakerChanged | setter 前比较旧/新 pair；继续使用 pair-level batching，只在真变化时发信号 |
| 推理早于模型重解析启动 | 包 ready 后的重试用 queued call，保证 resolver 连接顺序先于重试逻辑 |
| 包扫描失败时用户无法打开工程 | 扫描失败只阻止"完整解析"，不阻止打开；提示后允许降级打开 |
| packagesRefreshed 信号线程/锁问题 | 不在写锁内发出；UI 和模型 controller 用 queued/default 连接 |

## 关键文件（当前路径）

| 模块 | 文件 |
|---|---|
| 状态 | `src/app/Model/AppStatus/AppStatus.h/.cpp` |
| 启动 | `src/app/main.cpp` |
| 控制器 | `src/app/Controller/AppController.h/.cpp`、`AppController_p.h`、`src/app/Controller/ProjectPackageResolver.h/.cpp` |
| 对话框 | `src/app/UI/Dialogs/Base/ProgressDialog.h/.cpp`、`TaskDialog.h/.cpp` |
| UI | `src/app/UI/Controls/TwoLevelComboBox.h/.cpp`、`src/app/UI/Views/TrackEditor/TrackControlView.cpp`、`src/app/UI/Views/ClipEditor/ToolBar/ClipEditorToolBarView.cpp` |
| 包管理 | `src/app/Modules/PackageManager/PackageManager.h/.cpp` |
| 推理 | `src/app/Modules/Inference/InferController.cpp/.h`、`InferEngine.cpp` |
| 转换 | `src/app/Modules/ProjectConverters/DspxProjectConverter.cpp` |
