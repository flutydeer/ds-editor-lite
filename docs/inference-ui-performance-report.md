# 推理期间 UI 性能优化报告

## 问题描述

打开含有大量音符的工程后，大量片段同时推理时，界面滚动和缩放不流畅。每个片段完成一步状态转换都会在主线程触发大量连锁反应。

## 已完成的优化

### ✅ 将推理任务准备移至工作线程

**提交：** `3e5a197e` Offload inference task preparation to worker thread

**修改文件：**
- `States/BaseInferState.h` — 新增 `m_preparationEpoch` 取消令牌
- `States/BaseInferState.cpp` — `prepareTaskInput()` + `createTask()` 通过 `QtConcurrent::run()` 移至工作线程

**效果：** 解决按下播放后所有 pipeline 同时进入 InferAcousticState 导致的主线程阻塞。

### ✅ 修复 worker 线程 mutate 模型（线程安全）

**提交：** `886cb2f0` Fix main thread blocking from DEVICE_LOCKER and thread-unsafe resetState

**修改文件：**
- `BaseInferState.h/cpp` — 将 `prepareTaskInput()` 拆分为 `resetState()`（主线程执行，含 reset 和信号发射）+ `buildTaskInput()`（工作线程执行，纯计算）
- `InferDurationState.h/cpp` — 拆分实现
- `InferPitchState.h/cpp` — 拆分实现
- `InferVarianceState.h/cpp` — 拆分实现
- `InferAcousticState.h/cpp` — 拆分实现

**效果：** 修复了 `prepareTaskInput()` 整体移至工作线程后，`resetPhoneOffset`/`resetPitch`/`resetVariance` 等 reset 函数在工作线程修改 QObject 状态和发射信号的线程安全问题。

### ✅ 减小 AudioDevice 锁粒度

**提交：** `886cb2f0`

**修改文件：**
- `TrackInferenceHandler.cpp` — `handleInferPieceStatusChange` 中只在真正需要修改音频链时才获取 `DEVICE_LOCKER`（Success/Failed/Reset）
- `DspxInferencePieceContext.h/cpp` — 新增 `determineWithSources()` 方法

**效果：** 

1. `Running`/`Pending` 状态下不再拿锁（首次推理时 piece 从未 determined，直接跳过）
2. `Success` 分支：文件 I/O（getFormatLoad + AudioFormatInputSource + makeBufferable）在锁外执行，只在纯内存赋值时才加锁

### ✅ TimelineView 推理状态重绘节流

**提交：** `1788341a` Throttle TimelineView repaints from inference state changes

**修改文件：**
- `TimelineView.h` — 新增 `QTimer m_pieceUpdateThrottle` 成员
- `TimelineView.cpp` — `statusChanged`/`stateChanged` 信号改为触发 16ms 单次定时器

**效果：** 跨事件循环迭代的多次 `update()` 合并为一次 `paintEvent`。

**备注：** Qt 的 `update()` 本身会合并同一事件循环迭代内的多次调用，但推理状态变更通过 `QTimer::singleShot(0)` 分散在不同迭代中，Qt 的自动合并无法跨迭代生效，因此仍需显式节流。

---

## 仍存在的问题

### 编辑参数后播放，前几秒可响应随后冻结

**现象：** 同时编辑大量片段的参数后开始播放，UI 在开始几秒内正常响应鼠标操作，但过几秒后无法响应（UI 仍在更新），直到所有片段推理完成。

**已知信息：**

1. 经过三轮优化后（async prep + resetState 拆分 + DEVICE_LOCKER 缩小），问题从"一按播放就冻结"改善为"前几秒可响应然后冻结"
2. UI 仍在刷新（说明不是死锁），但鼠标键盘事件不被处理（事件循环饥饿）
3. 冻结持续到所有推理完成，说明与推理任务完成过程强相关

**待排查方向：**

1. **事件循环饥饿** — `QTimer::singleShot(0)` 和 `QMetaObject::invokeMethod(Qt::QueuedConnection)` 都通过 Qt 的 posted events 机制，Windows 下 `WM_QT_SENDPOSTEDEVENTS` 会批量处理所有 posted events 后才返回 OS 消息泵。当大量任务完成信号 + 状态转换信号密集到达时，事件队列被推理相关事件填满，不再处理 WM_MOUSEMOVE 等鼠标消息。

2. **声学任务完成后的事件级联** — 每个 `handleTaskFinished` 触发 `runNext()` → sort → 状态转换 → `statusChanged(Success)` → `DEVICE_LOCKER` determine（当前已缩小到锁外 I/O + 锁内赋值，但锁内赋值仍有短暂阻塞），N 个片段密集完成时事件洪流淹没主线程。

3. **`resetState` 的级联开销** — Duration/Pitch/Variance 的 `resetState` 会级联调用下游 reset（resetPhoneOffset → resetPitch → resetVariance → resetAcoustic），每次 resetParam 都调用 `updateOriginalParam` 深拷贝所有片段曲线 + 发信号。这发生在每个新任务开始前的主线程上。

4. **需要 profiling** — 需使用 `QElapsedTimer` 在 `handleTaskFinished`、`runNext`、状态转换 `onEntry` 中插入计时日志，确定冻结期间主线程时间花在哪个环节。

---

## 待优化项目（按性价比排序）

### 1. mergeCurves 每帧重新分配内存

| 维度 | 评估 |
|------|------|
| **收益** | 高。直接影响滚动和缩放流畅度。每帧深拷贝所有 original + edited 曲线（每条 1500-3000 int 值），合并后立即释放。 |
| **风险** | 低。`CommonParamEditorView` 已将曲线存为成员，4 个缓存失效点明确。 |
| **工作量** | 小。~20-30 行代码。 |
| **改动范围** | `CommonParamEditorView.h/cpp` |

**关键文件：**
- `CommonParamEditorView.cpp:208` — paint 中调用 mergeCurves
- `AppModelUtils.cpp:61` — mergeCurves 实现

---

### 2. updateOriginalParam 信号风暴

| 维度 | 评估 |
|------|------|
| **收益** | 高。`updateVariance` 完成时对 5 个参数各调用一次 `updateOriginalParam`，每次深拷贝所有片段曲线 + 发信号。20 片段 × 5 参数 = 100 次深拷贝。 |
| **风险** | 低。`paramChanged(Original)` 的接收者不依赖信号同步性。 |
| **工作量** | 中等。需拆分 `updateParam` 中"设曲线"和"刷新+发信号"两步。 |
| **改动范围** | `InferControllerHelper.h/cpp` + `SingingClip.h/cpp` |

**关键文件：**
- `SingingClip.cpp:154` — `updateOriginalParam` 实现
- `InferControllerHelper.cpp:250-289` — `updateParam`/`updateVariance`

---

### 3. TaskQueue 每次出队全量排序

| 维度 | 评估 |
|------|------|
| **收益** | 中。每次出队 `std::sort` + 比较器双重 O(n) 扫描。 |
| **风险** | 低。仅主线程操作。 |
| **工作量** | 中等。改用插入排序或预计算 sortKey。 |
| **改动范围** | `TaskQueue.h` + `Queue.h` + `InferController.cpp` |

---

### 4. notifyNoteChanged 触发 PhonemeView 全量重建

| 维度 | 评估 |
|------|------|
| **收益** | 中。每片段 duration 完成时触发一次全量重建。 |
| **风险** | 中。`buildPhonemeList` 维护双向链表，增量更新需仔细处理。 |
| **工作量** | 中等。仅更新受影响的 PhonemeViewModel 属性。 |
| **改动范围** | `PhonemeView.cpp` |

---

## 性价比排序总结

| 排名 | 项目 | 收益 | 风险 | 工作量 | 性价比 |
|------|------|------|------|--------|--------|
| 1 | （已完成）TimelineView 重绘节流 | 高 | 极低 | 极小 | ★★★★★ |
| 2 | mergeCurves 每帧深拷贝 | 高 | 低 | 小 | ★★★★☆ |
| 3 | updateOriginalParam 信号风暴 | 高 | 低 | 中等 | ★★★☆☆ |
| 4 | TaskQueue 全量排序 | 中 | 低 | 中等 | ★★☆☆☆ |
| 5 | PhonemeView 全量重建 | 中 | 中 | 中等 | ★★☆☆☆ |
