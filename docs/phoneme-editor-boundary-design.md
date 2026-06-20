# 音素编辑器边界与推理分片设计

本文说明音素编辑器、歌声片段分片算法和推理预处理之间的边界约束关系。目标是让用户编辑音素偏移时，不产生非法音素长度、不让推理输入出现负 start，也不破坏 piece 之间不重叠的前提。

## 设计目标

音素时间边界需要同时满足三层逻辑：

1. `SingingClipSlicer` 给出每个 piece 的可用范围。
2. `InferTaskHelper::buildWords()` 将 note 与 phoneme offset 转换为推理输入 word/phone。
3. `PhonemeView` 在用户拖拽时限制 offset，避免写入推理层无法安全解释的值。

核心原则是：**音素编辑器的可编辑范围必须和分片算法、推理预处理使用同一套边界定义**。推理层可以做兜底防崩溃，但 UI 不应该允许用户轻易制造明显非法的音素布局。

## 关键概念

### Header phoneme 与 onset

一个 note 的音素序列中，onset 前的音素称为 header phoneme。它们通常位于音符起点之前，用负 offset 表示。

例如：

```text
音符起点:           |==== note ====
header phoneme:   h1 h2
onset phoneme:             o
```

header offset 的定义来自 Duration 后处理：

```text
headerOffset = phonemeStartInWord - wordLength
```

因此 header offset 通常是负值。

### `paddingStartMs`

`paddingStartMs` 是 piece 首 note 前面的基础 padding SP word 长度，由 `SingingClipSlicer` 根据首 note 的 header phoneme 数量计算。

非休止音符的 header 最小长度为：

```text
padBaseLength + headerPhonemeCount * padUnitAdditionalLength
```

当前常量位于 `SingingClipSlicerGlobal`：

```text
padBaseLength = 100ms
padUnitAdditionalLength = 100ms
headerAvailableLengthMax = 1000ms
```

### `headAvailableLengthMs`

`headAvailableLengthMs` 是当前 piece 首 note 前面真实可用的最大头部空间。它由当前 piece 首 note 起点与上一个 piece tail 结束位置之间的距离决定，并且最大不超过 `headerAvailableLengthMax`。

```text
headAvailableLengthMs = min(firstNoteStart - previousTailEnd, headerAvailableLengthMax)
```

它表示当前 piece 最多可以向左占用多少空间，而不是基础 padding SP 的默认长度。

### firstWord

对 piece 首 note 来说，`InferTaskHelper::buildWords()` 会在首 note 前插入一个 SP word，本文称为 firstWord。它的基础长度是 `paddingStartMs`。

当用户把 piece 首音素继续向左拉长时，firstWord 的左边界会同步向左扩展。扩展极限是 `headAvailableLengthMs` 的左边界。

因此 piece 首音素的可拖拽额外空间不是 `headAvailableLengthMs`，而是：

```text
extraHeadMs = headAvailableLengthMs - paddingStartMs
```

对应到时间轴上的最左边界是：

```text
leftBoundary = noteStart - max(extraHeadMs, 0)
```

也就是说：首音素被拖到该位置时，firstWord 左边界刚好碰到 head available 左边界。

## 分片算法如何避免 piece 重叠

`SingingClipSlicer` 使用相邻 note 的 header/tail 需求判断是否切成不同 piece。

对当前 note A 和下一个 note B：

```text
B.headerStart = B.start - B.headerMinLength
A.tailEnd = A.end + A.tailLength
```

分片条件：

```text
commitFlag = B.headerStart > A.tailEnd
```

含义：

- 如果 `B.headerStart <= A.tailEnd`，说明 B 的 header 区域与 A 的 tail 区域相交或相接，A/B 必须合并到同一个 piece。
- 如果 `B.headerStart > A.tailEnd`，说明中间存在足够 gap，可以切成两个 piece。

示意：

```text
合并：
A: |==== note ====|←tail→|
B:          |←header→|==== note ====

分开：
A: |==== note ====|←tail→|
B:                         |←header→|==== note ====
```

因此，在所有参与分片的 note 都已有音素信息时，`headAvailableLengthMs`、`paddingStartMs` 和 `paddingEndMs` 已经给出了每个 piece 的合法可用范围。

## 推理预处理中的 word 构建

`InferTaskHelper::buildWords()` 中有三类 header 场景。

### 1. Piece 首 note 的 header

首 note 的 header phoneme 放入 firstWord：

```text
phone.start = firstWordLen + phonemeOffset
```

基础情况下：

```text
firstWordLen = paddingStartMs
```

如果用户编辑产生更负的 offset，推理层会扩展 firstWord 兜底，避免负 start：

```text
if useOffsetInfo && firstOffset < 0:
    firstWordLen += -firstOffset
```

这保证推理不会崩溃，但 UI 层仍应限制 firstWord 不超过 `headAvailableLengthMs` 的范围。

### 2. Gap word 的 header

如果两个 note 之间有 gap，下一 note 的 header 会进入 gap SP word：

```text
phone.start = gapLen + phonemeOffset
```

UI 中 gap Sil 的起点作为屏障，后一个 note 的首音素不能越过这个 Sil 起点。

### 3. 相邻 note 的 header

如果两个 note 相邻且属于同一个 piece，下一 note 的 header 会进入前一个 note 的 word：

```text
phone.start = previousWordLen + phonemeOffset
```

此时后一个 note 的首音素可以进入前一个 note 的区域，但不能无限向左侵入。

## 音素编辑器的边界规则

`PhonemeView` 使用链表形式保存可视音素，每个正常音素记录：

- `isFirstOfNote`
- `isLastOfNote`
- `isFirstOfPiece`
- `pieceHeadAvailableLengthMs`
- `piecePaddingStartMs`
- `start`
- `startOffset`

### 左边界

#### Piece 首 note 的首音素

```text
extraHeadMs = pieceHeadAvailableLengthMs - piecePaddingStartMs
leftBoundary = noteStart - max(extraHeadMs, 0)
```

语义：首音素向左拉长时，firstWord 左边界最多扩展到 head available 左边界。

#### 非 piece 首 note，且前驱是普通音素

这是相邻 note 场景，例如 A 在前、B 在后，B 的首音素被拖动。

```text
leftBoundary = max(A.noteStart, A.lastPhoneme.start)
```

语义：

- B 的首音素不能越过 A 的起始位置。
- 如果 A 的最后一个音素仍在 A 起点右侧，则 B 也不能拖穿 A 的最后一个音素。

#### 非 piece 首 note，且前驱是 gap Sil

```text
leftBoundary = priorSil.start
```

语义：Sil 是 gap 屏障，后一个 note 的 header 不能侵入 gap 前的 note 区域。

#### 普通内部音素

```text
leftBoundary = priorPhoneme.start
```

语义：不能拖穿前一个音素，避免产生负长度。

### 右边界

#### Note 尾音素

```text
rightBoundary = min(nextPhoneme.start, noteStart + noteLength)
```

如果没有后继音素：

```text
rightBoundary = noteStart + noteLength
```

语义：尾音素不能超过当前 note 结束位置。

#### 普通音素

```text
rightBoundary = nextPhoneme.start
```

语义：不能拖穿后一个音素。

## 已解决的问题

### Piece 首音素突破 head available 边界

错误做法是直接把首音素限制到：

```text
noteStart - headAvailableLengthMs
```

这会忽略 firstWord 本身已有的 `paddingStartMs`，导致 firstWord 左边界实际越过 head available 左边界。

正确做法是限制首音素的额外左移量：

```text
noteStart - (headAvailableLengthMs - paddingStartMs)
```

这样首音素到达边界时，firstWord 左边界正好与 head available 左边界对齐。

### 非首个 piece 的 head 约束不生效

旧逻辑依赖链表 head sentinel 判断首音素，因此只有整个 clip 的第一个音素能触发 head 约束。

当前逻辑在 `buildPhonemeList()` 中显式建立 note → piece 映射，并只把每个 piece 的首 note 的首音素标记为 `isFirstOfPiece`。

### 相邻 note 首音素活动范围为零

旧逻辑把相邻 note B 的首音素左边界夹到 B 的 `noteStart`，当 B 的 onset 也在 `noteStart` 时，可移动范围会变成零。

当前逻辑允许 B 的首音素进入 A 的区域，但最多到 A 的起点，且不能拖穿 A 的最后一个音素。

## 修改相关文件

- `src/app/Modules/SingingClipSlicer/SingingClipSlicer.cpp`
  - 分片算法与 `headAvailableLengthMs` / `paddingStartMs` 的来源。
- `src/app/Modules/Inference/Utils/InferTaskHelper.cpp`
  - 推理 word 构建逻辑。
- `src/app/Modules/Inference/Tasks/InferDurationTask.cpp`
  - Duration 输出转 phoneme offset 的规则。
- `src/app/Model/AppModel/InferPiece.cpp`
  - piece 本地起点与首 note 负 offset 的对齐。
- `src/app/UI/Views/ClipEditor/PianoRoll/PhonemeView.cpp`
  - 用户拖拽时的音素边界约束。
- `src/app/UI/Views/ClipEditor/PianoRoll/PhonemeView.h`
  - `PhonemeViewModel` 中与边界相关的状态。

## 验证建议

修改相关逻辑后，至少验证以下场景：

1. 单个 piece 的首音素向左拉长，firstWord 左边界不能越过 head available 左边界。
2. 多个 piece 中，非首个 piece 的首音素也受到同样限制。
3. 有 gap Sil 的两个 note，后一个 note 的首音素不能越过 Sil 起点。
4. 相邻两个 note，后一个 note 的首音素可以进入前一个 note 区域，但不能越过前一个 note 起点。
5. note 尾音素不能超过当前 note 结束位置。
6. 修改后运行 `cmake --build --preset debug`。
