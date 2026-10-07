# 文档换代的状态重置契约

> 状态：已实现。本文是宿主侧"跨工程不通用的状态"在文档换代时的重置契约。
> 实现位置：`Automation/DocumentAutomationFacade.cpp` 的 `replaceDocument()` 事务。

## 1. 背景

打开或新建工程会换一整个文档代次，这时有一批宿主状态必须回到默认值。此前没有统一入口，
各自监听 `AppModel::modelChanged` 自己清，于是漏了一批，还踩了一个时序陷阱。

实测症状（每条都对应一个真实缺口）：

| 症状 | 根因 |
|---|---|
| 播放中打开另一个工程，音频继续播、播放头不回 0 | 重置走自动化命令，而 `modelChanged` 发出时 session 处于 `Replacing`，命令被拒 |
| 底部编辑页停在"请选择歌声片段"，不显示新工程的第一个片段 | `activeClipId` 未清，`activateFirstClip` 撞上旧 id 被 `setActiveClip` 的等值早退短路 |
| 新工程里出现旧工程的片段选中高亮 | `TrackEditorView::onModelChanged` 拿 `appStatus->selectedClips` 重建 |
| 旧工程的轨道选中高亮残留 | `selectedTrackIndex` 无人清 |
| 编辑预览、钢琴卷帘可见矩形、轨道视口与缩放、工具态残留 | 无人清 |
| 播放头重置后仍停在 1 个音频 block 的位置 | 引擎在换代后仍上报 block 对齐位置，写回控制器把归零顶掉 |

## 2. 根因：`modelChanged` 发出时机落在事务中途

`DocumentAutomationFacade::replaceDocument()` 的顺序是：

```
session.setLifecycleState(Replacing)
beforeReplaceGeneration(prevDocId)      // 丢弃旧代次的导出/抽取任务
history->reset(Saved)
model->replaceProject(...)              // 内部 emit modelChanged()
applyLoopSettings(draft.loopSettings)
session.replaceGeneration(...)          // 到这里才回到 Active，换新 documentId
notifyCommitted(...)                    // 之后才是 activateFirstClip
```

`AppModel::modelChanged` 在 `replaceProject` 内部同步发出，此时 session 是 `Replacing`，
而 `AutomationDispatcher::resolveDocumentCommand()` 的第一道闸就是
`lifecycleState() != Active → documentBusy`。于是任何"监听 `modelChanged` 然后发自动化命令"的
重置都是静默失败的（返回值没人看）。旧实现在 `PlaybackController` 里正是这么写的。

由此得出一条硬约束：**换代事务期间的监听者只准做宿主直调，不准走 dispatcher。**

## 3. 契约

### 3.1 时机与顺序不变量

`DocumentRuntimeServices::resetDocumentScopedState` 必须在 `replaceDocument` 中满足：

1. 在 `beforeReplaceGeneration` **之后**，旧代次的异步任务已丢弃，不会有人回写半个重置的状态。
2. 在 `history->reset` 与 `model->replaceProject` **之前**，视图随后的重建第一次看到的就已经是干净状态，
   不需要第二轮信号回灌。
3. 只做宿主直调，不得发自动化命令（见 §2）。
4. 回调可缺省。headless 宿主或测试未注册时静默跳过，换代本身照常完成。

### 3.2 职责划分

| 层 | 承担者 | 做什么 |
|---|---|---|
| 事务 | `DocumentAutomationFacade::replaceDocument` | 在固定时机回调一次 |
| 宿主聚合 | `AppContext::resetDocumentScopedState` | 按序调播放、状态容器、编辑会话、编辑器视图 |
| 音频引擎 | `AudioContext::resetDocumentScopedState` | pause transport、seek 0、清 hold 锚点与 `m_lastStatus`、屏蔽引擎位置上报 |
| 播放 | `PlaybackController::resetForDocumentReplacement` | 先归零 lastPosition，再 stop，再 position=0，停视觉定时器，清 buffering |
| 选中与工具态 | `AppStatus::resetDocumentScopedState` | 清选中、activeClip、编辑对象、编辑预览、可见矩形、可编辑长度，工具态回默认 |
| 视图 | `EditorViewController::resetDocumentScopedState` → `IEditorView::resetDocumentScopedViewState` | 轨道面板视口回原点与 1:1，钢琴卷帘缩放回 1:1 与选择模式，参数编辑器工具态回默认 |

顺序细节一：播放重置必须**先**归零 `lastPosition`（`ReturnToStart` 策略下 transport 暂停时会用
`lastPosition` 重新定位），**再** stop，**最后** position=0，否则播放头会被拉回旧工程的位置。

顺序细节二：引擎复位排在控制器复位**之前**，且抑制标志要在 pause 与 seek **之前**置位。原因见下节。

### 3.3 状态分类

换代时重置：

- 播放：状态、播放头位置、上次位置、引擎缓冲标志、视觉锚点
- 选中：`selectedTrackIndex`、`activeClipId`、`selectedClips`/`selectedNotes` 与两个 primary id、`currentEditObject`
- 编辑中：钢琴卷帘的编辑与擦除预览、`EditSessionManager` 的活动会话
- 派生值：`pianoRollVisibleRect`、`projectEditableLength`
- 工具态：钢琴卷帘量化开关与值、编辑模式、参数编辑器前景/背景/编辑模式/值视口、自动翻页开关
- 视图：轨道面板视口与缩放、钢琴卷帘缩放

换代时保留：

- `loopSettings`（属于工程数据，由 `applyLoopSettings` 按新文档写入）
- 面板折叠状态、底部面板当前页、活动面板与区域（布局偏好，跨工程通用）
- 自动翻页可用性（由视图派生，不是用户选择）
- 撤销历史（事务自己 `history->reset`）、导出与抽取代次、推理管线与缓存注册
- 剪贴板（跨工程粘贴是特性）

### 3.4 引擎侧：位置上报必须一起复位

transport 位置是**双向**同步的。`AudioContext` 订阅 `TransportAudioSource::positionAboutToChange`，
把引擎报告的 block 对齐位置写回 `playbackController->setPosition()`。这条链路有两个后果：

1. 暂停与 seek 本身都会换来一条 block 对齐的上报。把 transport seek 到 0 之后，引擎报回来的是
   「0 之后一个 block」的位置（48kHz / 2048 samples / 120BPM 下约 40.96 tick），不是 0。
2. 换代后文档已恢复 `Active`，这条写回不再被 dispatcher 拒绝，于是它把刚刚归零的播放头推走。
   打开工程比新建更明显，因为引擎还会先补发一批换代前的陈旧预读位置。

实测（探针日志，新建工程与打开工程各一次）：控制器归零与 `transport()->setPosition(0)` 都成功
（上报过 `sample=0`），随后引擎仍报到 `sample=2048`，换算成 40.96 tick —— 与用户看到的
`001:01:040` 完全一致。

因此引擎侧必须参与换代：`AudioContext::resetDocumentScopedState()` 置 `m_suppressTransportPositionReports`
之后 pause 并 seek 0，抑制期间丢弃全部引擎上报，直到播放重新开始时解除抑制，并把引擎重新同步到
控制器持有的位置（抑制期间引擎可能停在陈旧的 block 边界上）。

### 3.5 不适用的情况

- **导入不是换代**。`commitImportedDocument` 是追加轨道，不重置任何文档级状态，只把新导入的轨道激活。
- **保存不换代**。`saveDocument` 不触发本契约。
- **未注册回调**。`m_services.resetDocumentScopedState` 为空时事务照常提交。

## 4. 新增"跨工程不通用"的状态时怎么做

1. 判断归属：这条状态是否应该在新文档里回到默认值。会的话继续，不会就什么都不用做。
2. 写进最近的单一所有者，不要新建监听：
   - 播放类 → `PlaybackController::resetForDocumentReplacement`
   - 音频引擎类（transport、预读、音频图）→ `AudioContext::resetDocumentScopedState`
   - 选中或工具类 → `AppStatus::resetDocumentScopedState`
   - 视图类 → `IEditorView::resetDocumentScopedViewState` 的实现
3. 用 `Property` 赋值而不是绕过信号，视图靠这些信号更新。
4. 在 `src/tests/TestDocumentScopedStateReset/main.cpp` 加一条断言（字段级），并在需要时补一条信号计数断言。
5. 若新增的是视图接口方法，记得补两个测试替身（`TestEditorViewController`、`TestUndoRedoController` 的 `FakeEditorView`）。

## 5. 验证

- `TestAutomationDocumentLifecycle` 的 `AFC-DOC-LIFECYCLE-011`：断言事务内的调用顺序恰为
  `generation → reset → model → loop → commit`（用 `AppModel::modelChanged` 当模型换装的证人），
  新建与打开同序，导入不得重置，回调缺省时仍能换届。
- `TestDocumentScopedStateReset`：断言字段级默认值与信号发布次数，并覆盖幂等与保留项。沿着
  "新增状态时怎么做"的第 2 步加断言，这条测试就是字段清单的守卫。
- 新断言必须做反向验证：临时注掉修复重跑，断言必须 FAIL，否则它是死测试。
- 播放头归零属于运行期行为，单元测试覆盖不到（需要音频设备）。做法是在 `AudioContext` 的
  `positionAboutToChange` 回调、`handlePlaybackPositionChanged`、`handlePlaybackStatusChanged`
  以及 `PlaybackController::applyPosition` 上挂临时 `[RESETPROBE]` 探针，跑一次「播放中换代」，
  用日志实锤归零与上报的先后顺序。定位靠探针，回归靠下面两条常驻日志：
  `AudioContext: dropped transport report ...` 与 `AudioContext: resynced the engine to ...`。

## 6. 相关文件

| 文件 | 角色 |
|---|---|
| `src/app/Automation/DocumentAutomationFacade.{h,cpp}` | 事务与回调时机 |
| `src/app/AppContext.{h,cpp}` | 宿主聚合入口 |
| `src/app/Controller/PlaybackController.{h,cpp}` | 播放状态重置 |
| `src/app/Modules/Audio/AudioContext.{h,cpp}` | 引擎（transport）复位与位置上报抑制 |
| `src/app/Model/AppStatus/AppStatus.{h,cpp}` | 选中与工具态重置 |
| `src/app/Controller/EditorViewController.{h,cpp}` | 视图重置入口 |
| `src/app/Interface/IEditorView.h` | 视图重置接缝 |
| `src/app/UI/Window/MainWindow.{h,cpp}` | 轨道面板视口重置 |
| `src/app/UI/Views/ClipEditor/ClipEditorView.{h,cpp}` | 钢琴卷帘缩放与编辑模式重置 |
| `src/app/UI/Views/ClipEditor/ParamEditor/ParamEditorView.{h,cpp}` | 参数编辑器工具态重置 |
| `src/tests/TestAutomationDocumentLifecycle/main.cpp` | 事务顺序回归 |
| `src/tests/TestDocumentScopedStateReset/main.cpp` | 字段清单回归 |
