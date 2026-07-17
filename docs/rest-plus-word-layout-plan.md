# Rest / Plus / Word Layout 整理计划

## 背景

当前应用从音符编辑到推理输入之间已经有多层语义：

- Note 层：保存音符时间、音高、歌词文本、发音/音素结果。
- Word / lyric 层：决定多个音符如何组成一个词、音节或延续关系。
- Phoneme 层：保存实际送入 G2P、Duration、Pitch、Variance、Acoustic 的音素序列和 offset。

最近的 phoneme offset normalizer 已经把失效的手动音素时长清理逻辑收口到模型层，但 rest、plus、word grouping 的语义仍然分散。下一步应先整理低风险的 rest 判断，再为 plus 和 word layout 留出正确抽象位置。

## 当前代码观察

### Rest 判断仍然分散

目前 `SP` / `AP` 判断至少出现在以下位置：

- `SingingClipSlicer.cpp` 内部 lambda `isRestNote`
- `InferInputNote.cpp` 构造 `isRest`
- `GetPronunciationTask.cpp` 跳过 rest / slur
- `GetPhonemeNameTask.cpp` 跳过 rest，并把 rest pronunciation 转为 phoneme
- `SingingClipPhonemeNormalizer.cpp` 内部 lambda `isRestNote`
- `InferTaskHelper.cpp` 通过 `InferInputNote::isRest` 处理推理 word
- `InferDurationTask.cpp` 用 `SP` / `AP` 判断 rest phoneme

这些判断的目标大体一致，但入口分散，后续维护容易出现规则偏差。

### Plus 不应先收口到 Note 层

当前 plus 逻辑主要在 `GetPhonemeNameTask`：

- `isPlusNote()` 只判断歌词是否完全由 `+` 构成。
- `checkTrailingPlus()` 支持 `lyric+` / `lyric++` 这类尾随 plus。
- `distributePhonemes()` 会把英文等多音节 phoneme groups 分配给主歌词音符和后续纯 plus 音符。

因此 plus 不是简单的 Note 类型。`+`、`++`、`lyric+`、`lyric++` 语义不同，直接添加 `Note::isPlus()` 会过早固化错误抽象。

### Slicer 应保持 deterministic

分片器不应依赖 edited phoneme offset。分片结果必须只依赖稳定输入，例如 note 时间、歌词、音素数量、tempo 和固定 padding 策略。

原因：

- edited offset 依赖历史编辑和推理结果。
- 如果 slicer 反向依赖 edited offset，会造成分片状态依赖历史路径。
- 工程重新打开后可能得到不同分片，破坏模型确定性。

因此后续 layout / normalizer 可以消费 slicer 结果，但不应让 edited offset 反向影响 slicer。

## 阶段计划

### Phase 1：统一 rest 判断

目标：低风险收口 `SP` / `AP` 判断，不改变行为。

实施内容：

- 在 `Note` 上新增 `bool isRest() const`。
- 规则固定为 `lyric().trimmed() == "SP" || lyric().trimmed() == "AP"`。
- 替换当前直接判断 `SP` / `AP` 的 Note-level 逻辑：
  - `SingingClipSlicer`
  - `InferInputNote`
  - `GetPronunciationTask`
  - `GetPhonemeNameTask`
  - `SingingClipPhonemeNormalizer`
- `InferTaskHelper` 保持使用 `InferInputNote::isRest`，不直接依赖 `Note`。
- `InferDurationTask` 对 phoneme token 的 `SP` / `AP` 判断暂不改，因为它处理的是 phoneme 层 token，不是 Note 层 rest。

验收点：

- 行为不变。
- 所有 Note 层 rest 判断统一走 `Note::isRest()`。
- 不引入 plus 相关抽象。
- 运行 `clang-format -i`。
- Debug 构建 `DsEditorLite` 通过。

### Phase 2：新增 lyric token parser，但不改变行为

目标：把 plus、slur、rest 的字符串解析集中起来，为 word grouping 做准备。

建议新增轻量结构，例如：

```cpp
class LyricToken {
public:
    QString raw;
    QString text;
    int trailingPlusCount = 0;
    bool isAllPlus = false;
    bool isSlur = false;
    bool isRest = false;
};
```

实施原则：

- parser 只负责解析字符串，不负责语言学解释。
- `lyric+` 应解析为 `text = "lyric"`，`trailingPlusCount = 1`。
- 纯 `+` / `++` 应解析为 `isAllPlus = true`，并保留 plus 数量。
- `SP` / `AP` 应解析为 rest。
- `-` 或当前 slur 规则应单独标记，不和 plus 混用。
- 初期只替换局部重复解析，不改变 `GetPhonemeNameTask::distributePhonemes()` 的行为。

验收点：

- `GetPhonemeNameTask` 中的 `isPlusNote()` / `checkTrailingPlus()` 可迁移到 parser。
- plus 分配结果保持不变。
- rest 仍以 Phase 1 的 `Note::isRest()` 为 Note 层来源。

### Phase 3：建立 word grouping / layout

目标：明确 Note 序列如何转换为 word / syllable / phoneme 布局，减少 slicer、phoneme editor、inference input 各自推断。

建议新增一个独立 helper，例如 `SingingClipWordLayoutBuilder`，输入稳定 note 序列和 lyric token，输出：

- authored rest word
- generated gap rest word
- generated padding rest word
- lexical word group
- pure plus continuation group
- slur continuation group
- 每个 group 对应的 note 范围
- 每个 group 是否需要 G2P / duration / acoustic input

原则：

- layout 可以消费 slicer 的 deterministic segment。
- layout 不改变 slicer 分片。
- layout 不依赖 edited phoneme offset 来决定 segment。
- edited offset 只用于 phoneme timing validation，不用于 word grouping 决策。

验收点：

- `GetPhonemeNameTask` 的 plus 分配可以逐步迁移到 word grouping。
- `InferTaskHelper::buildWords()` 可以消费更明确的 word layout，减少内部临时推断。
- PhonemeView / Normalizer 后续可共享同一套 effective note / word 边界。

### Phase 4：推理输入和 UI 逐步消费统一 layout

目标：降低约束分散，避免 UI、normalizer、inference input 各算一套合法性。

实施方向：

- `PhonemeView` 用 layout 提供的 effective note / word boundary 计算可拖动范围。
- `SingingClipPhonemeNormalizer` 用 layout 判断 edited offset 是否仍合法。
- `InferTaskHelper` 用 layout 生成 words，并保留 hard validation。
- hard validation 继续遵循 let it crash：问题数据直接 `qFatal()`。

验收点：

- PhonemeView、Normalizer、InferTaskHelper 对同一 note 序列得到一致边界。
- 手动 edited phoneme offset 不影响 slicer 分片。
- 导入、粘贴、移动、resize、split 后的行为稳定。

## 明确不做

- Phase 1 不处理 plus。
- 不新增 `Note::isPlus()`。
- 不让 slicer 依赖 edited phoneme offset。
- 不把 authored rest 和 generated gap/padding rest 在 word 层完全混为一谈。

## 建议下一步

下一次实施建议只做 Phase 1：统一 `Note::isRest()`。

原因：

- 范围小。
- 行为变化风险低。
- 能立刻减少重复判断。
- 不会提前固化 plus 的复杂语义。
