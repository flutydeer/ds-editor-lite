# 打包声库要点（2.4 迁移交接）

> 快照：2026-10-07。事实性结论都带来源，观点与未定项在最后一节单独放。规范引用一律以
> `ds-spec-2.4.md` 为准，版本机制的实测见 [conformance-yousa-2.4.md](conformance-yousa-2.4.md) 第 2.5、2.6 节。

## 1. 声库作为"提供方"时，版本字段怎么填

声库同时也是 Package 提供方，它的 `desc.json` 同样受兼容承诺约束：

| 字段 | 含义（来源） | 声库该怎么填 |
| :-- | :-- | :-- |
| `$version` | 清单格式版本（`ds-spec-2.4.md:149`、`:163`） | 保持与所用格式一致，不随产品版本变 |
| `version` | 本次发布的版本（`:183`） | 每次发布递增 |
| `compatVersion` | 承诺兼容的最低目标版本（`:149`、`:183-186`） | 见下方两条判定 |
| `runtimeLevel` | 依赖的运行时能力，不是软件版本（`:204`、`:208`） | 只换模型或词典**不要**动它 |
| `dependencies[].version` | 要求对方兼容到的**目标点**（`:157-161`） | 见第 3 节 |

区间内**必须保持**的公开表面（`:189-198`）：说话人与说话人集合、语言句柄与其路由、贡献的
`id`/`interface`/`level`/`variant`、音素表语义、`reservedPhonemes`。
区间内**允许变更**：模型与算法、私有 `configuration`、不改变公开表面的实现细节（`:198`）。

由此两条填法（推断，本仓采用）：

1. 新增语言或说话人、改动音素表或音素语义 ⇒ 把 `compatVersion` 抬到本次 `version`（即不向下兼容）。
2. 只替换模型权重、只改内部实现 ⇒ 保持 `compatVersion` 不变，只涨 `version`。

## 2. 打包清单要点

- 归档**扁平**：`desc.json` 与 `assets/`、`characters/`、`inferences/`、`linguists/` 直接在根，不套一层同名目录。
- **不要把依赖的语言包塞进声库 zip**。依赖由插件根的包目录提供，声库只声明依赖，见
  [developer-repro.md](developer-repro.md) 的获取与落点步骤。
- 成员名安全：拒绝绝对路径、盘符、反斜杠、`..`、重复名与非 ASCII 名，工具内已含负向自检
  （[tools/pack-voicebank-zip.py](tools/pack-voicebank-zip.py)）。
- 放对搜索根：成品直接放本机声库搜索根下（该目录不入库），编辑器即能识别（本仓成品就是这样验证的）。
- 交付前自证：解包成员数、`SHA256` 与字节数逐条比对，打包脚本写完 zip 后自动跑这项校验。
- **词级 duration predictor 必须声明词级输入**：predictor 若声明 `word_div`/`word_dur`，包里必须写
  `dur_type`，取值 `"abs"` 或 `"rel"`，只有 `"rel"` 才需要 `word_div` 与 `word_dur` 两个输入，
  省略表示不声明任何词级输入，取值非法会被解释器拒绝。转换器
  [../../scripts/convert-voicebank.py](../../scripts/convert-voicebank.py) 按 **predictor 的 onnx 输入名**补这个字段
  （`declarations_from_model`），判据只能是模型：2.3 的 `predict_dur` 描述的是 encoder，
  而 word encoder 的 predictor 通常只吃 `encoder_out`（本机 4 份真实包都是这种形状）。缺字段的表现是
  **装上以后不能合**：运行期报 `missing input names: "word_div", "word_dur"`（2026-10-07 实测，
  `1007_wolf_club`）。语义上这两张量就是 encoder 收到的那两张（`int64 [1, n_words]`，
  逐字段一致），所以声明之后两个模型不可能对词切分或词内预算各说各话。

## 3. 依赖声明要点（声库 → wolf 语言包）

- 每个支持的语言一条依赖，写**目标点**，不写范围表达式（实测结论见 conformance 第 2.6 节 B 组）。
- 目标点取该语言包发布件的 `compatVersion`，**不要抄归档名的 `.4` 修订号**（理由见 conformance 第 2.5 节）。
- 新增语言的 linguist 句柄必须与模型音素表前缀一致，做法见 [korean-via-wolf-kor.md](korean-via-wolf-kor.md)。

## 4. 词典型内容变更会怎样波及本声库（本次实测清点）

对端口安装目录下 15 个语言包逐个清点"声明文件"（`desc.json`、`linguist.json`、`inference.json`）
与"影响行为的内容文件"（其余全部）：

| 包 | version | compatVersion | 声明 | 内容文件 | 内容字节 | 主要内容 |
| :-- | :-- | :-- | --: | --: | --: | :-- |
| wolf-g2p-multi | 1.0.0.4 | 1.0.0.0 | 2 | 6 | 18 839 155 | 4 个 int8 编解码 ONNX |
| **wolf-g2p-pinyin** | **1.0.2.4** | **1.0.2.0** | 2 | **12** | 634 511 | `dict/mandarin/word.txt`、`phrases_dict.txt`、`dict/cantonese/word.txt` 等 |
| wolf-lang-cmn | 1.0.1.4 | 1.0.1.0 | 2 | 1 | 5 879 | `assets/ds-zh-pinyin-lite.txt` |
| wolf-lang-yue | 1.0.1.4 | 1.0.1.0 | 2 | 1 | 6 258 | `assets/jyutping_dict.txt` |
| wolf-lang-eng | 1.0.0.4 | 1.0.0.0 | 4 | 1 | 3 268 789 | `inferences/g2p/ds_cmudict-07b.txt` |
| wolf-lang-kor | 1.0.0.4 | 1.0.0.0 | 4 | 1 | 2 145 556 | `inferences/g2p/kor_dict.txt` |
| wolf-lang-rus | 1.0.0.4 | 1.0.0.0 | 2 | 1 | 4 412 764 | `inferences/g2p/rus_dict.txt` |
| wolf-lang-deu / fil / fra / ita / por / spa / jpn | 1.0.0.4 或 0.0.1.4 | 同左 | 2~4 | 1 | 0.2~1.0 MB | 各自的 `*_dict.txt` 或 `kana2romaji.txt` |
| wolf-lang-zxx | 1.0.0.0 | 1.0.0.0 | 4 | 0 | 0 | 纯声明，无内容 |

三条结论（事实）：

1. **词典全部住在提供方包里，声库不携带任何词典** ⇒ 词典一变，声库自己无法自保，只能靠提供方的承诺与自己的目标点。
2. **cpp-pinyin 后端就是 `wolf-g2p-pinyin`**，它同时服务普通话与粤语（词典目录 `mandarin/` 与 `cantonese/`）⇒
   它一变，**波及 `cmn` 与 `yue` 两类语言的声库**，而 `wolf/lang-cmn` 只是经由依赖边把它拉进来。
3. 该包当前的 `compatVersion` 为 `1.0.2.0`，**高于同批其它包的 `1.0.0.x` 一档**。推断它曾经历过一次
   不兼容变更并抬过承诺（推断，未查证历史发行）。

### 4.1 词典的真实来源与本仓的发行链（事实）

- 词典**不在 wolf 源码树里**（`git -C <wolf 仓库> ls-files` 查不到任何 `word.txt` 或词典条目）⇒
  它只是发行归档里的数据，改词典必须走重新发版。
- 本仓 `scripts/vcpkg-ports/wolf-lang-packages/` 是 wolf 同名端口的副本：`make-lang-release.py` 生成
  `assets.cmake`，端口按 `<包版本>` 拼发行标签，从
  `github.com/diffscope/wolf/releases/download/lang-v<bundle version>` 下载归档并校验 SHA512。
- wolf 侧**已经有一道版本一致性门禁**：`make-lang-release.py` 在"归档变了但包版本没变"时拒绝改写
  `assets.cmake`（见该端口 `portfile.cmake` 顶部注释）。要把"词典不兼容必须抬承诺"制度化，这里是
  最自然的落点，不必新造机制。
- 端口刻意不做凭据处理，无网环境用 `WOLF_LANG_PACKAGES_SOURCE`（本仓另认 `LITE_WOLF_LANG_PACKAGES`）
  指向本地已解包副本。
- 端口也说明归档按**目录**解包，而不是单个 `dspk` 文件，原因是主线的包加载器只接受目录（该注释的口径）。

### 4.2 判"这次词典更新算不算不兼容"的仪器（已建成并标定）

方法已固化成脚本：[tools/measure-g2p-output.ps1](tools/measure-g2p-output.ps1)。它自包含地启动一个隔离的
无头实例（`--headless --no-mcp --control-level l3`，隔离 `APPDATA` 并把 `packageSearchPaths` 指向声库根），
按固定歌词集建文档与音符，等引擎渲染完成后读回逐音符音素，输出 `歌词<TAB>音素` 便于两次运行直接 diff：

```
pwsh -File measure-g2p-output.ps1 -EditorExe <DsEditorLite.exe> -SingersRoot <Singers 目录> `
     -OutRoot <临时目录> -Out before.txt -Words "你 好" -Speaker <说话人> `
     -SingerPackage yousa -SingerPackageVersion 1.65.1 -SingerId yousa
```

判读方式：替换提供方词典后重跑并对 diff。脚本可信度已按三项检查标定（见下），负向对照（伪造歌手）
会明确报 `The selected singer is not installed`，不会静默产出。

**标定结果（2026-10-07，三项全部通过）**：

| 检查 | 操作 | 结果 |
| :-- | :-- | :-- |
| 自洽 | 同输入连跑两次 | `ni → [n i]`、`hao → [h ao]`，两次完全一致 |
| 灵敏度 | 把提供方 `word.txt` 里 `你:nǐ` 改成 `你:mǔ`（只改 1 行） | `你 → [m u]`（**变了**），`好 → [h ao]`（未变） |
| 可还原 | 从端口安装源逐字节还原 | 还原成功，落点 70 个文件与安装源 0 差异 |

**基线快照（本仓 `baselines/`）**：cmn 18 字、eng 9 词、jpn 15 音节，全部是**词典命中词**，将来直接 diff 即可。
另外 `baselines/dict-hashes.txt` 记录**在用词典文件**的 SHA256，任何一行变化即视为"可能不兼容"（判据见第 6 节）。
词表与语言必须照抄，否则基线不可比：

```
pwsh -File tools/measure-g2p-output.ps1 -EditorExe <DsEditorLite.exe> -SingersRoot <Singers 目录> `
     -OutRoot <临时目录> -Out baselines/cmn.txt -Words "你 好 世 界 中 国 人 民 音 乐 声 音 生 命 天 空 时 间" -Language cmn -Speaker <说话人> `
     -SingerPackage yousa -SingerPackageVersion 1.65.1 -SingerId yousa
pwsh -File tools/measure-g2p-output.ps1 -EditorExe <DsEditorLite.exe> -SingersRoot <Singers 目录> `
     -OutRoot <临时目录> -Out baselines/eng.txt -Words "hello world sing music voice sound life sky time" -Language eng -Speaker <说话人> `
     -SingerPackage yousa -SingerPackageVersion 1.65.1 -SingerId yousa
pwsh -File tools/measure-g2p-output.ps1 -EditorExe <DsEditorLite.exe> -SingersRoot <Singers 目录> `
     -OutRoot <临时目录> -Out baselines/jpn.txt -Words "a i u e o ka ki ku ke ko sa shi su se so" -Language jpn -Speaker <说话人> `
     -SingerPackage yousa -SingerPackageVersion 1.65.1 -SingerId yousa
```

韩语**没有**基线文件，原因见 [korean-via-wolf-kor.md](korean-via-wolf-kor.md) 的实测更正一节：
它的输出大多来自不可复现的模型回退，无法作为比较基准。

**与 speaker 无关（已实测）**：同一词表只换说话人（`Yousa_Normal` → `Yousa_Bright`），音素**完全一致**（差异 0 条）。
所以门禁结果与 `-Speaker` 无关，命令里的 `<说话人>` 填该歌手实际存在的任意说话人即可，不必固定。

**门禁的适用边界（实测教训）**：门禁歌词集必须是**词典命中词**。模型回退路径的输出不保证可复现
（实测：韩文 OOV 词 `zzq` 三次运行中出现过两种音素），拿它当判据会把模型的不确定性误判成词典变更。

**由此实测证实的一点**：词典改一行，**版本号完全没变**，声库演唱的音素就已经变了，而加载器不会也无法发现
（`ds-spec-2.4.md:198` 明确允许在兼容区间内修改"模型与算法"）。这就是"词典不兼容必须由提供方的版本承诺
来表达"的实测依据，也是依赖方无法自保的直接证据。

词典构成（事实）：`wolf-g2p-pinyin` 的普通话表以**汉字**为键，`word.txt` 17,502 行（形如 `你:nǐ`），
`phrases_dict.txt` 为词组表（形如 `一个样:yī,ge,yàng`），另有 `trans_word.txt`、`phrases_map.txt`、
`user_dict.txt`。ASCII 拼音歌词（`ni`）与汉字歌词（`你`）解析到同一结果（`[n i]`），
说明拼音输入另有一条解析路径，具体由哪张表承担**未查证**。

- **门禁仪器已标定，阈值不再作为判据**：仪器（§4.2）已通过自洽、灵敏度、可还原三项检查。原计划用两个
  **真实**词典修订跑同一批词的音素差异率来定"差异率达到多少算不兼容"，但实测发现两个真实发行之间
  `trans_word.txt` 只差 1 行，差异率分不出"算不算不兼容"⇒ 按决策 D-9 改为"词典文件任何变化即视为
  可能不兼容，再用门禁测影响"，见 §6 与 [wolf-release-history.md](wolf-release-history.md)。
- `wolf-g2p-pinyin` 高一档 `compatVersion` 的历史原因未查证。
- 政策选择已定稿，见第 6 节（决策 D-8，用户 2026-10-07 确认）。

## 6. 词典变更的处置政策（用户已确认按本方案推进）

**事实基础见 [wolf-release-history.md](wolf-release-history.md)**：wolf 的两个真实发行里，
`compatVersion` 从未被抬过，而词典确实变过（`trans_word.txt` +1 行），
`wolf-lang-zxx` 甚至内容变了而版本号一个字符没动。

### 提供方（wolf）侧的建议

**当变化会改变读音时抬 `compatVersion`**，让老目标点解析失败，迫使消费方显式决定接受新读音还是留在旧包。
落点现成：`make-lang-release.py` 已有"归档变了必须改包版本"的门禁，延伸一档即可。
落实情况可用 [tools/compare-wolf-releases.py](tools/compare-wolf-releases.py) 随发行复核，
它会在"内容变了而版本没变"时给出明确告警（`--fail-on-violation` 可让 CI 直接失败）。
只涨 `version` 不动 `compatVersion`（目前的实际做法）会让声库在毫无提示的情况下改唱法。

### 消费方（本项目／第三方编辑器）侧的三件事

1. **声明用目标点，不用精确版本**：`dependencies[].version` 写目标点，交给提供方的区间去满足。
2. **要稳定就锁定来源**：需要长期不变的唱法时，把实际使用的提供方包**固定下来（vendoring）**，
   并注明取自哪条供给路径（release 归档与 vcpkg port 的实现并不相同，见 release history §6.3）。
3. **每次更新跑门禁**：先比词典文件哈希（任何变化即触发），再用
   [tools/measure-g2p-output.ps1](tools/measure-g2p-output.ps1) 跑宽词表，看是否触及实际会唱的词。
