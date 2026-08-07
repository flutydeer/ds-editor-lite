# 推理期间 UI 性能优化 — 设计文档

> 状态：✅ 实施完成（5 项优化 + 综合修复 `656a5f42`；2026-08-06 复核）
> 本文由原 `plans/` 实施计划整理而来；分阶段实施记录与提交清单已移除。

## 问题描述

打开含有大量音符的工程后，大量片段同时推理时，界面滚动和缩放不流畅。每个片段完成一步状态转换都会在主线程触发大量连锁反应。

## 优化 1：推理任务准备移至工作线程

`BaseInferState` 新增 `m_preparationEpoch` 取消令牌；`prepareTaskInput()` + `createTask()` 通过 `QtConcurrent::run()` 移至工作线程。

**效果**：解决按下播放后所有 pipeline 同时进入 `InferAcousticState` 导致的主线程阻塞。

## 优化 2：声学缓存探测移出 UI 线程

`InferPipeline` 新增异步探测路径，`ProbeAcousticCacheState` 为独立状态，在工作线程执行声学缓存探测。

**效果**：进一步减少推理状态转换对主线程事件循环的阻塞。

## 优化 3：resetState / buildTaskInput 拆分（线程安全契约）

`prepareTaskInput()` 整体移至工作线程后，`resetPhoneOffset` / `resetPitch` / `resetVariance` / `resetAcoustic` 等 reset 函数会在工作线程修改 QObject 状态并发射信号，这是线程安全问题。因此拆分为：

| 阶段 | 线程 | 内容 |
|---|---|---|
| `resetState()` | 主线程 | reset 与信号发射 |
| `buildTaskInput()` | 工作线程 | 纯计算 |

应用于 Duration / Pitch / Variance / Acoustic 四个状态（`BaseInferState.h/cpp` 及四个派生 State 同步拆分）。同时修复 `DEVICE_LOCKER` 导致的主线程阻塞。

## 优化 4：AudioDevice 锁粒度

`TrackInferenceHandler::handleInferPieceStatusChange` 中**只在真正需要修改音频链时才获取 `DEVICE_LOCKER`**（Success/Failed/Reset）：

1. `Running`/`Pending` 状态下不再拿锁（首次推理时 piece 从未 determined，直接跳过）
2. `Success` 分支：文件 I/O（getFormatLoad + AudioFormatInputSource + makeBufferable）在锁外执行，只在纯内存赋值时才加锁

配套：`DspxInferencePieceContext.h/cpp` 新增 `determineWithSources()` 方法。

## 优化 5：TimelineView 推理状态重绘节流

`TimelineView` 新增 `QTimer m_pieceUpdateThrottle` 成员；`statusChanged`/`stateChanged` 信号改为触发 **16ms 单次定时器**，把跨事件循环迭代的多次 `update()` 合并为一次 `paintEvent`。

**背景**：Qt 的 `update()` 本身会合并同一事件循环迭代内的多次调用，但推理状态变更通过 `QTimer::singleShot(0)` 分散在不同迭代中，Qt 的自动合并无法跨迭代生效，因此仍需显式节流。

## 综合修复：播放中 UI 冻结（656a5f42）

**现象**：同时编辑大量片段的参数后开始播放，UI 在开始几秒内正常响应，但过几秒后无法响应（UI 仍在更新），直到所有片段推理完成。

**机制分析**（三轮优化后问题从"一按播放就冻结"改善为"前几秒可响应然后冻结"）：

- UI 仍在刷新（不是死锁），但鼠标键盘事件不被处理 → **事件循环饥饿**：`QTimer::singleShot(0)` 和 `QMetaObject::invokeMethod(Qt::QueuedConnection)` 都通过 posted events 机制，Windows 下 `WM_QT_SENDPOSTEDEVENTS` 会批量处理所有 posted events 后才返回 OS 消息泵；当大量任务完成信号 + 状态转换信号密集到达时，事件队列被推理相关事件填满，不再处理 `WM_MOUSEMOVE` 等鼠标消息
- 声学任务完成后的事件级联：每个 `handleTaskFinished` 触发 `runNext()` → sort → 状态转换 → `statusChanged(Success)` → determine，N 个片段密集完成时事件洪流淹没主线程
- `resetState` 的级联开销：每次 resetParam 都调用 `updateOriginalParam` 深拷贝所有片段曲线 + 发信号，发生在每个新任务开始前的主线程上

**修复内容**（10 个文件，281 行新增）：

- **播放位置更新节流**：`TimeGraphicsView` / `TimelineView` 的播放位置 setter 从"直接赋值 + update()"改为 `m_pendingPosition` + **33ms 单次定时器**（`m_positionThrottle`），自动翻页逻辑同样走节流路径
- **WaveformPainter pixmap 缓存修复**：缓存改为按 `devicePixelRatio` + 像素尺寸校验重建（修复缓存被 setter 清空后 `paint()` 画空白的问题）
- **CommonParamEditorView 曲线删除泄漏修复**：移除曲线时同时 `delete` 曲线对象
- 提交附带 MainWindow 的临时诊断事件过滤器（`EventDiagFilter`，测量事件间隙与 paint 阻塞时间，标注 "remove after investigation"）——**至今仍保留在 `MainWindow.cpp` 中，属于待清理的遗留代码**

**效果**：播放中冻结问题解决；剩余待优化项目由本次综合修复覆盖，不再单独跟踪。

## 设计约束（主线程 / 工作线程职责划分）

| 线程 | 允许做什么 |
|---|---|
| 主线程 | reset 状态、发射信号、修改 QObject、UI 更新 |
| 工作线程 | 纯计算：构建任务输入、缓存探测、I/O 读取 |

任何把"修改 QObject 状态 / 发信号"移入工作线程的优化都必须先拆分出主线程阶段（参照优化 3 的 `resetState` / `buildTaskInput` 模式）。

## 评估过但不再单独跟踪的优化项

以下项目在报告时点评估有效，冻结问题由 `656a5f42` 综合修复覆盖后不再单独跟踪；如未来性能回归可参考：

| 项目 | 收益 | 风险 | 工作量 | 改动范围 |
|---|---|---|---|---|
| mergeCurves 每帧重新分配内存（paint 中深拷贝所有 original + edited 曲线，每条 1500-3000 int 值） | 高 | 低 | 小（~20-30 行） | `CommonParamEditorView.h/cpp` |
| updateOriginalParam 信号风暴（`updateVariance` 完成时 5 参数各深拷贝 + 发信号；20 片段 × 5 参数 = 100 次深拷贝） | 高 | 低 | 中等 | `InferControllerHelper.h/cpp` + `SingingClip.h/cpp` |
| TaskQueue 每次出队全量排序（`std::sort` + 比较器双重 O(n) 扫描） | 中 | 低 | 中等 | `TaskQueue.h` + `Queue.h` + `InferController.cpp` |
| notifyNoteChanged 触发 PhonemeView 全量重建（`buildPhonemeList` 双向链表，增量更新需仔细处理） | 中 | 中 | 中等 | `PhonemeView.cpp` |

## 关键文件

| 模块 | 文件 |
|---|---|
| 推理状态机 | `src/app/Modules/Inference/States/BaseInferState.h/.cpp`、`ProbeAcousticCacheState.h/.cpp`、`InferDurationState`、`InferPitchState`、`InferVarianceState`、`InferAcousticState` |
| 推理编排 | `src/app/Modules/Inference/InferPipeline.cpp/h`、`TrackInferenceHandler.cpp`、`DspxInferencePieceContext.h/.cpp` |
| 视图 | `src/app/UI/Views/Common/TimelineView.h/.cpp`、`TimeGraphicsView.h/.cpp`、`src/app/UI/Views/ClipEditor/CommonParamEditorView.cpp`、`src/app/UI/Views/ClipEditor/PianoRoll/PhonemeView.cpp`、`src/app/UI/Utils/WaveformPainter.h/.cpp` |
| 诊断 | `src/app/UI/Window/MainWindow.cpp`（遗留的临时诊断过滤器） |
