# duration 的词级声明：从两个布尔量改成 `dur_type: abs/rel`（调研报告）

> **阅读提示（2026-10-08 追加）**：§2 与 §4 的取证是**改动前**的快照，其中引用的行号（如 `convert-voicebank.py:153`、`:870`、`:924-925`）指当时代码所在位置，改动后已漂移，复核时请按名字检索而不要按行号。§5 及之后是实施与实测记录，行号按改动后的现状写就。
>
> **上游词汇对照（2026-10-08 追加，来自 DiffSinger `feat/attn-dp-mulaw`）**：这两个取值正对应上游 #339（`ebc6638`）的两种架构形态——`abs` 是「predicts **absolute phoneme durations**」，只消费 `word_div`（用于词内位置）；`rel` 是「**word-level frame allocation**」，把每个词的音素变成在该词帧预算上的分布、输出整数帧数且每词之和恰等于该词预算，因此额外消费 `word_dur`。上游 #340（`1364e59`）的立意与本次一致：「**the architecture itself decides what the model consumes**」，消费者只读类别声明的 `needs_word_div` / `needs_word_dur`（`modules/fastspeech/variance_encoder.py:68-69`、`deployment/exporters/variance_exporter.py:272-273`、图内 `deployment/modules/fastspeech2.py:223,230`）。`dur_type` 就是把这两个声明位在运行时合并成一个文字字段，使非法组合（要 `word_dur` 不要 `word_div`）无法表达。

> 状态：**进行中**（2026-10-08）。本文是调研与方案稿，**未改任何代码**。按你的指示先出报告，确认后再动手。
> 范围边界：只针对「duration predictor 怎么声明词级输入」这一件事，不动同一张配置表里的其它字段。
> 本机两仓：lite = 本仓，synthrt = 本机检出（当前检出在 `spec2.4-uptake` @ `7ea424a`。该分支于 2026-10-08 由 `onnxruntime-builds-uptake` 改名而来，与 wolf、otter 的远端分支及 lite 本地分支统一为 `spec2.4-uptake` 这一联合名字，下文出现的旧名均指改名前的同一分支）。

## 0. 需人工确认的关键点（集中置顶）

| 编号 | 决策点 | 选项与取舍 |
| :-- | :-- | :-- |
| D-1 | `abs` / `rel` 的语义边界，现存第三种状态怎么表达 | **(a) 两取值 + 缺省**：`rel` = 词预算模型（等价旧 `useWordDiv`+`useWordDur`），`abs` = 消费划分但输出绝对帧（等价旧 `useWordDiv` 单开），**字段缺省 = 两个输入都不声明**（等价旧两个都关）。三态齐全而取值只有两个，推荐／(b) 三取值 `durType: abs\|rel\|div`，`div` 单独表示「只喂划分」，自解释但多一个取值／(c) 只把 `useWordDur` 换成枚举、`useWordDiv` 保留布尔，等于没简化，不推荐 |
| D-2 | 字段名与取值的拼写 | **(a) `durType: "abs" \| "rel"`**，与同表 `frameWidth` / `useLanguageId` / `useSpeakerEmbedding` 的命名一致／(b) 照用户原话写 `dur_type`，与 predictor 的输入名 `word_div` / `word_dur` 的蛇形一致，但与同表其它字段不一致。**实际采用 (b)**：JSON 键与取值是 `dur_type` / `"abs"` / `"rel"`，C++ 成员名 `durType`、枚举名 `DurationType`（与同表其它字段命名不一致是已知代价） |
| D-3 | 旧字段的兼容策略 | (a) 引擎同时接受新旧，新旧同时出现且语义冲突时报错，已转的包不必重转／**(b) 只认新字段**，干净但所有带旧键的包（本机 `1007_wolf_club` 就是一例）直接加载失败——**实际采用 (b)**，依据用户原话「完全不考虑兼容旧版、没有实际用户」／(c) 新字段优先、旧字段静默忽略，省事但埋雷 |
| D-4 | 是否同时改上游规范文档 | **(a) 一起改**：`dsinfer-level-1-revised.md` 的配置表、依赖规则、Model variables 激活条件同步改，推荐，否则规范与实现分叉／(b) 先只改实现，规范随后补 |
| D-5 | 改在哪条线上 | 事实：这两个布尔**只在 `onnxruntime-builds-uptake` 上**（lite 端口 pin 的分支），`refactor` / `main` / `language-level-1` / `desc-new-schema` 都没有。**(a) 就在这条线上改**，与 lite 的消费线一致，推荐／(b) 先在 `refactor` 引入再同步过去，代价是多一轮合并 |

## 1. 结论先行

- 这两个布尔量**不是 2.4 声库标准的一部分**。上游标准文档 `ds-spec-2.4.md`、OpenUtau 检出、wolf 检出里都没有它们。它们只出现在 synthrt 的 Level 1 规范与解释器（而且只在 `onnxruntime-builds-uptake` 这一条分支上），以及**由本仓转换器写出的包**里。
- 两个布尔实际编码的是**一个三选一**：「不喂词级输入」／「喂划分、输出绝对帧」／「喂划分 + 喂预算、输出词内比例」。合法组合只有 3 个，非法组合 1 个（`useWordDur` 不配 `useWordDiv`，解释器直接报错）。**用枚举表达比两个布尔更诚实，这是本次改动真正的价值所在。**
- 「绝对 / 相对」这对语义**已经写在实现注释里**：`DurationApiL1.h:81-87` 明说 `useWordDur` 是「splits the frame budget of every word **instead of predicting absolute phoneme durations**」。所以 `abs` / `rel` 不是新语义，而是把既有语义提到字段名上。
- 影响面小且可枚举：synthrt 侧 5 个文件（规范、API 头、解释器、任务、测试），本仓 2 处代码 + 5 个用例 + 2 处文档，打包器侧（本机项目，不在本仓）按它自己的仓库核对副本。**lite 的运行期代码不读这两个键**，它们只由转换器写入包，所以兼容策略一确定，改造就是一次贯通。

## 2. 现状取证（文件:行号）

### 2.1 规范侧（synthrt 权威）

- `dsinfer-level-1-revised.md:26`：依赖规则 —— 启用 `useWordDur` 时必须同时启用 `useWordDiv`。
- 同文 `:57-58`：Duration 的 `onnx` 配置表两行。`useWordDiv` = 「是否启用音节划分输入（predictor 接收 `word_div`）」，`useWordDur` = 「是否启用音节长度（帧）输入（predictor 接收 `word_dur`）」。
- 同文 `:73-74`：Model variables 表里 predictor 的 `word_div` / `word_dur` 两个输入，激活条件分别是 `useWordDiv == true` / `useWordDur == true`。
- 同文 `:82`：词预算模型的语义 —— 同一词内各音素预测值之和等于该词的 `word_dur`，**运行时只取比例**，再按乐谱给出的音节时长换算，所以消费方得到的时长与模型输出的单位无关。这句是 `rel` 语义的规范依据。

### 2.2 实现侧（synthrt，分支 `onnxruntime-builds-uptake`）

- `dsinfer/include/dsinfer/Api/Inferences/Duration/1/DurationApiL1.h:73-87`：两个 `bool` 与它们的注释。`:75-78` 明确 `word_div` 的两种来由 —— 分帧预算的模型，**或者**给自己的音素补词内位置的模型。`:81-87` 说明 `useWordDur` 必须与 `word_div` 同时声明。
- `dsinfer/plugins/inferenceinterpreters/duration/DurationInterpreter.cpp:107-124`：`parse_bool_optional` 解析两个布尔，并在 `:119-122` 对 `useWordDur && !useWordDiv` 收集解析错误（注释说明这是解析期错误而不是运行期失败）。
- `dsinfer/plugins/inferenceinterpreters/duration/DurationTask.cpp:99-112`：`shareWordInput()` 把词张量从语言编码器**共享**给 predictor，不重建，避免两边对词结构各说各话。
- 同文 `:265-279`：按两个布尔决定是否把 `word_div` / `word_dur` 共享给 predictor，缺张量时报错。
- 测试：`dsinfer/tests/auto/Inference/test_DurationInference.cpp:151,155,389`。

### 2.3 声明侧（谁的包里会出现这两个键）

- 它们写在**贡献声明**里：转换器把它们塞进 `<声库>/inferences/duration/config.json` 的 `configuration`（`scripts/convert-voicebank.py:442` 落声明、`:929` 写键）。
- 本机实测（声库搜索根下的三个样本）：`1007_wolf_club/inferences/duration/config.json:29-30` 带 `"useWordDiv": true` 与 `"useWordDur": true`。`yousa-2.4@1.65.1.0` 与 `junninghua-2.4@26.1.5.0` 的各 28 个配置文件里**没有**这两个键（它们的 duration predictor 不吃词级输入），与既有文档的记录一致。

### 2.4 本仓（lite）

- `scripts/convert-voicebank.py:153` `WORD_PREDICTOR_KEYS = {"word_div": "useWordDiv", "word_dur": "useWordDur"}`，`:134` `WORD_ENCODER_INPUTS`，`:870` 文档串，`:924-925` 「有 `word_dur` 没 `word_div`」的报错。
- 测试 `scripts/test_convert_voicebank.py:512-598`：5 个用例，含「两个键都写」「只声明划分」「只有 dur 要报错」「pitch 不许吃 word_div」「已声明的组合不再重复写」。
- 文档：`docs/synthrt/packaging-voicebank-essentials.md:36-41`（词级输入必须声明的实测结论）、`docs/plans/synthrt-main-migration.md:277`（行号漂移提醒里提到 `word_div` / `word_dur` 判据常量）。

### 2.5 其它仓

- wolf 检出：`useWordDiv` / `useWordDur` / `word_dur` 全部 0 命中。
- OpenUtau 检出：只在 `DiffSinger*.cs` 里**构造** `word_div` / `word_dur` 张量（`DiffSingerPitch.cs:136-142`、`DiffSingerVariance.cs:149-155`、`DiffSingerBasePhonemizer.cs:365-378`、`DiffSingerUtils.cs:119`），**不读**这两个声明布尔。也就是说上游标准实现与这次改动无关，改动不会波及它。

## 3. 为什么值得改

1. **非法组合在类型层面消失**。现在 `useWordDur && !useWordDiv` 要靠解释器报错拦住（`DurationInterpreter.cpp:119-122`），改成枚举后这个组合根本写不出来。
2. **字段名自解释**。`abs` / `rel` 直接说出模型输出是绝对帧还是词内比例，而 `useWordDur` 必须读注释才知道。
3. **转换器的判据少一次配对**。现在要把两个 onnx 输入名映射到两个键，再单独处理「只来一个」的两种情况，改成一个取值后判据是一对一。

## 4. 方案候选

### 方案 A（推荐，对应 D-1a / D-2a / D-3a）

- 新字段 `durType`，取值 `"abs"` / `"rel"`，**字段缺省 = 不声明任何词级输入**。
  - `rel` ⇒ 声明 `word_div` + `word_dur`（等价旧 `useWordDiv=true` 且 `useWordDur=true`）。
  - `abs` ⇒ 只声明 `word_div`（等价旧 `useWordDiv=true`，`useWordDur` 缺省）。
  - 缺省 ⇒ 两个都不声明（等价旧两者都缺省）。
- 引擎同时接受旧的两个布尔：只有旧键时按旧语义解释，新旧同时出现且语义冲突时报错，旧键里 `useWordDur` 不配 `useWordDiv` 仍然报错（保持既有行为不变）。
- 规范表的配置行、依赖规则、Model variables 激活条件同步改成 `durType`。

### 方案 B（对应 D-1b）

三取值 `durType: "abs" | "rel" | "div"`，`div` 表示「只喂划分」。自解释最好，代价是多一个取值，而「缺省 = 什么都不喂」这件事仍要靠字段缺省表达。

### 方案 C（不推荐，仅列出备查）

只把 `useWordDur` 换成枚举、`useWordDiv` 保留布尔。字段数没减，语义仍然分裂，等于白改。

## 5. 落地步骤（确认后执行）

1. **synthrt**：直接在当时的消费线 `onnxruntime-builds-uptake` 上改（**未**另开 `dur-type` 分支，实际提交为 `7ea424a`。该分支现已改名为 `spec2.4-uptake`），一处枚举贯通 API 头 → 解释器 → 任务 → 规范 → 测试。
2. **lite**：**已完成**（2026-10-08）。转换器已按新字段产出声明（按 predictor 的 onnx 输入名：只有 `word_div` ⇒ `abs`，`word_div` + `word_dur` ⇒ `rel`），5 个用例与 2 处文档同步改完。
3. **打包器**（你的本机项目，不在本仓）：它调用本仓转换器生成声明，转换器改完它的产物自动跟随。它自己若有该字段的副本、断言或说明，需要同步核对，具体位置以它自己的仓库为准，本报告不写它的路径。
4. **setdir 实测**（你已授权）：portfile 的 LOCAL-ONLY COPY 块指向本机 synthrt 检出 → 重建 vcpkg → 重建 lite → 跑验证。该块属本地调试态，绝不入库。

## 6. 验证方案（可执行）

1. **字段层面**：`python -m unittest discover -s scripts -p "test_*.py"`（转换器用例是这次改动的第一道门禁）。
2. **真实模型层面**：`.tmp/word-inputs/verify.py` 那套 6 例，断言点从 `useWordDiv` / `useWordDur` 换成 `durType`，在 setdir 重建后重跑。
3. **加载层面**：拿带旧键的 `1007_wolf_club` 与重转后带新键的包各加载一次，验证 D-3 选定的兼容策略真的成立。
4. **端到端**：用 `docs/synthrt/tools/measure-g2p-output.ps1` 打一次 G2P 基线，确认与 `docs/synthrt/baselines/cmn.txt` 逐行一致（这套工装上一轮已在中文路径下验过，可直接复用）。
5. **顺手订正**：规范表里 `useWordDiv` / `useWordDur` 的默认值栏写的是 true，而 API 头的默认值是 false（`:79` / `:87`），规范自己又规定「省略时用 API 类型定义的默认值」，属既有不一致，本次改到这张表时可以一并订正。

## 7. 风险与未证实项

- 【未证实】除本机搜索根下这三个样本，你手上是否还有别的包带旧键。若 D-3 不采纳兼容策略，需要先清点全部包。
- 【未证实】打包器侧对该字段是否有独立副本。它不在本仓，我没有查，这也是「打包器也要跟着改」的具体内容。
- 【推断】`abs` 只声明 `word_div` 这一条，是我按 `DurationApiL1.h:75-78`（分帧预算的模型**或者**补词内位置的模型都会声明 `word_div`）推断的，需要你确认这确实是要保留的第三种状态。
- 【风险】规范表与实现的默认值不一致（§2.1 最后一条与 §6 第 5 条），本次改动不引入它，但会把这张表改到，所以顺带暴露。

## 8. 实测结果（2026-10-08，setdir 本地源构建）

- **构建**：synthrt port 切本地源（`file(COPY)` 正向清单，本地调试态、不入库）后重建 vcpkg，synthrt + wolf + otter 全部重装成功（2.3 分钟）。首次构建在 `DurationInterpreter.cpp` 报 `DurationType` 未声明——我漏写了该文件的 `Dur::` 限定前缀，修正后通过。lite 应用随后重建成功（4 分钟），安装树里的 Duration 头文件已是新版（新字段命中 2、旧字段 0）。
- **转换器**：`Ran 38 tests … OK`（含 5 个 `dur_type` 用例）。真实模型 6 例 **6/6 PASS**：`1007_wolf_club:duration` 新增的字段恰为 `['dur_type']`（predictor 声明 `word_div` + `word_dur` ⇒ 取值 `rel`），其余 5 例不新增任何字段。
- **G2P 基线（cmn）**：新构建的无头实例对 18 词中文歌词集打出的输出与 `docs/synthrt/baselines/cmn.txt` **逐行完全一致（0 差异）**。
- **中文路径等极端情况（新构建，2026-10-08 补测）**：把新构建整份铺到 `.tmp\中文路径 测试2\编辑器 目录 中文\out\bin`（719 文件 / 5.1 GB，路径同时含中文与空格；该铺开目录连同另外两份副本已在随后的大扫除中删除，`.tmp` 由 40.41 GB 降到 1.2 GB），声库铺到 `…\声库目录 中文\yousa-2.4@1.65.1.0`，实例目录也指向带中文的 `…\实例目录 中文`。四种组合逐一与新构建的 ASCII 对照跑同一条 cmn 用例：

  | 编辑器路径 | 声库路径 | 与基线比对 | err.log |
  | :-- | :-- | :-- | :-- |
  | ASCII | ASCII | 0 差异 | 0 行 |
  | 中文 | 中文 | 0 差异 | 0 行 |
  | 中文 | ASCII | 0 差异 | 0 行 |
  | ASCII | 中文 | 0 差异 | 0 行 |

  即中文路径（含空格）在编辑器侧、声库侧与实例目录侧都不影响结果。
- **jpn 基线（口径不符，未验成）**：`docs/synthrt/baselines/jpn.txt` 实际是 15 行的假名音素对照表（`a a` / `i i` / …，即先前「jpn 15/15」的来源），而我用 `-Words` 传歌词得到的是 24 行歌词输出，两者不同口径、不可比，既不能证成也不能证伪。要复现基线需要它当时的那个调用方式。
- **真实声库端到端（2026-10-08 补测，`rel` 路径）**：把 `1007_wolf_club` 的 duration 声明从两个旧布尔改写成 `"dur_type": "rel"`（该包 predictor 声明 `word_div` + `word_dur`）后，用无头实例加载并渲染。**身份三元组的正确写法**（先前反复失败的原因，靠实例的 `voices.list` 才查清）：`package_id = 1007_wolf_club`、`package_version = 1.0`（**不是** desc.json 里的 `1.0.0.0`）、`singer_id = huyinyi`、`speaker = light`（歌手自己的说话人 id，`huyinyi-light` 是包内部 import 的 `speakerMapping` 目标，不是对外参数）。**先更正一处我早先的错误判断**：这个包**是**歌手包（`contributions.singer` 有 5 项：yelin / huyinyi / shark / xuanyi / shushu），我先前查了 `contributions.singers`（复数）才误判为空。
  跑通后的结果：`tracks.set_voice` 通过、歌手装载成功、duration 会话打开了包里的 `1007_mulaw_dur.dur.onnx`、**推理进入 predictor 内部**（自始至终没有出现 `missing input names: "word_div", "word_dur"`，说明 `rel` 确实把这两张量喂进去了）。在 DirectML 上失败在 ONNX Runtime 的执行层：`FusedMatMul node /dur_predictor/blocks.0/attn/MatMul/... Exception(3) 参数错误`（`DmlExecutionProvider/MLOperatorAuthorImpl.cpp(2508)`）。**已隔离**：把 `inference.executionProvider` 切成 `CPU` 后，同一模型、同一声明、同一实例流程下日志给出 `Session [1007_mulaw_dur.dur.onnx] - Finished inference in 0.0018194 seconds` 与 `InferDurationTask … Success`，**推理成功**。所以这是 DirectML 对该模型的限制，与字段改造无关，而 `dur_type: "rel"` 的端到端链路（共享 `word_div` + `word_dur` → 预测时长成功）由此完全跑通。
- **旧键形态**：本次没能用同一台机器演示"旧键包直接失败"——要演示它得把这个包改回两个旧布尔，而本次改造的现实是它已经被改成新键并跑到了 predictor 内部。旧键的失败形态仍是删掉兼容的直接推论（字段缺省 ⇒ 不喂两张量 ⇒ 会话报缺失输入，即 `packaging-voicebank-essentials.md` 记录过的那个形态）。
- **synthrt 自己的单测**：本机 `cmake-build-debug` 里 `Boost_DIR` 是 `NOTFOUND`（lite 的 vcpkg 树只有 boost 的各组件端口，没有 `boost/test`），auto-test 目标因此整批被跳过，`test_DurationInference` 编不出来。改用最小 Boost.Test 桩头加 `cl /Zs` 对该文件做真实语法与语义检查：**退出码 0、无错误**（只剩第三方头的 DLL 接口告警）。要真跑这套用例需要先装 `boost-test`。
- **未做**：打包器侧（本机项目，不在本仓）尚未核对。
- **当前状态与回滚清单（2026-10-08 迁移轮更新）**：synthrt 的改动是提交 `7ea424a`「Declare the word inputs of a duration predictor with dur_type」（7 文件 +100/−62），已推送到 `origin/spec2.4-uptake`（原名 `onnxruntime-builds-uptake`，2026-10-08 改名，提交未变）。lite 侧：本功能原先落成于 `synthrt/inferutil-binary-read`（转换器、5 个用例、两份文档、本报告，以及三个端口的 pin），该分支保留作历史参照（tip `a1f64be13`）；此后按"以新 main 为基座、只搬增量"的路线，从当时的 `origin/main`（`33dadf958`）另起迁移分支 **`synthrt/spec2.4-uptake`**，用 `git merge --squash` 取两条线的并集，再按主题拆成 **7 条提交**（**全部无 merge 记录**，拆分前后做了树一致性核对、内容零变化），冲突只有 `InferencePage.cpp` 一处并以 main 版为准（main 已把歌手名本地化下沉到引擎层的 `localizedNames()`）。三个端口的 `HEAD_REF` 与 `version-string` 同时改为 `spec2.4-uptake`（REF 指向改名前后同一个提交：synthrt `7ea424a`、wolf `eda2b48`、otter `e770ab9`），`synthrt` 端口的 `port-version` 因换线而从头计。**未向任何远端推送 lite 分支**。放弃本地状态的办法：旧分支用 `git checkout synthrt/inferutil-binary-read` 取回，迁移分支切回 `main` 后删除即可。
- **覆盖边界（2026-10-08 二次更新）**：`abs` 仍只有解析与声明层面的证据（本机没有"只声明 `word_div`"的歌手包可供端到端驱动）；`rel` 已端到端跑通（见上面 1007 用例）；缺省路径由 yousa 的 G2P 基线与 ctest 全套覆盖。
- **DirectML 失败的根因（2026-10-08 深挖 + 对照实验）**：那个模型的 duration predictor 是 DiffSinger #339 的 `attn` 架构，图里有 **8 个 5 维 × 5 维的 MatMul**（全在 `/dur_predictor/blocks.*/attn/`，其余 18 个是 3 维 × 常量）。ORT 的 Level-2 `MatmulTransposeFusion` 把带 `Transpose(perm=[0,1,2,4,3])` 的那一个融合成 `com.microsoft::FusedMatMul`（transA=0、transB=1、alpha=1），而 DirectML 的 FusedMatMul 实现直接映射到 `DML_OPERATOR_GEMM`——该算子官方只接受 **2–4 维**张量（FL1.0 为 4 维，FL4.0+ 为 2–4 维），**5 维在任何 DirectML 版本都不受支持**，于是内核创建期抛 `E_INVALIDARG (0x80070057)`，表现为"会话创建无警告、跑到该节点才炸"。
  **对照实验（本机实测）**：在 DML 下**成功**跑完的 `1007_…linguistic.onnx` 与 yousa 的 `dur.onnx`，5 维 MatMul 数都是 **0**（它们的 attention 是 4 维）；而**失败**的那个恰好有 8 个 5 维。⇒ 这是 DirectML 对该模型的限制（模型侧需用 ≤4 维表达滑窗注意力，或让该包走 CPU），与 `dur_type` 改造无关。
  **未证实项**：本机未开 DirectML 调试层，日志里没有 wil 的 `Msg:[…]` 内层消息，因此"5 维被 DML 拒绝"是官方文档 + ORT 源码链路 + 上述对照实验的共同结论，而不是对 DirectML 内部报文的直接观测。
