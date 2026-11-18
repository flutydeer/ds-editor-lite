# 轨道追加槽（Append Slot）— 设计文档

> 状态：✅ 实施完成
> 本文描述底部虚拟追加槽的完整契约，包含新建轨道的三种入口、拖放落点规则，以及一次手势只产生一个历史项的约束。

## Overview

轨道窗底部有一条永远存在、**不属于 `AppModel`** 的虚拟行，叫追加槽（append slot）。它同时出现在两半：

- 左侧轨道列表：`TrackListView` 末尾一个 `Qt::NoItemFlags` 的占位行，itemWidget 是 `TrackAppendSlotView`。
- 右侧画布：竖向内容的最后一行，`trackCount + 1` 条的末尾。

起初它只承担「拖放文件的落点」。现在它是「在此新建轨道」的一等入口，三种入口语义一致：

| 入口 | 行为 |
|---|---|
| 画布双击追加槽 | 在工程末尾追加一条轨道，等价于轨道面板左上角的加号 |
| 拖放剪辑到追加槽 | 新建一条轨道，并把落进该行的剪辑搬进去（一个历史项） |
| 拖放 MIDI / 音频文件到追加槽 | 按导入批次新建轨道（既有能力，见 `docs/plans/midi-audio-drag-import.md`） |

追加槽本身仍然不可选中、不可拖动、不计入轨道索引，也不参与内部排序。

## 落点解析

两个后端各自提供同一个几何函数：

- `TracksGraphicsScene::dragRowAt(sceneY)`（legacy）
- `TracksRhiWidget::dragRowAt(viewportPosition)`（RHI）

规则：`sceneY < 0` 返回 -1，否则 `min(floor(sceneY / 行高), 轨道数)`。

也就是说**追加槽下方那片空白属于追加槽**。轨道少时内容高度达不到视口高度（`setEnsureContentFillsViewport(true, false)`，legacy 场景 `updateSceneRect` 取 `max(内容高, 视口高)`），追加槽只占顶部一行，其余是空白。用户往底部拖时多半落在空白里，因此必须把空白折叠到追加槽，否则表现为「没有反应」。

`trackIndexAt()` 保持原义（只命中真实轨道），双击建剪辑、右键菜单等继续用它。文件拖放专用的 `TrackDropSlot::Kind::Append` 同样覆盖追加槽与下方空白。

## 拖入剪辑：只借一行

规划层 `ClipSelectionDrag` 的刚性块 clamp 语义不变，只是调用方把 `Environment::rowCount` 传成 `真实轨道数 + 1`，于是最底部一条剪辑可以落到 `trackIndex == 轨道数` 这一行。

由此得到的契约：

- 单轨选区：剪辑直接进入追加行，提交时新建一条轨道。
- 多轨选区：整块最多下移一格，**只有最底部那条剪辑进入追加槽**。追加槽只有一行，这是刚性块模型的必然结果，不做「按选区高度建 N 条轨道」。
- 一次拖放最多新建**一条**轨道。

拖拽期间不写模型。预览靠 `ClipSelectionDrag::Placement::trackIndex` 等于轨道数实现：legacy 直接 `setTrackIndex`（颜色保留源轨道），RHI 在 `buildClipSnapshot` 里对越界的 `displayTrack` 回落到剪辑的真实轨道取色。

## 提交：一个历史项

一次手势 = 一个 `ActionSequence`，撤销后轨道与剪辑一起复原。

```
TrackController::onMoveClips
   ├─ trackIndex < 轨道数   → ClipMoveDto（既有路径）
   └─ trackIndex == 轨道数  → ClipStartDto（新轨组，取第一条剪辑的来源轨道做模板）
          ↓
ProjectAutomationFacade::moveClipsWithNewTrack
          ↓
MoveClipsWithNewTrackActions = InsertTrackAction + ClipActions::editClips
   ├─ execute：先插轨道，再搬剪辑
   └─ undo：先搬回剪辑，再删轨道
```

新轨组为空时仍然走原来的 `ProjectAutomationFacade::moveClips`，行为与改动前完全一致。

### 新建轨道的属性来源

模板取自被搬剪辑当前所在的轨道（新轨组第一条），按 `Automation::trackDraftDto` 复制后：

| 字段 | 取值 | 理由 |
|---|---|---|
| `singerInfo` / `speakerInfo` / `speakerMixData` | 复制源轨道 | 声音上下文一致时 `InferController` 的推理管线保持存活，不因换轨被拆掉重建 |
| `defaultLanguage` | 复制源轨道 | 同上，剪辑的默认语言随轨道 |
| `name` | `tr("New Track")` | 与菜单新建一致，不制造同名轨道 |
| `colorIndex` / `resolveColorIndex` | `0` / `true` | 走普通新建轨道的取色规则，让新轨道可区分 |
| `gain` / `pan` / `mute` / `solo` | 归零 | 这是一条新轨道，不是源轨道的克隆 |
| `clips` | 清空 | 剪辑由同一命令搬进来 |

## 关键文件

- `src/app/UI/Views/TrackEditor/TrackAppendSlotView.h/.cpp`（占位行的绘制与滚轮策略）
- `src/app/UI/Views/TrackEditor/TrackListView.h/.cpp`（占位行永远是最后一行，排除计数、选择与排序）
- `src/app/UI/Views/TrackEditor/TracksGraphicsScene.h/.cpp`（`dragRowAt`、`isAppendSlotAt`）
- `src/app/UI/Views/TrackEditor/TracksGraphicsView.cpp`（命中、预览、双击、文件落点）
- `src/app/UI/Views/TrackEditor/TracksRhiWidget.h/.cpp`（同上，RHI 版）
- `src/app/UI/Views/TrackEditor/ClipSelectionDrag.h/.cpp`（`Environment::rowCount`）
- `src/app/Controller/TrackController.cpp`（`onMoveClips` 分流）
- `src/app/Controller/Actions/AppModel/Clip/MoveClipsWithNewTrackActions.h/.cpp`
- `src/app/Automation/ProjectAutomationFacade.h/.cpp`（`moveClipsWithNewTrack`）

## 测试

- 规划层：`TestEditorInteraction` 的 `clipSelectionGroupMoveReachesAppendRow`（追加行落点、多轨块的越界预算）。
- 占位行：`TestApplicationGui` 的 `trackAppendSlotRemainsTheLastRow`（永远是最后一行、不计入轨道数、插入后仍在末尾）。
- 剪辑拖放：`TestApplicationGui` 的 `trackClipDropOnAppendRowCreatesTrack`，`TestNativeDesktop` 的 `rhiTrackClipDropOnAppendRowCreatesTrack`。
- 双击：`trackAppendRowDoubleClickCreatesTrack`、`rhiAppendRowDoubleClickCreatesTrack`。
- 下方空白：`droppingAudioFilesCommitsOneBatchToTheSelectedTracks` 与 `rhiTrackFileDropImportsAtTheChosenSlot` 的 `below-the-append-slot` 数据行。
