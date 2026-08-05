# Cross-Track Clip Movement — 设计文档

> 状态：✅ 实施完成（含后续修复：`ed9dcbd4` 钢琴卷帘位置、`a4126c5e` 推理管线保留）
> 本文是最终设计说明；修改文件清单等过程记录已移除。

## Overview

在轨道编辑器中支持将 clip 垂直拖动跨越轨道。此前 clip 只能在同一轨道内水平移动，跨轨移动实际是"删除 + 新建"，会丢失视图状态并错误拆除推理状态。

## Architecture

```
TracksGraphicsView (drag interaction)
    ↓
TrackController (dispatch: same-track vs cross-track)
    ↓
MoveClipToTrackAction (model: remove from old track, insert to new)
    ↓
Track::clipChanged signal (notifies view + audio/inference subsystems)
    ↓
TrackEditorView (view: preserve/reuse clip graphics item)
```

## Key Design Decisions

### MoveClipToTrackAction — 专用跨轨 action

`EditClipCommonPropertiesAction` 只编辑 clip 属性（start、length 等）且假定 clip 留在原轨道，跨轨移动需要独立 action。

执行顺序（execute / undo 相同）：

1. `oldTrack->removeClip(clip)`
2. 设置新 clip 属性（start、clipStart、length、clipLen）
3. `newTrack->insertClip(clip)`
4. `oldTrack->notifyClipChanged(Removed)` — 触发视图缓存 + 子系统 teardown
5. `newTrack->notifyClipChanged(Inserted)` — 触发视图复用 + 子系统 setup

步骤 3 在步骤 4 之前：`appModel->findClipById()` 在 `handleClipRemoved` 中能找到新轨道上的 clip，从而区分"移动"与"删除"。

### 视图保留 — 不销毁重建 clip 图形

`notifyClipChanged(Removed)` 触发时，`TrackEditorView::onClipRemoved` 把 `AbstractClipView` 缓存到 `m_pendingRemoveClipViews` 而不是销毁；后续 insert 复用它。`QTimer::singleShot(0, ...)` 处理清理：若缓存视图未被随后的 insert 复用（真正的删除），则销毁。

复用 SingingClip 视图时必须**重新连接**类型相关信号（`singerChanged`、`speakerChanged` 等）。

### Active clip 保留

`ProjectStatusController::handleClipRemoved` 原来无条件清空 `activeClipId`；跨轨移动时保持 active clip（编辑上下文不因换轨丢失）。

### 轨道颜色更新

- 拖动期间：`TracksGraphicsView::mouseMoveEvent` 在跨越轨道时更新 clip view 的 `colorIndex`。
- 提交后：`TrackController` 调 `ClipController::notifyActiveClipTrackChanged()`，从模型取最终颜色。
- 丢弃时：`notifyLiveTrackColorChanged(m_mouseDownColorIndex)` 恢复原色。

`liveTrackColorChanged` handler 只更新颜色并重绘，不触碰信号连接。

### 信号泄漏修复

`Track::removeClip()` 现在断开被移除 clip 的 `singerOrSpeakerChanged` 连接，防止陈旧信号穿透到已卸载的视图。

## Signal Design: 三信号方案

| Signal | PianoRoll / ParamEditor | TrackEditorView | InferController |
|--------|------------------------|-----------------|-----------------|
| `clip→notifyPropertyChanged()` | 更新 offset、scene length、notes | `updateClipOnView`（位置同步） | — |
| `oldTrack→notifyClipChanged(Removed)` | — | 缓存 clip view、断信号 | **移动：**保留 pipeline 只断信号 |
| `newTrack→notifyClipChanged(Inserted)` | — | 复用缓存视图、重连信号 | **移动：**跳过重启；**新 clip：**启动推理 |

信号顺序：`notifyPropertyChanged()` 先于 track 信号触发（PianoRoll/ParamEditor 的 property 连接在视图销毁前完成更新）。

### 推理管线保留（跨轨移动）

`InferControllerPrivate::handleSingingClipRemoved` 曾无条件取消该 clip 的所有推理任务。现在区分"移动"与"删除"：

```cpp
handleSingingClipRemoved:
  if findClipById(clip->id()) → 移动: 只断信号连接，不动任务/管线
  else                         → 删除: 完整拆除推理状态

handleSingingClipInserted:
  if !pieces().isEmpty()       → 移动: 只重连信号，不启动推理
  else                         → 新clip: 启动获取发音→音素→reSegment→管线
```

已知边界：移动后若目标轨道 singer/speaker 上下文与源不同，仅保留 singer/speaker 上下文匹配的 pipeline；不匹配的 pipeline 正常拆除（当前实现已覆盖）。

## 关键文件

- `src/app/Controller/Actions/AppModel/Clip/MoveClipToTrackAction.h/.cpp`
- `src/app/Controller/TrackController.cpp`（跨轨分发 + notify）
- `src/app/Controller/ClipController.h/.cpp`（`notifyActiveClipTrackChanged` / `notifyLiveTrackColorChanged`）
- `src/app/Controller/ProjectStatusController.cpp`（active clip 保留）
- `src/app/UI/Views/TrackEditor/TracksGraphicsView.cpp`（垂直拖拽 + 颜色同步）
- `src/app/UI/Views/TrackEditor/TrackEditorView.cpp`（视图缓存/复用 + 信号重连）
- `src/app/Modules/Inference/InferController.cpp`（移动 vs 删除区分）
