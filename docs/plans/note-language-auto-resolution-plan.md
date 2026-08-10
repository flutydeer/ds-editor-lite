# 音符语言 auto 解析与歌手默认语言跟随方案
> 状态：🔵 Phase 1–2 已实现+构建通过（待用户冒烟 2026-08-11）；Phase 3 收窄为"无需代码改动，见设计决策+Phase 3 段落"

## TL;DR

音符的语言不再在**创建时**写死，而是默认"未指定"(auto)，推理时沿 `音符 → 片段默认语言 → 歌手包默认语言` 解析。新绘制音符永远 auto，不烘焙冗抉。`unknown` 保留为**独立的、G2p 检测失败而产生的终端语义**，绝不与 auto 混淆、不自动解析。UI 采用 SynthV 模式：选歌手后音符语言显示为歌手默认带"(默认)"来源标记；无歌手时语言选项禁用。

## 背景

当前体验痛点：默认未指定歌手时绘制音符，音符被写成 `m_language = "unknown"` + 歌词 `la`；选择歌手后这些音符语言仍是 `unknown`，推理时 S2P 语言加载失败应有尽有 → 音符无声。用户被迫全选音符 → 右键 → 手动设置语言与歌词。

核心根因：**语言在创建音符时被写死(烘焙)**，而歌手是之后才选择的，两者时刻脱节。

## 当前代码观察

- `Note.h:93`：`QString m_language = "unknown";`
- `Note.cpp:205-227 serialize`：无条件写入 `obj["language"] = m_language;`；`Note.cpp:236 deserialize` 缺省也是 `"unknown"`。
- `Note.cpp:145-151 language()`/`setLanguage()` 无解析，原始字符串直出/直存。
- `PianoRollGraphicsViewHelper.cpp:29-30 drawNote()`：创建音符时把 `singingClip->defaultLanguage()` 快照进 `note->setLanguage(...)`，歌词取 `defaultLyricForLanguage(...)`。
- `DrawNoteHandler.cpp:150-152 prepareForDrawingNote()`：预览态同样按 clip 默认语言取歌词（预览音符 `m_currentDrawingNote` 不落库）。
- `SingingClip.h:111`：`Property<QString> m_defaultLanguage{"unknown"};`
- `AppModel.cpp:207-209`：载入时仅当 clip 语言 empty/unknown 才从 track 兜底；`newProject()` 建的 clip 没有兜底逻辑，直接保持 `unknown`。
- 推理消费音符语言的地方：
  - `InferController.cpp:56-71 buildNoteInferenceSnapshots()` → `snapshot.language = note->language();`（注意：这是快照构建，重构点）
  - `InferInputNote.cpp:14`：`languageDictId = note.language();`（播放/渲染语义的 TODO 注释在此：`// TODO: language dict id form singer info`）
  - `GetPronunciationTask.cpp:148`：`langGroups[note.language]`，G2p 分组。
  - `GetPhonemeNameTask.cpp:151`：`convertS2p(identifier, input.language, ...)`。
  - UI 读取点：`PianoRollGraphicsView.cpp:249`、`PianoRollRhiWidget.cpp:2649`、`PhonemeView.cpp:625`、`PhonemeEditorDialog.cpp:15`、`PianoRollContextMenuController.cpp:118-130`。
- `AppGlobal.h:27`：`languageNames = {"cmn","eng","jpn","yue","unknown"}` —— `unknown` 是一个被广泛识别的**伪语言项**。
- `SingerInfo::defaultLanguage()` 来自声库包 manifest 的 `singerSnapshot.defaultLanguage`（`PackageManager.cpp:242-247`），非推算。`SingerInfo::languages()` 是歌手支持语言列表(每种有自己的 g2p/dict)。
- `ClipEditorToolBarView.cpp:509-512 refreshLanguageComboPresentation()`：片段语言组合框在片段语言 empty/unknown 时把 `singerInfo.defaultLanguage()` 写入片段（**latch 逻辑已存在**）。
- `unknown` 的可信产生路径：FillLyric(`G2pService.cpp:77-79,138-140` 经 `TextTagger::tag()` 检测失败时保持 `"unknown"`；`TextTagger.cpp:58,82` 仅匹配成功才改写)。

## 已定的设计决策

### 三段语义（绝对区分）

| 状态 | 含义 | 产生时点 | 推理行为 |
|------|------|----------|----------|
| `""`(auto) | 未指定，跟随语境 | 默认；新绘制音符 | 解析为片段/歌手默认语言，正常发声 |
| `unknown` | 语言确实未知(检测失败/终端错误) | G2p 检测失败；用户手动指定 | **不自动解析**，维持失败语义 |
| `cmn`/`eng`/`jpn`/`yue` | 显式指定 | 用户设置 / FillLyric 检测成功回填 | 按指定语言走 G2p/S2p |

### 解析链（推理时惰性，不写回）

```
Note::effectiveLanguage()
  = 显式(非空且非unknown) ? m_language
    : SingingClip::effectiveDefaultLanguage()

SingingClip::effectiveDefaultLanguage()
  = 显式(非空且非unknown) ? m_defaultLanguage
    : singerInfo().defaultLanguage()   // 声库包声明
    : track-default / 应用默认          // 兜底链
```

要点：
- **只在推理快照构建处解析一次**（`buildNoteInferenceSnapshots`），下游任务全部拿到已解析的具体语言，`unknown` 音符行为与现状完全一致。
- 改变歌手 → 回忆析 → auto 音符全量跟进，无需数据迁移。

### 片段语言组合框语义（保持原设计 + 不越界）

- 组合框 = 片段默认语言模板，只影响**新音符**以及**仍处 auto 的音符**；显式覆盖的音符永远不会被动。
- 选择歌手时，仅当片段语言为空/unknown 才 latch 歌手 `defaultLanguage`（现有逻辑保留）。
- **不做**混合态 UI（如"(多个数值)")：组合框代表片段默认，按定义单值；多语种由音符级覆盖表达。
- 批量改全片交给已有"全选 → 右键语言"交互，组合框不背这个责任。

### UI 呈现（SynthV 模式）

- 音符语言显示解析后的值；解析结果 == 当前歌手默认语言 时追加标记 `(默认)`，其余(用户/片段显式设置)不追加、`unknown` 显示"未知"。
- 音符右键语言菜单：无歌手/歌手未解析 → 禁用或无选项；有歌手 → 列出 `singerInfo.languages()`，默认项勾选并显"(默认)"字样。
- （可选，后续实现）组合框 tooltip/文案说明"该语言将用于新绘制的音符"。

## 分阶段计划

### Phase 1：Note 层 auto 语义 + 推理单点解析

目标：音符默认语言改为 auto，推理按语境解析；行为对显式语言/unknown 保持不变。交付：写音符→选歌手→出声音。

实施内容：

- `Note.h/.cpp`：
  - `m_language` 默认改为 `""`。
  - 新增 `QString effectiveLanguage() const`：
    ```
    返回 m_language.isEmpty() ? (clip ? clip()->effectiveLanguage() : QString())
                              : m_language;
    ```
    同步新增 `SingingClip::effectiveDefaultLanguage()` 实现解析。
- `SingingClip.cpp`：
  - `m_defaultLanguage` 默认保持 `"unknown"` 语义但运行时按 `effectiveDefaultLanguage()` 处理；避免破坏 load 兜底路径（可先加函数，默认值改动放 Phase 3 一起）。
- 修改 `InferController::buildNoteInferenceSnapshots()`：`snapshot.language = note->effectiveLanguage();`（唯一解析点）。
- 修改绘制路径：`PianoRollGraphicsViewHelper::drawNote()` 删除 `note->setLanguage(...)`（离开默认 auto）；歌词仍按当前解析语言 `defaultLyricForLanguage(effectiveLanguage)` 给。
- 同步将 `InferInputNote.cpp:14`、`GetPronunciationTask.cpp:148` 改用入参已解析值（此处是从快照读，非从 Note 读，需核对数据流）。

验收点：

- 新绘制音符 `language()` 恒为空(auto)。
- 未选歌手：auto 音符无语言可选（菜单禁用）。
- 选歌手后：auto 音符立即能推理出声(无需手动改语言/歌词)。
- 显式/unknown 音符行为与现状一致。
- Debug 构建通过；冒烟：启动→选歌手→铅笔画音符→验证可以推理。

### Phase 2：UI 来源标记 + 语言菜单跟随歌手

目标：呈现与 SynthV 一致。

实施内容：

- 语言显示点(`PhonemeView.cpp:625`、`PianoRollGraphicsView.cpp:249`、`PianoRollRhiWidget.cpp:2649`、`PhonemeEditorDialog.cpp:15`)改用 `effectiveLanguage()` + 标记判断：
  `note 无覆盖 && effective == 当前 singer defaultLanguage()` → 附 `(默认)`。
- `PianoRollContextMenuController.cpp:118-130` 语言子菜单：
  - 无歌手 → 禁用；
  - 有歌手 → 只列 `singerInfo.languages()`，默认项加"(默认)"。
  - 保留 `unknown` 手动项(终端态，与 auto 区分)。

验收点：

- 语言菜单状态与歌手/解析一致。
- `(默认)` 标记逻辑正确(显式片段语言时不出标记)。
- i18n 新串进 `.ts`。

### Phase 3：序列化兼容（已并入 Phase 1）

实施结果与最终决策：

- `Note::serialize()`：auto 不写字段；显式/unknown 照写。✅（Phase 1 内完成）
- `Note::deserialize()`：缺省/空 → auto；`"unknown"` 保持。✅（Phase 1 内完成）
- `SingingClip` 默认语言字段**保持 `"unknown"` 不改**（最终决策）：clip 层 `unknown` 无检测失败语境，`effectiveDefaultLanguage()` 已将其视为 auto → 行为正确；若把默认值改为 `""` 会让 `defaultLanguage()` 对新建片段改为跟随 parent，并牵动序列化，收益小而涟漪大，不值得。
- 老文件兼容：音符 `unknown` 按既有语义保留、不自动解析（用户明确要求区分），需人工处理；这是设计决定，不是缺陷。

验收点：

- 保存→重载：auto 音符仍为 auto 且能发声。
- 老文件(所有音符 language==unknown)打开后语义不变，不自动改写。
- `git diff --check` 无输出。

## 明确不做

- 不把 `unknown` 当作 auto；不自动解析 unknown。
- 不给片段语言组合框做"(多个数值)"混合态。
- 不用组合框批量改写整片音符语言(保留"全选→语言"通道)。
- 不改 dspx 规范/外部格式的 Note 语言字段语义(仍按 OpenDSPX 原样写）。
- 不引入"该语言是否用户手动设置过"的持久化标志位（auto 用空串表达，无需标志）。

## 建议下一步

先做 Phase 1。理由：单个逻辑改动(Note effectiveLanguage + 快照解析点 + 取消绘制烘焙)，行为对显式/unknown 零回归，能直接消除核心痛点；完成后即可验证"选歌手即出声"。

## 开放问题(实施时逐个解决)

- 片段默认语言与 SingerInfo 的 latch 时机细节（组合框 vs track 选歌手入口）。
- 无歌手也无兜底时 `effectiveLanguage()` 返回空串导致 UI 空白 → 显示回退"未知"。
- `InferInputNote::languageDictId` 的语义（当前有 TODO：应取歌手 dict）是否顺带修正。
- 预览音符(DraWNoteHandler)的歌词取值与最终 commit 保持一致。
