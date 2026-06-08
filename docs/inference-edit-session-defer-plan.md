# 推理写回与编辑会话挂起恢复方案

## 背景

当前任务生命周期重构已经引入了 snapshot input、clip revision gate 和统一
`InferenceApplyGate`。这些机制解决了旧任务写回过期模型的问题，但 edit session gate
目前仍然过于粗暴：

- `InferControllerPrivate::onEditingChanged()` 在进入编辑状态时会取消当前 active clip 的相关任务。
- `InferenceApplyGate::resolve()` 只要发现 `appStatus->currentEditObject != None`，就直接
  `Drop("active-edit-session")`。
- `InferenceApplyGate::Decision::Defer` 只是枚举值，当前没有任何代码返回 `Defer`。
- `onEditingChanged(None)` 只清空编辑状态和输出日志，不会恢复或重新判定此前的结果。

这意味着用户按住音符、拖动音素或编辑参数期间，所有启用 edit gate 的异步写回都会进入
全局丢弃窗口。即使用户最后按 Esc 取消编辑，编辑期间完成的推理结果也已经丢失。

这个行为不是理想的“编辑事务保护”，而是“编辑期间禁止所有异步写回”。后续需要改成
冲突感知的挂起/恢复机制。

## 目标

- 用户编辑期间，异步结果不能写入正在编辑的对象。
- 与当前编辑无关的结果可以继续正常写回。
- 与当前编辑冲突的结果不应立即丢弃，而应进入短暂挂起。
- 用户提交编辑后，旧结果因 revision 或输入上下文过期而丢弃。
- 用户取消编辑后，如果模型 revision 未变化，挂起结果可以重新通过 gate 并写回。
- 挂起队列必须有上限。同一个 clip / piece / task type 只保留最新结果。
- 不长期持有 `Task *`、`Note *`、`InferPiece *` 作为挂起结果的所有权依据。

## 非目标

- 不实现复杂的跨编辑事务重放系统。
- 不保证所有旧结果都能恢复。对象不存在、revision 变化、singer/speaker 变化时仍然必须 drop。
- 不让后台任务阻塞 UI 编辑。
- 不在第一版实现真正的 defer 优先级调度，只保留最新可用结果。

## 当前问题

### 1. 编辑开始即取消任务

当前进入编辑状态时会调用 `cancelClipRelatedTasks()`：

```cpp
void InferControllerPrivate::onEditingChanged(AppStatus::EditObjectType type) {
    if (type != AppStatus::EditObjectType::None) {
        ...
        cancelClipRelatedTasks(singingClip);
    }
}
```

这会导致用户只是按住音符尚未提交时，已有的相关任务就被终止。如果用户最后取消编辑，
原本可用的任务结果已经无法恢复。

### 2. Edit gate 是全局 drop

当前 gate 的核心逻辑是：

```cpp
if (options.checkActiveEditSession &&
    appStatus->currentEditObject != AppStatus::EditObjectType::None) {
    return drop("active-edit-session");
}
```

它没有判断当前结果是否与正在编辑的 clip、note、piece 或 param 冲突。因此编辑 A clip
时，B clip 的推理结果也会被丢弃。

### 3. 没有挂起队列

`Decision::Defer` 已经存在，但没有实际使用。Drop 后任务由 `TaskQueue` 收尾并删除，
结果对象随之丢失。

### 4. 编辑结束没有恢复入口

`onEditingChanged(None)` 没有 flush pending apply 的逻辑，也没有区分 commit / discard。

## 目标语义

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

### 使用轻量编辑事务，而不是只用 UI 编辑状态

已讨论确认：这里需要借鉴数据库事务的边界模型，但不实现完整 ACID 式事务系统。
新的对象建议命名为 `EditSessionManager` 或 `EditTransactionManager`，职责只包括：

- 记录当前编辑事务的完整作用域。
- 向 `InferenceApplyGate` 提供当前事务上下文。
- 在事务结束时携带 commit / discard / cancel 语义通知 pending flush。

它不负责保存所有模型旧值，也不替代 `HistoryManager`、UI preview 或现有 action 的撤销逻辑。
`appStatus->currentEditObject` 后续只表示 UI 当前处于哪类编辑模式，不能作为推理写回隔离的唯一依据。

推荐事务流程：

```text
UI 计算完整 scope
    -> beginTransaction(scope)
    -> 用户交互预览
    -> commit: 先写模型并触发 revision / paramChanged / noteChanged，再 endTransaction(Commit)
    -> discard/cancel: 先恢复 UI preview，再 endTransaction(Discard/Cancel)
    -> manager 清空 active transaction，并发出 transactionEnded
    -> pending flush 重新 gate
```

commit 路径必须先写模型再结束事务。这样 pending flush 能看到提交后的 revision；
discard / cancel 路径则保持 revision 不变，使可恢复的 pending 结果重新通过 gate。

### 不在编辑开始时取消任务

编辑开始时只创建 edit session，不主动取消相关推理任务。任务自然完成后，根据 gate
结果选择 apply / drop / defer。

提交编辑导致模型真正变化时，现有 `noteChanged`、`paramChanged`、`piecesChanged` 等机制会触发
新任务或 pipeline 重跑。旧结果在 flush 时因为 revision 或输入上下文不匹配而 drop。

### Drop 与 Defer 必须分开

- `Drop` 表示结果永远不能写回，例如 clip 不存在、revision mismatch、note 不属于当前 clip。
- `Defer` 表示结果当前不能写回，但编辑结束后可能仍然有效。

`active-edit-session` 不应该再作为 drop reason。新的 reason 建议为：

- `edit-session-conflict`
- `edit-session-deferred`
- `edit-session-flush-apply`
- `edit-session-flush-drop`

### Gate 必须感知编辑作用域

不能只读取 `appStatus->currentEditObject`。gate 需要知道当前 edit session 的详细上下文：

```cpp
enum class EditSessionOutcome {
    Unknown,
    Commit,
    Discard,
};

struct EditSession {
    quint64 sessionId = 0;
    AppStatus::EditObjectType domain = AppStatus::EditObjectType::None;
    int clipId = -1;
    QList<int> clipIds; // Clip 编辑可为多个；其他编辑域通常只有一个
    QList<int> noteIds;
    QList<int> pieceIds;
    QList<ParamInfo::Name> params;
    bool wholeClipScope = false; // 插入音符、动态擦除等无法提前列出 noteIds 时使用
    quint64 baseRevision = 0;
};
```

已讨论确认：`EditSession` 不能只放在 `InferControllerPrivate`。`InferenceApplyGate` 是全局工具，
pipeline update state、clip task 写回和部分 UI/cache 异步结果都会调用 gate；如果 session 只存在于
controller 私有字段，gate 无法做冲突感知判断，最终仍会退回全局 `currentEditObject` 判断。

第一版应引入单独的 `EditSessionManager` / `EditTransactionManager`，作为当前编辑事务的唯一权威来源。
`InferenceApplyGate` 通过 manager 查询 active transaction。`InferControllerPrivate::m_activeEditSession`
应删除或改成只作为迁移期缓存，避免维护两套 session 状态。

### UI 必须在 scope 准备好后显式 begin transaction

当前很多 UI 入口先设置 `currentEditObject`，后更新 selection 或查找实际对象。例如音符编辑中，
`currentEditObject = Note` 发生在 note selection 更新之前；音素编辑中，类型设置发生在具体
phoneme 查找之前；参数编辑中，底层 `CommonParamEditorView` 只知道自己在编辑参数，但不知道外层
`ParamEditorGraphicsView` 当前的 `ParamInfo::Name`。

因此不能再让 `currentEditObject` 的变更隐式创建 edit session。UI 应先确定完整 scope，再调用：

```cpp
editSessionManager->beginTransaction(scope);
appStatus->currentEditObject = scope.domain;
```

不同编辑域至少需要提供：

| EditObjectType | 必需 scope |
| --- | --- |
| `Note` | `clipId`、实际要移动/缩放/删除/插入的 `noteIds`、`baseRevision` |
| `Phoneme` | `clipId`、目标 phoneme 所属 `noteIds` / `pieceIds`、`baseRevision` |
| `Param` | `clipId`、`params`、`baseRevision` |
| `Clip` | `clipId` 或 clip id 集合 |

第一版必须明确动态 / 未知 scope 的降级规则，避免实现时重新退回全局 edit gate：

- 普通移动 / 缩放音符：先更新 selection，再用实际选中 `noteIds` begin transaction。
- 插入音符：提交前没有 note id，使用 `clipId + wholeClipScope + baseRevision`，同 clip 结果保守 defer。
- 擦除音符：如果拖动过程中持续增加待删除音符，可以在事务内追加 `noteIds`；若不想实现动态扩展，
  第一版使用 `wholeClipScope`。
- Clip 编辑：多选移动 / 缩放时记录 `clipIds`；单 clip 情况可继续用 `clipId` 作为简写。
- 音素编辑：在找到目标 phoneme 后再 begin transaction，scope 至少包含所属 `noteIds`。
- Param 编辑：事务应由知道 `ParamInfo::Name` 的外层 view 创建；`CommonParamEditorView` 不应独自推断 scope。

第一版只需要一个 active transaction，不实现嵌套或多事务并发。如果 begin 后 UI 因窗口失焦、
视图销毁或异常路径退出，必须兜底调用 `endTransaction(Cancel/Discard)`。

`currentEditObject` 后续可以继续用于禁止 undo / playback 等 UI 行为，但不参与冲突作用域推断。

### Param session 第一版按 clip-param 粒度处理

已讨论确认：Param 编辑第一版不做 piece / tick range 精细 scope。参数编辑窗口通常只持续鼠标按下到松手，
为了实现稳定和避免锚点插值范围计算复杂化，Param session 采用保守但非全局的 clip-param 粒度：

```text
Param session scope = clipId + paramNames + baseRevision
```

不在第一版记录：

```text
tickRange / affectedPieceIds
```

这意味着用户编辑 clip A 的 `Pitch` 时，clip A 内与 `Pitch` 相关的推理写回会暂缓；其他 clip 不受影响。
如果后续发现同 clip 内过多无关 piece 被 defer 影响体验，再考虑扩展 tick range / pieceIds。

锚点参数编辑也属于 Param session。`EditPitchAnchorHandler` 直接提交 `ParamInfo::Pitch` 的
`Param::Edited`，虽然不经过 `CommonParamEditorView`，但仍应显式 begin/end Param transaction：

```text
domain = Param
clipId = active singing clip
params = { Pitch }
```

### Pending 不持有 task 指针

任务完成后，结果要复制到 pending store，再交给 `TaskQueue` 删除 task。pending store
只保存 snapshot context 和结果值。

## 冲突判断

第一版采用保守但非全局的规则。

| EditObjectType | 冲突范围 |
| --- | --- |
| `Note` | 同 clip，且 result note ids 与 session note ids 相交；`wholeClipScope` 时同 clip 保守冲突 |
| `Phoneme` | 同 clip，且 result note ids 与 session note ids 相交 |
| `Param` | 同 clip，且 result task type 依赖或写入 session params；第一版不细分 piece 范围 |
| `Clip` | result clip id 与 session `clipIds` / `clipId` 相交时冲突 |
| `None` | 无冲突 |

对于 piece 级任务，如果 edit session 只有 note ids，可以通过当前 piece 的 note ids 判断是否相交。
如果无法可靠判断，允许对同 clip 保守 defer，但不能影响其他 clip。

Param 冲突判断建议按参数依赖关系保守处理：

| 正在编辑的参数 | 同 clip 内需要 defer 的结果 |
| --- | --- |
| `Expressiveness` | Pitch / Variance / Acoustic 相关写回 |
| `Pitch` | Pitch apply / Variance / Acoustic |
| `Breathiness` / `Tension` / `Voicing` / `Energy` / `MouthOpening` | Variance apply / Acoustic |
| `Gender` / `Velocity` / `ToneShift` | Acoustic |

## InferenceApplyGate 调整

建议把 options 中的 `checkActiveEditSession` 改为可返回 Defer 的行为：

```cpp
struct Options {
    QString phase = "apply";
    qsizetype expectedNoteCount = -1;
    bool requirePiece = true;
    bool requireNotesInPiece = true;
    bool checkSingerSpeaker = true;
    bool checkEditSession = false;
};
```

`resolve()` 的顺序建议为：

1. 检查 context 基本合法性。
2. 查找 clip / piece / notes。
3. 检查 revision、membership、singer / speaker。
4. 如果前面失败，返回 `Drop`。
5. 如果启用 edit session 检查且存在冲突，返回 `Defer`。
6. 否则返回 `Apply`。

注意：revision mismatch 仍然优先于 defer。过期结果不应进入 pending。

### Param::Edited 必须让依赖它的旧推理结果失效

已讨论确认：`Param::Edited` 是推理输入的一部分，提交时必须使依赖旧参数输入的 pending 结果失效。
状态机收到 `paramChanged` 后会尝试重跑相关阶段，但它不能阻止已经进入 pending 的旧结果在事务结束后
重新 apply。因此不能只依赖状态机转移兜底。

建议规则：

| Param 类型 | revision 行为 | 原因 |
| --- | --- | --- |
| `Param::Edited` 且影响推理输入 | bump `inferenceRevision` 或专用 `inferenceInputRevision` | 用户提交改变了后续推理输入，旧 pending 必须 drop |
| `Param::Original` | 不 bump | 这是推理输出写回，不能让 pipeline 自己写回后把自身结果判 stale |
| `Param::Envelope` | 按是否参与推理输入决定 | 第一版可保守 opt-in 或明确暂不处理 |

影响关系至少包括：

| 参数 | 影响阶段 |
| --- | --- |
| `Expressiveness` | Pitch 及其后续 Variance / Acoustic |
| `Pitch` | Variance / Acoustic |
| `Breathiness` / `Tension` / `Voicing` / `Energy` / `MouthOpening` | Acoustic |
| `Gender` / `Velocity` / `ToneShift` | Acoustic |

如果暂时不想让 param commit bump clip 级 revision，也必须在 edit transaction `Commit` 结束时按 scope
显式 drop 冲突 pending；这里的 pending 包括 controller pending store 和 pipeline awaiting apply state，
否则旧参数输入生成的结果可能在提交后误 apply。

## Pending 结果设计

### Clip 级 pending

适用于：

- `GetPronunciationTask`
- `GetPhonemeNameTask`

建议结构：

```cpp
struct PendingPronunciationApply {
    InferenceTaskContext context;
    QStringList pronunciations;
};

struct PendingPhonemeNameApply {
    InferenceTaskContext context;
    QList<PhonemeNameResult> phonemeNames;
};
```

key：

```text
taskType + clipId
```

同 key 只保留最新结果。

### Pipeline 级 pending

pipeline 已经有状态机和结果缓存，建议不要把 task 指针或大结果再复制一份到 controller。
更好的做法是：

1. `BaseInferState::handleTaskFinished()` 只做 revision / membership / singer gate，不做 edit session gate。
2. 任务结果通过后保存到 `InferPipeline`。
3. `UpdateDurationState` / `UpdatePitchState` / `UpdateVarianceState` / `UpdateAcousticState`
   在真正写回前做 edit session gate。
4. 如果 gate 返回 `Defer`，pipeline 进入等待编辑结束的状态，而不是 final。
5. 编辑结束后重新进入对应 update state，再次 gate 后 apply 或 drop。

这样可以避免复制 pitch / variance / acoustic 大结果，也能保持 pipeline 后续状态转移语义。

已讨论确认：pipeline 不能继续把 gate 结果压成 `bool`。`Drop` 和 `Defer` 语义不同：

| Decision | 语义 | pipeline 行为 |
| --- | --- | --- |
| `Apply` | 当前可以写回 | update state 调用对应 update helper，继续后续阶段 |
| `Drop` | 结果永久不可写回 | 丢弃结果，进入 final / dropped |
| `Defer` | 当前冲突，但结果仍可能有效 | 保留 pipeline 内已有 result，进入 awaiting apply state |

建议让 pipeline 暴露三态结果，例如：

```cpp
struct ApplyGateResult {
    InferenceApplyGate::Decision decision = InferenceApplyGate::Decision::Drop;
    InferenceTaskResolution resolution;
    QString reason;
};

[[nodiscard]] ApplyGateResult resolveApplyContext(
    qsizetype expectedNoteCount = -1,
    bool checkEditSession = true
) const;
```

`BaseInferState::handleTaskFinished()` 调用时应设置 `checkEditSession = false`，用于判断 task result
是否已经永久 stale；update state 调用时设置 `checkEditSession = true`，用于判断真正写回是否需要
apply / drop / defer。

建议增加一个轻量状态：

```cpp
class AwaitingEditSessionApplyState : public QState {
    // 保存要恢复的 update stage，例如 Duration / Pitch / Variance / Acoustic
};
```

已讨论确认：`UpdateXxxState` 适合“试着写一次”，不适合“等待一段时间再写”。因此 `Defer`
最好显式进入 awaiting apply state，而不是让 `UpdateXxxState` 内部停住。

第一版可以先为每个 update state 添加 `deferred` 信号和对应 awaiting state：

```text
UpdateDurationState --deferred--> AwaitingDurationApplyState --editSessionEnded--> UpdateDurationState
UpdatePitchState    --deferred--> AwaitingPitchApplyState    --editSessionEnded--> UpdatePitchState
UpdateVarianceState --deferred--> AwaitingVarianceApplyState --editSessionEnded--> UpdateVarianceState
UpdateAcousticState --deferred--> AwaitingAcousticApplyState --editSessionEnded--> UpdateAcousticState
```

`Drop` 不需要新状态，由 update state emit dropped / final 即可。`Defer` 需要新状态，因为它表示结果
仍然有效，只是当前事务隔离窗口内不能写回。

Awaiting edit apply state 还必须响应等待期间的失效事件，不能只等待 `editSessionEnded`。例如：

| 等待的结果 | 等待期间发生 | 期望处理 |
| --- | --- | --- |
| Duration apply | note / phoneme 变化、piece removed | drop 或重启对应 pipeline |
| Pitch apply | `expressivenessChanged`、piece removed | 丢弃旧 pitch 结果，回到 pitch 推理 |
| Variance apply | `pitchChanged`、piece removed | 丢弃旧 variance 结果，回到 variance 推理 |
| Acoustic apply | `varianceChanged`、piece removed | 丢弃旧 acoustic 结果，回到 acoustic 推理 |

否则用户在 pending 等待期间提交新的前置参数后，旧 pending 仍可能在 flush 时写回。

已讨论确认：这不是重做 pipeline 状态机，而是适配原有状态机规则。当前状态机已经在
`InferPitchState`、`InferVarianceState`、`AwaitingInferAcousticState`、`InferAcousticState`
和 `PlaybackReadyState` 等状态上处理了 `expressivenessChanged` / `pitchChanged` /
`varianceChanged` 这类前置参数变化；新增的 awaiting apply states 不会自动继承这些 sibling
state transition，因此必须显式补上同类转移。

第一版建议按原有规则补齐：

```text
AwaitingPitchApplyState
    --expressivenessChanged--> InferPitchState
    --pieceRemoved--> FinalState

AwaitingVarianceApplyState
    --pitchChanged--> InferVarianceState
    --expressivenessChanged--> InferPitchState
    --pieceRemoved--> FinalState

AwaitingAcousticApplyState
    --varianceChanged--> InferAcousticState
    --pitchChanged--> InferVarianceState
    --expressivenessChanged--> InferPitchState
    --pieceRemoved--> FinalState
```

`AwaitingDurationApplyState` 第一版可以主要依赖 piece 删除和 resume 后的 revision gate；如果后续
补齐 phoneme / note 细粒度 pipeline signal，再按同样原则接入。

## 编辑结束处理

当前 `AppStatus::editingChanged(None)` 不携带 commit / discard 语义。为了正确处理 pending，
建议新增明确的结束原因。

### 推荐接口

```cpp
enum class EditSessionEndReason {
    Commit,
    Discard,
    Cancel,
    Unknown,
};

Q_SIGNAL void editSessionEnded(const EditSession &session, EditSessionEndReason reason);
```

如果采用 `EditSessionManager`，推荐接口语义为：

```cpp
quint64 beginTransaction(const EditSession &session);
void endTransaction(quint64 sessionId, EditSessionEndReason reason);
Q_SIGNAL void editSessionEnded(const EditSession &session, EditSessionEndReason reason);
```

`endTransaction()` 应先清空 active transaction，再发出 `editSessionEnded`。这样 flush pending 时不会因为
刚结束的同一事务再次得到 `Defer`。

如果短期不想改 AppStatus，可以在 `InferControllerPrivate` 中先做保守推断：

- revision 与 `baseRevision` 不同：视为 commit 后变化，pending drop。
- revision 与 `baseRevision` 相同：视为 discard 或 no-op commit，pending 可重新 gate。

但该推断对 param 编辑不一定可靠，因为当前 param change 未必会 bump clip inference revision。
因此完整方案仍建议增加明确的 edit session end reason，或为影响推理输入的 param commit 补齐 revision。

## Flush Pending 规则

编辑结束时执行：

1. 取出结束的 edit session。
2. 遍历 pending store 中与该 session 可能相关的结果。
3. 对每个 pending 重新调用 `InferenceApplyGate::resolve()`。
4. 如果返回 `Apply`，调用对应 update helper。
5. 如果返回 `Drop`，删除 pending。
6. 如果返回 `Defer`，说明又进入新的冲突编辑，继续保留最新 pending。

对于 clip 级任务：

- pronunciation apply 后可以继续创建 phoneme-name task。
- phoneme-name apply 后可以 reSegment 并创建 pipeline。

对于 pipeline 级任务：

- update state apply 成功后继续原有状态转移。
- drop 后进入 final 或 failed，具体按现有 stale result 语义处理。

## 内存与生命周期

- pending store 不保存 QObject 指针作为所有权依据。
- pending store 的 key 覆盖旧结果，避免长期编辑造成内存增长。
- clip 删除时删除该 clip 的所有 pending。
- piece 删除或 reSegment 时删除该 piece 的 pending。
- `reset()`、工程关闭 / 切换、模块错误、重新调度同 key 任务时删除相关 pending。
- singer / speaker / tempo 变化后，revision gate 会让旧 pending drop。
- TaskQueue 仍然拥有 task 生命周期，pending 保存的是 task result copy 或 pipeline 内已有 result。

### Pending 清理入口

已讨论确认：pending 带来的额外清理需求，本质上来自“推理完成”和“结果写回”被拆开。
以前 task 完成后立刻 apply / drop，`TaskQueue` 清理 task 基本等于清理结果；现在 task 完成后 result
可能进入 pending，task 被删除后 result 仍然存活，所以所有让上下文失效的入口都要同步考虑 pending。

第一版至少需要覆盖：

| 入口 | 清理动作 |
| --- | --- |
| `InferControllerPrivate::reset()` | 清全部 clip 级 pending，并让 pipeline deferred result 失效 |
| 工程关闭 / 切换 / 重新加载 / `appModel::modelChanged` | 清全部 pending |
| 真正删除 clip | `clearPendingForClip(clipId)`；cross-track move 不应误清 |
| discarded piece / reSegment | 删除对应 pipeline，清 piece 级 deferred result |
| 同 key 新任务调度或新 deferred result 到来 | 覆盖旧 pending，只保留最新 |
| module error | 清相关 pending，language error 至少清 pronunciation / phoneme-name pending |
| tempo / inference options 变化 | 清相关 pending 或确保 revision / option fingerprint 让旧 pending drop |
| transaction commit | revision mismatch 自然 drop；若某类 commit 未 bump revision，则显式清冲突 pending 和 pipeline deferred |

建议接口：

```cpp
void clearAllPendingApplies();
void clearPendingForClip(int clipId);
void clearPendingForPiece(int pieceId);
void clearPendingForKey(const PendingApplyKey &key);
void clearPendingConflictingWithSession(const EditSession &session);
```

Pipeline 级 deferred result 不一定进入 controller pending store，但需要等价的清理能力：

```cpp
void InferPipeline::clearDeferredApplyResult(Stage stage);
void InferPipeline::dropDeferredApplyAndRestart(Stage targetStage);
```

### UI/cache 异步结果不进入 pending

已讨论确认：Defer / pending 只用于模型写回，不用于 UI/cache 异步结果。UI/cache 类结果仍可复用
`InferenceApplyGate` 做 clip / piece / revision stale guard，但不启用 edit session gate。

典型例子是 `PhonemeView::loadWaveformAsync()` / `onWaveformReady()`。它异步加载 waveform peak cache，
完成后只更新 view cache，不修改 `SingingClip`、`Note` 或 `InferPiece` 的推理模型状态。因此它应该只有：

| Decision | 行为 |
| --- | --- |
| `Apply` | 当前 clip / piece / audioPath 仍匹配，更新 UI cache |
| `Drop` | 上下文过期，丢弃结果 |
| `Defer` | 不使用 |

规则：

| 调用点类型 | `checkEditSession` | 是否允许 Defer |
| --- | --- | --- |
| pronunciation / phoneme-name 写回 | `true` | 是 |
| pipeline update state 写回 | `true` | 是 |
| acoustic 写回 `InferPiece::audioPath` / status | `true` | 是 |
| waveform cache | `false` | 否 |
| audio engine context cache | `false` | 否 |
| 只读 async loader / preview cache | `false` | 否 |

因此 `InferenceApplyGate::Options::checkEditSession` 必须默认 `false`，只有真正会写模型的调用点显式打开。
如果后续认为命名容易混淆，可再拆出只做 stale resolution 的 helper；第一版无需拆分。

## 日志 reason

已讨论确认：引入 Defer / pending 后，一个结果的生命周期不再是“完成后立刻 apply / drop”，而是可能经历
初次 gate、pending store、flush pending、pipeline awaiting state 等多个阶段。日志必须区分这些阶段，
否则手动验证时很难判断行为是否正确。

建议新增统一日志入口：

```cpp
void logDecision(const InferenceTaskContext &context,
                 const QString &phase,
                 InferenceApplyGate::Decision decision,
                 const QString &reason,
                 const LogExtra &extra = {});
```

`logDrop()` 可以保留，但内部走 `logDecision()`。日志字段建议包含：

| 字段 | 说明 |
| --- | --- |
| `decision` | Apply / Drop / Defer / Clear |
| `reason` | 稳定 token |
| `phase` | clip-task / pipeline-task-finished / pipeline-update / pending-flush / pending-clear |
| `taskType` | pronunciation / phoneme-name / duration / pitch / variance / acoustic |
| `taskId` | 原 task id |
| `clipId` / `pieceId` | 当前上下文 |
| `taskRevision` / `currentRevision` | stale 判断依据 |
| `sessionId` / `sessionDomain` | 当前编辑事务信息 |
| `pendingKey` | pending store key |
| `stage` | pipeline stage，可选 |

第一版至少覆盖：

| 阶段 | reason |
| --- | --- |
| 初次 gate 因编辑冲突挂起 | `edit-session-conflict` |
| pending 新增 | `pending-added` |
| 同 key pending 覆盖旧结果 | `pending-replaced` |
| cancel / discard 后 flush 成功写回 | `edit-session-flush-apply` |
| commit 后 revision mismatch 丢弃 | `edit-session-flush-drop-revision-mismatch` |
| commit 后显式丢弃冲突 pending | `edit-session-commit-conflict-drop` |
| flush 时又遇到新编辑事务 | `edit-session-flush-defer` |
| reset 清 pending | `pending-cleared-reset` |
| clip 删除清 pending | `pending-cleared-clip-removed` |
| piece 删除清 pending | `pending-cleared-piece-removed` |
| module error 清 pending | `pending-cleared-module-error` |
| awaiting state 被前置变化打断 | `pipeline-awaiting-invalidated` |

验收日志示例：

```text
Defer inference result ... reason=edit-session-conflict
Apply pending inference result ... reason=edit-session-flush-apply
Drop pending inference result ... reason=edit-session-flush-drop-revision-mismatch
```

`active-edit-session` 不再作为 drop reason。编辑冲突应记录为 `decision=Defer`、
`reason=edit-session-conflict`。

## 建议实施步骤

### 阶段 0：建立编辑事务基础设施

- 新增 `EditSessionManager` / `EditTransactionManager`。
- 明确 `currentEditObject` 只表示 UI 编辑状态，不再作为 gate scope 来源。
- UI 入口改为先构造完整 scope，再 `beginTransaction(scope)`。
- 对插入音符、擦除音符、多选 clip 这类动态 scope 明确使用 `wholeClipScope`、`clipIds`
  或事务内扩展 scope。
- commit / discard / cancel 路径显式 `endTransaction(reason)`。
- `Param::Edited` 影响推理输入时 bump revision，或在 transaction commit 时显式 drop 冲突 pending。

### 阶段 1：停止全局丢弃

- 移除 `onEditingChanged()` 中编辑开始即 `cancelClipRelatedTasks()` 的逻辑。
- `InferenceApplyGate` 不再因为任意 `currentEditObject != None` 直接 drop。
- 增加 edit session 冲突判断，只对同 clip 且冲突的结果返回 `Defer`。

### 阶段 2：clip 级 pending

- 为 pronunciation / phoneme-name 增加 pending store。
- 任务完成且 gate 返回 `Defer` 时复制结果并收尾 task。
- `onEditingChanged(None)` 或新的 `editSessionEnded` 中 flush pending。

### 阶段 3：pipeline 级 defer state

- `BaseInferState::handleTaskFinished()` 去掉 edit session gate，只保存通过 revision gate 的 result。
- update state 写回前如果返回 `Defer`，进入 awaiting edit session apply state。
- 编辑结束后恢复对应 update state。

### 阶段 4：明确 edit session outcome

- 为 AppStatus 或单独 service 增加 edit session id 与结束原因。
- UI commit / discard 路径显式传递 `Commit` / `Discard`。
- 对 param、clip、note、phoneme 的 session scope 做更准确记录。

### 阶段 5：清理日志与测试

- 日志区分 Apply / Drop / Defer / Flush Apply / Flush Drop。
- 补齐手动测试矩阵。
- 如后续添加自动测试，可构造 fake context 和 fake edit session 验证 gate 决策。

## 验收用例

### 长按音符不放

1. 创建 singing clip 并触发推理。
2. 鼠标按住一个音符不放，进入编辑状态但不移动。
3. 等待后台 pronunciation / phoneme / duration / acoustic 完成。
4. 松开且无实际变更，或 Esc 取消。
5. 预期：冲突结果在编辑期间 defer，编辑结束后 revision 未变则 apply。

### 编辑 A clip，不影响 B clip

1. A clip 进入 note 编辑状态。
2. B clip 的推理任务完成。
3. 预期：B clip 结果直接 apply，不受 A clip 编辑影响。

### 提交音符移动

1. 推理运行中拖动音符并提交。
2. 旧结果若与该 note / piece 冲突，应 defer 到编辑结束。
3. 提交后 revision 变化，旧 pending drop。
4. 新的 noteChanged 触发新推理。

### Esc 取消音符移动

1. 推理运行中拖动音符。
2. 推理完成并进入 pending。
3. 按 Esc 取消。
4. revision 未变化，pending 重新 gate 后 apply。

### 长时间拖动不产生无限 pending

1. 长时间保持编辑状态。
2. 多个同类型任务结果完成。
3. 预期：同 key pending 只保留最新结果。

## 本轮验证记录

| 项目 | 结果 | 备注 |
| --- | --- | --- |
| IDE 诊断 | 通过 | 新增 `AwaitingEditSessionApplyState` 及本轮修改文件无错误；剩余提示为既有 include/static/const 类建议。 |
| CMake Configure | 通过 | 使用 `CMake Configure` skill，`cmake --preset debug` 成功；`xsimd_DIR` 为可选未找到提示。 |
| CMake Build | 通过 | 使用 `CMake Build` skill，`cmake --build --preset debug` 成功生成 `bin/DsEditorLite.exe`；MSVC 输出既有 `/Zi` 被 `/ZI` 覆盖 warning。 |

| 手动场景 | 覆盖点 | 预期日志 / 行为 |
| --- | --- | --- |
| 长按音符后取消 | note scope + pending flush | 编辑期间出现 `edit-session-conflict` / `pending-added`，取消后 revision 未变则 `edit-session-flush-apply`。 |
| 拖动音符并提交 | note scope + commit 失效 | 冲突结果先 Defer，提交后旧 revision 结果以 `edit-session-flush-drop-revision-mismatch` 丢弃，新推理重新调度。 |
| 编辑 A clip 时 B clip 完成推理 | clip scope 隔离 | B clip 不进入 pending，直接 Apply。 |
| 拆分 / 绘制 / 擦除音符 | dynamic whole clip scope | 同 clip 旧结果 Defer 或因提交后 revision 变化 Drop，不依赖未知新 note id。 |
| 参数曲线 / Pitch anchor 编辑 | param scope | Pitch / Expressiveness 等相关 pipeline 写回 Defer；等待期间前置参数变化记录 `pipeline-awaiting-invalidated`。 |
| 长时间拖动产生多次结果 | pending 上限 | 同 clip / task type 仅保留最新 pending，旧结果记录 `pending-replaced`。 |

## 风险点

- 如果 `EditSessionManager` 不是唯一权威来源，gate、controller 和 UI 容易维护多套 session 状态。
- 如果 UI 在 scope 尚未准备好时 begin transaction，conflict 判断会漏判或误判。
- 如果插入音符、擦除音符、多选 clip 等动态 scope 没有降级规则，最终仍会退回过度全局的 edit gate。
- 如果 `Param::Edited` commit 不 bump revision，也不显式 drop 冲突 pending，旧 pending 可能误 apply。
- Pipeline defer state 需要小心状态机转移，避免 final 后无法恢复。
- Awaiting apply state 必须处理等待期间的前置参数变化，否则状态机兜底不足。
- Clip 级 pronunciation apply 可能继续触发 phoneme-name task，flush 时要避免与当前编辑再次冲突。
- 当前 `m_activeEditSession` 记录了 note ids 和 base revision，但 gate 没有使用。改造时要避免重复维护两套 session 状态。

## 第一版必须明确与可保守简化

### 必须明确

| 事项 | 第一版结论 |
| --- | --- |
| edit scope 表达 | 需要支持实际对象 scope、`wholeClipScope` 和多 clip scope；未知 note id 时同 clip 保守冲突 |
| Param commit 失效 | `Param::Edited` 影响推理输入时 bump revision，或显式清冲突 pending / pipeline deferred |
| pipeline gate 时机 | task finished 只做 stale gate；update apply 才做 edit session gate，并保留 Drop / Defer 三态 |
| pending 清理 | result 生命周期独立于 task，`reset`、`modelChanged`、clip/piece 删除、module error、同 key 新任务都要清 |
| transaction end 顺序 | commit 先写模型再 end；discard/cancel 先恢复 preview 再 end；manager 先清 active 再发 ended |

### 可以保守简化

| 事项 | 第一版处理 |
| --- | --- |
| Param tick range / piece ids | 不做，按 `clipId + params` 保守处理 |
| 多 active transaction / 嵌套事务 | 不做，只支持一个 active transaction |
| pending 优先级 / 重放系统 | 不做，同 key 只保留最新结果 |
| waveform / audio cache | 不进 pending，只做 Apply / Drop stale guard |
| options fingerprint | 可先在 tempo / inference options / module error 时清相关 pending，后续再补 fingerprint |

## 讨论进度记录

| 事项 | 结论 / 状态 |
| --- | --- |
| Edit session 放置位置 | 已确认：引入独立 `EditSessionManager` / `EditTransactionManager`，不只放在 `InferControllerPrivate` |
| 事务模型 | 已确认：采用轻量编辑事务，负责异步写回隔离和 pending flush，不负责完整模型回滚 |
| UI scope | 已确认：UI 必须在 scope 准备好后显式 `beginTransaction(scope)`，`currentEditObject` 只做 UI 状态 |
| 结束顺序 | 已确认：commit 先写模型再 end transaction；discard/cancel 先恢复 preview 再 end transaction |
| Param revision | 已确认：`Param::Edited` 影响推理输入时必须 bump revision 或在 commit 时显式 drop 冲突 pending；`Param::Original` 不 bump |
| Pipeline Decision API | 已确认：`resolveApplyContext()` 应返回 `Decision + reason`，不能把 Drop / Defer 压成 bool |
| Awaiting apply state | 已确认：Defer 需要显式 awaiting apply state；第一版可按 Duration / Pitch / Variance / Acoustic 分别实现 |
| Awaiting invalidation | 已确认：这是对原有状态机的适配；新增 awaiting apply states 必须补齐前置参数和 pieceRemoved 转移 |
| Param session scope | 已确认：第一版按 clip-param 粒度处理，即 `clipId + params + baseRevision`；不记录 tick range / pieceIds，锚点 Pitch 也纳入 |
| 动态 scope | 已确认：插入音符、擦除音符、多选 clip 第一版使用 `wholeClipScope`、`clipIds` 或事务内扩展 scope；不能依赖 `currentEditObject` 推断 |
| Pending 生命周期清理 | 已确认：pending 让 task result 在 task 清理后继续存活，因此 reset、clip/piece 删除、工程切换、模块错误、同 key 新结果等入口都要同步清理 |
| UI/cache 异步结果 | 已确认：waveform、audio context cache 等非模型写回只做 Apply / Drop stale guard，不进入 Defer / pending |
| 日志 reason | 已确认：需要统一记录 Apply / Drop / Defer / Clear，并覆盖 pending-added、flush apply/drop/defer、commit drop 和 pending clear 等路径 |

## 实施进度记录

| 阶段 | 状态 | 本轮实现要点 |
| --- | --- | --- |
| 阶段 0：建立编辑事务基础设施 | 已完成 | 新增 `EditSessionManager` 作为唯一 active transaction 来源；音符移动/绘制/擦除、音素拖动、参数曲线、Pitch anchor 与 clip 移动/缩放入口显式 begin/end transaction；`Param::Edited` 提交时 bump `inferenceRevision`。 |
| 阶段 1：停止全局丢弃 | 已完成 | 移除编辑开始即取消相关任务的逻辑；`InferenceApplyGate` 不再读取 `currentEditObject` 全局 drop，而是在 stale 校验通过后按 `EditSessionManager` scope 对冲突结果返回 `Defer`，无关 clip / note / param 继续 `Apply`。 |
| 阶段 2：clip 级 pending | 已完成 | 为 pronunciation / phoneme-name 写回增加 clip 级 pending store；Defer 时复制 context/result 并让 task 正常收尾；`editSessionEnded` 后重新 gate 并按 Apply / Drop / Defer flush；reset、clip 删除、模块错误和新任务调度会清理相关 pending。 |
| 阶段 3：pipeline 级 defer state | 已完成 | `InferPipeline::resolveApplyContext()` 返回 Apply / Drop / Defer 三态；task finished 只做 stale gate，Duration / Pitch / Variance / Acoustic 写回阶段遇到编辑冲突进入 `AwaitingEditSessionApplyState`，事务结束后重新 gate；awaiting state 补齐 pieceRemoved 和前置参数变化转移，并在事务已结束时立即重试。 |
| 阶段 4：明确 edit session outcome | 已完成 | UI 正常路径保持显式 Commit / Discard，Pitch anchor 停用使用 Cancel；新事务覆盖旧事务时旧事务以 Cancel 结束；split note 这类会新增 note / 改变 piece membership 的动态操作改为 `wholeClipScope`，保守阻止同 clip 旧结果写回。 |
| 阶段 5：清理日志与测试 | 已完成 | pending store / flush / clear 已统一走 `InferenceApplyGate::logDecision()`；awaiting state 因 pieceRemoved 或前置参数变化离开时记录 `pipeline-awaiting-invalidated`；补齐手动测试矩阵，并按项目 skill 完成 `cmake --preset debug` 与 `cmake --build --preset debug`。 |

## 推荐结论

下一轮应优先修复 edit session gate。最小可接受版本是：

1. 建立 `EditSessionManager` / 轻量编辑事务，作为唯一 session scope 来源。
2. 不再编辑开始即取消任务。
3. edit gate 只对冲突结果返回 `Defer`。
4. `Param::Edited` commit 能让旧 pending 明确失效。
5. pronunciation / phoneme-name 先实现 pending + flush。
6. pipeline update state 再实现 defer state。

完成后，编辑事务保护才会从“全局丢弃窗口”变成“冲突对象暂缓写回”，既保护用户正在编辑的对象，也避免丢失无关或可恢复的推理结果。
