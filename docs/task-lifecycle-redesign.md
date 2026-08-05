# 任务生命周期与异步写回 — 设计文档

> 状态：✅ 实施完成（6 个阶段，2026-06-07 全部通过 configure/build 验证）
> 本文是最终设计说明，实施过程记录已随重构完成而移除。

## 背景

剪辑移动、歌手切换和快速撤销问题暴露出一类共同风险：后台推理任务可能在文档模型已经变化后完成，并继续把旧结果写回当前模型。表现形式包括：

- 旧发音/音素任务写回已经变化的音符。
- 旧 `InferPiece` 或旧 `InferPipeline` 在重新分段后继续写回。
- `UpdateDurationState` 使用过期的 `piece.notes` 更新音素偏移，触发视图收到不存在音符的变更通知。
- 快速 `Ctrl+Z` / `Ctrl+Y`、Esc 丢弃交互、跨轨移动、切换歌手等操作与推理完成交错时，异常点不固定。

根因不是任务取消时序，而是缺少统一的异步写回规则：旧任务可以自然完成，但**过期结果永远不能修改当前工程**。

## 总体目标

- 后台任务可以继续异步执行，不阻塞用户操作。
- 用户编辑、撤销、重做、Esc 丢弃时，旧任务结果不能写回过期模型。
- 推理任务不长期依赖 live `Note *` / `InferPiece *` 作为语义依据；worker thread 只读 snapshot。
- `TaskQueue` 拥有任务生命周期，其他模块只观察任务状态或消费有效结果。
- 所有异步写回通过同一个入口输出 `Apply / Drop / Defer` 结果和统一日志 reason。

## 核心原则

### 1. Revision Gate（乐观锁）

任务创建时记录当前 revision，完成写回前再次校验：

- clip 是否仍存在；
- clip 是否仍是 `SingingClip`；
- 当前 revision 是否等于任务创建时的 revision；
- piece / note 是否仍属于当前 clip。

校验通过才允许写回；失败则丢弃结果。推理结果不需要强制重试——模型变化本身会触发新一轮推理，旧结果直接丢弃即可。

### 2. Edit Session Gate（编辑会话）

- 用户开始拖动 note、clip、param 时创建 edit session（轻量 `EditSession`，记录编辑 domain、clip id、note ids 和 base revision）。
- commit / discard 时结束 edit session。
- 异步结果完成时若与当前 edit session 冲突，按保守策略直接 **drop**（reason `active-edit-session`），不实现复杂挂起队列。

### 3. Snapshot Input（快照输入）

后台线程不读取 live model 指针。任务创建时快照必要输入：

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

## 最终架构（实施后）

### 统一写回入口：`InferenceApplyGate`

- 集中处理 clip 查找、piece 查找、clip revision、note 归属、edit session 冲突检查，统一输出 `Apply / Drop / Defer` decision。
- 所有写回路径（pronunciation / phoneme 的 clip 级写回、pipeline update state 写回）都经过该 gate；`logDecision()` 统一记录 reason。
- 过期结果发出 `dropped` 后直接进入 pipeline 最终态，不触碰模型。

### 任务上下文与快照

- `SingingClip` 提供 `inferenceRevision()` / `bumpInferenceRevision()`，在 note 变更、piece 清空/重分段、singer/speaker 变更时递增。
- `GetPronunciationTask` / `GetPhonemeNameTask` 保存 note snapshot、`clipRevision`、`SingerInfo` 和 tempo；worker 不再读取 live `Note *`。
- `InferInputBase` 记录 `clipRevision` 并生成 `InferenceTaskContext`（task type、clip / piece id、note id 列表、singer/speaker、task id）。
- `BaseInferState::onRunningInferenceStateEntered()` 在主线程同步执行 `buildTaskInput()` / `createTask()`；worker 只负责执行。

### 写回校验时机

- `InferPipeline::resolveApplyContext()`：任务结果保存和 update state 写回前统一检查。
- `BaseInferState::handleTaskFinished()`：保存结果到 pipeline 前先通过 apply context gate。
- `UpdateDurationState` / `UpdatePitchState` / `UpdateVarianceState` / `UpdateAcousticState`：写回前再次 resolve 当前 context；duration 使用 gate 重新解析的 `QList<Note *>` 调用 `updatePhoneOffset()`；result 数量不匹配时 warning + drop（不再 `qFatal()`）。

### 任务所有权：`TaskQueue`

- `TaskQueue` 拥有任务生命周期：`isCurrent()` 判断 late finished；`onCurrentFinished()` 统一负责 `taskManager->removeTask()` 和 `deleteLater()`；pending task 由 `disposePendingTask()` 统一删除。
- `InferController` / `BaseInferState` 只持有 non-owning task 指针。
- 完成回调通过 `inferenceContext()` 拿到 task id，drop 日志统一输出 task id、revision 和 reason。

### 视图防御性修补（止血层）

- 缺失 note view、过期 note 更新、重复移除通知等场景统一 warning 后跳过，不再 `Q_ASSERT` 崩溃。
- 若 warning 频繁出现，说明 revision/snapshot gate 仍有漏点，应先查 gate 而非扩展 warning。

## 日志与 reason 约定

```text
Apply inference result: taskType=..., clipId=..., pieceId=..., revision=...
Drop inference result: taskType=..., clipId=..., pieceId=..., taskRevision=..., currentRevision=..., reason=...
Defer inference result: taskType=..., clipId=..., pieceId=..., editSession=...
```

推荐 reason：

- `clip-not-found` / `not-singing-clip` / `revision-mismatch` / `piece-not-found` / `note-not-found` / `note-not-in-clip` / `piece-revision-mismatch` / `singer-speaker-mismatch` / `edit-session-conflict` / `task-terminated` / `update-state-stale`

## 关键文件

- `src/app/Modules/Inference/InferenceApplyGate.cpp/.h`
- `src/app/Modules/Inference/EditSessionManager.cpp/.h`（`beginTransaction` / commit / discard）
- `src/app/Modules/Inference/TaskQueue`（任务所有权）
- `src/app/Modules/Inference/InferController.cpp`（`onEditingChanged`、pending 写回入口）
- `src/app/Modules/Inference/States/AwaitingEditSessionApplyState.cpp`（defer 状态）
- `src/app/Modules/Inference/Tasks/GetPronunciationTask.cpp`、`GetPhonemeNameTask.cpp`（snapshot 输入）
