# Cross-Track Clip Movement

## Overview

Implements dragging clips vertically across tracks in the Track Editor. Previously clips could only be moved horizontally (time position), not vertically between tracks.

## Architecture

The clip movement pipeline spans 5 layers:

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

### MoveClipToTrackAction — Dedicated action for cross-track moves

`EditClipCommonPropertiesAction` only edits clip properties (start, length, etc.) and assumes the clip stays on the same track. Cross-track movement is a different operation: it manipulates two tracks' clip membership. A new `MoveClipToTrackAction` was created for this purpose.

**Files:**
- `src/app/Controller/Actions/AppModel/Clip/MoveClipToTrackAction.h`
- `src/app/Controller/Actions/AppModel/Clip/MoveClipToTrackAction.cpp`

**Execution order:**
1. `oldTrack->removeClip(clip)`
2. Set new clip properties (start, clipStart, length, clipLen)
3. `newTrack->insertClip(clip)`
4. `oldTrack->notifyClipChanged(Removed)` — triggers view destruction + subsystem teardown
5. `newTrack->notifyClipChanged(Inserted)` — triggers view creation + subsystem setup

Note: step 3 happens before step 4, so `appModel->findClipById()` finds the clip on the new track when `handleClipRemoved` runs.

### View preservation — No destroy/recreate of clip graphics

When `notifyClipChanged(Removed)` fires, `TrackEditorView::onClipRemoved` caches the `AbstractClipView` instead of deleting it. When `notifyClipChanged(Inserted)` follows, `onClipInserted` reuses the cached view. This preserves selected/active/hover states.

**Files:**
- `src/app/UI/Views/TrackEditor/TrackEditorView.h` — added `m_pendingRemoveClipViews` map
- `src/app/UI/Views/TrackEditor/TrackEditorView.cpp` — cache/reuse logic

A `QTimer::singleShot(0, ...)` handles cleanup: if the cached view is not reused by a subsequent insert (genuine delete), it's destroyed.

### Active clip preservation

`ProjectStatusController::handleClipRemoved` previously cleared `activeClipId` unconditionally when the removed clip matched. Now it checks `appModel->findClipById()` first — if the clip still exists on another track, it's a move, not a delete.

**File:** `src/app/Controller/ProjectStatusController.cpp`

### Track color updates

- **During drag:** `TracksGraphicsView::mouseMoveEvent` updates `colorIndex` on the clip view as it crosses track boundaries, using `appModel->tracks().at(targetTrackIndex)->colorIndex()`.
- **After commit:** `TrackController` calls `ClipController::notifyActiveClipTrackChanged()` which emits `activeClipTrackChanged`. `ClipEditorView` handles this by updating `NoteView`/`PianoRollView`/`ParamEditorView` track colors and calling `update()` for repaint.

**Files:**
- `src/app/UI/Views/TrackEditor/TracksGraphicsView.cpp`
- `src/app/Controller/ClipController.h` — added signal + method
- `src/app/Controller/ClipController.cpp`
- `src/app/UI/Views/ClipEditor/ClipEditorView.cpp`
- `src/app/Controller/TrackController.cpp`

### Signal leak fix

`Track::removeClip()` now disconnects `singerOrSpeakerChanged` from the removed clip, preventing stale signal propagation from the old track.

**File:** `src/app/Model/AppModel/Track.cpp`

### Signal reconnection for cached views

When a SingingClip view is reused (cross-track move), the type-specific signals (`singerChanged`, `speakerChanged`, `noteChanged`, etc.) are reconnected since `disconnect(clip, nullptr, this, nullptr)` in `onClipRemoved` disconnected them.

## Files Modified

| File | Change |
|------|--------|
| `src/app/Model/AppModel/Track.cpp` | Disconnect signal in `removeClip` |
| `src/app/Controller/Actions/.../MoveClipToTrackAction.h` | **New** — Cross-track move action |
| `src/app/Controller/Actions/.../MoveClipToTrackAction.cpp` | **New** — execute/undo |
| `src/app/Controller/Actions/.../ClipActions.h` | Added `moveClipToTrack` |
| `src/app/Controller/Actions/.../ClipActions.cpp` | Implement `moveClipToTrack` |
| `src/app/Controller/TrackController.h` | Added `onClipPropertyChanged(args, newTrackIndex)` overload |
| `src/app/Controller/TrackController.cpp` | Cross-track dispatch + notify clipController |
| `src/app/Controller/ProjectStatusController.cpp` | Don't clear activeClipId on cross-track move |
| `src/app/Controller/ClipController.h` | Added `notifyActiveClipTrackChanged` + `notifyLiveTrackColorChanged` + signals |
| `src/app/Controller/ClipController.cpp` | Implement `notifyActiveClipTrackChanged` + `notifyLiveTrackColorChanged` |
| `src/app/UI/Views/TrackEditor/TracksGraphicsView.h` | Uncommented `m_mouseDownTrackIndex`, added `m_mouseDownColorIndex` |
| `src/app/UI/Views/TrackEditor/TracksGraphicsView.cpp` | Vertical drag + color sync, live color preview, discard restore |
| `src/app/UI/Views/TrackEditor/TrackEditorView.h` | Added `m_pendingRemoveClipViews` |
| `src/app/UI/Views/TrackEditor/TrackEditorView.cpp` | Cache/reuse clip views + signal reconnection |
| `src/app/UI/Views/ClipEditor/ClipEditorView.cpp` | Handle `activeClipTrackChanged` + `liveTrackColorChanged` |
| `src/app/Modules/Inference/InferController.cpp` | Don't tear down inference pipelines on move |

## Signal Design: Three-Signal Approach

`MoveClipToTrackAction` emits three signals, each serving a distinct set of consumers:

| Signal | PianoRoll / ParamEditor | TrackEditorView | InferController |
|--------|------------------------|-----------------|-----------------|
| `clip→notifyPropertyChanged()` | Updates offset, scene length, notes | `updateClipOnView` (position sync) | — |
| `oldTrack→notifyClipChanged(Removed)` | — | Caches clip view, disconnects signals | **Move:** keep pipelines alive; **Delete:** full teardown |
| `newTrack→notifyClipChanged(Inserted)` | — | Reuses cached view, reconnects signals | **Move:** skip restart; **New clip:** start inference |

This mirrors the pattern in `EditClipCommonPropertiesAction` (same-track), which emits only `notifyPropertyChanged()`. Cross-track moves add the two track signals to communicate track membership changes.

### Signal ordering

`notifyPropertyChanged()` fires before the track signals. PianoRoll/ParamEditor connections to `clip→propertyChanged` survive the entire sequence (they are not affected by `TrackEditorView::disconnect(clip, nullptr, this, nullptr)`). TrackEditorView's connection is torn down during `Removed` and rebuilt during `Inserted`, so `notifyPropertyChanged` reaches it through the old-track connection — redundant but harmless since all signals fire synchronously within one call stack.

### Fix: Piano roll position after cross-track move

`MoveClipToTrackAction` was missing the `m_clip->notifyPropertyChanged()` call. After setting new properties (start, clipStart, length, clipLen), the piano roll views (`PianoRollGraphicsViewPrivate`, `ParamEditorGraphicsView`) caches were not updated — they still held the old `m_offset` and scene length. Clicking away and back triggered a full re-init via `setDataContext`, masking the bug.

Added `m_clip->notifyPropertyChanged()` to both `execute()` and `undo()`.

**File:** `src/app/Controller/Actions/AppModel/Clip/MoveClipToTrackAction.cpp`

### Fix: Inference pipeline teardown on cross-track move

**Root cause:** `InferControllerPrivate::handleSingingClipRemoved` unconditionally cancelled all inference tasks and deleted pipelines for the removed clip. For a cross-track move (where the clip still exists on a new track), this destroyed the inference pipeline unnecessarily. Subsequent `handleSingingClipInserted` started a new `GetPronunciationTask` chain, which eventually called `reSegment()`. Since notes hadn't changed and existing pieces were not dirty (their inference was complete), `reSegment` reused old pieces without creating new ones, so `createPipeline` was never called — the inference state machine never restarted.

**Fix:** Both `handleSingingClipRemoved` and `handleSingingClipInserted` now distinguish between "move" and "delete/insert" using `appModel->findClipById(clip->id())` and `clip->pieces().isEmpty()`:

```
handleSingingClipRemoved:
  if findClipById(clip->id()) → 移动: 只断信号连接，不动任务/管线
  else                         → 删除: 完整拆除推理状态

handleSingingClipInserted:
  if !pieces().isEmpty()       → 移动: 只重连信号，不启动推理
  else                         → 新clip: 启动获取发音→音素→reSegment→管线
```

This is the same "distinguish move from delete" pattern already used by `ProjectStatusController` and `TrackEditorView`.

**File:** `src/app/Modules/Inference/InferController.cpp`

### Live piano roll color preview during drag

During cross-track drag in `TracksGraphicsView::mouseMoveEvent`, the clip view color follows the target track immediately. A new `ClipController::liveTrackColorChanged` signal extends this to the piano roll, so note colors and background reflect the target track in real time:

```
mouseMoveEvent (target track changes):
  → clipController->notifyLiveTrackColorChanged(targetColorIndex)
  → ClipEditorView: updates NoteView color, PianoRollView color, ParamEditorView color + repaint

discardAction (Esc / release outside):
  → clipController->notifyLiveTrackColorChanged(m_mouseDownColorIndex)  // restore original

commitAction (mouse release):
  → clipController->notifyActiveClipTrackChanged()  // final color from model
```

The `liveTrackColorChanged` handler only updates colors and repaints — it does not touch signal connections (unlike `activeClipTrackChanged` which reconnects `m_trackColorConnection` to the new track). This makes the live preview side-effect-free and safe to fire at high frequency during drag.

**Files:**
- `src/app/Controller/ClipController.h` — Added signal + method
- `src/app/Controller/ClipController.cpp` — Implementation
- `src/app/UI/Views/ClipEditor/ClipEditorView.cpp` — Connect signal
- `src/app/UI/Views/TrackEditor/TracksGraphicsView.cpp` — Emit during drag + restore on discard

## Known Issues (Next Steps)

1. **Inference state machine not transitioning:** After cross-track movement, the acoustic model inference state machine does not properly transition to the new track context. The `TrackSynthesizer` handles `clipChanged(Removed)` by tearing down the singing clip context, and `clipChanged(Inserted)` by creating a new one. The teardown/recreation may not properly restart the inference pipeline.
