# 文档换代的状态重置契约

打开或新建工程时，[文档换代事务](../../src/app/Automation/DocumentAutomationFacade.cpp)重置当前工程的播放、选择、编辑和视图状态。重置在替换模型之前进行，使视图第一次读取新模型时就看到对应的默认状态。

## 1. 文档事务

`DocumentAutomationFacade::replaceDocument()` 的顺序是：

```
session.setLifecycleState(Replacing)
beforeReplaceGeneration(prevDocId)      // 丢弃旧代次的导出/抽取任务
resetDocumentScopedState()              // 清理当前工程的宿主状态
history->reset(Saved)
model->replaceProject(...)              // 内部 emit modelChanged()
applyLoopSettings(draft.loopSettings)
session.replaceGeneration(...)          // 到这里才回到 Active，换新 documentId
notifyCommitted(...)                    // 之后才是 activateFirstClip
```

`AppModel::modelChanged` 在 `replaceProject` 内同步发出，session 此时处于 `Replacing`。
此阶段文档命令会被自动化 dispatcher 以 `documentBusy` 拒绝。事务内部的重置和监听回调直接调用宿主逻辑。

## 2. 契约

### 2.1 时机与顺序不变量

`DocumentRuntimeServices::resetDocumentScopedState` 必须在 `replaceDocument` 中满足：

1. 在 `beforeReplaceGeneration` **之后**，旧代次的异步任务已丢弃，不会有人回写半个重置的状态。
2. 在 `history->reset` 与 `model->replaceProject` **之前**，视图随后的重建第一次看到的就已经是干净状态，
   不需要第二轮信号回灌。
3. 直接调用宿主逻辑（见第 1 节）。
4. 回调可缺省。headless 宿主或测试未注册时静默跳过，换代本身照常完成。

### 2.2 职责划分

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

### 2.3 状态分类

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

### 2.4 引擎位置上报

transport 位置是**双向**同步的。`AudioContext` 订阅 `TransportAudioSource::positionAboutToChange`，
把引擎报告的 block 对齐位置写回 `playbackController->setPosition()`。这条链路有两个后果：

1. 暂停与 seek 本身都会换来一条 block 对齐的上报。把 transport seek 到 0 之后，引擎报回来的是
   「0 之后一个 block」的位置（48kHz / 2048 samples / 120BPM 下约 40.96 tick），不是 0。
2. 换代后文档已恢复 `Active`，这条写回不再被 dispatcher 拒绝，于是它把刚刚归零的播放头推走。
   打开工程比新建更明显，因为引擎还会先补发一批换代前的陈旧预读位置。

因此引擎侧必须参与换代：`AudioContext::resetDocumentScopedState()` 置 `m_suppressTransportPositionReports`
之后 pause 并 seek 0，抑制期间丢弃全部引擎上报，直到播放重新开始时解除抑制，并把引擎重新同步到
控制器持有的位置（抑制期间引擎可能停在陈旧的 block 边界上）。

### 2.5 不适用的情况

- **导入不是换代**。`commitImportedDocument` 是追加轨道，不重置任何文档级状态，只把新导入的轨道激活。
- **保存不换代**。`saveDocument` 不触发本契约。
- **未注册回调**。`m_services.resetDocumentScopedState` 为空时事务照常提交。

## 3. 新增文档级状态

1. 判断归属：这条状态是否应该在新文档里回到默认值。会的话继续，不会就什么都不用做。
2. 写进最近的单一所有者，不要新建监听：
   - 播放类 → `PlaybackController::resetForDocumentReplacement`
   - 音频引擎类（transport、预读、音频图）→ `AudioContext::resetDocumentScopedState`
   - 选中或工具类 → `AppStatus::resetDocumentScopedState`
   - 视图类 → `IEditorView::resetDocumentScopedViewState` 的实现
3. 用 `Property` 赋值而不是绕过信号，视图靠这些信号更新。
4. 为新增状态补充可观察的换代结果，复用文档事务和编辑器状态的现有测试。

文档事务与编辑器状态的回归用例见 [`tst_lifecycle.cpp`](../../src/tests/TestDocumentIO/tst_lifecycle.cpp) 和 [`tst_document_state.cpp`](../../src/tests/TestEditorInteraction/tst_document_state.cpp)。使用真实音频设备确认播放行为时，在播放中切换工程，确认播放停止、播放头停在开头，随后从该位置重新开始播放。
