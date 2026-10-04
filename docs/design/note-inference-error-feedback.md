# 音符推理错误可视反馈

## 背景

音符存在错误时不进入推理流程，最终表现为不发声。此前错误信息在链路上被层层丢弃，用户只看到音符安静，没有任何界面反馈。

本文描述错误反馈的完整链路：分段器产出排除诊断，任务层补充失败详情，片段模型保存瞬态错误地图，钢琴卷帘画角标并提供原因 tooltip，时间轴为被跳过的乐句补红条。

## 数据链路

### 分段诊断（SingingClipSlicer）

`SliceResult` 新增两类诊断，由 `SlicingClipSlicer::slice` 产出，函数保持纯函数：

- `excludedNotes`，`QList<ExcludedNoteInfo>`（noteId + 原因枚举）。只记根因音符，不含被连坐的乐句邻居。原因四类：
  - `MissingPhonemes`，普通音符的音素结果为空（乐句内所有肇事音符都会记入，不只第一个）
  - `FirstNoteInvalid`，乐句以 Slur 或缀字音符开头
  - `UnassignedSyllabification`，缀字音符未分配到音素（原 `hasUnassignedSyllabificationNotes` 改造为返回肇事列表）
  - `Overlapped`，重叠音符在过滤阶段单独记入，全部被过滤时另报一个跳过范围
- `skippedPhraseRanges`，被跳过乐句的本地 tick 范围（首音符 start 到末音符 end）

### 任务失败详情穿透

`GetPhonemeNameTask::getPhonemeNames` 的每个失败分支填写 per-note 的 `errorMessage`（语言模块未就绪、歌手不可用、音素模块加载失败、发音转换失败、发音无音素）。`PhonemeNameResult` 与自动化 DTO `InferencePhonemeNamesDto` 增加 success/errorMessage 字段，`phonemeNameMutation` 填充，`preparePhonemeNames` 透传并在 apply 中构建 noteId 到消息的映射。G2P 的 errorType 本期不穿透，G2P 失败回退原词后级联表现为 S2P 失败，S2P 详情足够定位。

### 片段瞬态错误地图（SingingClip）

- `noteInferenceErrors()`，`QHash<int, NoteInferenceErrorInfo>`（reason + detail）
- `skippedPhraseRanges()`，跳过范围列表
- 信号 `noteInferenceErrorsChanged()`

维护规则：

- 唯一发布点是 `InferenceAutomationAdapter::preparePhonemeNames.apply`，顺序为 updatePhoneName、setPendingNoteTaskErrors、reSegment。reSegment 内部用分段诊断重建地图并与任务详情合并，变化才发信号。prepareResegmentClip 的 reSegment 调用自然刷新切片侧诊断
- `notifyNoteChanged` 中会推进推理修订的变更类型清除受影响音符的条目，编辑根因音符立即消角标，下个推理周期重新评估
- `notifyEffectiveVoiceContextChanged` 推理输入变化时清空全部
- 全部瞬态，不序列化、不进撤销

## 呈现

### 角标

音符上方悬挂 Fluent `dismiss_circle_16_regular`（新图标经 `tools/sync_fluent_icons.py` 同步，qrc 挂载），左缘与歌词内边距对齐，镜像发音视图的下挂方式，逻辑尺寸 14px，与音符保持 2px 间距。颜色读独立 token `piano.roll.noteErrorMark`（lite-dark 初始 #FF9B9D，lite-light 初始 #B3262E，可自行调值）。

- Legacy 后端：徽标是挂在 NoteView 下的子图形项 `NoteErrorBadgeItem`（发音视图同款思路），不接受鼠标事件，视图在 `noteInferenceErrorsChanged` 时通过 `syncNoteInferenceErrors` 推送可见性
- RHI 后端：`EditorGlyphAtlas` 新增 `appendImage`（与字形块同一分配与染色约定），`appendNoteErrorBadges` 在 `appendClipMask` 之后绘制，主题切换走已有的 themeChanged 重绘连线

徽标在音符矩形之外，两个后端的悬停与点按命中都不经 `noteAt`/`noteViewAt`，而是遍历有错误的音符直接测试徽标矩形（`errorBadgeAt`）。

### 原因 tooltip

新建 `NoteErrorToolTipController`（仿歌词 tooltip），标题为错误类别短语（`noteInferenceErrorTitle`，如"无法生成音素"、"音符重叠"），正文为一行说明，取舍规则是任务详情存在时用详情（语言模块未就绪、歌手不可用、音素模块加载失败、发音转换失败四类），否则用 `noteInferenceErrorText` 的通用文案。歌词与发音文本都在音符本体上展示，tooltip 不再重复。ToolTip 标题标签显式居左，与正文共用左缘。两个后端各持一个实例，角标命中优先于歌词截断 tooltip。鼠标悬停角标或触屏点按角标显示，离开、滚轮、按下、双击隐藏。

### 时间轴红条

`TimelineView::drawPieces` 对 `skippedPhraseRanges` 画红条，样式与 piece 状态条一致（底部 2px 圆头），复用 `pieceFailedColor`。被跳过乐句没有 piece，这条红条补齐时间轴语义。

## 行为边界

- 角标与红条反映上一次推理周期的结果，与 piece 状态条同源出现，首次加载需等首次推理
- 只标根因，被连坐的邻居无角标，由时间轴红条覆盖范围感知
- 任务详情被推理门控丢弃时（文档已过期）不发布，避免陈旧标记
- G2P 回退成功（音符仍发声）的情况本期不标

## 测试

`TestSyllabification::testSlicerExclusionDiagnostics` 覆盖重叠、缺音素、首音 Slur 三类诊断与跳过范围，以及 reSegment 发布到 clip、编辑音符清除条目。
