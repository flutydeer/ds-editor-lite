# 推理写回与编辑会话挂起恢复 — 设计文档

> 状态：✅ 阶段 0–5 全部实施完成（2026-06-08 验证记录：IDE 诊断 / configure / build 通过，手动场景覆盖 pending flush、commit 失效、clip 隔离、动态 scope、pending 上限）
> 本文是最终设计说明与实施约束，过程性记录已移除。

## 背景与目标

任务生命周期重构引入了 snapshot input、clip revision gate 和统一 `InferenceApplyGate`，解决了旧任务写回过期模型的问题。但 edit session gate 若只做"全局丢弃窗口"（编辑期间禁止所有异步写回），用户按住音符 / 拖动音素 / 编辑参数期间所有启用 edit gate 的写回都会被丢弃，即使最后 Esc 取消编辑，期间完成的推理结果也已丢失。

目标：**冲突感知的挂起/恢复机制**。

- 用户编辑期间，异步结果不能写入正在编辑的对象。
- 与当前编辑无关的结果可以继续正常写回。
- 与当前编辑冲突的结果不立即丢弃，进入短暂挂起（pending）。
- 用户提交编辑后，旧结果因 revision 或输入上下文过期而丢弃。
- 用户取消编辑后，若模型 revision 未变化，挂起结果重新通过 gate 并写回。
- 挂起队列有上限：同一个 clip / piece / task type 只保留最新结果。
- 不长期持有 `Task *` / `Note *` / `InferPiece *` 作为挂起结果的所有权依据。

非目标：不做完整跨编辑事务重放系统；不保证所有旧结果都能恢复；不阻塞 UI；第一版不做 defer 优先级调度。

## 目标语义（gate 决策表）

| 场景 | 期望决策 |
| --- | --- |
| 没有编辑，revision / membership / singer 校验通过 | `Apply` |
| 没有编辑，但 revision 或对象归属已过期 | `Drop` |
| 正在编辑，但结果属于其他 clip | `Apply` |
| 正在编辑，同 clip 但 note / piece / param 不冲突 | `Apply` |
| 正在编辑，结果会写入正在编辑的对象 | `Defer` |
| 用户提交编辑，模型 revision 或输入上下文变化 | pending 结果 `Drop` |
| 用户取消编辑，模型 revision 未变化 | pending 结果重新 gate，通过后 `Apply` |
| pending flush 时又进入新的冲突编辑 | 继续 `Defer`，保留最新结果 |

## 设计原则

### 轻量编辑事务（EditSessionManager）

借鉴数据库事务边界模型，但不实现完整 ACID。`EditSessionManager` 是**当前编辑事务的唯一权威来源**，职责：

- 记录当前编辑事务的完整作用域；
- 向 `InferenceApplyGate` 提供当前事务上下文；
- 事务结束时携带 `Commit / Discard / Cancel` 语义通知 pending flush。

它不保存模型旧值，不替代 `HistoryManager`、UI preview 或 action 撤销逻辑。`appStatus->currentEditObject` 只表示 UI 处于哪类编辑模式，**不参与冲突作用域推断**（可继续用于禁止 undo / playback 等 UI 行为）。

事务流程：

```text
UI 计算完整 scope
    -> beginTransaction(scope)
    -> 用户交互预览
    -> commit: 先写模型并触发 revision / paramChanged / noteChanged，再 endTransaction(Commit)
    -> discard/cancel: 先恢复 UI preview，再 endTransaction(Discard/Cancel)
    -> manager 清空 active transaction，并发出 transactionEnded
    -> pending flush 重新 gate
```

commit 必须先写模型再结束事务（pending flush 能看到提交后的 revision）；discard / cancel 保持 revision 不变，使可恢复的 pending 重新通过 gate。

### 编辑开始不取消任务

编辑开始时只创建 edit session，不主动取消相关推理任务。任务自然完成后由 gate 决定 apply / drop / defer。提交编辑导致模型真正变化时，现有 `noteChanged` / `paramChanged` / `piecesChanged` 机制触发新任务或 pipeline 重跑，旧结果在 flush 时因 revision 不匹配而 drop。

### Drop 与 Defer 必须分开

- `Drop`：结果永远不能写回（clip 不存在、revision mismatch、note 不属于当前 clip）。
- `Defer`：结果当前不能写回，但编辑结束后可能仍然有效。

reason 约定：`edit-session-conflict` / `edit-session-deferred` / `edit-session-flush-apply` / `edit-session-flush-drop`（不再使用 `active-edit-session` 作为 drop reason）。

### Gate 感知编辑作用域

```cpp
enum class EditSessionOutcome { Unknown, Commit, Discard };

struct EditSession {
    quint64 sessionId = 0;
    AppStatus::EditObjectType domain = AppStatus::EditObjectType::None;
    int clipId = -1;
    QList<int> clipIds;   // Clip 编辑可为多个
    QList<int> noteIds;
    QList<int> pieceIds;
    QList<ParamInfo::Name> params;
    bool wholeClipScope = false;  // 插入音符、动态擦除等无法提前列出 noteIds 时使用
    quint64 baseRevision = 0;
};
```

`EditSession` 不能只放在 `InferControllerPrivate`——`InferenceApplyGate` 是全局工具，pipeline update state、clip task 写回和部分 UI/cache 异步结果都调用它。

### UI 必须在 scope 准备好后显式 begin transaction

不能由 `currentEditObject` 的变更隐式创建 session（很多入口先设类型、后更新 selection）。UI 先确定完整 scope，再 `beginTransaction(scope)`。

| EditObjectType | 必需 scope |
| --- | --- |
| `Note` | `clipId`、实际要移动/缩放/删除/插入的 `noteIds`、`baseRevision` |
| `Phoneme` | `clipId`、目标 phoneme 所属 `noteIds` / `pieceIds`、`baseRevision` |
| `Param` | `clipId`、`params`、`baseRevision` |
| `Clip` | `clipId` 或 clip id 集合 |

动态 / 未知 scope 降级规则（第一版）：

- 普通移动 / 缩放音符：先更新 selection，再用实际选中 `noteIds` begin transaction。
- 插入音符：提交前没有 note id → `clipId + wholeClipScope + baseRevision`，同 clip 结果保守 defer。
- 擦除音符：拖动中持续增加待删音符可在事务内追加 `noteIds`；否则第一版用 `wholeClipScope`。
- Clip 编辑：多选记录 `clipIds`；单 clip 用 `clipId` 简写。
- 音素编辑：找到目标 phoneme 后再 begin，scope 至少包含所属 `noteIds`。
- Param 编辑：由知道 `ParamInfo::Name` 的外层 view 创建事务，`CommonParamEditorView` 不独自推断 scope。

第一版只支持**一个 active transaction**（无嵌套/多事务并发）；begin 后因失焦、视图销毁或异常路径退出必须兜底 `endTransaction(Cancel/Discard)`。

### Param session 按 clip-param 粒度

Param 编辑第一版 scope = `clipId + paramNames + baseRevision`，不做 piece / tick range 精细 scope（参数编辑窗口通常只有鼠标按下到松手，避免锚点插值范围计算复杂化）。

## 实施状态与最终架构（阶段 0–5 完成后）

1. **阶段 0 — 编辑事务基础设施**：新增 `EditSessionManager` 作为唯一 active transaction 来源；音符移动/绘制/擦除、音素拖动、参数曲线、Pitch anchor 等入口已接入 `beginTransaction` / `endTransaction`。
2. **阶段 1 — 停止全局丢弃**：移除编辑开始即取消相关任务的逻辑；`InferenceApplyGate` 不再按 `currentEditObject` 全局 drop，在 stale 校验通过后按 `EditSession` 冲突判断。
3. **阶段 2 — clip 级 pending**：pronunciation / phoneme-name 写回有 clip 级 pending store；`Defer` 时复制 context/result（不持有 task 指针），任务正常收尾。
4. **阶段 3 — pipeline 级 defer state**：`InferPipeline::resolveApplyContext()` 返回 Apply / Drop / Defer 三态；task finished 只做 stale gate，update apply 才做 edit session gate；Defer 进入 `AwaitingEditSessionApplyState`。
5. **阶段 4 — edit session outcome**：UI 正常路径显式 Commit / Discard；Pitch anchor 停用使用 Cancel；新事务覆盖旧事务时旧事务以 Cancel 收尾。
6. **阶段 5 — 日志与测试**：pending store / flush / clear 统一走 `InferenceApplyGate::logDecision()`，区分 Apply / Drop / Defer / Flush Apply / Flush Drop / pending-added / pending-replaced。

### Pending 生命周期（必须覆盖的清理入口）

pending 让 task result 在 task 清理后继续存活，因此以下入口都要同步清理：`reset`、`modelChanged`、clip/piece 删除、工程切换、模块错误、同 key 新结果（保留最新）。

### 第一版明确不做（保守简化）

| 事项 | 处理 |
| --- | --- |
| Param tick range / piece ids | 不做，按 `clipId + params` 保守处理 |
| 多 active transaction / 嵌套事务 | 不做，只支持一个 active transaction |
| pending 优先级 / 重放系统 | 不做，同 key 只保留最新结果 |
| waveform / audio cache | 不进 pending，只做 Apply / Drop stale guard |
| options fingerprint | 可先在 tempo / inference options / module error 时清相关 pending，后续再补 |

## 风险点（后续维护注意）

- `EditSessionManager` 必须保持唯一权威来源，避免 gate / controller / UI 各维护一套 session 状态。
- UI 在 scope 未准备好时 begin transaction 会导致 conflict 判断漏判/误判。
- `Param::Edited` commit 必须 bump revision 或显式 drop 冲突 pending，否则旧 pending 可能误 apply。
- Pipeline defer state 注意状态机转移，避免 final 后无法恢复；awaiting apply state 必须处理等待期间的前置参数变化。
- Clip 级 pronunciation apply 可能继续触发 phoneme-name task，flush 时避免与当前编辑再次冲突。

## 关键文件

- `src/app/Modules/Inference/EditSessionManager.cpp/.h`
- `src/app/Modules/Inference/Utils/InferenceApplyGate.cpp/.h`（三态决策 + `logDecision()`）
- `src/app/Modules/Inference/States/AwaitingEditSessionApplyState.cpp/.h`
- `src/app/Modules/Inference/InferPipeline.cpp/.h`（`resolveApplyContext()`）
- `src/app/Modules/Inference/InferController.cpp`（pending store / flush）
- UI 入口：`PianoRoll/DrawNoteHandler.cpp`、`EditPitchAnchorHandler.cpp`、`EraseNoteHandler.cpp`、`PhonemeView.cpp`、`ParamEditor/ParamEditorGraphicsView.cpp`
