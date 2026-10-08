# yousa-2.4@1.65.1.0 合规核对与实测结论

> 快照：2026-10-07。对象：`<声库搜索根>/yousa-2.4@1.65.1.0`，已解压目录形态，60 文件 / 598,641,468 字节，
> 指纹（打包时记录，2026-10-07 复核）：目录 60 文件 / 598,641,468 字节，zip 541,443,863 字节 / SHA256
> `97980256A208A819967B799EDBDD0F2846C2C8CD4FAEAC954657F94DB873BEEA`。
> 复核方式是**内容级**的：逐成员 CRC32 与磁盘文件一致、缺 0、多 0，不只看字节数。
> 顶层为 `desc.json` 与 `assets/ characters/ inferences/ linguists/`。结论仅对这份快照成立。

## 1. 结论

**满足 2.4 标准格式要求，可以分发。** 三层证据都过：

| 层 | 内容 | 结果 |
| :-- | :-- | :-- |
| 静态 | 15 个贡献（11 inference + 3 linguist + 1 singer）的声明文件与所引资源全部实存，结构合法 | 通过 |
| 语义 | 卡拍层由声库侧闭环，三语言 `maxDepth` 实测为 `Onsets` | 通过 |
| 真加载 | 无头编辑器认出包并完成一次真实合成与导出 | 通过 |
| 审计 | `TestVoicebankAudit` 以依赖闭包为语言包目录运行 | 退出码 0 |

## 2. 三层的具体证据

### 2.1 静态

- `desc.json`：`$version=1.0`、`id=yousa`、`version=1.65.1.0`、`compatVersion=1.65.1.0`、`runtimeLevel=1`，
  三条依赖 `wolf/lang-cmn@1.0.1.0`、`wolf/lang-eng@1.0.0.0`、`wolf/lang-jpn@0.0.1.0`。
  三个目标点与**发布件本体**的 `compatVersion` 逐条吻合（见 2.4）。
- 11 条 inference 的 `path` 全部存在，`interface/level/variant` 齐全（5 条 dsinfer `.inference.*` level 1 variant `onnx`，
  6 条 wolf `.wolf.inference.{S2P,Onset}` level 1）。
- 声明引用的模型、词表、音素表、embedding 共 34 条相对引用全部解析成功，无缺失。
- 3 条 linguist 固定三元组 `org.openvpi.wolf.linguist.WolfLinguist / 1 / wolf`，`configuration` 显式为 `{}`，
  role 基数正确（各含 `linguist/g2p`、`linguist/s2p`、`linguist/onset` 各一条）。
- 歌手 `characters/yousa/config.json`：`languages` 为 `cmn/eng/jpn`，三个 role 均在 imports 中存在，
  `defaultLanguage=cmn` 是键，`reservedPhonemes` 14 项互不重复，5 个 speaker 与四个模型的 `exports.speakers` 一致。
- 打包安全：无空文件、无符号链接或 junction、最长相对路径 41 字符、文件名全 ASCII、无仅大小写不同的重名、
  60 个文件无 BOM、28 个 JSON 严格 UTF-8 解码通过。

### 2.2 卡拍层（这一条过去只能静态推断，现已实测）

审计工具对每个声明语言查询 `maxDepth`，实测输出：

```
cmn -> Onsets
eng -> Onsets
jpn -> Onsets
```

并有逐词证据，可见 onset 列：

```
cmn 你 -> ni [n i] onsets . ^
cmn 好 -> hao [h ao] onsets . ^
eng hello -> hh ax l ow [hh ax l ow] onsets . ^ . ^
jpn ん -> n [N] onsets ^
```

按 `wolf/docs/linguist-domain-contract.md` 的口径，报 `Depth::Onsets` 即该语言的卡拍层被真正抵达。
语言包侧不提供 onset 是规范允许的（`linguist-distribution.md` 只把 cmn/yue 的卡拍层算在声库侧），
本包为三个语言都自带了 onset 规则模块，因此不存在"两方都不提供却又未登记"的情形。

### 2.3 真加载

无头实例（隔离的用户数据目录，声库搜索根内**只有这一个包**）：

- `voices.list` 返回 `{"display_name":"YOUSA 1.65b","package_id":"yousa","package_version":"1.65.1","singer_id":"yousa"}`。
- `tracks.set_voice` 通过，插入音符后引擎渲染 `rendered with phonemes: True`。
- `exports.audio.start` 任务 `state: succeeded`、进度 100，导出 8 秒波形 1,411,280 字节。
- 编辑器日志有真实推理记录：`Session [variance.onnx] - Finished inference in 0.126 s`、
  `Session [model.onnx] - Finished inference in 0.521 s`、`Session [vocoder.onnx] - Finished inference in 0.299 s`。

注意：`package_version` 在接口层会裁掉末尾的零段（`1.65.1.0` 报 `1.65.1`，`2.7.0.0` 报 `2.7`），
按接口返回的值传参，不要按目录名猜。

### 2.4 依赖目标点与发布件对齐

| 依赖声明 | 发布件（`assets.cmake` 对应归档） | 发布件 `compatVersion` |
| :-- | :-- | :-- |
| `wolf/lang-cmn@1.0.1.0` | `wolf-lang-cmn-1.0.1.4.zip` | `1.0.1.0` |
| `wolf/lang-eng@1.0.0.0` | `wolf-lang-eng-1.0.0.4.zip` | `1.0.0.0` |
| `wolf/lang-jpn@0.0.1.0` | `wolf-lang-jpn-0.0.1.4.zip` | `0.0.1.0` |

依赖方写目标点、提供方给区间，第四位是打包修订号，因此归档名是 `.4` 而目标点是 `.0`，两者并不矛盾。

### 2.5 依赖版本该怎么写：区间在提供方一侧

规范把"区间"放在**提供方**，依赖方只写**目标点**：

- `ds-spec-2.4.md:157-161`：`dependencies[].version` 是"要求依赖 Package 兼容到的版本，不表示必须加载该精确版本"。
- `ds-spec-2.4.md:183-186`：提供方满足 `compatVersion <= target <= version` 才算兼容该目标。
- `ds-spec-2.4.md:398`：加载时"dependency 要求的目标版本必须位于候选 Package 的闭区间 `[compatVersion, version]` 内"。
- `ds-spec-2.4.md:177`：版本各分量按任意精度非负整数比较 ⇒ 该字段是版本号。

因此**不要在依赖声明里写 `1.0.0 <= x < 2.0.0` 这类判断表达式**（推断：非版本号取值，清单不合法，
实测结论见本节末）。要防"提供方升级导致不必要不兼容"，靠的是两端配合：

| 目标 | 谁做 | 怎么写 |
| :-- | :-- | :-- |
| 将来 wolf 升到 `1.0.1.x` 仍能用 | 依赖方 | 目标点写**真正依赖的最低提供方版本**，即发布件的 `compatVersion`（本项目为 `1.0.1.0`），不要写归档的 `.4` 修订号 |
| 真正不兼容时要拦住 | 依赖方 | 不无谓抬高目标点。提供方把 `compatVersion` 抬到目标点之上（即不再承诺覆盖它）时，目标点落区间外，加载失败并给诊断 |
| 承诺"1.x 都兼容" | **提供方** | 维持低位 `compatVersion`：发布 `1.0.1.4` 时仍写 `compatVersion: 1.0.1.0`。wolf 发布件正是这么做的，本包才能既写 `1.0.1.0` 又用上 `1.0.1.4` 的归档 |

一句话：**依赖方写目标点、提供方维持低位 `compatVersion`**。想要更窄的区间只能由这两端配合表达，
不能靠在依赖字段里塞范围语法。

### 2.6 上述规则的五组实测（改声明 → 无头编辑器 `voices.list` → 还原）

被测对象：本机临时目录内的韩语副本（未入库，依赖 `wolf/lang-kor`）与构建树落点的提供方 `wolf-lang-kor`。
判定口径：`voices.list` 能列出 `singer_id` 即 LOADS，为空即 FAIL。

| 组 | 依赖方目标点 | 提供方 version / compatVersion | 结果 | 编辑器诊断原文 |
| :-- | :-- | :-- | :-- | :-- |
| A | `1.0.0.0` | `1.0.0.4` / `1.0.0.0` | **LOADS** | — |
| B | `1.0.0 <= x < 2.0.0` | `1.0.0.4` / `1.0.0.0` | **FAIL** | `invalid Package dependency: dependency version has an invalid version` |
| C | `1.0.0.0` | **`1.0.0.9`** / `1.0.0.0` | **LOADS** | — |
| D | `1.0.0.0` | **`1.0.1.0`** / **`1.0.1.0`** | **FAIL** | `failed to resolve dependency of yousa: no installed Package satisfies dependency wolf/lang-kor` |
| E | `1.0.0.0` | `1.0.0.4` / `1.0.0.0`（还原） | **LOADS** | — |

结论（事实）：**C 组正是"提供方升级不该造成不必要不兼容"的证明**（版本号涨到 `1.0.0.9`、承诺不动 ⇒ 老目标点照用），
**D 组是"真不兼容必须被拦住"的证明**（承诺抬到 `1.0.1.0` ⇒ 老目标点落在区间外，整包拒绝并给诊断），
**B 组证明范围表达式不是合法版本号**（此前标注的推导结论由此升格为实测）。

### 2.7 数据内容变更（词典/模型）与 Level 的关系

- Runtime Level 表示"Package 可以依赖的核心运行时能力"，**不是软件版本**（`ds-spec-2.4.md:204`、`:208`）⇒
  **词典更新不需要也不会改变 Level**，G2P 模块接口可以一直停在 `level 1`。
- 但 Level 不变**不等于**兼容承诺覆盖内容变更：`compatVersion` 区间内明确允许修改"模型与算法"
  （`ds-spec-2.4.md:198`）⇒ 依赖方**不能**靠它锁定 G2P 输出行为。
- 加载器只比较版本区间、**不校验文件内容**（本组实验已确认：同版本换文件内容不会被发现）⇒
  词典不兼容这件事只能由**提供方的版本承诺**表达：不兼容就抬 `compatVersion` 让老目标点失效，
  兼容就只涨 `version`。此处的政策选择已定稿（决策 D-8，用户 2026-10-07 确认），见
  [packaging-voicebank-essentials.md](packaging-voicebank-essentials.md) 第 6 节。

## 3. 审计工具的运行口径（重要，易误判）

`TestVoicebankAudit` 的"每个依赖语言都应触达 linguist"这一项，会拿 `desc.json` 里 `wolf/lang-` 的
**出现次数**与运行时触达的语言数比较。传入**整组 15 个语言包**时，三个互不相关的声库
（yousa、junninghua、zzm-kl）都会多出一个 `yue` 并报 `declared 3, reached 4` 失败。
传入该声库的**依赖闭包**时结果正确：

```powershell
# yousa 的闭包：cmn/eng/jpn 三个语言包 + 它们的依赖 g2p-pinyin、g2p-multi
TestVoicebankAudit.exe <yousa 包目录> <闭包目录>   # cmn eng jpn，declared 3, reached 3，exit 0
```

语言包的依赖边（2026-10-07 实测）：`wolf/lang-cmn → wolf/g2p-pinyin`、
`wolf/lang-eng → wolf/g2p-multi`、`wolf/lang-jpn → 无`。这也解释了为什么发布件要整组 15 个一起取：
只带 3 个语言包会直接报 `no installed Package satisfies dependency wolf/g2p-pinyin`。

**给审计工具传目录时请传闭包**，或理解为整组传入会产生"多出一个 yue"的已知假阳性。

二分定位（2026-10-07 实测）：闭包 + `wolf-lang-yue` **一个包**即复现"多出 yue"，
闭包 + `wolf-lang-zxx` 不复现（`cmn eng jpn`，declared 3 reached 3，exit 0），
闭包 + 全部 10 个额外包复现。因此来源是 `wolf-lang-yue`。该包本身只有 `inference/g2p` 一条贡献
（无 linguist），所以"多出 yue"的机制是它让 `yue` 这个语言被视为可达，而不是它提供了 yue 的 linguist，
该机制未读源码证实。

## 4. 规范版本

`ds-spec-2.4.md` 在 synthrt 与 wolf 各有一份，实测两份**同为 891 行、逐行比对恰好 51 行不同且全是
同索引替换（无插入删除）**，行号可互换引用。规范情态词漂移扫描（19 个约束词逐行计数）显示约束的
有无与强弱无一处变化，仅两处小节标题改名（`用在哪些字段` / `适用字段`，
`为什么 id 在这里而不在模块里` / `id 位于贡献条目而非模块声明中的设计理由`）。以 synthrt 版为权威。

## 5. 已知但不算不合规

| 项 | 说明 |
| :-- | :-- |
| `assets/dictionary-en.txt`（3.4 MB）、`assets/jyutping_dict.txt`、`assets/jyutping_dict_onset.json` 未被任何声明引用 | 规范未要求包内文件都被引用；同级 0913_wolf_club、zzm-kl 同样如此。合计约 3.41 MB（占包体 0.57%） |
| 模型 `languages.json` / `phonemes.json` 里含 `ko` 而本包未声明 kor | 属 onnx variant 的 configuration 内容，多余条目无效用也无害 |
| `desc.json.name` 规范未列为必选也未列为可选 | 按"未知字段不得导致拒绝"处理；同级普遍使用 |
| `compatVersion == version` | 读作声明不向下兼容，同级先例一致 |
| 目录用 `characters/` 而非规范示例的 `singers/` | 规范示例是推荐结构，同级包一律用 `characters/` |
| cmn/jpn 的 G2P 模块（在**语言包**里）省略 `exports.languages` | 规范规定该侧判定被跳过、宿主应告警，责任在语言包一侧，不是本包问题 |

## 6. 未证实

1. `configuration.dict`（歌手声明里的 `../../assets/opencpop-extension.txt`）在 2.4 下是否仍被引擎消费：
   openvpi Singer variant 的 `configuration` 语法不在已核对的规范来源内。同级所有包都保留该键。
2. 语言包 rev3（本地构建）与 rev4（发布件）的差异已量化：67→70 文件，15 个文件内容不同，
   rev4 多出 `wolf-lang-zxx` 的 4 个文件。`compatVersion` 与三个依赖目标点未变，但"两侧语义完全等价"未逐字段证实。
