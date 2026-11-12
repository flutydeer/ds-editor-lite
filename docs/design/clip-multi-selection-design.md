# 轨道窗剪辑多选批量编辑

## 背景

轨道窗早就支持多选：Ctrl 点按由 `EditorSelectionUtils::selectionForPress` 处理，RHI 画布还有橡皮筋框选，`AppStatus::selectedClips` 是所有视图与命令的唯一真相。但**选择集不能作为一个整体被编辑**：拖动任意一个剪辑只移动它自己，拖边缘只裁它自己，方向键在画布上没有任何作用。批量删除、剪切、复制、粘贴本来就作用在 `selectedClips` 上，所以最刺眼的反差是「能一起删，不能一起挪」。

钢琴卷帘和锚点编辑器早就有多选整体移动并原子提交（`rhiMultiNoteSelectionAndMoveCommitAtomically`、`rhiAnchorSelectionMovesTheGroupAtomically`）。轨道窗是最后一块没有跟上「选择集 = 一个变换单元」的地方。

本文描述把剪辑选择集变成整体变换单元的完整链路，覆盖 `Legacy`（默认，QGraphics）与 `RhiExperimental` 两个渲染后端。

## 行为定义

### 选择

- 左键按下剪辑主体：Plain 单选语义。若该剪辑已在选择集内，选择集保持不变（这正是整体拖动的入口），松开且未移动时收拢为只选该剪辑。
- Ctrl 点按：加选 / 减选。
- Shift 点按：从锚点连选到该剪辑（视觉顺序 = 轨道顺序，轨道内按 start）。
- Ctrl+Shift 点按：在现有选择集上追加范围。
- 右键按下：Plain 语义，但**不收拢**选择集，右键菜单的剪切/复制/删除因此作用于整个选择集。
- 选择集变化后 `activeClipId` 恒为被按住的剪辑（anchor）。

### 拖动

| 手势 | 单选 | 多选且被按剪辑在选择集内 |
| --- | --- | --- |
| 拖动主体 | 移动它，可跨轨 | 整块移动，同一 tick 偏移 + 同一轨道偏移 |
| 拖动边缘（容差内） | 裁它 | 整块裁边，同一 delta，每个剪辑自行钳制 |

- **移动是刚性的**：水平方向按选择集里最小的 `start + clipStart` 钳住整块，任何剪辑可见左沿不低于 tick 0（opendspx 要求 `pos >= 0`）。垂直方向按选择集的轨道上下沿钳住 delta，整块留在 `[0, trackCount - 1]`。
- **裁边是逐剪辑的**：同一 delta 下每个剪辑各自调用 `ClipResizeUtils::updateLeftEdge` / `updateRightEdge`，或对有实时锚点的音频剪辑调用 `AudioClipDragState::resizeLeftTo` / `resizeRightTo`。谁的素材到头了就自己停住，其它剪辑继续跟随。同一选择集内歌声与音频剪辑可以混排。
- **吸附只算一次**：以 anchor 的可见左沿（或被拖的那条边）为基准做 `TimelineSnapUtils::snapNearest`，得到实际生效的 delta，再套到全部剪辑上。因此吸附不会破坏相对关系。
- Alt 按下不吸附（沿用各后端现有的 `gridStepAt`，Alt 时步长退化为 1 tick）。
- Esc、窗口失活、面板隐藏、触摸取消都走丢弃路径：模型不动，预览还原。

### 键盘微移

画布拥有焦点且选择集非空时：

- ← / →：按当前 WYSIWYG 网格步长整体水平微移，Alt 时 1 tick。
- ↑ / ↓：整体上/下移一条轨道，按轨道范围钳制。
- 每次被接受的按键是一次独立命令，因此也是独立的一条撤销。按住不放会连续产生多条撤销记录，这是已知限制，本方案不做历史合并。

## 架构

```
指针事件（两套后端各自把位置换算成 tick / 轨道行）
    ↓
ClipSelectionDrag::Session        // 唯一的交互数学，纯计算，不碰模型
    ↓  placements（每个剪辑的目标几何 + 目标轨道）
视图预览（Legacy 写入图形项属性，RHI 写入预览表）
    ↓  提交
TrackController::onMoveClips / onResizeClips / nudgeSelectedClips
    ↓
ProjectAutomationFacade::moveClips / setClipsProperties
    ↓
ClipActions::editClips            // 一条 ActionSequence
    ↓
HistoryManager::record            // 一步撤销
```

### ClipSelectionDrag

`src/app/UI/Views/TrackEditor/ClipSelectionDrag.h/.cpp` 是新抽出的规划器，两个后端共用。它只接收按下瞬间的剪辑快照：

- `ClipState`：clipId、轨道行、`Clip::ClipCommonProperties`、`lengthResizable`、`contentLength`、`realTimeAnchored`。
- `Environment`：轨道总数、`Timeline`、`gridStepAt` 回调（把各后端现有的网格步长实现注入进来）。
- `update(pointerTick, pointerTrackIndex, snapOff)`：算出 `Placement` 列表，并顺带算出 `moved()`（是否真的产生了差异）。

规划器不依赖 `AppModel`，因此可以在 `TestEditorInteraction` 里直接单测（吸附、块级钳制、逐剪辑钳制、实时锚点真值）。

有实时锚点的音频剪辑在移动/裁边时走 `AudioClipDragState`，与命令层 `moveClips` / `EditClipCommonPropertiesAction` 的换算完全一致，所以预览和提交不会差一个 tick：

- 移动：`begin(trim, play, material, 可见左沿, 可见左沿, timeline)` + `moveTo(目标, props, timeline)` + `writeTruth`。
- 裁边：同一 `begin` 之后用 `resizeLeftTo` / `resizeRightTo`，再 `writeTruth`。

### 单步撤销的两条命令

- **移动**：`TrackController::onMoveClips(QList<ClipGeometryEdit>)` → `ProjectAutomationFacade::moveClips`（既有批量命令）。跨轨移动仍然走 `MoveClipToTrackAction`，因此「移动 vs 删除」的推理管线判定（`InferController` 用 `findClipById` 区分）完全不变。
- **裁边与其它几何编辑**：新增 `ProjectAutomationFacade::setClipsProperties(context, QList<ClipPropertiesDto>)`。它先整体校验再提交，一次 dispatch 产生一条 `ActionSequence`。派发沿用 `OperationIds::clips::set_properties`，不新增 operation id、不改公共工具契约与 `docs/automation` 矩阵。
- **键盘微移**：`TrackController::nudgeSelectedClips(deltaTicks, deltaTracks)` 自己按选择集算钳制后的 delta，组装 `ClipMoveDto` 后复用 `moveClips`。两个后端的按键处理因此只有一行调用。

两个入口都会重新解析每个剪辑：手势可能跨过一次并发删除（音频导入完成、异步任务写回），消失的剪辑被跳过，剩下的照常提交。没有剩余时直接不派发命令，避免空撤销项。

### ClipActions::editClips

`ClipActions` 原有的批量入口只处理同类剪辑，而且**每次调用都会覆盖**历史名与 `HistoryFocusTransition`，`moveClips` 的循环因此在混排时只剩最后一个剪辑的名称和聚焦范围。新增的 `ClipActions::editClips(QList<ClipPropertyEdit>)` 一次收下全部编辑：

- `newTrack != oldTrack` → `MoveClipToTrackAction`，否则按剪辑类型选 `EditClipCommonPropertiesAction` / `EditSingingClipPropertiesAction`。
- 历史名只设一次：全歌声 → `Edit singing clip(s)`，全音频 → `Edit audio clip(s)`，全跨轨 → `Move clip to track`，混合 → 新增的 `Edit clip(s)`。
- `setFocusTransition` 只设一次，用文件内既有的 `mergeFocus` 把全部 before/after 合成一个包络，undo 后能一次揭示整块。

既有的 `editSingingClipProperties` / `editAudioClipProperties` / `moveClipToTrack` 都改为转调 `editClips`，行为不变，代码只有一处。

### 视图接线

两个后端的字段从「单个剪辑的拖动状态」换成「规划器 + 预览表」：

| | Legacy `TracksGraphicsView` | RHI `TracksRhiWidget` |
| --- | --- | --- |
| 选择模型 | `EditorSelectionUtils::OrderedSelectionModel` | 同左 |
| 拖动状态 | `ClipSelectionDrag::Session` | 同左 |
| 预览 | `applyDragPlacements` 直接把目标几何写进 `AbstractClipView` | `QHash<int, Placement>`，由 `previewOrModelProperties` / `buildClipSnapshot` 查表 |
| 还原 | `restoreDragOriginals` | 清空预览表 |
| 键盘 | 新增 `keyPressEvent`，并在按下时 `setFocus` | 既有 `keyPressEvent` 增加方向键分支 |

Legacy 的 `m_currentEditingClip`、`m_mouseDownStart` 等一组单剪辑标量字段全部删除。RHI 的 `m_dragPreview`、`m_audioDragState`、`m_mouseDownProperties` 同样删除。

## 与既有契约的关系

- `AppStatus::selectedClips` 仍是选择唯一真相，两个后端的图形项选中态是它的镜像。
- `EditSessionManager` 事务仍然挂在 anchor 上、携带全部选中 id（`EditSession` 的 `clipIds`），推理脏区不受影响。
- 文档代次重置不变：`DocumentAutomationFacade::resetDocumentScopedState` 已经清掉 `activeClipId` 与 `selectedClips`，规划器只活在单次手势里。
- 模型在预览期间不被修改，`documentVersion` 在提交前保持不变。
- 批量跨轨移动仍是 `MoveClipToTrackAction`，`InferController` 的「移动 vs 删除」判定不变，详见 [cross-track-clip-movement.md](cross-track-clip-movement.md)。

## 已知限制与非目标

- 方向键自动重复不合并历史，按住不放会连续产生撤销记录。
- 不做块级刚性裁边极限：裁边是逐剪辑钳制的，到达极限的剪辑停住，其它继续跟随。
- 不做多选 Duplicate（Ctrl+D）、批量 Split、批量增益/静音/改名面板、拖动复制（Alt+拖出副本）。
- 不合并 `Legacy` 与 `RHI` 两个渲染后端，本次只让它们共享交互数学。

## 关键文件

- `src/app/UI/Views/TrackEditor/ClipSelectionDrag.h/.cpp`（新）
- `src/app/Controller/Actions/AppModel/Clip/ClipActions.h/.cpp`（`editClips`）
- `src/app/Automation/ProjectAutomationFacade.h/.cpp`（`setClipsProperties`，`moveClips` 改用 `editClips`）
- `src/app/Controller/TrackController.h/.cpp`（`ClipGeometryEdit`、`onMoveClips`、`onResizeClips`、`nudgeSelectedClips`）
- `src/app/UI/Views/TrackEditor/TracksGraphicsView.h/.cpp`
- `src/app/UI/Views/TrackEditor/TracksRhiWidget.h/.cpp`
- 测试：`TestEditorInteraction/tst_tracks.cpp`（规划器）、`TestApplicationGui/tst_track_editing.cpp`（Legacy）、`TestNativeDesktop/tst_rhi_tracks.cpp`（RHI）
