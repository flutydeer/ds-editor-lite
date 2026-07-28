# 音素编辑器边界与推理分片设计

本文说明音素编辑器、歌声片段分片算法、Duration 后处理和后续推理任务如何共享同一套
piece 首部时间语义。目标是保证推理输入中的 phone start 非负、音素顺序合法，并且
相邻 piece 的推理、参数曲线、波形和音频范围不会重叠。

## 统一的首部布局

`PhonemeHeadLayout` 是首部时间范围的唯一计算入口。它接收
`paddingStartMs`、`headAvailableLengthMs` 和首 note 的音素 offset，并给出：

```text
baseWordLengthMs     = paddingStartMs
minimumFirstOffsetMs = min(0, firstNoteOffsets...)
requiredHeadLengthMs = max(paddingStartMs, -minimumFirstOffsetMs)
maximumHeadLengthMs  = max(paddingStartMs, headAvailableLengthMs)
```

`paddingStartMs` 是首个 SP word 的基础**总长度**，不是可以在负 offset 之外再次叠加的
一段额外 padding。`requiredHeadLengthMs` 也因此使用 `max`，不能写成：

```text
paddingStartMs + abs(minimumFirstOffsetMs)
```

后一种写法会把同一个 header offset 同时解释为 word 内位置和额外预卷，导致 piece
越过 slicer 分配的头部边界。

### 首 piece 特例

第一个 piece 前面没有上一 piece 的 tail。此时 `headAvailableLengthMs` 可能小于
`paddingStartMs`，但首个 SP word 仍需要基础预卷。因此允许上限取两者的最大值。

对非首 piece，`SingingClipSlicer` 只有在当前 header 与上一 piece tail 之间存在足够
间隔时才分片，所以 `headAvailableLengthMs` 已经覆盖基础 padding；同一公式会严格服从
slicer 给出的上限。

### 绝对时间锚点与取整

所有毫秒与 tick 的换算都以首 note 的项目绝对位置为锚点：

```text
firstNoteStartMs = timeline.tickToMs(firstNoteGlobalTick)
pieceStartMs     = firstNoteStartMs - requiredHeadLengthMs
leftBoundaryMs   = firstNoteStartMs - maximumHeadLengthMs
```

然后再用同一 `Timeline` 转回 tick。转换为整数 tick 或整数 offset 时使用不向左越界的
保守取整；不能把一段“毫秒时长”直接传给项目绝对 `msToTick()`。这一点对高曲速和跨
tempo 点场景尤其重要。

## 分片算法如何避免重叠

`SingingClipSlicer` 对当前 note A 和下一 note B 计算：

```text
B.headerStart = B.start - B.headerMinLength
A.tailEnd     = A.end + A.tailLength
```

只有满足以下条件才切成两个 piece：

```text
B.headerStart > A.tailEnd
```

提交 segment 时，slicer 保存：

- `paddingStartMs`：首 note 对应 SP word 的基础总长度；
- `headAvailableLengthMs`：首 note 到上一 piece tail 结束位置之间可用的头部范围；
- `paddingEndMs`：末 note 后的 tail 长度。

后续代码不得在这些值之外自行叠加另一份 header offset。

## Duration 的往返关系

Duration 模型仍接收 word、note 和 phone 序列，并返回逐音素 duration。输入首 word
始终使用基础 `paddingStartMs`。后处理在每个 word 内累计 duration，得到 phone start：

```text
phoneStart[0] = 0
phoneStart[n] = sum(duration[0..n-1])
```

首 note 的 header offset 按原有语义投影：

```text
headerOffset = phoneStartInFirstWord - firstWordLength
normalOffset = phoneStartInNoteWord
```

把结果重新送入 Pitch、Variance 或 Acoustic 预处理时，首 word 长度为：

```text
max(paddingStartMs, -minimumFirstOffsetMs)
```

于是每个 header phone 满足：

```text
phone.start = requiredHeadLengthMs + headerOffset >= 0
```

这构成 duration → start → offset → word 的往返关系。Duration 结果在写回前必须验证：

- duration 数量与输入 phone 数量相同；
- 每个 duration、word 长度和 phone start 都是有限非负值；
- 输出 token 能逐一映射回原 note 的音素；
- note 内 offset 不降序；
- 首 note 的 `requiredHeadLengthMs` 不超过 `maximumHeadLengthMs`。

验证失败时任务携带 clip、piece、note 上下文失败，不通过截断、重排或扩大 piece
来掩盖不一致。

## 推理、缓存、参数与音频范围

`InferPiece::localStartTick()` 和 `InferControllerHelper` 的输入快照都使用
`PhonemeHeadLayout`。快照显式保存首部最小 offset、所需长度和允许上限，并把它们纳入
语义签名。因此首部布局改变后不会复用旧布局的 Duration、Pitch、Variance 或 Acoustic
结果。

参数曲线采样、结果曲线对齐、波形显示和音频定位继续消费统一的 `pieceStartTick`。
它们不再各自重算或再次扩展 header。

## 音素编辑器边界

`PhonemeView` 为每个 piece 首 note 的首音素保存 canonical 左边界：

```text
leftBoundaryTick =
    timeline.msToTick(firstNoteStartMs - maximumHeadLengthMs)
```

拖拽预览不能越过该 tick。提交时再以首 note 的绝对毫秒位置计算 offset，并以
`ceil(-maximumHeadLengthMs)` 作为最小整数 offset，避免 tick 与整数毫秒量化把结果向
约束外扩。

其他边界规则保持不变：

- gap 后的 note 不能越过 gap Sil；
- 相邻 note 的首音素不能越过前一有效 note 的边界或最后音素；
- 内部音素不能拖穿前后音素；
- note 尾音素不能超过 note 结束位置。

Timeline 的开发者调试覆盖层也使用同一 canonical 左边界，以便直接比较 slicer 上限、
实际 piece 起点和首 note 起点。

## 已有编辑的载入与归一化

`SingingClipPhonemeNormalizer` 在载入和结构编辑后检查人工 offset：

- offset 数量必须与音素名称数量相同；
- offset 必须不降序；
- piece 首 note 的布局必须位于 canonical 首部上限内；
- 其他 note 的最早音素必须位于基于绝对时间锚点计算的左边界内。

仍在 canonical 上限内的历史编辑保持不变。真正非法的编辑沿用现有可撤销流程：清空
edited offset，让自动结果重新生效；相关 Action 在撤销时恢复原 edited offset。不会
静默裁剪某个值后继续推理。

## 相关文件

- `src/libs/ProjectModel/Utils/PhonemeHeadLayout.*`
- `src/libs/ProjectModel/SingingClipSlicer/SingingClipSlicer.cpp`
- `src/libs/ProjectModel/InferenceData/InferPiece.cpp`
- `src/app/Modules/Inference/InferControllerHelper.cpp`
- `src/app/Modules/Inference/Utils/InferTaskHelper.cpp`
- `src/app/Modules/Inference/Tasks/InferDurationTask.cpp`
- `src/app/Model/AppModel/SingingClipPhonemeNormalizer.cpp`
- `src/app/UI/Views/ClipEditor/PianoRoll/PhonemeView.*`
- `src/app/UI/Views/Common/TimelineView.cpp`

## 验证建议

1. 分别验证 120、160、180 和 240 BPM，并覆盖临界分片间隔与跨 tempo 点。
2. 检查相邻 piece 的参数、波形和音频范围只相接或保留间隔，绝不重叠。
3. 把非首 piece 的首音素拖到最左边界并继续向左，确认预览和提交都阻止越界。
4. 重推理并复用缓存，确认 piece 起点不漂移。
5. 覆盖首 piece、无 header、SP/AP、转音与已有合法 edited offset。
6. 运行标准 CMake 构建；不要用 CTest 代替真实应用验证。
