# wolf 语言包的两个发行：真实历史比对

> 快照：2026-10-07。数据来源是 `github.com/diffscope/wolf` 的 releases（`lang-v0.1.0.0` 与 `lang-v0.1.2.0`），
> 29 个归档全部下载、逐文件 SHA256 与 API 提供的 `digest` 比对相符（0 个不符）。
> 这份比对回答一个问题：**wolf 实际是怎么表达"语言包变了"的**，从而判断声库能不能靠版本号自保。

## 1. 两个发行

| 发行 | 日期 | 资产 | 备注 |
| :-- | :-- | --: | :-- |
| `lang-v0.1.0.0` | 2026-09-09 | 15（manifest + 14 包） | 无 `wolf-g2p-pinyin` |
| `lang-v0.1.2.0` | 2026-09-28 | 16（manifest + 15 包） | 新增 `wolf-g2p-pinyin@1.0.2.4` |

## 2. 逐包比对结果（事实）

14 个包在两个发行间都有：除 `wolf-lang-zxx` 版本号一字未动外，其余 13 个的 `version` 第 4 位从 `0`
变成 `4`，`compatVersion` **一律未动**。

| 包 | 版本 | compatVersion | 内容变化 |
| :-- | :-- | :-- | :-- |
| `wolf-lang-cmn` | 1.0.1.0 → 1.0.1.4 | 1.0.1.0 → 1.0.1.0 | **删掉整个普通话词典目录（6 个文件）**，新增依赖 `wolf/g2p-pinyin@1.0.2.0` |
| `wolf-lang-yue` | 1.0.1.0 → 1.0.1.4 | 不变 | **删掉整个粤语词典目录（6 个文件）**，新增依赖 `wolf/g2p-pinyin@1.0.2.0` |
| `wolf-lang-eng` | 1.0.0.0 → 1.0.0.4 | 不变 | `ds_cmudict-07b.txt` 内容改变，新增 `s2p` 与 linguist |
| `wolf-lang-jpn` | 0.0.1.0 → 0.0.1.4 | 不变 | `kana2romaji.txt` 内容改变 |
| `wolf-lang-kor` | 1.0.0.0 → 1.0.0.4 | 不变 | `kor_dict.txt` 内容改变，新增 `s2p` 与 linguist |
| `wolf-lang-deu` / `fil` / `fra` / `ita` / `por` / `rus` / `spa` | `…0 → …4` | 不变 | 各自词典文件内容改变 |
| `wolf-g2p-multi` | 1.0.0.0 → 1.0.0.4 | 不变 | `bundle.json`、`inference.json` 改变 |
| `wolf-lang-zxx` | **1.0.0.0 → 1.0.0.0** | 不变 | **4 个文件内容改变，版本号一个字符都没动** |

## 3. 三条结论

1. **`compatVersion` 在实际历史里从未被抬过。** 13 个包里有的**删掉了自己的词典**、有的换了词典内容，
   声明上却都是"兼容升级"。按 `ds-spec-2.4.md:189-198`，这表示这些变化都被提供方判定为落在公开表面之内。
2. **删词典本身靠依赖声明兜住。** `wolf-lang-cmn`／`wolf-lang-yue` 把词典抽出去之后，
   新增了 `wolf/g2p-pinyin@1.0.2.0` 这个**目标点**依赖，所以老消费者仍能工作，但**读音由新包决定**。
3. **读音变化对消费者不可见。** 词典换内容（13 个包全部）没有抬高任何 `compatVersion`，
   甚至 `wolf-lang-zxx` 连 `version` 都没变。**版本号不能用来保证"唱出来一样"。**

## 4. 对声库打包的含义

- 依赖声明只能表达"装哪个包"，**表达不了"发音不许变"**。目标点机制（`dependencies[].version`）解决的是可装载性。
- 想稳定演唱结果，只能靠这三件事之一：**锁定（vendoring）实际使用的包版本**、
  **每次更新后跑音素门禁**（见 [packaging-voicebank-essentials.md](packaging-voicebank-essentials.md) 第 4 节与
  [tools/measure-g2p-output.ps1](tools/measure-g2p-output.ps1)）、或**双边约定提供方在读音变化时抬 `compatVersion`**。
- 第三条是本项目给 wolf 的建议（用户已确认按此推进）：**当变化会改变读音／发音时，抬 `compatVersion`，
  让老目标点失效，迫使消费者显式决定**。第 1 条结论说明这条纪律目前并未执行。

## 5. 复现方式

用 [tools/compare-wolf-releases.py](tools/compare-wolf-releases.py) 一条命令即可，它把上面所有比对内置了：

```
# 负控：同一个 tag 自比，必须 0 差异（会打印实际比较了多少个包）
python tools/compare-wolf-releases.py --releases <发行清单.json> --cache <缓存目录> --self-check lang-v0.1.2.0
# 正式比对两个发行（新的在前）
python tools/compare-wolf-releases.py --releases <发行清单.json> --cache <缓存目录> \
    --tags lang-v0.1.2.0 lang-v0.1.0.0
# 想在 CI 里对"内容变了但版本没变"直接失败，加 --fail-on-violation
```

它按 `desc.json` 的 `id` 归并同一包的不同修订（不是按归档名，归档名与声明版本可能不一致），
逐文件 SHA256 比对，并单独标出三类需要关注的情况：内容变了而版本没变、`compatVersion` 被抬、
声明版本与归档名不一致。不做脚本也可以手工复现：取两个 tag 的资产逐个解包，比对
`desc.json` 的 `version`／`compatVersion` 与包内每个文件的 SHA256。

`wolf-g2p-pinyin` 只在 `lang-v0.1.2.0` 出现，所以它**没有历史修订**，
"拼音词典跨版本差异率"只能用旧 `wolf-lang-cmn` 自带词典与新包词典对比得到
（逐语言音素基线见 `baselines/`）。

## 6. 补充实测（声明版本、词典差异、接口与供给路径）

### 6.1 声明版本与依赖（事实）

| 修订 | desc.json `version` | `compatVersion` | `dependencies` | G2P `variant` |
| :-- | :-- | :-- | :-- | :-- |
| `wolf-lang-cmn` 旧（`lang-v0.1.0.0`） | 1.0.1.0 | 1.0.1.0 | 无 | `algo-pinyin` |
| `wolf-lang-cmn` 新（`lang-v0.1.2.0`） | 1.0.1.4 | **1.0.1.0（未动）** | `wolf/g2p-pinyin@1.0.2.0` | `pipe-chain` |
| `wolf-g2p-pinyin` | 1.0.2.4 | 1.0.2.0 | 无 | `algo-pinyin` |

两个修订的接口声明都是 `org.openvpi.wolf.inference.G2P`、`level: 1` —— **接口层没有变**。

### 6.2 词典的真实差异（事实）

旧包内嵌的普通话词典与新包 `wolf-g2p-pinyin` 的词典各 6 个文件，比对结果：**5 个逐字节相同**
（含 `word.txt`，两份都是 164588 字节、SHA256 前缀 `80CC1C9FD51B2242`），**只有 `trans_word.txt` 变了**，
且是纯增量：旧 6110 行、新 6111 行，**只多 1 行，没有删除或改写**。

### 6.3 一系列现实后果

1. **接口保持 level 1 也不等于输出不变**：这一次真实变更里 `compatVersion` 一个字符没动，
   而词典确实变了（`trans_word.txt` +1 行）。所以"G2P 接口保持 level 1"是必需的，但**不足以**保证读音稳定。
2. **旧包会被引擎直接拒绝**：把旧 `wolf-lang-cmn`（1.0.1.0）装进当前树后，声库报"未安装"，
   引擎日志原文是 `unknown pinyin configuration key: dictPath`
   （旧配置格式已不被支持）。而它的 `desc.json` 里版本与 `compatVersion` 都还是 1.0.1.0，
   **版本号完全没有提示这次不兼容**。
3. **供给路径不同、实现不同**（本轮新发现，风险项）：vcpkg port 落点里的 cmn 是
   `variant: algo-pinyin` + 内嵌 `assets/ds-zh-pinyin-lite.txt`，而 release 归档里的 cmn 是
   `variant: pipe-chain` + 依赖外部词典包。两者的声明版本同属 `1.0.1.x`、`compatVersion` 都是 1.0.1.0，
   所以**同一个声库用同一个依赖目标点，从 port 构建与从 release 归档安装，实现不同**。
   **已实测（2026-10-07）**：把 release 的 `wolf-lang-cmn@1.0.1.4`（`pipe-chain`）与
   `wolf-g2p-pinyin@1.0.2.4` 装进落点，与 port 供给跑同一份 18 字词表
   （`你 好 世 界 中 国 人 民 音 乐 声 音 生 命 天 空 时 间`），**逐词音素完全一致（差异 0 条）**，
   例如 `你 = n i`、`世 = sh ir`、`乐 = y0 ve`。⇒ 常用字范围内这条差异不影响读音。
   生僻字、异体字与词组是否也一致**未验证**（port 侧是内嵌轻量拼音表，release 侧是完整词典包，覆盖面本就不同）。
   实验后落点已从 port 安装源重建并双向校验：70 文件、不一致 0。
4. **版本号来源不一致**：release 归档名与 `desc.json` 都是 `1.0.1.4`，而 port 落点的同一包
   `desc.json` 写的是 `1.0.1.0`（推断是发布流程给第 4 位加了发布序号，未查证）。
   这意味着**锁定精确版本号的做法在两条供给路径之间不通用**。

### 6.4 因此，门禁的判据不是"差异率"

真实样本说明词典变更可以小到 1 行、也可以大到整目录搬家，用"差异率阈值"去判不兼容没有意义。
可靠的判据是**两个动作**：

1. **看内容**：记录提供方词典文件的哈希。本项目已把**在用**的词典文件哈希记进
   [baselines/dict-hashes.txt](baselines/dict-hashes.txt)，**任何一行变化都视为"可能不兼容"**。
2. **测影响**：用 [tools/measure-g2p-output.ps1](tools/measure-g2p-output.ps1) 跑门禁词表，
   看这次变化是否触及声库实际会唱的词。注意门禁的覆盖面等于它的词表 ——
   上面那 1 行改动就落在当时的 4 词基线之外，所以本项目已把基线加宽到 cmn 18 字、eng 9 词、jpn 15 音节，**词表仍应随声库用途继续加宽**。
