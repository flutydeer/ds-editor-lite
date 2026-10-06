# 音符共享边界联动调整

## 背景

相邻音符共享边界时（前一音符的结束 tick 等于后一音符的起始 tick），拖动这条边界想同时改变两个音符的长度是歌唱剪辑的高频操作：一个被拉长，另一个同步被缩短。此前拖动任一侧边界只改被按住的音符，用户需要先拖长一个、再拖短另一个，且两步都要手动对齐到同一 tick。

本文描述按住 Shift 拖动音符左/右边缘时，与该侧相邻音符联动调整共享边界的完整链路。该行为覆盖所有本就支持音符大小调整的编辑模式（选择、区间选择、绘图），实现落在公共交互入口，Legacy 与 RHI 两个渲染后端行为一致。

## 行为定义

### 触发与分支

- 指针位于音符左/右边缘热区（沿用 `NoteHandleGeometry::resizeEdgeAt` 判定，抓取环扩张照旧）且按住 Shift 时，边缘悬停光标表达联动可行性：
  - 该侧存在时间上相邻的音符且严格相邻 → 正常缩放光标（`SizeHorCursor`）
  - 相邻音符存在但有间隙或重叠 → `ForbiddenCursor`，按下不启动调整、不改变选择
  - 该侧没有音符 → 完全走普通单音符调整路径，Shift 的范围选择语义不变
- 联动模式在按下时锁定，拖拽中途松开 Shift 继续联动；普通（无 Shift）拖拽中途按住 Shift 不会切换为联动。
- 联动按下采用 Plain 单选语义（被按音符单选），Shift 的范围选择语义在边缘热区上让位于联动；音符主体上的 Shift 范围选择不受影响。

### 邻居判定

`clip->notes()` 按 `Note::compareTo`（start、key、id）排序，即时间序列。邻居定义为序列中紧邻的前一个/后一个音符，**不限音高**（歌唱剪辑中相邻音节常跨琴键）。合法条件：

- 拖右边缘：`next->localStart() == note->localStart() + note->length()`
- 拖左边缘：`prev->localStart() + prev->length() == note->localStart()`

间隙（`>`）与重叠（`<`）都不合法。同 tick 的和弦音符因时间重叠同样视为非法。

### 几何与约束

共享边界位移 d（左边缘 snapDown、右边缘 snapNearest，Alt 临时关闭量化，最小长度为量化步且 ≥ 1，与单音符 resize 一致）：

- 拖右边缘（A 左、B 右）：`A.length += d`；`B.start += d, B.length -= d`
- 拖左边缘（C 左、A 右）：`A.start += d, A.length -= d`；`C.length += d`

约束交集：`minLen − 左音符长 ≤ d ≤ 右音符长 − minLen`。某音符已短于最小长度导致区间倒置时，边界保持不动（返回 0）。

已知取舍：联动可能使第三个和弦音符与新边界产生重叠，与现有 resize 允许自由重叠一致，不做额外限制。

### 提交与撤销

一次联动拖拽提交为一个原子命令（`notes.resize_joint`），撤销一步恢复两个音符。facade 在命令执行时按当前模型重新校验严格相邻与最小长度约束，模型状态已漂移则拒绝。

## 实现

### 纯工具

- `src/libs/ProjectModel/Utils/NoteResizeUtils.h` 新增 `clampJointBoundaryDelta(leftNoteLength, rightNoteLength, requestedDelta, minimumLength)`，即上述约束交集，区间倒置返回 0
- `src/app/UI/Views/ClipEditor/PianoRoll/NoteAdjacencyUtils.h` 新增 `nextNote/prevNote`（`std::lower_bound` 于有序音符列表）、`strictlyAdjacent` 与 `neighborForEdge`（返回邻居加是否严格相邻；null note/clip 安全返回空）

### Legacy 后端（QGraphicsView）

覆盖选择、区间选择（与选择共用 `SelectNoteHandler`）、绘图（`DrawNoteHandler`），三者都经 `NoteInteractionController::prepareForEditingNotes` 进入 resize：

- 控制器新增 `setDataContext(SingingClip*)`（随 `moveToSingingClipState`/`moveToNullClipState` 同步）与 `m_jointNeighborId` 状态；按下时先以临时边缘做 Shift 邻接判定，非法直接返回（无选择变更），合法记邻居 id 并降级为 Plain 选择后再按 settle 后的把手状态解析最终边缘
- `resizeLeftSelectedNote/resizeRightSelectedNote` 在联动时同时设置邻居 NoteView 的 start/length offset（邻居按 id 经 `findNoteViewById` 解析，新增公有方法）
- `updateNoteDragAt` 联动分支用 `jointBoundaryDelta` 计算 delta；编辑会话的音符 id 列表追加邻居
- `publishNoteEditPreview` 追加邻居预览条目，轨道侧缩略图同步显示两音符移动
- `commitAction` 联动时调 `handleNoteSharedBoundaryResized`；`discardAction`/`commitAction` 重置邻居 offset 并清联动状态
- `onHoverMove` 悬停光标按 Shift + 邻接判定给 `SizeHorCursor`/`ForbiddenCursor`

### RHI 后端（PianoRollRhiWidget）

`mousePress` 的统一交互分类同样覆盖三种模式：

- 新增 `jointResizeActive`/`jointNeighborId` 状态，`resetNoteInteraction` 清空
- 按下在 `updateNoteSelection` 之前判定：非法直接 return，合法以 `Qt::NoModifier` 走 Plain 选择并记录邻居
- `updateNoteCursor` 增加 modifiers 参数，Shift 时做同款判定
- `updateInteractionDelta` 联动分支用 `jointBoundaryDelta`；`beginNoteEditSession` 追加邻居 id
- `noteDrawSceneRect` 为邻居 id 增加方向对应的几何偏移（左边缘联动加长邻居，右边缘联动平移+缩短邻居）
- `publishNoteEditPreview` 追加邻居条目；`mouseRelease` 联动时调 `onResizeNotesSharedBoundary`

### 提交命令链

- `OperationIds::notes::resize_joint`
- `NoteActions::editNotesGeometry`：按音符传绝对 `{start, length}` 对，复用 `QuantizeNotesAction`（抽出共享体 `addGeometryChanges`，撤销名 "Resize notes"，与 quantize 区分）
- `NoteAutomationFacade::resizeNotesSharedBoundary(context, clipId, leftNoteId, rightNoteId, deltaTick, minimumLength)`：dispatch 内解析两音符、重新校验严格相邻、重 clamp delta、越界与非法走现有 invalid/preview/unchanged 分支，提交 `editNotesGeometry`
- `ClipController::onResizeNotesSharedBoundary` 转发到 facade

## 测试

`src/tests/TestPianoRollInteractions`：

- `clampJointBoundaryDelta`：约束交集、无效最小长度退化为 1 tick、倒置区间返回 0
- 邻接查找：严格相邻（含跨琴键）、间隙、重叠、首尾无邻居、时间序不受音高影响
