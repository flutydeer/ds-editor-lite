# 音素时长还原（Phoneme Offset Reset）计划

> 状态：⬜ 未实施（2026-08-24 方案讨论稿。对应工单「音素时长还原功能」，腾讯文档记录 `rzuFxl`，状态 待处理）
>
> 工单描述原文：*"建议是右键，要考虑还原粒度、dspx支持、级联还原等细节"*（类型：建议；进度：待处理）

## 背景

音符的音素时长以 **offset 序列**保存（每个音素相对词根音符起点的毫秒偏移），双层结构：
原始模型推理值（`original`）与用户手动编辑值（`edited`），`result()` 取 `edited` 非空时的值。

**音素时长还原** = 右键一个或多个音符，清空其 `edited` offsets，回退到时长模型推理出的
`original` offsets。

单纯清空会引入音素重叠。例如：目标词 A 之后无缝紧贴词 B，B 的第一个音素被用户往前方移动
很大长度、进入了 A 的内部；此时若清空 A 的 `edited`，A 的音素回到模型位置后，A 末尾音素与
B 的首音素在时间轴重叠。需要**级联清空**：当清空某词的 `edited` 后其相邻词仍是 edited 且侵入
边界时，继续清空相邻词，直到操作完成后没有任何音素重叠。

## 当前代码观察

### 音素只挂在词根上、只有词根可编辑

- `Note::canEditPhonemes()`（`src/libs/ProjectModel/AppModel/Note.cpp:194-197`）
  = 非 AP/SP 且非 slur 且非 syllabification。
- `PhonemeView::buildPhonemeList()`（`src/app/UI/Views/ClipEditor/PianoRoll/PhonemeView.cpp:589-610`）
  跳过 `isSlur() || isSyllabification()` 的音符，词尾 `noteEndTick` 沿 slur/syllabification
  成员链扩展。
- `Phonemes`（`src/libs/ProjectModel/AppModel/Phonemes.h:31-41`）：
  `PhonemeOffsetSeq{original, edited}`，`result()` 逻辑为 `edited` 空则取 `original`。

→ 一个音素序列的单位是**词**（词根 + 全部成员链），offsets 集中在词根音符上
（`Note.h:106 m_phonemeInfo`）。还原粒度天然是"词"，不是任意单个音符。

### on-set = 卡拍音素（通常是元音），是音节切分点

- `Syllabification::splitSyllables`（`src/app/Modules/Inference/Tasks/Syllabification.cpp:13-32`）
  用 `isOnset` 切分音节——每个 on-set 音素开启一个新音节。
- `distributeForInference` / `collectForStorage`（`Syllabification.cpp:115-191`）把词根的整词
  offsets 按音节分发给成员音符（推理时各自平移到相对本词根的偏移，可出现负偏移），存储时
  再归拢回词根。

### 由此得到"重叠"的正确语义

模型 `original` 布局本身允许负偏移（词首辅音抢在词头前 / 无 on-set 元音前置），引擎靠共享边界
（`InferTaskHelper.cpp:170-187`）消化，这是**合法基线**。真正的"重叠"是用户手写的
`edited` 偏移越过了相邻词边界。因此判据只对 **edited 状态**生效：

> 右词 B 处于 edited，且 B 首音素绝对起点 < 左词 A 的词尾 tick ⇒ B 与 A 重叠。

注意不按"首音素是不是 on-set"来判——辅音/元音一视同仁（模型负偏移本来就可能不是 on-set），
只拒绝"edited 值越过词尾"；模型 `original` 负偏移一律视为合法基线、不参与判据。

### 必须绕开的坑：normalizer 的判词尾与 PhonemeView 不一致

`SingingClipPhonemeNormalizer::buildEffectiveNotes`（`SingingClipPhonemeNormalizer.cpp:116-140`）
只用 `isSlur()` 扩展词尾，**没有包含 `isSyllabification()`**；而 `PhonemeView` 的"词" =
slur ∪ syllabification 成员链。实现级联还原时**不能复用 normalizer 的 effective-end 计算**，
要按 PhonemeView 的逻辑单独算词尾，否则带 `++++` 分字音符的词会被误切成两个"词"。

### 现有可复用机制

- `SingingClipPhonemeNormalizer::ResetRecord` / `restoreEditedOffsets()` /
  `notesFromResetRecords()`（`SingingClipPhonemeNormalizer.h:15-19, 50-51`）——undo 恢复。
- `PhonemeView.cpp:620-653` 逐音素构建 view model 的偏移解析方式。
- 音符右键菜单 Note 分支（`PianoRollContextMenuController.cpp:125-183`），"Edit Phonemes..."
  现有 `context.phonemeEditorEnabled` 只允许单词根（`PianoRollRhiWidget.cpp:2814-2815`）。
- dspx 双层往返已支持（`DspxPhonemeCompat.cpp:135-192`，original/edited + workspace snapshot）。

## 方案

### 1. 入口

- 在 `PianoRollContextMenuController` 的 Note 分支加菜单项 `Reset Phoneme Durations`
  （中文：还原音素时长，走 i18n 管线），置于 "Edit Phonemes..." 附近。
- 作用于 `context.selectedNoteIds`（多选生效，与 Language / Delete 一致）。
- 只对**词根**（`canEditPhonemes()`）生效；集合内任一词根 `offsetSeq.isEdited()` 才可点。
- slur / syllabification / AP / SP 自动跳过；original 为空（外部 dspx 单层文件）的词根不还原。

### 2. 重叠判据（确保还原后无重叠）

- 单位：词（词根 + 成员链），词尾按 PhonemeView 语义计算（slur ∪ syllabification 成员扩展）。
- 判据（SKILL 正文权威版，只对 edited 态判）：右词 B **生效**首音素绝对起点 <
  左词 A **还原后**末音素绝对起点（`A.start + A.origOffsets.last()`），且 B 当前 edited。
- 首音素绝对起点 = `timeline.tickToMs(B 词根 globalStart) + B resultOffsets[0]`，换算回 tick。
- 不判模型 `original` 负偏移（合法基线）；`original` 为空的词根永不判、不还原。

### 3. 级联算法（向右传播，必收敛）

```
resetting = 选中集内的有效词根（去重，按时间序）
对 resetting 中每个词根 R（按时间序遍历）：
    B = R 的右邻词根（下一个词）
    while B 存在 且 B.hasEdited 且 判据(B 首音素侵入 R 词尾):
        resetting 追加 B      // 清 B 后其 original 可能仍压 R 尾——少见，接受为基线
        R = B; B = R 的右邻词根
边界：
  - 每词至多入集一次（QSet 去重）→ 沿着"词链"向右、有限长度，必然收敛。
  - B 清空回 original 后若仍侵入（无 on-set 元音词负偏移大于左词尾富余）→ 停止，
    模型基线视为合法输入（与 InferTaskHelper 共享边界语义一致）。
对 resetting 统一执行：
  for each 词根 note:
      ResetRecord{note, editedOffsets = note->phonemeOffsetSeq().edited}
      note->setPhonemeOffsetSeq(Note::Edited, {})
  notifyNoteChanged(EditedPhonemeOffsetChange, notesFromResetRecords(records))
```

要强调：级联先算闭包（与清空顺序无关，因为判据只依赖 B 的当前 edited + R 的词尾长度，
两者都不随"清空"改变），再统一执行、统一 undo。

### 3.1 为什么不需要向左级联（左侧不受影响）

判定重叠的方向性是**右词侵入左词**（右词首音素起点 < 左词词尾）。还原只改动目标 R 自身的
音素位置（回 original），不改变 L 的词尾。清 R 后唯一的"左侧影响"候选是
R 首音素（original 值）< L 词尾：

- `R.original[0] >= 0`（有卡拍 on-set 元音在词内）→ 无影响。
- `R.original[0] < 0`（无 on-set 元音词，模型把首音素抢在词头前）→ 模型基线形态，本方案
  不判为重叠，且不是还原引入的新状态（编辑前 PhonemeView 显示的就是该形状）。

即便想清 L 也不生效：L 词尾固定，清 L 只重组 L 内部音素排列，无法把 R 的音素右移；
消除"R 首侵入 L"只能改 R 的 offsets，与还原语义矛盾。故左侧无需级联。

### 3.5 外溢确认对话框（硬性要求）

当级联闭包集合**大于用户选中部分**（即存在外溢：级联清到的相邻词超出了用户直接选中的
词根），在执行前弹出**确认对话框**，明确告知用户影响范围：

- 对话框内容：逐个列出被顺带还原的相邻词根（第几个 / 歌词 / 起始 tick），并说明原因——
  「还原该词后其相邻词音素重叠，将一并还原」。同时给出影响范围汇总
  （如「将还原 N 个词：用户选中 X 个，因重叠顺带还原 M 个」）。
- 按钮：`还原` / `取消`（默认焦点在「还原」，避免打断流程；取消则不执行任何修改）。
- 仅当存在外溢时弹出；纯选中集内自清空（无外溢）不弹窗，直接执行，保持轻快。
- 独立于 undo：无论是否经过确认对话框，执行后仍可 `Ctrl+Z` 完整撤销（含级联词）。
- 文案走 tr + lupdate，长度需适配多选较多词的场景。

外溢的精确判定：`级联闭包集合 \ 选中词根集合 ≠ ∅` 即视为外溢。

### 4. 数据操作与 undo

- 新 action：`src/app/Controller/Actions/AppModel/Note/ResetPhonemeOffsetsAction.{h,cpp}`
  持有 `QList<ResetRecord>`。
- execute：对还原集词根 `setPhonemeOffsetSeq(Note::Edited, {})` + notify。
- undo：`restoreEditedOffsets(records)` + notify。
- `ClipController` 新增入口 `onResetPhonemeOffsets(const QList<int> &noteIds)`，
  经 `historyManager->record` 记录（与 `onAdjustPhonemeOffset` 一致，
  `ClipController.cpp:275-284`）。

### 5. dspx

已支持，无需改动。还原后 `edited` 清空，保存 dspx 只写 `original`，重开无损。
唯一边缘：外部 dspx 只写 effective 单层（进 `edited` 而 `original` 为空）→ 该词根禁用还原。

### 6. 测试

- 新增 `src/tests/TestPhonemeOffsetReset/`（沿用 `tests/TestSyllabification/main.cpp`
  手工构造 Note/Timeline 的风格）。
- 覆盖：单词根清空；多选词根清空；无缝相邻 B 侵入 A → 级联清 B；三级链 A→B→C 级联；
  有 gap 不级联；slur/syllabification 成员链 word-end 计算（含 `++++` 分字）；
  original 为空不还原；undo 恢复；dspx 往返后还原基线仍在。
- 加入 `src/tests/CMakeLists.txt`，构建时拾取（注意 GLOB_RECURSE 需 reconfigure）。

### 待拍板点

1. 判据下界：`B 首音素起点 < A 词尾`（推荐，数据层重叠）——
   是否还要叠加"视觉穿序"下界（`B 首音素 < A 末音素起点`）？暂按前者，后者不引入。
2. 外溢定义与对话文案细节：外溢判定已定（闭包 \ 选中 ≠ ∅）；具体显示字段
   （歌词 / tick / 序号的取舍与排序）待 UI 评审。
3. 无基线 note（外部 dspx 单层）：禁用还原（推荐）vs 降级为清空。
4. 文案：`Reset Phoneme Durations` / 还原音素时长；外溢对话扩句（走 tr + lupdate）。

## 验收点

- 右键单个/多个词根可还原，编辑入口约束（仅词根、有 edited）正确。
- 级联向右传播、无死循环、undo 完整恢复（含级联词）。
- 存在外溢（级联清到的词超出选中集）时弹确认对话框、列出影响范围，取消则不执行任何修改；
  纯选中集内自清空不弹窗。
- 词尾按 PhonemeView 语义（slur ∪ syllabification 成员）计算，不误切 `++++` 词。
- 不判模型 original 负偏移为重叠。
- dspx 往返后还原基线仍在；外部单层 dspx 的 note 禁用。
- clang-format 改动文件；Debug 构建 DsEditorLite 通过；ctest 相关用例通过。
