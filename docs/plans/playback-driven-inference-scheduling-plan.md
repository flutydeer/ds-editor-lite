# 播放驱动的声学推理调度优化方案（方案 B：动态跟随播放头 + 前瞻窗口）

> 状态：待实施（2026-08-11 用户确认方案 B，允许重构）
> 涉及：`InferController.{h,cpp}`、`InferController_p.h`、`InferPipeline.{h,cpp}`、`States/*`

## TL;DR

**问题**：关闭自动启动声学推理（lazy acoustic inference）后，**按下播放**会对"播放头所在 + 之后所有 pending 片段"一次性启动推理；**暂停**时不做任何事，已触发的整条队列仍串行跑完，长时间占用 GPU/CPU。

**方案 B**：把声学推理调度从"一次播放 = 一次全量触发"改成"**动态跟随播放头**"：

1. **前瞻窗口**：启动推理的片段限定在 `[播放头, 播放头+窗口]` 内，窗口外的不触发；
2. **播放中跟随**：订阅 `PlaybackController::positionChanged`，播放头前进时实时刷新窗口，新进入窗口的 pending 片段被触发，移出窗口的已排队片段被取消并回到待命状态；
3. **暂停/停止收敛**：暂停时 `epoch++` 抵消队列中未执行的触发，取消所有未启动的推理任务，**正在运行的当前片段让它自然完成**（对应"推完当前正在推理的片段就不继续"），完成后队列停止；
4. **epoch（代际）计数**：解决 `notifyNextPipeline` 链式 `QTimer::singleShot` 展开中途被暂停打断，仍会触发后续的串台问题——每次状态切换/调度版本递增，过期回调直接丢弃。

## 现状梳理（已读源码核实）

### 调度链

```
onPlaybackStatusChanged(Playing)      InferController.cpp:288-299
  → 收集所有 acousticInferStatus==Pending 的 pipeline
  → makePlaybackPriorityComparator 排序（档位 0=播放头所在/1=之后/2=之前/3=已消失）
  → notifyNextPipeline(...)          InferController.cpp:1008-1019
      → 逐个 QTimer::singleShot(0) 链式调用 pipeline->notifyPlaybackStarted()
  → InferPipeline::notifyPlaybackStarted()   InferPipeline.cpp:144-146
      → emit playbackStarted()
  → probeAcousticCacheState / awaitingInferAcousticState
      → playbackStarted 转换到 inferAcousticState
  → BaseInferState::onRunningInferenceStateEntered   BaseInferState.cpp:48-68
      → piece.acousticInferStatus = Running
      → createTask() → addTaskToController() → TaskQueue::add()
```

### 任务执行

- `TaskQueue<T>`（`src/libs/Tasking/TaskQueue.h`）：单线程串行，`current` = 正在运行的 task，`pending` = 排队未启动的 task；每次 `runNext()` 前按优先级比较器对 `pending` 排序。
- `cancelIf(pred)` 同时取消 pending 中的（直接 `disposePendingTask` 删除）和 current（`terminateTask` + 等待 finished 清理）。**删 current 前的清理链依赖 finished 信号**。
- `finishCurrentInferAcousticTask` → `onCurrentFinished` → 自动 `runNext()`。

### 现状关键缺陷

1. **播放时触发范围无上限**：`onPlaybackStatusChanged(Playing)` 对**所有** pending pipeline 触发，整首歌的片段一次性入队。TaskQueue 串行跑完所有片段——即使在播放中途暂停。
2. **暂停无钩子**：`InferController.cpp:288-299` 只处理 `Playing`，`Paused`/`Stopped` 分支不存在，链式 singleShot 已把后续 pipeline 全部触发。
3. **优先级档位而非常量显示**：当前只用"距播放头距离"排序，没用前瞻窗口（这些概念已经存在但没和暂停/取消挂钩）。

## 方案 B 设计

### 1. 前瞻窗口定义

- **窗口大小**：以 tick 为单位的前瞻常量（建议 `PlaybackLookaheadTicks`，例如 ~6000 tick ≈ 一小节到两小节视曲速而定）。需要换算成固定秒数时用曲速 map 换算（`Player` 单元已有 tick 概念，参考 `timeline` 换算调用）。**更稳妥**：给一个以「秒」为单位的窗口 + TickConverter 换算成 tick，窗口 = `pos` + `lookaheadSeconds × ticksPerSecond`。秒数更直观也更稳定（不受曲速变化影响），建议做成选项或常量，`InferenceOption` 现有配置结构可扩展。
- 判定：某个 pending 片段 `pieceGlobalRange` 满足 `range.start < pos + windowTicks && range.end > pos`（和窗口有交集）→ 触发推理。
- **实现定稿（2026-08-16）：** 窗口单位为「秒」（默认 20s，可在推理设置页调整），配置项 `InferenceOption::playbackLookaheadSeconds`，运行时经 `appModel->timeline().secToTick()` 换算成 tick 再判定。起因：推理引擎以秒计时，tick 常量在慢速曲速下会退化为覆盖整首的窗口；秒更直观、不受曲速变化影响。初始曾用 `kPlaybackLookaheadTicks = 4800` 常量，后按用户要求改为秒。

### 2. epoch 代数（防串台的核心）

`InferControllerPrivate` 增加 `int m_playbackEpoch = 0;`：

- 每次 `Playing`、`Paused`、`Stopped`、窗口刷新 都自增/使用新 epoch 快照；
- `notifyNextPipeline` 的 `QTimer::singleShot(0, _)` 回调携带创建时的 epoch，执行前检查 `epoch == m_playbackEpoch`，不匹配直接 return（丢弃过期回调）；
- 同理，pipeline 处于 `awaitingInferAcousticState`，排队但未启动，收到 epoch 变更后自动取消（下详）。

### 3. 播放中动态跟随（位置订阅）

新增订阅 `PlaybackController::positionChanged`（仅播放中启用）：

```
[节流/无损直接调用] onPlaybackPositionChanged(tick):
    if 状态 != Playing: return
    refreshPlaybackWindow(pos)
```

`refreshPlaybackWindow(pos)` 逻辑：
- 遍历所有 pipeline，收集满足"和窗口 [pos, pos+windowTicks] 相交"且 `acousticInferStatus == Pending` 的 → 按播放优先级排序触发（复用现比较器，但触发范围限定在窗口内）；
- 收集"已入队/已运行但完全移出窗口"的 pipeline → `cancelInferAcousticTask(taskId)`（保留当前正在运行的 task 不受影响，`cancelIf` 的逻辑只删未运行的 + 对 current 做终止——**但暂停时我们希望 current 跑完**，所以暂停场景**不能** 用 `cancelIf` 终止 current，要用只删 pending 的方式，见下）。

### 4. 暂停/停止收敛

`onPlaybackStatusChanged` 增加 `Paused`/`Stopped` 分支：

1. `m_playbackEpoch++`（使排队中的 singleShot 和已入队但未运行的触发全部失效）；
2. **只取消 pending（未启动）的推理任务，current 保留自然跑完**。TaskQueue 目前没有"只删 pending 不碰 current"的公开接口——`disposePendingTasks()` 存在但不会自动通知各 pipeline？其实每个 pipeline 持自己的 task 指针，若 TaskQueue 直接删 pending task 而 pipeline 层不知情，运行中的 `currentTask` 指针悬垂。**因此收敛不能在队列层静默删**，必须走 pipeline 信号：新增 `InferPipeline::suspendPendingPlayback()` 或复用 `playbackStarted` 的反向信号。

推荐做法（最小侵入且安全）：
- 在 `InferPipeline` 增加信号 `playbackSuspended()`，转换：`awaitingInferAcousticState / awaiting 状态 / 排队未运行的` → 回到 `probeAcousticCacheState`（重新探测，若有缓存则更新音频）；
- `InferControllerPrivate::onPlaybackStatusChanged(Paused/Stopped)` 对每个 pipeline 发出 `notifyPlaybackSuspended()`（新方法），同时对各 TaskQueue **不调用 `cancelIf` 终止 current**；
- 对"已进入 inferAcoustic 状态但队列里还没轮到"的片段：通过 `BaseInferState`（其 currentTask 在队列 pending 或 current）新增"调度版本"检查——任务真正开始前若播放已暂停且状态被重置，直接丢弃任务结果。

衡量过**两种收敛方式**：

| 方式 | 做法 | 风险 |
|---|---|---|
| A. 队列层 `cancelAll` | 调 `m_inferAcousticTasks.disposePendingTasks()` + `cancelIf(current)` | pending task 被删除后各 pipeline 的 `BaseInferState::currentTask` 指针悬垂；且会终止 current，与"保留当前跑完"冲突 |
| B. pipeline 信号层 | 新增 `playbackSuspended` 信号，等待/排队状态迁移回 probe+等待 | 侵入 `InferPipeline` 状态机，但状态天然回环，安全 |

选 B：语义正确、状态回环复用现成的 `probeAcousticCacheState`；代价是多一套过渡。为降低改动量，可直接在 `awaitingInferAcousticState` 依赖 playbackStarted 触发这个基本结构上增加"取消触发"路径。

### 5. 与 autoStartInfer 的交互

- `autoStartInfer = true`（默认）：保持现状，推理任务随片段就绪直接跑（不走播放窗口控制）；播放触发仍补上暂停收敛，避免暂停后排队尾大。
- `autoStartInfer = false`（本次优化对象）：播放窗口动态调度 + 暂停收敛。

### 6. 缓存配合

`probeAcousticCacheState` 已保证缓存命中的片段不推理直接出声。暂停收敛让被打断片段回探缓存：若该片段推理完成过则立刻出声；未完成则等待下次播放，语义正确且不浪费——**推理过的音频结果会写缓存**，所以"推完当前"的成果可复用。

### 7. 边界与陷阱

- `TaskQueue::cancelIf` 会终止 current，暂停**不能**用它；需仅处置 pending。可给 `TaskQueue` 增加 public `disposePending()` 或 `cancelIfPending()` 只删 pending、不碰 current —— 但 pipeline 的 `BaseInferState::currentTask` 需同步（它们自己管理，因此在 pipeline 信号层做更安全）。
- `QTimer::singleShot(0)` 链——暂停打断串台 → 用 epoch 判定丢弃。
- 播放头不在任何片段范围内（空白处）→ 触发范围为空，什么都不做，正常。
- 曲速变化 → `handleTempoChanged` 已重建 pipeline，窗口重新计算即可。
- 循环播放：窗口在 loop 范围内动态跟随即可，无需特殊处理。

### 8. 改动清单

| 文件 | 改动 |
|---|---|
| `InferController_p.h` | 增加 `m_playbackEpoch`、常量 `kPlaybackLookaheadTicks`（或秒）、`onPlaybackPositionChanged` slot 声明 |
| `InferController.cpp` | 重写 `onPlaybackStatusChanged`（增加 Paused/Stopped 分支 + `notifyPlaybackSuspended`）；新增 `onPlaybackPositionChanged` + `refreshPlaybackWindow`；`onInferOptionChanged` 中订阅/解订阅 positionChanged（跟随开关） |
| `InferPipeline.{h,cpp}` | 新增 `playbackSuspended` 信号 + `notifyPlaybackSuspended()` 方法；`initAcousticTransitions` / `initPlaybackReadyTransitions` 等处加回退过渡 |
| `States/AwaitingInferAcousticState.{h,cpp}` | onExit 无需做（状态迁移足够） |
| `States/ProbeAcousticCacheState.cpp` | 可选：暂停收敛回探时可复用 |

> 是否将前瞻窗口秒数做成 `InferenceOption`（设置页新增一行）仍在讨论：先常量，用户认可后加选项。

## 验收标准

1. `autoStartInfer=false`，选中未推理片段按播放 → 只推理窗口内（而非整首后续全部）；
2. 播放中暂停 → 当前正在推理的片段跑完，其余排队 fragment 全部回收，GPU 占用回落；
3. 再次播放从新播放头开始 → 窗口内重新触发；
4. 暂停后立刻再播放，不串台（epoch 生效）；
5. `autoStartInfer=true` 行为不变：追加暂停收敛，不要破坏现状。
