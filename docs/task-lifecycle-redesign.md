# 任务生命周期与异步写回重构计划（修订版）

## 背景

近期的剪辑移动、歌手切换和快速撤销问题暴露出一类共同风险：后台推理任务可能在文档模型已经变化后完成，并继续把旧结果写回当前模型。表现形式包括：

- 旧发音/音素任务写回已经变化的音符。
- 旧 `InferPiece` 或旧 `InferPipeline` 在重新分段后继续写回。
- `UpdateDurationState` 使用过期的 `piece.notes` 更新音素偏移，触发视图收到不存在音符的变更通知。
- 快速 `Ctrl+Z` / `Ctrl+Y`、Esc 丢弃交互、跨轨移动、切换歌手等操作与推理完成交错时，异常点不固定。

这说明问题不只是任务取消时序，而是缺少统一的异步写回规则。后续重构目标是让旧任务可以自然完成，但过期结果永远不能修改当前工程。

## 本次修订重点

阅读当前实现后，本计划需要做以下顺序调整：

- `Snapshot Input` 不能放到靠后阶段。当前 `GetPronunciationTask`、`GetPhonemeNameTask::distributePhonemes()` 以及 pipeline 的 `buildTaskInput()` 都存在后台线程读取 live `Note *` / `InferPiece *` / `appModel` 的情况。它们不仅可能写回旧结果，也可能在运行中读到已删除对象。
- `Revision Gate` 不能只放在任务完成回调。`BaseInferState::handleTaskFinished()` 把结果写入 `InferPipeline` 后，真正修改模型的是 `UpdateDurationState`、`UpdatePitchState`、`UpdateVarianceState`、`UpdateAcousticState`。这些 update state 也必须二次校验。
- 需要一个统一的 apply/drop helper，而不是在每个任务回调里各写一套判断。后续所有异步写回都应通过同一个入口输出 `Apply / Drop / Defer` 结果和 reason。
- `TaskQueue` 所有权收口会影响取消和 late finished 的 race，不只是收尾重构；但它不直接解决旧结果写回，应在 revision/snapshot 之后单独验证。
- 视图 warning 防御仍然需要，但只能作为止血。若 warning 频繁出现，说明 revision/snapshot gate 仍有漏点。

## 总体目标

- 后台任务可以继续异步执行，不阻塞用户操作。
- 用户编辑、撤销、重做、Esc 丢弃时，旧任务结果不能写回过期模型。
- 推理任务不长期依赖 live `Note *` / `InferPiece *` 作为语义依据；worker thread 只读 snapshot。
- `TaskQueue` 拥有任务生命周期，其他模块只观察任务状态或消费有效结果。
- 为未来其他异步写入提供统一的 apply / drop / defer 判断入口，并统一日志 reason。

## 核心原则

### Revision Gate

使用类似乐观锁的 revision 机制。任务创建时记录当前 revision，完成写回前再次校验：

- clip 是否仍存在。
- clip 是否仍是 `SingingClip`。
- 当前 revision 是否等于任务创建时的 revision。
- piece / note 是否仍属于当前 clip。

校验通过才允许写回；校验失败则丢弃结果。

推理结果不需要像数据库事务那样强制重试。大多数情况下，模型变化本身会触发新一轮推理，旧结果可以直接丢弃。

### Edit Session Gate

现有 Esc / commit / discard 机制主要在视图层维护交互原子性。后续需要把它提升为项目级编辑会话：

- 用户开始拖动 note、clip、param 时创建 edit session。
- 用户 commit 或 discard 时结束 edit session。
- 异步结果完成时，如果与当前 edit session 冲突，则 drop 或 defer。

第一版可以采用保守策略：有冲突编辑时直接 drop，等待后续重新推理，不急着实现复杂挂起队列。

### Snapshot Input

后台线程不应该长期读取 live model 指针。任务创建时应把必要输入快照化，例如：

```cpp
struct NoteSnapshot {
    int noteId;
    QString lyric;
    QString language;
    QString pronunciation;
    int start;
    int length;
    int key;
};

struct InferenceContext {
    int clipId;
    int pieceId;
    quint64 clipRevision;
    SingerInfo singer;
    SpeakerInfo speaker;
};
```

worker thread 使用 snapshot；主线程写回时再按 id 查找当前对象，并经过 revision gate。

本修订版要求 snapshot 与前两阶段并行实施：

- 发音/音素任务创建时立即生成 note snapshot、singer snapshot、tempo snapshot。
- pipeline 的 `buildTaskInput()` 不应在后台线程里读取 `m_pipeline.piece()` 或 `piece.notes`。可以先在主线程生成 `Infer*Input`，后台线程只负责创建 task / 运行 task。
- snapshot 中必须包含用于写回校验的上下文：`clipId`、`pieceId`、`clipRevision`、必要时的 `pieceRevision`、note id 列表、singer/speaker。

## 阶段 1：Clip 级 Revision + 发音/音素 Snapshot

### 交付内容

为 `SingingClip` 增加 clip 级推理 revision：

```cpp
quint64 inferenceRevision() const;
quint64 bumpInferenceRevision();
```

以下操作需要 bump revision：

- note 插入、删除、歌词变化、语言变化、音高变化、时值变化。
- singer / speaker / effective Follow Track 变化。
- `removeAllPieces()`、`reSegment()` 等导致 piece 结构变化的操作。
- tempo 或其他会影响推理输入、切片、结果映射的全局变化。

`GetPronunciationTask`、`GetPhonemeNameTask` 创建时记录 `clipRevision`，并同时改成 snapshot 输入：

- `GetPronunciationTask` 不再保存 `QList<Note *> m_notes` / `notesRef` 作为 worker 输入。
- `GetPhonemeNameTask` 不再在 `distributePhonemes()` 中读取 live `Note *` timing。
- task 内部使用 `NoteSnapshot`、`SingerInfo`、tempo 等不可变输入。
- 完成写回前按 note id 从当前 clip 查找 note，并检查当前 `clipRevision`。

第一阶段可以先让写回仍调用 `Helper::updatePronunciation()` / `Helper::updatePhoneName()`，但调用前必须已经重新组装当前 `QList<Note *>`，且所有 note membership 校验通过。

### 实施记录

状态：已完成并通过 configure/build 验证。（2026-06-07）

本阶段实际落点：

- `SingingClip` 已增加 `inferenceRevision()` / `bumpInferenceRevision()`，并在 note 变更通知、piece 清空/重分段、singer/speaker 变化、默认语言变化、tempo 变化时推进 revision。
- 新增 `NoteInferenceSnapshot`，用于记录 note id、歌词、语言、发音、起点、长度和音高。
- `GetPronunciationTask` 已改为保存 note snapshot、`clipRevision` 和 `SingerInfo`，worker 不再读取 live `Note *` 或通过 `appModel` 查找 clip。
- `GetPhonemeNameTask` 已改为保存 note snapshot、`clipRevision`、`SingerInfo` 和 tempo，`distributePhonemes()` 不再读取 live `Note *` timing。
- `InferControllerPrivate::handleGetPronTaskFinished()` / `handleGetPhoneTaskFinished()` 已在写回前按 note id 重新解析当前 note，并检查 clip revision、result count 和 note membership；失败时直接 drop 并输出 reason。
- 本阶段仍保留 `Helper::updatePronunciation()` / `Helper::updatePhoneName()` 作为最终写回入口，调用前已重新组装当前 `QList<Note *>`。

验证记录：

- `cmake --preset debug`：通过。
- `cmake --build --preset debug`：通过。构建期间仅出现既有 MSVC `/Zi` 被 `/ZI` 覆盖 warning。

### 预计成果

- 旧发音任务不会写回已经变化的 clip。
- 旧音素任务不会写回已经变化的 clip。
- 快速新增、删除、修改歌词后，旧任务完成时会被丢弃。
- 切换 singer / Follow Track 后，旧 singer 上下文的任务结果不会写回。
- 发音/音素 worker 线程不再读取已删除或已撤销的 live note。

### 对快速撤销问题的影响

能修复一部分快速撤销问题，主要覆盖发音和音素任务导致的旧结果写回。

但截图中的典型崩溃路径是 `UpdateDurationState -> updatePhoneOffset -> notifyNoteChanged -> PianoRollGraphicsView`，属于 piece / pipeline 级写回，阶段 1 不能完整修复。

### 测试用例

#### 手动测试

1. 连续快速创建多个音符，观察发音/音素任务反复取消重建。
2. 任务还在运行时继续改歌词或删除其中一个音符。
3. 等旧任务完成。
4. 预期：不崩溃；旧歌词对应的 pronunciation / phoneme 不会写回当前音符；日志显示 stale task 被丢弃。

#### 日志验收

任务创建时记录：

```text
Create pronunciation task: clipId=..., taskRevision=N
```

修改 note 后 revision 变成 `N+1`。

旧任务完成时应出现：

```text
Drop stale pronunciation task: clipId=..., taskRevision=N, currentRevision=N+1
```

并且不应调用 `Helper::updatePronunciation()` 或 `Helper::updatePhoneName()`。

## 阶段 2：Pipeline / Piece 级 Revision + Update State Gate

### 交付内容

把 revision gate 扩展到完整推理链路：

- `InferPipeline`
- `InferDurationTask`
- `InferPitchTask`
- `InferVarianceTask`
- `InferAcousticTask`
- `UpdateDurationState`
- `UpdatePitchState`
- `UpdateVarianceState`
- `UpdateAcousticState`
- 其他状态机内部结果应用点

piece 级任务创建时记录：

- `clipId`
- `pieceId`
- `clipRevision`
- 必要时记录 `pieceRevision`
- note id 列表
- singer / speaker
- 任务输入 hash 或必要输入摘要

写回前检查：

- clip 是否仍存在。
- piece 是否仍存在。
- piece 是否仍属于当前 clip。
- revision 是否匹配。
- 任务携带的 note id 是否仍属于当前 clip / piece。
- singer / speaker 是否仍匹配当前 clip / piece 上下文。

同时修正 pipeline task input 的构建时机：

- 当前 `BaseInferState::onRunningInferenceStateEntered()` 在 `QtConcurrent::run()` 中调用 `buildTaskInput()`，会在后台线程读取 `m_pipeline.piece()`、`piece.notes`、`appModel`、param curve 等 live model。
- 修订后应在主线程进入 state 时先构造 `InferDurationTask::InferDurInput`、`InferPitchTask` input、`InferVarianceTask` input、`InferAcousticTask` input。
- 后台线程只允许使用这些 input snapshot 创建或运行任务。

`BaseInferState::handleTaskFinished()` 和各 `Update*State::onEntry()` 都需要经过统一 gate：

- `handleTaskFinished()` gate 失败：删除 task，pipeline 进入 final 或 failed，不向 pipeline 保存结果。
- `Update*State` gate 失败：不调用 `InferControllerHelper::update*()`，pipeline 进入 final 或发出 drop 成功事件。
- 不允许 `UpdateDurationState` 直接使用未校验的 `m_pipeline.piece().notes`。

### 实施记录

状态：已完成并通过 configure/build 验证。（2026-06-07）

本阶段实际落点：

- `InferInputBase` 已记录 `clipRevision`，并可生成 `InferenceTaskContext`，包含 task type、clip / piece id、note id 列表、singer 和 speaker。
- `InferDurationTask`、`InferPitchTask`、`InferVarianceTask`、`InferAcousticTask` 通过 `IInferTask::inferenceContext()` 暴露创建时的 snapshot context。
- `InferControllerHelper::buildInfer*Input()` 在主线程构建 input 时写入当前 `SingingClip::inferenceRevision()`。
- `BaseInferState::onRunningInferenceStateEntered()` 已改为在主线程同步执行 `buildTaskInput()` / `createTask()`；worker 线程只运行已经构造好的 task。
- `InferPipeline::resolveApplyContext()` 在任务结果保存和 update state 写回前统一检查 clip / piece 是否仍存在、revision 是否匹配、piece 是否仍属于 clip、note membership 是否仍有效，以及 singer / speaker 是否仍匹配。
- `BaseInferState::handleTaskFinished()` 在保存结果到 pipeline 前先通过 apply context gate；过期结果发出 `dropped`，直接进入 pipeline final，不再触发后续 update state。
- `UpdateDurationState`、`UpdatePitchState`、`UpdateVarianceState`、`UpdateAcousticState` 写回前再次 resolve 当前 context；gate 失败时不调用 `InferControllerHelper::update*()`。
- `UpdateDurationState` 改为使用 gate 重新解析出的当前 `QList<Note *>` 调用 `updatePhoneOffset()`，不再直接传入未校验的 `m_pipeline.piece().notes`。
- duration result 数量不匹配时改为 warning + drop，不再 `qFatal()`。

验证记录：

- `cmake --preset debug`：通过。
- `cmake --build --preset debug`：通过。构建期间仅出现既有 MSVC `/Zi` 被 `/ZI` 覆盖 warning。

### 预计成果

- 旧 `InferPiece` 已经被重新划分或删除后，旧 duration 结果不会再写回。
- `updatePhoneOffset(piece.notes, ...)` 不会收到过期 notes。
- 旧 pipeline 完成后不会驱动已经过期的 piece 状态机继续转移。
- pitch / variance / acoustic 的旧结果不会覆盖当前 piece 参数或音频。
- pipeline 后台准备阶段不再读取已经失效的 live piece/note。
- 快速撤销导致的视图找不到 `NoteView` 问题应基本修复。

### 对快速撤销问题的影响

这是快速 `Ctrl+Z` 连发崩溃的主要修复阶段。

当前已观察到的崩溃链路：

```text
UpdateDurationState
-> updatePhoneOffset(piece.notes, ...)
-> notifyNoteChanged
-> PianoRollGraphicsView
-> findNoteViewById() failed
```

阶段 2 完成后，过期 piece / note 的结果会在 `updatePhoneOffset()` 前被丢弃，不再触发视图更新。

### 测试用例

#### 手动测试

1. 新建一个 singing clip。
2. 连续画 5 到 10 个音符，让推理开始。
3. 按住 `Ctrl+Z` 连续撤销，直到音符逐个消失。
4. 在不同推理阶段重复测试：刚开始发音、duration 正在跑、pipeline 正在切状态。
5. 预期：不崩溃；不会出现 `noteView` assert；不会出现旧 piece 更新当前 note。

#### 日志验收

应能看到类似日志：

```text
Drop stale duration result: clipId=..., pieceId=..., taskRevision=N, currentRevision=N+1
Drop duration result because piece not found: clipId=..., pieceId=...
Drop duration result because note no longer belongs to clip: noteId=...
```

并且过期路径不应调用：

```cpp
InferControllerHelper::updatePhoneOffset(...)
```

## 阶段 3：Snapshot 查漏与统一 Gate Helper

### 交付内容

在阶段 1、2 已经完成主要 snapshot 后，本阶段用于查漏和收口统一判断入口。

新增统一 helper，例如：

```cpp
struct InferenceApplyContext {
    QString taskType;
    int clipId = -1;
    int pieceId = -1;
    quint64 clipRevision = 0;
    quint64 pieceRevision = 0;
    QList<int> noteIds;
    SingerIdentifier singer;
    QString speaker;
};

enum class InferenceApplyDecision {
    Apply,
    Drop,
    Defer,
};
```

统一 helper 负责：

- 按 `clipId` 查找当前 `SingingClip`。
- 按 `pieceId` 查找当前 `InferPiece`。
- 检查 clip / piece revision。
- 检查 note id 是否仍属于当前 clip / piece。
- 检查 singer / speaker。
- 检查 active edit session。
- 输出统一日志。

查漏范围：

- `GetPronunciationTask`、`GetPhonemeNameTask`。
- `InferDurationTask`、`InferPitchTask`、`InferVarianceTask`、`InferAcousticTask`。
- `BaseInferState::buildTaskInput()` 与 task result 保存。
- `UpdateDurationState`、`UpdatePitchState`、`UpdateVarianceState`、`UpdateAcousticState`。
- `InferControllerHelper::updatePronunciation()`、`updatePhoneName()`、`updatePhoneOffset()`、`updatePitch()`、`updateVariance()`、`updateAcoustic()` 的调用方。
- UI / audio 中异步缓存 piece 指针的路径，例如 `PhonemeView::loadWaveformAsync()` 和 `TrackInferenceHandler` 的 piece context。

### 实施记录

状态：已完成并通过 configure/build 验证。（2026-06-07）

本阶段实际落点：

- 新增 `InferenceApplyGate`，统一输出 `Apply` / `Drop` / `Defer` decision，并集中处理 clip 查找、piece 查找、clip revision、note membership、singer / speaker 和可选 edit session 检查。
- `InferPipeline::resolveApplyContext()` 已改为调用 `InferenceApplyGate`，Phase 2 的 pipeline gate 不再散落在 pipeline 内部。
- `GetPronunciationTask` / `GetPhonemeNameTask` 的完成写回已改为构造 clip 级 `InferenceTaskContext` 并调用统一 gate；本地 `resolveTaskNotes()` / `logDropClipTask()` 已移除。
- `IInferTask::inferenceContext()` 返回的 context 现在包含 task id，drop 日志可统一输出 task id、revision 和 reason。
- `PhonemeView::loadWaveformAsync()` 不再把 live `InferPiece *` 捕获到后台 worker；worker 只携带 clip id、piece id、clip revision 和 audio path，回主线程后通过统一 gate 重新解析当前 piece，并核对 audio path。
- `TrackInferenceHandler` 的 inference piece context 字典改为按 piece id 索引，status 回调只捕获 clip id / piece id，处理时重新解析当前 clip / piece，避免旧 piece 指针驱动音频上下文。

验证记录：

- `cmake --preset debug`：通过。
- `cmake --build --preset debug`：通过。构建期间仅出现既有 MSVC `/Zi` 被 `/ZI` 覆盖 warning。

### 预计成果

- 所有推理写回点使用同一套 apply/drop/defer 规则。
- 日志 reason 一致，方便手动复现快速撤销、Esc 丢弃、切歌手等问题。
- 后台任务和异步 UI/audio 缓存路径不再依赖未校验的 live 指针。

### 对快速撤销问题的影响

阶段 2 后快速撤销应已基本稳定。阶段 3 负责防止新增漏点和减少未来随机崩溃。

### 测试用例

#### 手动测试

1. 创建较长音符序列，让推理任务耗时明显。
2. 推理运行中快速删除、撤销、重做、改歌词。
3. 预期：后台任务可以完成，但不会读取已移除的 live note；旧结果要么 drop，要么由新任务覆盖。

#### 自动化测试思路

1. 构造 pronunciation / phoneme / duration / pitch / variance / acoustic snapshot 后立即改变 clip revision。
2. 运行 task 并触发完成回调。
3. 预期：任务本身不崩溃；完成后因为 revision 或 membership 不匹配而 drop。

#### 阶段验收

- worker thread 只读 snapshot。
- 主线程写回才按 id 查找当前 model。
- 查不到对象或 revision 不匹配时 drop。
- 所有 drop 日志使用统一 reason。

## 阶段 4：Edit Session Gate

### 交付内容

把当前视图层的类事务机制提升为项目级编辑会话。

当前已有 `AppStatus::currentEditObject` / `editingChanged`，但推理控制器中的
`InferControllerPrivate::onEditingChanged()` 需要先确认是否已连接；若未连接，应作为本阶段第一步补上。
后续不应只依赖“编辑中取消任务”，而应把 edit session 纳入统一 apply/drop/defer 判断。

建议引入轻量结构：

```cpp
struct EditSession {
    AppStatus::EditObjectType domain;
    int clipId;
    QList<int> noteIds;
    quint64 baseRevision;
};
```

开始拖动 note、clip、param 时创建 session。commit / discard 时关闭 session。

异步结果完成时先判断：

- 当前是否有 active edit session。
- 结果影响的 domain 是否和编辑 session 冲突。
- task revision 是否仍匹配当前 model revision。

第一版策略：

- 无冲突：apply。
- 冲突编辑中：drop。
- revision 过期：drop。

后续如有需要，再扩展 defer 队列。

### 实施记录

状态：已完成并通过 configure/build 验证。（2026-06-07）

本阶段实际落点：

- `InferController` 构造时已连接 `AppStatus::editingChanged` 到 `InferControllerPrivate::onEditingChanged()`。
- 新增轻量 `EditSession` 记录当前编辑 domain、clip id、note ids 和 base revision；进入编辑状态时记录 session，退出编辑状态时清空。
- `onEditingChanged()` 现在覆盖 Note / Clip / Phoneme / Param 等非 None 编辑状态：若 active clip 是 singing clip，会取消该 clip 相关推理任务。
- `InferenceApplyGate` 的 `checkActiveEditSession` 现在按第一版策略在编辑中直接 drop，并输出统一 reason `active-edit-session`。
- pronunciation / phoneme clip 级写回和 pipeline / update state 写回均启用 active edit session gate；迟到结果不会绕过用户正在进行的编辑事务。

验证记录：

- `cmake --preset debug`：通过。
- `cmake --build --preset debug`：通过。构建期间仅出现既有 MSVC `/Zi` 被 `/ZI` 覆盖 warning。

### 预计成果

- 用户拖动 note / clip / param 尚未 commit 时，异步结果不会插入不合时宜的模型写入。
- Esc discard 和推理完成同时发生时，推理结果不会破坏“丢弃编辑”的语义。
- 未来新增其他异步写入时，有统一入口判断 apply / drop / defer。

### 对快速撤销问题的影响

快速撤销主要靠 revision gate 修复。Edit Session Gate 主要保护“交互尚未完成”的边界，例如拖动未松手、Esc 丢弃、编辑中推理完成。

### 测试用例

#### 拖动音符后 Esc

1. 创建音符并等待推理开始。
2. 鼠标拖动一个音符，但不要提交。
3. 推理任务完成时按 Esc 丢弃拖动。
4. 预期：音符回到原位；推理结果若冲突则 drop；不会新增历史记录；不会触发视图和模型半提交状态。

#### 拖动 clip 后 Esc

1. 拖动 clip 改变位置或跨轨预览。
2. 推理任务完成时按 Esc。
3. 预期：clip 位置、轨道、颜色都恢复；冲突推理结果 drop；不会错误重分段。

#### 编辑中 Ctrl+Z

1. 开始拖动 note 或 param，不提交。
2. 按 `Ctrl+Z`。
3. 预期：`HistoryManager` 仍然拦截 undo；推理写回也不能绕过 edit session。

## 阶段 5：TaskQueue 所有权收口

### 交付内容

明确任务生命周期所有权：

- `TaskQueue` 拥有 task。
- `TaskManager` 只展示任务状态，不拥有 task。
- `InferController` 创建 task、连接结果，但不直接 delete task。
- task 完成、取消、丢弃都由 `TaskQueue` 统一销毁。

第一步可以继续使用裸指针，但删除点要收口。行为稳定后再考虑接口迁移：

```cpp
void add(std::unique_ptr<T> task);
```

### 预计成果

- task 完成、取消、删除点分散导致的悬空指针风险降低。
- current task 取消和 `finished` 信号交错时，不容易卡队列或重复删除。
- 后续迁移 `unique_ptr` 有清晰边界。

### 对快速撤销问题的影响

这是间接修复。它不会直接阻止旧结果写回，但会减少快速撤销过程中大量取消任务导致的生命周期崩溃。

当前实现中，`TaskQueue::onCurrentFinished()` 会移除 task，但发音/音素完成回调仍在
`InferControllerPrivate::handleGetPronTaskFinished()` / `handleGetPhoneTaskFinished()` 中手动
`delete &task`。current 取消路径还会注册 queued cleanup。收口时要重点验证 late finished、
重复 cancel、stopped-but-not-cleaned 的 race。

### 测试用例

#### 正常完成

1. 添加 task A、B。
2. A finished 后 B 自动启动。
3. A 被删除一次。
4. 队列最终为空。

#### current 取消

1. A 正在运行，B pending。
2. cancel A。
3. A finished 后 cleanup，B 启动。
4. 队列不堵塞。

#### stopped 但 finished callback 未处理时取消

1. A 已经设置 `TaskStopped`。
2. queued finished 尚未进入主线程处理。
3. cancel A。
4. 预期：不会漏 cleanup，不会卡 current。

#### 重复取消

1. 对同一个 current 连续 cancel 多次。
2. 预期：只 cleanup 一次，不 double delete。

#### pending 取消

1. A current，B/C pending。
2. cancel B。
3. B 被移除并删除，A/C 正常。

#### 旧 finished 信号

1. A 已经不再是 current。
2. A finished 信号晚到。
3. 预期：忽略，不影响当前任务。

### 实施记录

状态：已完成并通过 configure/build 验证。（2026-06-07）

本阶段实际落点：

- `TaskQueue` 增加 `isCurrent()`，完成回调可先判断 late finished，不再为了判断 current 而提前移除任务。
- `TaskQueue::onCurrentFinished()` 现在统一负责 current task 的 `taskManager->removeTask()` 和 `deleteLater()`，包括正常完成和 cancellation pending 完成。
- pending task 仍由 `TaskQueue::disposePendingTask()` 统一删除；current cancel cleanup 仍由 `cleanupCancelledCurrent()` 统一 `deleteLater()`。
- `InferControllerPrivate::handleGetPronTaskFinished()` / `handleGetPhoneTaskFinished()` 不再 `delete &task`，完成、drop、terminated 路径都在处理完结果后调用对应 `TaskQueue::onCurrentFinished()`。
- `BaseInferState::handleTaskFinished()` 不再 `delete currentTask`，改为在读取 task result / 保存 pipeline result 后调用对应队列的 finish 入口，并清空本地 non-owning 指针。
- `InferController` / `BaseInferState` 现在只持有 non-owning task 指针；task 生命周期由 `TaskQueue` 收口。

验证记录：

- `cmake --preset debug`：通过。
- `cmake --build --preset debug`：通过。构建期间仅出现既有 MSVC `/Zi` 被 `/ZI` 覆盖 warning。

## 阶段 6：视图防御性修补

### 交付内容

视图收到过期 note 更新时不直接 fatal，而是 warning 后忽略。此阶段重点是
`PianoRollGraphicsViewPrivate::findNoteViewById()` 后直接 `Q_ASSERT` 的路径。

例如：

```cpp
const auto noteView = findNoteViewById(note->id());
if (!noteView) {
    qWarning() << "Ignore note update because note view is missing"
               << "noteId:" << note->id();
    return;
}
```

### 实施记录

状态：已完成并通过 configure/build 验证。（2026-06-07）

本阶段实际落点：

- `PianoRollGraphicsViewPrivate` 新增缺失 note view 的统一 warning 日志，包含 context 和 note id。
- `updateSceneSelectionState()` 在选中状态同步时找不到 note view 会 warning 后跳过。
- `updateOverlappedState()` 在重叠状态刷新时找不到 note view 会 warning 后跳过。
- `updateNoteTimeAndKey()` / `updateNoteWord()` 在过期 note 更新抵达时 warning 后返回，不再触发 `Q_ASSERT`。
- `handleNoteRemoved()` 在移除通知抵达但 view 已不存在时 warning 后清理本地 note 引用和连接，不再崩溃。

验证记录：

- `cmake --preset debug`：通过。
- `cmake --build --preset debug`：通过。构建期间仅出现既有 MSVC `/Zi` 被 `/ZI` 覆盖 warning。

### 预计成果

- 类似 `findNoteViewById()` 找不到就 `Q_ASSERT` 的直接崩溃会减少。
- 用户体验从进程终止变成可恢复 warning。

### 对快速撤销问题的影响

这是止血保护，不是根治。若频繁出现 warning，说明 revision gate 仍有漏点。

### 测试用例

1. 临时模拟 `notifyNoteChanged()` 传入当前视图不存在的 note id。
2. 预期：warning 后忽略，不崩溃。
3. 正常 note 更新仍能刷新视图。

同时需要留意其他 UI / audio 异步缓存路径：

- `PhonemeView::loadWaveformAsync()` 异步加载完成后用 `InferPiece *` 写回 waveform cache，当前已有
  `m_clip->pieces().contains(piece)` 检查，但后续可改成 piece id / revision 检查。
- `TrackInferenceHandler` 以 `InferPiece *` 维护音频 context，应确保 piece 删除、重分段、跨轨移动时不会保留旧 context。

## 推荐实施顺序

为了最快修复快速撤销连发问题，建议按以下顺序实施：

1. 增加 clip 级 `inferenceRevision`，并补齐 note、singer/speaker、piece、tempo 的 bump 点。
2. 给发音/音素任务做 snapshot input，并加 clip revision / note membership gate。
3. 给 pipeline task input 增加 context snapshot，把 `buildTaskInput()` 的 live model 读取移回主线程或提前完成。
4. 给 `InferPipeline`、duration、pitch、variance、acoustic 加 piece/clip revision gate。
5. 修 `BaseInferState::handleTaskFinished()` 和 `UpdateDuration/Pitch/Variance/AcousticState` 的双重 gate。
6. 抽出统一 apply/drop/defer helper 和日志 reason，并回头替换分散判断。
7. 加视图缺失 `NoteView` 的 warning 防御，并审计 `PhonemeView` / `TrackInferenceHandler` 的异步 piece 缓存。
8. 收口 `TaskQueue` 删除权，验证 cancel / late finished / repeated cancel。
9. 接入项目级 Edit Session Gate。
10. 行为稳定后再考虑 `TaskQueue::add(std::unique_ptr<T>)` 迁移。

## 快速验收矩阵

### 阶段 1 后

- 快速建音符、改歌词、删音符。
- 观察旧发音/音素结果是否 drop。
- 预期：不写回旧 pronunciation / phoneme，worker 不再读取 live note。

### 阶段 2 后

- 连续建音符，然后按住 `Ctrl+Z`。
- 重点观察是否还崩在 `UpdateDurationState`、`updatePhoneOffset`、`PianoRollGraphicsView`。
- 预期：快速撤销连发基本稳定；旧 pitch / variance / acoustic 结果也不会覆盖当前 piece。

### 阶段 3 后

- 推理运行中删除、撤销、重做。
- 预期：所有 drop 使用统一 reason；查漏范围内不再有未校验的异步写回。

### 阶段 4 后

- 拖动 note / clip / param 过程中按 Esc。
- 推理完成与 Esc 丢弃交错。
- 预期：交互语义保持一致，冲突异步结果 drop。

### 阶段 5 后

- 快速创建、撤销、重做，制造大量任务取消和重建。
- 预期：任务队列不卡死，任务列表不残留僵尸任务，不 double delete。

## 建议日志格式

每个异步写回点统一输出 apply / drop / defer 结果，便于手动验证：

```text
Apply inference result: taskType=..., clipId=..., pieceId=..., revision=...
Drop inference result: taskType=..., clipId=..., pieceId=..., taskRevision=..., currentRevision=..., reason=...
Defer inference result: taskType=..., clipId=..., pieceId=..., editSession=...
```

推荐 reason：

- `clip-not-found`
- `not-singing-clip`
- `revision-mismatch`
- `piece-not-found`
- `note-not-found`
- `note-not-in-clip`
- `piece-revision-mismatch`
- `singer-speaker-mismatch`
- `edit-session-conflict`
- `task-terminated`
- `update-state-stale`

## 当前结论

快速撤销连发问题预计在阶段 2 完成后基本修复，但前提是阶段 1 已经把发音/音素任务改成 snapshot 输入。阶段 2 解决当前最可疑的 piece / pipeline 旧结果写回，并覆盖 duration、pitch、variance、acoustic 的 update state。阶段 3 负责统一 gate 和查漏，阶段 5 降低任务取消/完成交错的生命周期风险，阶段 4 则把现有视图事务提升成真正的模型写入屏障，避免未来新的异步写入绕过交互原子性。
