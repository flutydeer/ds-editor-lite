# 用 wolf 的 `wolf-lang-kor` 给 2.4 声库补韩语：链路跑通，音素不可用（实测记录）

> **更正（2026-10-07，后续实测）**：本文原先说"韩语可配音、实测通过"，**那只表示链路跑通并导出了波形**，
> **不代表音素正确**。后续用 [tools/measure-g2p-output.ps1](tools/measure-g2p-output.ps1) 读回音素后发现：
> `wolf-lang-kor` 的词典键是**字母串**（形如 `ㄱㅏ`），而链条里没有"谚文/罗马字 → 字母"的分解步骤，
> 导致常用词大多命中不了词典、落到 multig2p 模型回退，输出为退化产物。**结论修正为：韩语链路可用但
> 音素质量不可用，不能声称支持韩语。** 详见文末"实测更正"一节。

> 快照：2026-10-07。对象是 `yousa-2.4@1.65.1.0` 的**副本**（本机临时目录内，未改动交付成品）。
> 结论只对这份副本与当时的语言包集合成立。

## 1. 结论

**结论（2026-10-07 更正后）：链路可用，音素不可用，不能声称支持韩语。** 用发布件里的 `wolf-lang-kor`，
确实配出了编辑器要的 linguist 与推理链，在无头编辑器里用韩语歌词真渲染并导出成功。但读回音素后发现
词典键与歌词字形对不上（见文首更正块与文末"实测更正"一节），所以这条路径**当前不可用**。
全程只需一处**包内数据修正**（见第 4 节），不改任何代码。

## 2. 依据

### 2.1 wolf 侧具备的部件（`wolf-lang-kor@1.0.0.4`）

| 部件 | 内容 |
| :-- | :-- |
| linguist | `kor-romaja`，固定三元 `org.openvpi.wolf.linguist.WolfLinguist / 1 / wolf`，`language=kor`、`scheme=romaja`、`openSet=true`、34 个音素，`configuration: {}` |
| inference g2p | `org.openvpi.wolf.inference.G2P`，variant `pipe-chain`，词表 `kor_dict.txt`（2.1 MB），后端引用 `wolf/g2p-multi:inference/multig2p` |
| inference s2p | `org.openvpi.wolf.inference.S2P`，variant `direct` |
| 依赖 | `wolf/g2p-multi@1.0.0.0` |

它**没有 onset 模块**（与 dist 约定一致：卡拍层只在声库侧或 cmn/yue 提供）。

### 2.2 声库侧本来就支持韩语

`inferences/acoustic/languages.json` = `{"eng":1,"jpn":2,"ko":3,"cmn":4}`，四个模型
（acoustic/duration/pitch/variance）的 `phonemes.json` 均含 34 个韩语音素条目，
与 wolf 的 34 个符号**逐一相同**（`K L M N NG P T a b ch d e eo eu g h i j jj k kk m n o p pp r s ss t tt u w y`）。
所以缺的不是能力，而是**包内没有声明 kor**。

### 2.3 实测证据

审计工具（语言包目录传含 kor 的依赖闭包：cmn/eng/jpn/kor + g2p-pinyin + g2p-multi）：

```
languages  cmn eng jpn kor (default cmn)
ok   every language the package depends on reaches a linguist: declared 4, reached 4
ok   every speaker offered reaches the acoustic model
kor/romaja  declares 34 phoneme(s) and an open set, coverage at least 100%
ok   kor has a usable route
ok   kor declares no reserved marker as a phoneme
```

无头编辑器：`voices.list` 认出包 → 插两音符（歌词 `annyeong sarang`，语言 `kor`）→
`rendered with phonemes: True` → 导出任务 `state: succeeded`、wav 1,411,280 字节。
日志里是真实推理链：multig2p 编解码 → 时长 → 声学 `model.onnx` 0.539 s → 声码器 0.379 s → `InferAcousticTask Success`。
（修正语言句柄后连跑两次，两次都 `succeeded`、波形都是 1,411,280 字节。）

音素**符号**没有从接口打印出来（脚本只判断"有没有音素"）。但这不影响"韩语音素被正确解析"这一结论：
引擎的查表是"任意一个 token 查不到就整段报错"（`InputWord.cpp:39-47`），渲染与推理都成功，
即证明每个韩语 token 都在 `kor/<符号>` 下命中了模型音素表。
（**更正**：这里只证明"token 能在模型音素表里查到"，不代表读音正确。文首更正块记录了后续读回音素的结果：
歌词大多命中不了词典，落到 multig2p 模型回退。）

## 3. 副本上的三处新增（等价于转换器会生成的形状）

1. `linguists/kor-romaja/linguist.json`：照 `wolf/lang-kor` 的声明，但两条 ref 必须补包名前缀，
   写成 `wolf/lang-kor:inference/g2p` 与 `wolf/lang-kor:inference/s2p`（原样照抄时 `:inference/...`
   只会解析到包内，而包内没有这两个贡献）。
2. `desc.json`：`dependencies` 加 `wolf/lang-kor@1.0.0.0`，`contributions.linguist` 加
   `kor-romaja -> ./linguists/kor-romaja/linguist.json`。
3. `characters/<singer>/config.json`：`imports` 加 `{"role":"lang/kor","ref":":linguist/kor-romaja"}`，
   `languages` 加 `"kor": "lang/kor"`。

改动自证：与原成品扁平化对比为 `added=4 removed=0 changed=0`（desc.json）与
`added=3 removed=0 changed=0`（歌手声明），即只有这三处新增。

## 4. 必须做的包内数据修正：语言句柄 `ko` → `kor`

**症状**：渲染出了音素，但推理阶段报
`failed to build the linguistic input: unknown token i`，导出任务 `state: failed`、`io_error / Inference failed`。

**根因**（读代码确证）：`dsinfer/util/inferutil/src/InputWord.cpp:34-47` 把 token 拼成
`<phone.language>/<phone.token>` 去模型音素表里查，查不到再退回裸符号，都查不到就报裸符号。
表里的键是 `ko/i`，而运行期 `phone.language` 是 `kor`（来自 wolf linguist 与歌手声明）⇒ 两者对不上。

**规范口径**：`ds-spec-2.4.md:732` 写明 `languages` 的键**推荐 ISO 639-3**（即 `kor`）。
该模型的音素表对 cmn/eng/jpn 都用了 639-3，唯独韩语用了 639-1 的 `ko`，与自身其余条目、与 wolf 都不一致。

**修正**（在副本内，只改键名，**id 一律不动**，声学零影响）：四个模型的 `phonemes.json` 里
`ko/<符号>` 改名为 `kor/<符号>`（每个模型 34 个键），`languages.json` 里 `ko` 改名为 `kor`。
自证：`renamed_keys=34 total_keys=200 ids_unchanged=True`（四个模型一致），改完即通过实测。

**给后人的说法**：模型音素表的前缀必须与歌手声明的语言句柄一致。遇到 `unknown token <符号>`
而该符号明明在表里，就先查这个前缀是否与句柄同名（对照 `InputWord.cpp` 的拼接规则）。

## 5. 未证实与限制

1. kor 没有 onset 模块，审计**没有**为 kor 打印深度行 ⇒ 推断其 `maxDepth` 为 `Phonemes`
   （音素层可用、卡拍层不可用），未逐项证实。
2. 韩语输入用的是 `romaja` 词表（本次 `annyeong`、`sarang`），词的覆盖度取决于 `kor_dict.txt`，
   未做系统性抽查。未覆盖的词按 g2p 的 `fallback: useOriginal` 处理，行为未实测。
3. **发音质量未评估**：只证实了链路跑通与波形产出，没有做听感判断。
4. 这是**修改过的变体**，包 id 与版本仍与原成品相同，因此不能与成品同放一个搜索根。
   若要作为交付物，需要另定包名或版本号。

## 实测更正（2026-10-07）

### 事实（均可复跑）

| 观测 | 证据 |
| :-- | :-- |
| `wolf-lang-kor` 的 G2P 链条只有 4 步：`verify` → `dict(kor_dict.txt)` → `model(multig2p)` → `fallback(useOriginal)` | `wolf-lang-kor/inferences/g2p/inference.json:8-42` |
| 它对外声明 `language kor, scheme romaja` | 同上 `:44-52` |
| 词典键是**字母串**（兼容字母），不是谚文音节也不是罗马字 | `kor_dict.txt` 首行 `ㄱㅏ<TAB>a` |
| 链条里**没有**谚文/罗马字到字母的分解步骤 | 同上 `:8-42` 的步骤清单 |
| `ㅎㅏㄴㅏ`（하나）有 5 条候选，含 `h a n a`，但字母串输入返回的是 `a` | 工具实测 |
| `ㅇㅏㄴㄴㅕㅇ`（안녕）在词典里 0 条 ⇒ 只能走模型回退 | 词典检索 |
| 谚文输入 `안녕`、`사랑`、`하나` 三者输出**完全相同**：`v r o` | 工具实测（-Words "안녕 사랑 하나"） |
| 罗马字输入：`annyeong → e N n i e e`、`sarang → a e i d eu`、`hana → a` | 工具实测 |
| 同一 OOV 词 `zzq` 三次运行中出现过两种结果（`v tt eu NG` 与 `v d eu`） | 工具实测（3 次运行，1 次不同） |

### 推断（有支持但未直接证实）

- 词典变体选择取的是**首条命中**，而词典里不少词的首条并非最佳读音（例如 하나 首条是 `a`）⇒ 即便命中，
  输出质量也不可靠。
- 常用词普遍缺失（안녕 即缺失）⇒ 大多数输入落到 multig2p，而它对韩文的输出是退化产物。
- 该模型回退路径**疑似不确定**（同输入跨运行结果不同）⇒ 不能用它做门禁判据，门禁歌词集必须选词典命中词。

### 这对交接文档的影响

1. [developer-repro.md](developer-repro.md) 的 §4.1 词典兼容性判据仍然成立，但**歌词集要选词典命中词**，
   不要把 OOV/模型回退纳入门禁（英文 OOV 在本次记录里可复现，韩文 OOV 不可复现）。
2. "给 2.4 声库补韩语"这条路径**暂不可用**，属 wolf 侧问题（词典键脚本与链条步骤不匹配），
   按本项目约定应**在 wolf 源头修复**，不在 lite 侧做补偿（例如自行插入谚文分解）。
3. 已交付的产物（`yousa-2.4@1.65.1.0` 与 zip）**未受影响**，它们不含韩语声明。
