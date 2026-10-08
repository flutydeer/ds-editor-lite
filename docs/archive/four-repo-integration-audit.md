# 四仓联合增量审计报告：lite / synthrt / wolf / otter

> **本文为历史归档（2026-09-30 快照），正文保留不改。现行台账见
> [`../plans/synthrt-main-migration.md`](../plans/synthrt-main-migration.md)**（发现项的处置状态与唯一编号在该文。
> 本文不再作为待办来源）。

> 状态：审计已完成（§6 的发现与 §8 的后续项待处置）。**otter 侧已按用户授权收敛并复验：见 §4.2.1。**

> 快照时间：2026-09-30（本地时区）。审计对象为四个仓库**相对各自上游主分支的增量**。
> 本报告所有结论均标注「事实 / 推断 / 未证实」，行号引用均经本人或对抗复核员在本次快照上重读确认。

> **时效说明（2026-10-01 与 10-02 两轮增量压缩后补）**：本报告写于提交重写之前。为压缩增量，四仓的分支历史
> 已于 2026-10-01 重写，并于 10-02 与 10-03 又按「一类一提交」各压一次（每轮的最终树都逐字节不变，synthrt
> 自推送后不再改动，故不在 10-03 这一轮内）。因此文中出现的增量提交 SHA（`772da8a9`、`ce003df`、`16cd579`、
> `1853d8c`、`cf09e1ec` 等）**不再是任何分支的尖端**，只保留在各仓本地的 `backup/*`、`safety/*` 锚点与 reflog
> 中。要在代码里定位，请用「提交主题 + 文件路径」而不是 SHA。作者本机另有两份**未入库**的过程报告
> （缺陷扫描与增量报告各一份）：它们是本地资料，
> **不是本仓的引用目标**，本报告不依赖它们成立。

---

## 一、结论速览

### 1.1 一句话结论

**这批改动是一次「换发动机」级别的大改：编辑器（lite）把合成、语言学、音频分析三块能力分别交给了
synthrt、wolf、otter 三个独立仓库，自己只当接线员。** 实测结果是**能构建、能加载 2.4 声库、能合出正确的
歌声、GUI 能画能播能存盘**。但有 **1 个阻塞级问题**（otter 被固定的那个版本编译不过 —— 根因是 C++ 成员名
隐藏，**与平台无关**，我先前"Windows 专属"的判断已更正）、**一批中低危问题**，以及 **3 条我先前判断错、
已被独立复核推翻的结论**（见 §7.1，已如实修正并降级）。

### 1.2 交付状态一览

| 维度 | 结果 | 证据 |
| --- | --- | --- |
| 依赖重拉（子模块 / vcpkg） | 四仓**均已在上游尖端**，重拉实际是空操作 | §2.2 |
| 构建 | **成功**（Debug，MSVC v145 / Qt 6.11.1） | §4 |
| 单元/集成测试 | 73 项：**72 通过 / 1 失败 / 0 跳过**（唯一失败项是测试自身的 Windows 缺陷） | §4.4 |
| 无头模式端到端合成 | **成功**，且波形数值级验证通过（音高误差 ≤ 0.27 半音） | §5.1 |
| GUI 端到端冒烟 | **通过**（载声库→画音符→出波形→播放推进→存 DSPX） | §5.2 |
| 2.4 声库加载 | **4/4 全部可加载**（yousa、0913_wolf_club、junninghua、zzm-kl） | §4.3 |
| 曾失败的测试 | 2 项失败 + 1 项跳过，**全部定位且都不是产品缺陷**：1 项已修（我的配置）、1 项是测试自身的 Windows 缺陷、1 项接上夹具后**通过** | §4.5 |
| 打包安装器 | 前置检查已跑。**完整打包未执行**（本机缺 VC++ 运行库） | §8 |

### 1.3 最需要关注的五件事

1. **P0 · otter 被固定的版本编译不过**：原 pin `f4820d99` 报 `C2660`，根因是标准 C++ 的**成员名隐藏**，
   **与平台无关**。pin 已临时回退到 `6f6b0a2`（已随 `bc4bd79b` 提交），**结论与处置见 §6.2 F-1**。
2. **P1 · 自动化「能力查询」与「实际受理」可以互相矛盾**：查询说"能提取音高"，实际发起却被拒绝
   （`PublicAutomationHostAdapter.cpp:1406-1407` vs `ExtractionAutomationAdapter.cpp:54-79`）。
   智能体据此编排任务会直接踩空。
3. **P1 · 「隔离数据根」只隔离了一半**：重定向 `APPDATA` 能搬走配置与单实例锁，但**推理缓存、日志、
   崩溃转储仍然写到真实用户目录**（`InferenceOption.h:80-82` 用 `QStandardPaths`，
   `SingleInstanceIdentity.cpp:13-20` 用 `APPDATA`）。实测：隔离运行仍命中真实用户缓存。
4. **P2 · 安装包可能带着不完整的 ONNX Runtime 而构建成功**：`cmake/LiteBuildApi.cmake` 对 ORT payload
   是"猜"，猜不准只发 WARNING，而同类问题在该文件 `:398-402` 是 FATAL_ERROR，且**没有下游检查兜底**。
   （复核已把"运行期崩"降级为非事实，故为 P2。）
5. **P1 · 推理失败可能没有错误信息**：失败路径只写 `qCritical` 不写 `error`，日志出现 `Task failed:` + 空串
   （`InferPitchTask.cpp:148-152`、`161-164`），线上排障会很痛苦。

### 1.4 本次审计的边界与局限（先说清楚）

- **otter 的审计是在回退后的 pin（`6f6b0a2`）上做的**，不是 lite HEAD 原本固定的 `f4820d99`。原因是后者
  编译不过（根因跨平台，见 §4.2），不修就无法完成"构建 + 实测"要求。
- **otter 的分支在审计过程中被重写**：审计开始时它领先 `origin/analysis-level-1` **48 个提交**，审计期间先被
  压成 **5 个提交**（`8758fb2`），报告定稿后我又按用户授权把文档改动折进去、复验了整条链路，**尖端为 `d59ffc1`**
  （仍是 5 个提交，见 §4.2.1）。两次压缩都只动历史形态，代码主体未变。
  本报告对 otter 的**代码级描述**以**审计时的 48 提交版本**为准。涉及 tifa 的行号在最新尖端**可能已漂移**。
- **打包安装器路径未实测**：只做了源码级审查，没有跑 `packaging/windows/build-installer.ps1`（耗时长且会
  改动构建树）。
- **GUI 测试不是"computer use"**：请按"另一种驱动方式"而不是"人工点测"来看待 §5.2 的结论。该能力在本会话
  不可用的现状与理由见 §5.5。
- **本机没有 Qt 源码**，"`QStandardPaths::AppDataLocation` 在 Windows 上不读 `APPDATA` 环境变量"这一条
  只能由「Qt 实现按 shell 已知文件夹取值 + 本次实测路径未随重定向改变 + 代码两处取值不同源」三项互相印证，
  属**推断**，不是源码级硬证（见 §6.2 F-4）。
- **未覆盖范围**清单见 §8（wolf 推理解释器全量、Linux/macOS 部署分支等）。

---

## 二、审计对象与方法

### 2.1 四个仓库与增量区间

| 仓库 | 本地路径 | 分支 | 本仓 HEAD | 上游基线 | 增量规模 |
| --- | --- | --- | --- | --- | --- |
| **lite**（编辑器） | `<lite 检出>` | `synthrt/inferutil-binary-read` | `97002f18` | `origin/main` = `c20f7a9a` | **4 提交**，226 文件，+12792 / −3165 |
| **synthrt**（合成引擎） | `<synthrt 检出>` | `onnxruntime-builds-uptake` | `f2f0f8e` | `origin/main` = `3c7549d` | **4 提交**，32 文件，+755 / −462 |
| **wolf**（语言学） | `<wolf 检出>` | `linguistic-level-1-v2` | `876482e` | `origin/main` = `cecedba` | **4 提交**，299 文件，+28039 / −267 |
| **otter**（音频分析） | `<otter 检出>` | `analysis-level-1` | `7ef8555`（审计时）→ 收敛后 `d59ffc1` | `origin/analysis-level-1` = `f4820d99` | **48 提交**（审计时）→ **5 提交**（收敛后），22 文件，+6588 / −54 |

说明：

- lite / synthrt / wolf 三仓都是「**4 个提交、且相对 origin/main 只领先不落后**」（`rev-list --left-right --count`
  均为 `0  4`），属于干净的线性增量。
- otter 的分支结构特殊：远端 `origin/HEAD -> origin/analysis-level-1`，**没有 `origin/main`**。本地分支领先
  远端尖端 **48 个提交**（尚未推送），这 48 个提交里绝大多数是文档与测试，真正的代码改动集中在 tifa 对齐
  解码与包声明（见 §3.3.4）。
- **lite 是三仓增量的"消费方"**：它通过 `scripts/vcpkg-ports/{synthrt,wolf,otter}/portfile.cmake`
  把这几个仓库按提交号钉死（pin），所以"四仓联合"的本质是"一个宿主 + 三个被钉死的依赖"。

### 2.2 验证方法与仓库状态

- **依赖重拉是空操作**：四仓**均已在上游尖端**，此前环境就是目标快照，不存在"本地落后于远端"的隐藏差异。
- **验证手段**：机器本地 preset `debug-mainline` 构建。`ctest` 73 项（§4.4）。无头模式以
  `--headless --no-mcp --control-level l3 --control-port <P>` 启动、经 `POST http://127.0.0.1:<P>/automation/v1`
  （JSON-RPC 2.0）驱动"载声库→建轨→建唱段→填词→合成→导出 WAV"，并对 WAV 做数值分析（§5.1）。
  真实 GUI 实例（隔离数据根）做端到端冒烟并截图取证（§5.2）。对抗复核的结论见 §7。

### 2.3 证据等级约定

- **事实**：我在本次快照上亲自读过代码或跑过命令并拿到输出。
- **推断**：由事实推导，但缺源码级或运行时硬证。
- **未证实**：只到假设，不能当缺陷排期。
- 未标注的结论默认是**事实**。

---

## 三、这批改动到底是什么（通俗版）

### 3.1 一句话：编辑器换了发动机

改之前，lite 是**自己干所有的活**：自己实现 G2P（字→音素）转换
（已删除的 `src/libs/Language/G2pConvertRunner.*`、`G2pInputAdapter.*` 就是干这个的），自己在
`src/app/Modules/Inference` 里编排推理，自己处理音频分析。

改之后，lite 变成了**接线员**：新增一个内部库 `src/libs/SynthrtEngine`（14 个新文件）负责"怎么把三个外部
引擎接上"，`Modules/Inference` 新增 14 个文件、`Modules/Extractors` 新增 7 个文件（并删掉 2 个旧的），
剩下的活交给三个独立仓库。

好处是：**引擎可以独立演进、独立发版、独立测试**。代价是：**任何一处接口不一致，都会在运行时才炸**，
而这次审计发现的多数问题，正是"接口两侧各写一遍、慢慢长歪了"这一类。

### 3.2 三个新伙伴分别是谁

| 仓库 | 类比 | 负责什么 | 本次增量的关键词 |
| --- | --- | --- | --- |
| **synthrt** | 发声引擎 | 真正把音素+音高+时长变成声音的那套推理 | 从 `onnxruntime-builds` 取 ORT、插件按"简单名"安装、歌手可声明语言与保留音素、id 映射文件改二进制读、CUDA 换 cuDNN 算法选择 |
| **wolf** | 语言学家 | 字怎么变成音素、什么语言用什么规则 | 新增 **linguist（语言学家）类别**及其 Level 1 契约、会话与支撑层。新增 **7 个推理解释器插件**。新增 **zxx 语言包**。CI 与 ABI 策略 |
| **otter** | 听觉分析 | 从音频里读回音高、读出对齐信息 | **tifa 强制对齐**（按参考实现逐帧解码）、对齐包声明、夹具与门槛工具链 |

### 3.3 逐仓特性清单

#### 3.3.1 lite（编辑器）——4 个提交

| 提交 | 做了什么（通俗版） |
| --- | --- |
| `454c38a7` `feat(synthrt)!: build the editor on synthrt, wolf and otter` | **主改造**：把编辑器整体搬到新三件套上（`!` 表示破坏性变更） |
| `d13a60be` `fix: harden the engine, inference, extraction and automation integration` | 加固：引擎、推理、提取、自动化四处接口的健壮性修补 |
| `38b49bdf` `build: define the deployment layout once, pin the dependencies and test against real otter executives` | **部署布局只定义一次** + 钉死依赖版本 + 用**真实** otter executive 做测试 |
| `97002f18` `docs: describe the integration and the changes required outside it` | 补文档：描述这次集成以及**本仓之外**还需配合的改动 |

新增的内部库 `src/libs/SynthrtEngine`（单一定义部署布局、启动引导、语言桥、分析器引用、执行后端、歌手流水线）：

```
DeployLayout.h            部署目录布局的单一真相源
SynthrtBootstrap.{h,cpp}  引擎引导
LanguageBridge.{h,cpp}    与 wolf 语言模块之间的桥
SingerPipeline.{h,cpp}    歌手（声库）流水线
SingerStages.h            流水线各阶段定义
ReservedPhonemes.h        保留音素
AnalysisContracts.h       分析契约
AnalyzerReference.h       分析器引用
ExecutionBackend.h        执行后端（CPU/DirectML/CUDA）
VoicebankCatalog.{h,cpp}  声库目录扫描
```

`Modules/Inference` 新增：`GpuCatalog`、`InferStageModel`、`InferFrameLayout`、`PitchSampling`、
`PronunciationText`、`ReservedPhonemes`、`InferLogging`。
`Modules/Extractors` 新增：`AudioSlicer`、`AnalysisAudio`、`ExtractTask`、`ExtractionErrorDialog`。

#### 3.3.2 synthrt（引擎）——4 个提交

| 提交 | 做了什么 | 用户/集成方感知 |
| --- | --- | --- |
| `e94e022` | 从 `onnxruntime-builds` 取 ONNX Runtime。dsinfer 插件按**简单名**安装 | 依赖来源更规整，插件目录名不再带路径前缀 |
| `c56013c` | **让歌手声明自己的语言与保留音素**，并发布内置类别名 | 声库可以说"我只支持这几种语言"，避免加载不该加载的语言 |
| `1c7de62` | id 映射文件按**二进制**读，**拒绝大小无法确定的流** | 修掉一类"读到一半才发现流不对"的隐蔽错误 |
| `f2f0f8e` | CUDA 推理改用 cuDNN `HEURISTIC` 算法选择 | 行为更确定，但**放弃了自动调优**（见 §6 的 P2 条目） |

#### 3.3.3 wolf（语言学）——4 个提交

| 提交 | 做了什么 |
| --- | --- |
| `b651506` | 新增 **linguist 类别**、Level 1 契约、会话（session）与支撑层 |
| `8ef560b` | 新增 **7 个推理解释器插件** + **zxx 语言包**（zxx = 无语言内容，用于纯音素/哼唱类素材） |
| `03af778` | 端到端覆盖语言学域，并按加载器规则 lint 声明文件 |
| `876482e` | CI：构建、测试、**检查已安装包**，并写清构建与 **ABI 策略** |

这批改动体量最大（+28039 行），其中 162 个文件在 `src/tests/auto` 下——**测试占了大头**，属于"把新域用
测试围起来"的做法，方向是对的。

#### 3.3.4 otter（分析）——48 个提交，主要是一条 tifa 主线

- `fbafc70` 让音频与 tifa 图对齐 → `42c08fe` 按选择结果读标签 → `dc0dea8` 跟随分支的预处理样本与测试夹具
  → `006fd55` **按参考实现逐帧解码** → `a4f6978` 钉住参考实现在某个边界上的答案 → `262cd0a` 声明 tifa 对齐包。
- `e35724d` 修了一个很典型的 C++ 陷阱：**限定共享的 `run()`，避免被 provider 自己的 `run()` 隐藏**。
- 其余三十多个提交是文档：把测量口径、门槛读数、失败分类、语言代码陷阱一条条写清楚。
  **这份文档密度本身是优点**——它让后来者能复跑同样的测量。

### 3.4 用户能感知的变化（旧 → 新）

| 场景 | 改动前 | 改动后 |
| --- | --- | --- |
| 支持哪些声库 | 编辑器自带一套包读取逻辑 | 声库包**自己声明**支持的语言与保留音素（`wolf/lang-*` 依赖），编辑器据此裁剪 |
| 字→音素 | lite 内置 G2P 运行器（`G2pConvertRunner`） | 交给 **wolf** 的语言学模块，按语言包分发 |
| 推理编排 | lite 内直接编排 | 交给 **synthrt** 的歌手流水线（发音→音素→时长→音高→唱法→声学→声码器） |
| 从音频取音高/对齐 | 能力有限 | 交给 **otter**（含新的 tifa 强制对齐） |
| 给智能体用 | 无 | **无头模式 + MCP**：L1/L2/L3 控制级别，共 **100 个工具**，GUI 侧也能通过 MCP 被驱动 |
| 部署 | 目录散落多处 | `DeployLayout.h` 定义一次，打包脚本引用它 |
| 语言 | 固定语言集 | 语言包可独立升级（本机实测 15 个语言包） |

### 3.5 迁移与兼容性影响

- `feat(synthrt)!` 的 `!` 意味着**破坏性变更**：旧的自研 G2P/推理路径被删除，任何依赖旧路径的代码/脚本都会失效。
- 声库包需要能被新的 `VoicebankCatalog` 识别。实测 OpenUtau 那边的 **2.4 系列四个声库全部可加载**。
- 语言包与引擎的版本匹配变成**必须关注项**：本机实测踩到过"固定了旧语言包 → 所有 2.4 声库都打不开"的坑（§4.3）。

---

## 四、构建与依赖链现状

### 4.1 依赖是怎么钉死的

lite 通过三个 vcpkg overlay 端口把依赖钉到具体提交（`scripts/vcpkg-ports/<lib>/portfile.cmake`）：

| 端口 | 钉住的提交 | 备注 |
| --- | --- | --- |
| `synthrt` | `f2f0f8ee3669206ed90f951c17c397a23c5e4b6d` | 与 synthrt 仓 HEAD 一致 |
| `wolf` | `876482ec7033e8984c10b581f8358bcc435da9d7` | 与 wolf 仓 HEAD 一致 |
| `otter` | **`6f6b0a2`（审计时临时回退的值）** | 原值为 `f4820d99`，编不过（根因跨平台，见 §4.2）。otter 侧待推送的尖端见 §4.2 末：审计时快照为 `d59ffc1`，此后又经注释清理与提交压缩，SHA 已变，**以推送后的 `git rev-parse` 为准** |

> **时效注（2026-10-03 联合实测）**：`synthrt` 行的 pin 已随该分支推送前移到 `63bef25…`
> （`scripts/vcpkg-ports/synthrt/portfile.cmake:15`），四仓联合实测正是用该提交（从 GitHub 真实拉取并构建）。
> `wolf` 与 `otter` 两行仍是审计快照——两仓的本地尖端已重写且此后又各修了缺陷（wolf：失败运行不再吞掉
> stop 标志、`maxLen` 加上 `int` 上界。otter：lint 与对齐加载器的词典规则对齐），但**两仓都未推送**，
> 端口只能继续固定各自远端已有的提交。
>
> **时效注（2026-10-06）**：`otter` 行的 pin 已前移到 `e770ab94ac0a27a31c6cec502aeac765c9db70ef`
> （`scripts/vcpkg-ports/otter/portfile.cmake:20`），实测即远端 `origin/analysis-level-1` 尖端、比此前的
> `eb72a1f` 新 4 笔。**本机门禁通过**：真实 `vcpkg install` 从 GitHub 拉取该提交、只重建 otter（退出码 0），
> lite 构建 `BUILD_EXIT=0`，`ctest` **73/73、0 failed**（唯一 skip 仍是 `TestOtterExtraction`）。
> §4.2 与 §6.2(F-1) 的“推送后要改的那一行”至此已处置。

**要特别说明**：本次审计对四仓工作区的改动只有一处——`scripts/vcpkg-ports/otter/portfile.cmake` 的
pin 回退 + 一段说明为什么回退的英文注释。**该改动已随 `bc4bd79b` 提交**（审计快照时尚未提交），
`REF` 当时位于该文件 `:17`。**该处此后已随 pin 前移改动（见下方 2026-10-06 时效注），现行位置与取值以端口文件当前内容为准**。

### 4.2 P0：otter 固定版本编译不过（根因与平台无关）

- **现象**：用原 pin `f4820d99` 构建 otter 时，本地编译报 `C2660`——`F0Executive::run` 的调用点传了 5 个
  参数，而 Level 1 契约声明的是 `run(const F0StartInput &)`。
- **根因（对抗复核后更正）**：这是**标准 C++ 的成员名隐藏**，不是平台问题——
  `src/plugins/inferenceinterpreters/onnx/OnnxSupport.h:174` 在模板里**非限定**调用 5 参的 `run(...)`，
  而该模板被实例化到 `F0Api::F0Executive` 时，名字查找优先命中它自己的 1 参成员
  `run(const F0StartInput &)`（`include/otter/Api/F0/1/F0ApiL1.h:207`）→ 参数个数不符。
  **任何工具链都会失败**（先前我据 CI 现象判断"Windows 专属"，这个判断是**错的**，已更正）。
  至于 GitHub CI 上 `ubuntu-24.04` 为何是绿的，很可能是该 job 没有编译这组 onnx 解释器插件 —— **这一条未证实**。
- **修复已存在、但没被采纳**：分支上有提交 `fix(onnx): qualify the shared run() so a provider's own run()
  will not hide it`（审计时 SHA `e35724d`，两轮分支重写后为 `84f0c68`），把调用改成 `otter::onnx::run(...)`，
  提交说明原文是 "every Align provider failed to compile with an argument count error"。
  该提交现在是**收敛后提交序列里的第 1 个**（审计时快照：共 5 个提交、尖端 `d59ffc1`），但**尚未推送**。
  此后该分支又经注释与文档清理、并压缩了未推送提交，提交数与 SHA 均已变（尖端以 `git rev-parse` 实测为准）。
- **我做的处理**：把 pin 回退到 `6f6b0a2`（`feat!: identify languages by ISO 639-3…`，2026-09-28）,
  它在**本机可成功构建并安装**。注意 `6f6b0a2` 与 `f4820d99` **不是祖先关系**
  （`git merge-base --is-ancestor` 返回 1，两者相隔 4 个提交）——otter 的 `analysis-level-1` 分支被
  **force-squash** 过，所以"回退"实际是切到另一条线，不是简单地往回走一格。
- **建议**：二选一的条件与结论见 §6.2 F-1。
  **该前移已于 2026-10-06 完成**：`scripts/vcpkg-ports/otter/portfile.cmake` 的 `REF` 已改为 otter 远端尖端
  `e770ab94ac0a27a31c6cec502aeac765c9db70ef`（提交 `51cc9c7`，真实 `vcpkg` 拉取 + 构建 + `ctest` 复跑全绿），
  那段 `ROLLBACK 2026-09-30` 注释也已随之删除，并按当时的指引重跑过真实 `vcpkg install` 与门禁。
  **现行 `REF` 以端口文件当前内容为准，本节不再给出"要改成什么"的操作步骤**。
  下面这段是审计当时的原始指引，保留为历史记录：

  ```cmake
  # 审计时（回退值）  REF 6f6b0a2fa0dc18d1d42b5d9440d7d3cd7cfd4d28
  # 审计时拟改成      REF <otter 推送后的尖端 SHA>   # 不写死：审计快照 d59ffc1… 不会被推送
  ```

### 4.2.1 用户授权后对 otter 做的收敛与复验（快照 `d59ffc1`）

用户在报告之后授权修改 otter，要求"**实测 tifa 模型** + 收敛代码 + 清理临时产物 + 更新文档 + 压缩未推送提交"。
结论先行：**tifa 链路本来就是对的，本轮没有改一行代码**。价值在于用"干净读法"把结论重新钉死一遍，并清掉临时产物。

| 项 | 命令 | 读数 |
| :-- | :-- | :-- |
| 构建 | `cmake --build <构建树目录>` | `ninja: no work to do`（构建树已与源码同步） |
| 单测 | `ctest --test-dir <构建树目录>` | **10/10 通过**（含真实权重的 `test_Tifa`） |
| 端到端 | otter 门禁 `report-both.py`（**恒重跑变体、不读任何旧产物**） | **逐 take 42 通过 / 0 失败**，恒 `max 0.00f`、`moved 0/*`。batch-8 31 通过 / 11 失败 |
| 声明 lint | `check-declarations.py --declarations-only packages/{rmvpe,game,hfa,tifa}` | **0 error / 0 warning** |
| 脚本单测 | `python -m unittest discover -s scripts` | **Ran 27 tests — OK** |

- **42/42 是什么口径**：把 tifa 变体与 Python 参考原版（用其自带 venv 与真实权重）在**同一批 48 kHz 16-bit
  副本**上逐 take 比对，**每一条边界都落在同一帧**——不是"看起来差不多"，是零位移。
- **11 个 batch-8 失败不是缺陷**：已证实是**参考自身对批组成敏感**（同一 take 换批，参考自己的相似度
  `mean|Δ|` 达 0.049。11 例里连音素数都对不上，如 `p 27/26`、`p 58/60`），而宿主真实调用形态就是
  "一次一个 span" = per-take。**对本集成无影响。**
- **清理**：删旧构建树与逐轮运行产物，otter 仓占用 3422 MB → **2742 MB**（释放 680 MB）。两套门禁工具
  与模型素材**一个没删**，且清理后单跑门禁仍复现同一读数。
- **提交**：otter 仍是 **5 个**提交（本轮文档改动折进第 5 个），相对改写前的树差异只有该文档。
  提交消息、身份、新增内容**无本机路径/用户名**（本轮我一度在文档里写进了绝对路径，自查发现后已修掉）。
- **一条连带改动**：`TestOtterExtraction` 的夹具路径原指向被删掉的旧构建树夹具目录，
  实测改指当前构建树的夹具目录后同一用例**依然通过**（6 个 `ok`、退出码 0）。

### 4.3 语言包版本陷阱（本机实测踩到的坑）

这是复现过程中最容易浪费时间的坑，值得写清楚：

- 预设里原本固定了 `LITE_WOLF_LANG_PACKAGES=<wolf 检出>/build/lang-packages-0.1.0.0`，
  那是**过期**的开发产物。结果是：**四个 2.4 声库一个都打不开**，报错分别是
  `the multig2p configuration needs a languageMap`（yousa / 0913_wolf_club / zzm-kl）与
  `unknown pinyin configuration key: dictPath`（junninghua）。
- 改成让它走 vcpkg 端口产物后恢复正常：构建日志出现 `Staging 15 wolf language package(s)`，
  实测 `build/Debug/out/bin/wolf/packages` 下正好 **15 个包**：

  ```
  wolf-g2p-multi 1.0.0.4   wolf-g2p-pinyin 1.0.2.4      ← 2 个 G2P 引擎
  wolf-lang-cmn 1.0.1.4    wolf-lang-deu 1.0.0.4   wolf-lang-eng 1.0.0.4
  wolf-lang-fil 1.0.0.4    wolf-lang-fra 1.0.0.4   wolf-lang-ita 1.0.0.4
  wolf-lang-jpn 0.0.1.4    wolf-lang-kor 1.0.0.4   wolf-lang-por 1.0.0.4
  wolf-lang-rus 1.0.0.4    wolf-lang-spa 1.0.0.4   wolf-lang-yue 1.0.1.4
  wolf-lang-zxx 1.0.0.0                            ← 13 个语言包（12 门语言 + zxx）
  ```
- **版本不是精确匹配**：声库声明依赖 `wolf/lang-cmn 1.0.1.0`，而实际装的是 `1.0.1.4`，依然解析成功。
  这一点在排查"版本对不对"时很重要，不能按"必须完全相等"去判断。
- **顺带纠正一处文档错误**：`docs/plans/synthrt-main-migration.md:103` 写"manifest 请求全部
  **13 门语言与 zxx**"，实际是 **12 门语言 + zxx = 13 个语言包**（权威来源：
  `scripts/vcpkg-ports/wolf-lang-packages/assets.cmake:7`、该端口 `vcpkg.json` 的特性表、以及上面实测的
  装入树）。**多算了一门**。（P3，文档勘误）

### 4.4 测试结果：73 项，72 通过 / 1 失败 / 0 跳过

| 结果 | 数量 | 说明 |
| --- | --- | --- |
| Passed | **72** | 含核心的引擎、自动化、包管理、单实例等套件，以及**接上夹具后真正跑起来的 otter 提取**（§4.5③） |
| Failed | **1** | `TestAnalyzerPackages` —— **测试自身的 Windows 缺陷**（§4.5②），与产品行为无关。**2026-10-01 已修，suite 现 73/73** |
| Skipped | **0** | 首跑时 `TestOtterExtraction` 因缺夹具跳过（exit 77）。接上 `LITE_OTTER_FIXTURES` 后通过 |

复跑命令见 §9.1。**唯一失败项不影响"合成无误"的结论**，理由见 §4.5②（该项已于 2026-10-01 修复，套件现 73/73）。

### 4.5 两个曾失败项与一个曾跳过项，逐条说清楚

#### ① `TestVoicebankAudit`：我的配置错了（已修复）

- **断言**：报 `every language the package depends on reaches a linguist: declared 3, reached 4`。
- **根因**：这个测试会扫描**声库所在的父目录**并取 `singers.front()`
  （`src/tests/TestVoicebankAudit/main.cpp:383`、`:392`），再与声库 `desc.json` 里声明的
  `wolf/lang-*` 数量比对（`:431-443`）。我把环境变量指到装有 **4 个声库**的 `Singers` 目录，
  于是"取到的第一个歌手"（`0913_wolf_club` 的 4 语言）与被读 `desc.json` 的声库（yousa 的 3 语言）对不上。
- **修复**：建一个只含 yousa 一个包的目录（junction），指向该目录即可 → 该测试**通过**。
- **顺带结论**：测试对"父目录里有多少包"是敏感的，这是一个**测试脆弱点**（P3）：建议测试把扫描根
  收敛到声库自身，而不是它的父目录。

#### ② `TestAnalyzerPackages`：Windows 专属的测试缺陷（**已于 2026-10-01 修复**）

- **测试意图**（`src/tests/TestAnalyzerPackages/main.cpp:209-217`）：造一个"无权访问"的目录
  `scanRoot/locked`，用 `fs::permissions(locked, fs::perms::none)` 模拟权限不足。然后
  （`:265-276`）探测 `fs::is_regular_file(locked/"desc.json", probe)`，断言"探测失败 **或** 引擎报了问题"。
- **实测结论**：在 Windows 上 `fs::permissions(dir, fs::perms::none)` **并不真的拒绝访问**。
  我编译了一个独立探针（本机临时产物，未入库）验证：目录仍然 `is_directory=true`、`permissions=365`。
  而探测拿到的错误码是 `value=2` = `no_such_file_or_directory`（"系统找不到指定的文件"）。
  于是 `!probe` 为真——测试要求引擎"必须报问题"，但引擎**故意**把 ENOENT 过滤掉不报
  （`src/libs/SynthrtEngine/VoicebankCatalog.cpp:414` 只在
  `entryError && entryError != std::errc::no_such_file_or_directory` 时才报问题），于是测试失败。
- **为什么 Linux 上能过**：Linux 的 `perms::none` 是真的拒绝（EACCES），引擎会报问题，断言成立。
- **结论**：这是**测试的可移植性缺陷**（P2，仅测试）。建议改用**显式注入的错误码**或改到 POSIX 上跑，
  而不是指望 Windows 的权限模型。
- **补正（2026-10-01 第二轮）**：上面的因果链对**当时**的代码是对的。其后为了在 Windows 上不再必然失败，
  断言被放宽成 `expect(!readDenied || reported)`（`readDenied = probe && probe != no_such_file_or_directory`），
  于是探针拿到 ENOENT 时断言**恒真**——Windows 天天绿了，但这个用例**不再真正验证任何东西**（本轮复跑 4 次全部
  通过，从结果上看不出问题）。按用户决定，本轮重新处理了它：
- **修复（2026-10-01）**：让 `locked` 目录里**放入一个必然被拒绝的包**
  （`writePackage(..., noteDeclaration("LOCKED", false))`，即声明 `supportsKnownNotes` 却没有
  `durationToBoundary` 模型），同时保留 `perms::none`。于是三条路径必有一条命中：POSIX 上读 `desc.json`
  得到 EACCES → `VoicebankCatalog.cpp:413-417` 报问题。权限模型不生效的平台（Windows，或以 root 运行）
  上声明可读但 `openPackage` 失败 → `:419-424` 报问题。断言因此改为**无条件**要求 `reported`
  （现 `main.cpp:266-279`），删掉了那个会把竞态变成红/绿两种结果的逃生口。

#### ③ `TestOtterExtraction`：接上夹具后**真正跑起来并通过**（已解决）

- **首跑为什么跳过**：找不到夹具包，提示 "set `LITE_OTTER_FIXTURES` to the directory that contains
  fixture-rmvpe and fixture-note"（exit 77）。
- **夹具其实就在本机**：otter 检出内的构建夹具目录下同时有 `fixture-rmvpe`（3 个文件）
  与 `fixture-note`（7 个文件）——它们是**合成模型**，输出确定：rmvpe 为每 10 ms 帧返回一个音高，
  note 每 0.2 s 转录一个音符、键号递增（见 `src/tests/TestOtterExtraction/main.cpp:3-4`）。
- **接上之后**（`LITE_OTTER_FIXTURES` 指向该目录）实测**通过**，而且是真的在跑推理，断言逐条可见：

  ```
  音高提取  Session [rmvpe.onnx] - Finished inference in 0.0001689 seconds
            ok  the pitch extraction succeeds: Successfully extracted pitch.
            ok  one span yields one segment
            ok  the curve starts at the first grid point of the visible region: 865 vs 865
            ok  the curve ends at the last frame: 306 points vs 306
            ok  the fixture reports voiced frames
            ok  no point lies between the unvoiced marker and a sung pitch
  MIDI 提取 Session [encoder.onnx] / [segmenter.onnx] / [bd2dur.onnx] / [estimator.onnx] 依次推理
            ok  the MIDI extraction succeeds: Successfully extracted note.
            ok  the fixture transcribes notes
            ok  every note is placed on its 0.2 s step in clip-local ticks
            ok  the first note starts at the trim, not at the start of the clip
  取消语义  ok  a task terminated before it runs reports Terminated
            ok  a task stopped during the analysis reports Terminated, not ModelRunFailed
  ```
- **意义**：这条链覆盖了 **otter 的真实 ONNX 推理 + 到编辑器的数据回写 + 任务取消语义**，
  是"合成之外"最实质的一块可靠性证据（详见 §5.4）。
- **建议**：把 `LITE_OTTER_FIXTURES` 写进 CI 与本地测试约定，否则 otter 提取在 Windows 上会**静默变成测试盲区**
  （首跑就是这样：exit 77 跳过，很容易被忽略）。

---

## 五、可靠性实测

### 5.1 无头模式：端到端合成（通过，且做了数值级验证）

**怎么跑的**：以隔离数据根启动
`DsEditorLite.exe --headless --no-mcp --control-level l3 --control-port <P>`，通过
`POST http://127.0.0.1:<P>/automation/v1`（JSON-RPC 2.0）依次驱动：
`application.get_status` → `voices.list` / `voices.describe` → `tracks.insert` → `tracks.set_voice`
→ `clips.insert` → `notes.insert` → `exports.audio.start` → `tasks.get`。

**合成输入**：声库 `yousa` 2.4（`package_id=yousa`、`package_version=1.65.1`、
singer `yousa`、speaker `Yousa_Bright`）。歌词 `ni hao`（普通话 `cmn`）。两个音符各 960 tick，
分别落在 tick 960 与 1920。唱段长 7680 tick。

**过程证据**：

- `voices.describe` 返回 `resolution_state: "resolved"`、`g2p_ready: true`（cmn / eng / jpn），
  5 个 speaker（`Yousa_Bright` 为默认，另有 Classic / Cute / Normal / Whisper），`mixing_supported: true`。
- 日志链路完整：获取发音 → 获取音素名称 → 时长推理 → 音高推理 → 唱法推理 → 声学推理。
- 导出任务：`state: "succeeded"`、`progress.value: 100`。

**产物数值分析**（自写 WAV 检查器，本机临时产物，未入库。我逐个字段解析而不是猜）：

```
format: code=3 (float) channels=1 rate=44100 bits=32
frames=352800 duration=8 s
peak=-17.02 dBFS   rms=-32.15 dBFS（含 5 秒静音，故整体 RMS 偏低）
voiced region: 1.0 s .. 2.9 s
  f0 near 1.119 s = 265.66 Hz  (+0.26 半音)
  f0 near 1.594 s = 262.50 Hz  (+0.06 半音)
  f0 near 2.069 s = 259.41 Hz  (-0.15 半音)
  f0 near 2.544 s = 265.66 Hz  (+0.26 半音)
```

**结论（事实）**：

1. 音频是 **float32 / 单声道 / 44100 Hz / 8.0 秒**。
2. **发声区间 1.0–2.9 秒**，与"两个音符在 tick 960/1920、时长 960 tick"完全对上
   （7680 tick = 8 秒 ⇒ **960 tick/秒**）。区间外是**精确的零**（−180 dBFS）。
3. 基频 **259.4–265.7 Hz，围绕 C4 = 261.63 Hz 波动 ±0.27 半音以内**——这是带颤音的神经歌声模型的正常表现，
   **音高是对的**。
4. 峰值 −17 dBFS、发声段约 −23 dBFS，电平正常，没有削顶。

> 我最初把 32 位数据当 16 位读，得出过"时长 16 秒、非静音区 1.6–6.2 秒"的错误中间结论，已按正确位深重算并纠正。
> 这条写在这里是为了说明：**中间结论也可能错，最终数值是重算过的**。

### 5.2 GUI 端到端冒烟（通过）

**环境说明**：本会话**没有可用的 computer-use 能力**（`wincu` MCP 服务器断开、技能目录里也没有
`computer-use` 技能）。因此我改用**应用自身对外提供的 GUI 控制通道**（GUI 模式下的 MCP `/mcp`，
`--mcp --control-level l3`）+ **真实窗口截图**来观察与驱动，全程只对一个隔离实例操作，不碰用户已有的实例与工程。

| 断言 | 结果 | 证据 |
| --- | --- | --- |
| 真实 GUI 实例启动 | ✅ | 进程 + 窗口标题 `新工程 - DS Editor Lite`、日志 `App launched in 4124 ms` |
| 声库加载 | ✅ | 轨道显示 `YOUSA 1.65b / Yousa_B_`，唱段标题 `新歌声剪辑 0dB YOUSA 1.65b / Yousa_Bright cmn` |
| 绘制音符（留出前导空白） | ✅ | `ni`（C4）、`hao`（E4），各 960 tick，起点在 tick 1920/2880（前面留 2 拍） |
| 合成：音高曲线 | ✅ | 音符上下可见深色音高曲线（含颤音） |
| 合成：音素信息 | ✅ | 音符内显示唱词与音素标签（`ni` / `hao`） |
| 合成：非空非平直波形 | ✅ | 音符内可见能量波形，编曲区唱段内有两个蓝色波形块 |
| 推理真的跑了 | ✅ | 日志：`linguistic.onnx` / `pitch.onnx` / `variance.onnx` / `model.onnx` / `vocoder.onnx` 依次创建，`InferPitchTask` / `InferVarianceTask` / `InferAcousticTask` 均 `Success` |
| 播放推进 | ✅ | `state: stopped` → `playback.play` → `state: playing`，`position` 由 **0** → **4792.32 / 5632.0** tick。界面运输时间 **001:01:00 → 003:02:00**，播放头前移 |
| 保存 DSPX | ✅ | `documents.save_as` → `saved:true`、`dirty:false`、`on_save_point:true`。文件 **14456 字节**，标题变为 `gui-smoke - DS Editor Lite`（无未保存标记） |
| 文件内容 | ✅ | DSPX 是明文 JSON：`tempos:[{pos:0,value:120}]`、4/4、note `keyNum:60/64`、`lyric:"ni"/"hao"`、`language:"cmn"`、含 phonemes |

截图（本机会话产物，未入库）：

- 合成完成、音高曲线/音素/波形齐备：本机会话产物（未入库）。
- **播放中**（运输时间前进 + 播放头前移 + 播放键高亮）：本机会话产物（未入库）。
- 存盘后（标题无未保存标记）：本机会话产物（未入库）。

复现脚本：本机临时脚本（未入库），分别负责起实例、截图与 MCP 调用。

**与技能的差异（如实说明）**：项目技能 `gui-smoke-test` 要求把测试目录建在**系统临时目录**并以
`ds-editor-lite-gui-smoke-` 开头，我用的是工作区内的一个未入库临时目录（同样隔离、可清理，且在
gitignore 内）。**测试会话的产物我保留在本机（未入库）**，没有按技能要求删除：它们是 §5.2 三张截图与复现脚本那一组结论的原始证据。
如需清理，删掉该临时目录即可。按技能的临时限制，**未打开音频导出对话框、未做文件级 WAV 导出校验**
（WAV 侧的证据由 §5.1 的无头通道提供）。

### 5.3 两种模式的对比结论

| 维度 | 无头模式 | GUI 模式 |
| --- | --- | --- |
| 控制通道 | 原生 `/automation/v1`（**仅无头模式存在**，`EditorMcpController.cpp:37-41`） | MCP `/mcp`（需要 `--mcp`，`--no-mcp` 时根本不监听） |
| 合成 | ✅ 成功且数值验证 | ✅ 成功（同一套推理链，界面渲染正确） |
| 播放 | 未测（无头无音频设备语义） | ✅ 运输推进、播放头前移 |
| 存盘 | ✅ 导出 WAV 成功 | ✅ 另存 DSPX 成功 |
| 发现的问题 | 缓存不随数据根隔离（§6） | 同上。另观察到 150 ms 退出延迟的 MCP 关闭逻辑（§6） |

**综合结论：合成链路在本机（DirectML / NVIDIA <model>）上是"可信"的**——同一条推理链在两条完全不同的
驱动路径下都产出了正确结果，且无头侧有数值级证据。

### 5.4 第三条腿：otter 提取链路（接上夹具后通过）

无头与 GUI 验证的是"合成"，而**提取**（从音频反推音高与音符）是另一条独立链路。本机用 otter 自带的
合成夹具把它跑通了：

- 命令：`LITE_OTTER_FIXTURES=<otter 检出内的构建夹具目录>` +
  `ctest --test-dir build\Debug -R TestOtterExtraction --output-on-failure`。
- 结果：**通过**，且断言覆盖到"曲线首尾位置""fixture 报了 voiced 帧""unvoiced 标记与音高之间没有点"
  "每个音符落在 0.2 s 步进上""首个音符起点在 trim 处"，以及**取消语义**
  （取消要报 `Terminated`，不能报 `ModelRunFailed`）。断言全文见 §4.5③。
- 夹具是**输出确定的合成模型**，所以这些断言是**可复现**的，不是"看着像对"。

> **一处方法论提醒**：首跑时这个测试是 **exit 77 跳过**的——**跳过不等于通过**。只看
> "73 项 / 71 通过"的汇总数字，会误以为 otter 提取有覆盖。这是本次审计里"差点漏掉"的一块，
> 也是为什么 §4.5 要把每一项单独说清楚。

### 5.5 computer use 这一路的现状（如实交代）

目标里要求"允许使用无头模式和 computer use 分别测试可靠性"。**本会话的 computer-use 能力不具备**：

- `wincu` MCP 服务器**始终处于断开状态**（本轮再次确认：`mcp-client(wincu): server is disconnected`，
  资源列表与模板列表均不可用）。
- 技能目录里**没有** `computer-use` 技能（`gui-smoke-test` 技能明确要求"完整读取并遵循 `computer-use` skill"，
  但该技能在本环境不可用）。

因此 GUI 一侧我改用**应用自身对外提供的控制通道**（GUI 模式下 `--mcp` 暴露的 `/mcp`）+ **真实窗口截图**
来驱动与取证（§5.2），并用 `SendInput`/截图以外的**只读观察**来确认真实渲染结果。
**这不是"人工点测"**，请按"另一种自动化驱动方式"看待本节与 §5.2 的结论。
若要严格意义上的 computer use，需要在环境里恢复该 MCP 服务器后重跑一遍 GUI 冒烟。

---

## 六、发现清单

> 严重性定义：**P0** 阻断发布 / **P1** 高（可导致功能错误或测试失真）/ **P2** 中（可维护性或边界风险）/
> **P3** 低（整洁性、文档、防御性缺口）。
> 「复核」列：`确认` = 对抗复核轮独立确认。`降级` = 复核后严重性下调。`推翻` = 复核找到反例，**不作缺陷计**。

### 6.1 汇总表

| 编号 | 严重性 | 位置 | 问题一句话 | 复核 |
| --- | --- | --- | --- | --- |
| F-1 | **P0** | `scripts/vcpkg-ports/otter/portfile.cmake:17`（原 REF `f4820d99`） | 固定版本编不过。根因是 C++ 成员名隐藏，**与平台无关** | 确认（并**纠正**我"仅 Windows"的误判） |
| F-2 | **P1** | `PublicAutomationHostAdapter.cpp:1406-1407` vs `ExtractionAutomationAdapter.cpp:54-79` | 能力查询说"可用"，实际发起被拒 | 确认（无统一收口） |
| F-3 | **P1** | `InferPitchTask.cpp:148-152,161-164`。`InferTaskHelper.cpp:41-50` | 失败可能**没有错误信息**，空 words 仍继续推理 | 降级（原"失败仍报成功"被推翻） |
| F-4 | **P1** | `InferenceOption.h:80-82` vs `SingleInstanceIdentity.cpp:13-20` | 隔离数据根**不隔离**缓存/日志/转储 | 确认（并补出同类路径） |
| F-5 | **P2** | `cmake/LiteBuildApi.cmake:184,190,454-458` | ORT payload 靠猜 + 只 WARNING，**无下游兜底** | 降级（"运行期崩"未证实）→ **2026-10-03 部分已修**（见 §6.2 F-5） |
| F-6 | **P2** | `McpHttpServer.cpp:664-685`（尤其 673-675） | 关闭时固定等 150 ms：白等，且超时响应被切断 | 确认（双向问题） |
| F-7 | **P2** | `PackageManager.cpp:221-226` | 扫描失败一律映射成"后端未初始化" | 确认（后果被补全） |
| F-8 | ~~P2~~ **非缺陷** | `AutomationTaskManager.cpp:188-222` + `ExtractionAutomationFacade.cpp:507-515` | `beginCommitting` 返回 `false` 表示"已在提交边界取消"，终态**已由管理器回调**，facade 只补一次 `notifyFinished`——行为正确 | **已澄清**（读到实现后结案） |
| F-9 | **P2** | `cmake/LiteBuildApi.cmake:586,643-644,702-703` | 用 `CMAKE_BUILD_TYPE` 判配置：多配置生成器下会剥掉 Debug 符号 | 新发现（潜在） |
| F-10 | ~~P2~~ **已修** | `src/tests/TestAnalyzerPackages/main.cpp:214-216,266-279` | 用 `perms::none` 模拟拒访，Windows 上不成立 → 夹具改为"必然被拒绝的包"，断言改无条件（见 §4.5②） | 我方实证 → 2026-10-01 修复 |
| F-11 | **P2** | `packaging/windows/*.ps1` / `LiteBuildApi.cmake:294` | `plugins/<lib>` 目录名三处写死，且**不在** `DeployLayout.h` 里 | 确认 |
| F-12 | **P3** | `PackageManager.cpp:247-251` | 声库 `url`/`readme` 被静默丢弃，编辑器永不显示 | 新发现 |
| F-13 | **P3** | `src/app/CMakeLists.txt:54` | `ONNXRUNTIME_ENABLE_DML` 定义了但**无人读** | 确认 |
| F-14 | **P3** | `docs/plans/synthrt-main-migration.md:103` | "13 门语言与 zxx"多算一门（实为 12 + zxx） | 确认 |
| F-15 | **P3** | `scripts/vcpkg-ports/synthrt/portfile.cmake:76-79` | 无条件删 `lib/cmake`：实际无害，属语义冗余 | 降级 |
| F-16 | **P3** | `wolf/include/wolf/Support/ExecutiveTask.h:52-66` | `start()` 无重复启动/状态前置校验 | 未再复核 |
| F-17 | **P3** | `wolf/src/lib/Session/LinguistSession.cpp:815-824` | 槽位借出/归还手工配对，非 RAII：抛异常则泄漏 | 未再复核 |
| F-18 | **P2（推断）** | `synthrt` `ExecutionProvider_CUDA.cpp:37-38` | 固定 `HEURISTIC` 放弃 cuDNN 自动调优，CUDA 上可能变慢 | 未实测 |
| F-19 | **P3** | `scripts/convert-voicebank.py:698-710` vs `wolf/.../SingerContrib.cpp:122-167` | 脚本自述"与加载器同样的校验"，实际不同构（漏 NUL 与 map 值路径解析） | 新发现 |
| F-20 | **P3** | `scripts/convert-voicebank.py:58-65` | 契约三元组（interface/variant/kind）三处重复，无单一真相源 | 新发现 |
| F-21 | **P3** | `ExtractionAutomationFacade.cpp:68-101` | 音高回写只校验溢出与有限性，**不校验段是否落在片段内 / 值是否为空 / 点数是否有界**。同文件 MIDI 回写反而更严 | 主代理亲读 |
| F-22 | **P2** | `wolf/.../lua/main.cpp:122` + `LuaSandbox.cpp:332` | lua 插件**装载期执行脚本没有打断路径**：顶层 `while true do end` 会**永久挂死包加载** | 复核新发现（已核） |
| F-23 | **P3** | `wolf/.../lua/main.cpp:89`、`multig2p/Decoder.cpp:161` | manifest 里的脚本路径**不限定在包内** → 任意宿主机文件被当 Lua 代码读入执行 | 复核新发现（已核） |
| F-24 | **P3** | `wolf/.../lua/LuaSandbox.cpp:44-161` | 自制的 `utf8` 库有语义缺陷（`char` 不校验码点范围、`len` 忽略 i/j、`offset` 语义错），与自己头文件"完整标准接口"的声明矛盾 | 复核新发现（已核） |
| F-25 | **P3** | `wolf/.../multig2p/main.cpp:146-153`、`main.cpp:222-227` | 具体错误被丢弃（只报笼统失败）。`maxLen` 只校验为正、不设上界，截断后**静默退化成"全部失败"** | 复核新发现（已核） |
| F-26 | **P3（未证实）** | `otter/.../tifa/Decode.cpp:109-130` | 平局口径疑与文档记载的参考规则不符（文档写"平局取最小 state 序号"，代码按"施加顺序 + 严格大于"定平局）。**需真实 Python 参考才能判定** | 复核新发现 |
| F-27 | **P3（背景项）** | `wolf/.../lua/main.cpp:297-299`、`multig2p/main.cpp:332-335`、`chain/main.cpp:418-426` | 无校验下转型在 lua/multig2p **当前不可达**。仓内反而有更严的 `chain` 三重校验（interface/level/variant + 空指针） | 复核核实（**与台账 #9 重叠，不单独排期**） |

### 6.2 重点条目详述

#### F-1（P0）otter 固定版本编不过 —— **而且不是 Windows 的问题**

- **现象**：本地构建 otter @ `f4820d99` 报 `C2660`：`F0Executive::run` 不接受 5 个参数。
- **根因（复核后纠正）**：`src/plugins/inferenceinterpreters/onnx/OnnxSupport.h:174` 在模板 `runModel` 里
  **非限定**调用 `run(session, std::move(inputs), wanted, what, lambda)`（5 参），而模板被实例化为
  `OnnxExecutive<F0Api::F0Executive>`，该类自己有一个 1 参成员
  `run(const F0StartInput &)`（`include/otter/Api/F0/1/F0ApiL1.h:207`）。
  C++ 名字查找优先命中成员 → **参数个数不符**。
  **这是标准 C++ 的成员名隐藏，与平台、编译器无关**——我先前"仅 Windows 失败"的说法是**错的**，特此更正。
  （Linux CI 之所以是绿的，很可能是该 job 没有编译这组 onnx 解释器插件，这一条**未证实**。）
- **修复已存在但没被采纳**：分支上有提交 `fix(onnx): qualify the shared run() so a provider's own run() will
  not hide it`（审计时 SHA 为 `e35724d`，分支重写后为 **`84f0c68`**；已核对它是收敛尖端 `d59ffc1` 的祖先），
  把调用改成 `otter::onnx::run(...)`。该提交**尚未推送到远端**（远端 `origin/analysis-level-1` 仍停在
  `f4820d99`）。
- **我做的处理**：把 lite 的 pin 回退到 `6f6b0a2`（**2026-09-28** 的提交，本机可构建可安装）。该改动已随
  `bc4bd79b` 提交（`REF` 在 `scripts/vcpkg-ports/otter/portfile.cmake:17`）。
- **结论（二选一）**：正式发布前必须由 otter 侧推送该分支、并把 pin 前移到含修复的提交（收敛尖端
  `d59ffc1`），或明确把 pin 停在 `6f6b0a2`——**别停在"本机能编、pin 编不过"的中间状态**。推送后要改的那一行
  见 §4.2 末。

> **时效注（2026-10-06）**：本项已处置——`otter` 的 pin 已前移到 `e770ab94ac0a27a31c6cec502aeac765c9db70ef`
> （提交 `51cc9c7`），实测该提交的 `src/plugins/inferenceinterpreters/onnx/OnnxSupport.h:196` 已是限定调用
> `otter::onnx::run(...)`，故上面“二选一”的未决状态结束，**现行 pin 以端口文件当前内容为准**。

#### F-2（P1）自动化"能力查询"与"受理"不一致

- **能力侧**：`PublicAutomationHostAdapter.cpp:1406-1407` 判断"可用的音高模块 = 装了**任一** f0 analyzer"，
  `:1474-1475` 的 `available` 由"来源就绪 && 模块就绪 && 已配置"拼出。
- **受理侧**：`ExtractionAutomationAdapter.cpp:361-365` → `validateAnalyzer`（`:54-79`）要求
  **配置里写明的那个 reference 必须在已安装集合里**，否则报 `FileNotFound` 拒绝。
- **可复现的不一致**：配置里残留了一个**已卸载/已改名**的 analyzer reference，而机器上装着别的 f0 analyzer →
  能力查询回答 `available: true` 且没有 `unavailable_reason`，实际发起却被拒。**同一个响应内部还自相矛盾**：
  `available: true` 但 `models[]` 里每一项 `configured/available` 都是 false（`:1445-1447`）。
  代码注释（`:1372-1375`）自称要把"未选择"和"已选择但不再安装"分开，`:1474-1475` 并没有实现该区分。
- **为什么没有兜住**：能力查询只在查询路径使用（`PublicAutomationRegistry.cpp:3806`），发起路径走
  `ExtractionAutomationFacade.cpp:189` → `preparePitch`，**同一语义写了两遍**，没有统一收口。
- **影响**：智能体/连接器按能力清单编排任务时会踩空，且错误信息指向"文件找不到"而不是"配置过时"。
- **建议**：把"reference 是否命中已安装集合"抬成唯一判据，查询与受理共用同一函数。

#### F-3（P1）推理失败可能"没有错误信息"

> 这一条的原判（"失败仍上报成功"）**已被对抗复核推翻**，下面是修正后的真实问题。

- **被推翻的部分**：`InferAcousticTask.cpp:142-145` 的失败分支在 `m_success.store(true)`（`:148`）**之前**
  就 `return` 了，上层 `BaseInferState.cpp:116` 用 `task.success()` 判定失败并 `emit failed()`。
  `InferPitchTask.cpp` 同构（`:117-120` vs `:130`）。**所以"失败被当成成功"不成立。**
- **真实问题一：失败可以没有原因**。`InferPitchTask.cpp:148-152`（speaker mapping 缺失只 `qCritical`、
  不写 `error`）与 `:161-164`（收到终止请求后直接 `return false`）会让日志出现 **`Task failed:` + 空串**，
  排障时拿不到任何线索。
- **真实问题二：空输入仍继续推理**。`InferTaskHelper.cpp:41-50` 校验失败只 `qCCritical` 后 `return {}`，
  调用方 `InferPitchTask.cpp:225` **不检查**返回的 words 是否为空，于是 `frames = 0` 的模型继续往下走
  （`:232-259`）。最终是否被引擎判失败**未证实**，但"校验失败后继续"这条链路是事实。
- **建议**：所有失败分支必须落一条可机读的原因。对空输入做前置拒绝。

#### F-4（P1）「隔离数据根」只隔离了一半

- **机制（事实）**：两处"应用数据目录"解析**不同源**——
  - 配置目录与单实例锁走 `SingleInstanceIdentity.cpp:13-20`：Windows 上读 **`APPDATA` 环境变量**
    （`AppOptions.cpp:13`、`SingleInstanceCoordinator.cpp:342/365`）。
  - 推理缓存默认值走 `InferenceOption.h:80-82`：`QStandardPaths::standardLocations(AppDataLocation).first()`
    + `/Cache`，**并在选项加载时就 `mkpath`**（`InferenceOption.cpp:7-16`）。
- **实测**：以重定向后的 `APPDATA` 启动（隔离数据根）跑合成，日志里的缓存路径是**真实的**
  `<用户目录>\AppData\Roaming\OpenVPI\DS Editor Lite\Cache`，隔离根下**没有** `Cache` 目录。
- **同类路径**（复核轮补出）：`AppLogDirectory.cpp:14`（日志）、`CrashHandler.cpp:17`（崩溃转储）、
  `LoggingBootstrap.cpp:21,28`。
- **后果**：① 依赖"隔离数据根"的测试**并不隔离**，会命中真实用户的缓存（可能命中不匹配的缓存——
  命中判定只看文件属性，`InferAcousticTask.cpp:106-113`）。② 真实用户缓存被测试/临时实例污染。
  ③ 缓存**没有跨进程锁**，而不同数据根 = 不同单实例锁 → 两个实例可以并发读写同一份缓存。
  ④ 无写权限环境下 `mkpath` 失败只 `qCritical`（`InferenceOption.cpp:14`），不阻断启动，故障推迟到写盘。
- **未证实**："`AppDataLocation` 在 Windows 上不读 `APPDATA`"这一点没有 Qt 源码级证据（本机无 Qt 源码），
  依据是 Qt 实现按 shell 已知文件夹取值 + 本次实测路径未随重定向改变 + 代码两处取值不同源，三项互相印证。
- **建议**：让缓存/日志/转储都从同一条"数据根"解析函数取路径。

#### F-5（P2）ORT payload 靠猜，且没有下游兜底

- `cmake/LiteBuildApi.cmake:184` 硬编码 `share/onnxruntime-builds/runtime/default`。`:190`
  `find_package(onnxruntime-builds CONFIG QUIET)` 之后**没有**使用包自己声明的
  `ONNXRUNTIME_BUILDS_RUNTIME_DIR`（该变量确实存在，见 `scripts/vcpkg/ports/onnxruntime-builds/
  onnxruntime-buildsConfig.cmake.in:6`，而且同仓 `src/tests/TestVoicebankAudit/CMakeLists.txt:50` 就用了它）；
  `:456-460` 猜不到时**只 WARNING**，而同文件 `:398-402`（语言包，`LITE_INSTALL` 下）与
  `cmake/OrtRuntimeGate.cmake:39`（CUDA）都是 **FATAL_ERROR**。
- **复核带来的修正**：① 当前硬编码路径与实际 payload **一致**（实测 `runtime/default` 下 4 个 DLL：
  `onnxruntime.dll`、`onnxruntime_providers_shared.dll`、`DirectML.dll`、`DirectML.Debug.dll`，
  与端口 `portfile.cmake:52-53` 一致）。② "运行期崩"**未证实**——加载失败时
  `OnnxDriver.cpp:68` 是返回错误，不是崩溃。所以严重性从 P1 下调为 P2。
- **仍然成立的部分**：**没有兜底**——`cmake/LitePackagingLayoutCheck.cmake` 只检查语言包，
  `OrtRuntimeGate` 只检查 cuda，`build-installer.ps1:263-273/298-309` 只检查插件三棵树与 cuda 目录。
  一处路径改变就会静默产出"能装但推理不可用"的包。
- **建议**：优先使用包声明的变量。把"找不到 payload"提到 FATAL。在打包后布局校验里补一条 ORT payload 检查。
- **2026-10-03 处置（落地提交见 `cmake/LiteBuildApi.cmake` 的改动，四仓联合实测同日）**：第一条已落地——`cmake/LiteBuildApi.cmake` 先
  `find_package(onnxruntime-builds CONFIG QUIET)`，再从 `ONNXRUNTIME_BUILDS_RUNTIME_DIR` 取默认 payload
  （CUDA 那条本来就读包变量），缺失告警区分"包未声明"与"目录不存在"。改后完整重跑 `ctest` **73/73**，
  无头端到端合成真跑 5 个 ONNX 模型并导出 8 s 波形通过。**未做**：FATAL 升级（会把"树里没有 payload"
  从告警变成构建失败，是否要这条需拍板）与打包后布局校验补 ORT payload 检查。

#### F-9（P2，新发现）`CMAKE_BUILD_TYPE` 与多配置生成器

复核员通读 `cmake/LiteBuildApi.cmake:542-751` 后给出 6 项，其中两项值得单列：

- `:591`、`:650-651`、`:710-711` 用 **`CMAKE_BUILD_TYPE`** 判断当前配置，**多配置生成器**（VS 解决方案）
  下这个变量为空 → **符号 staging 被静默跳过**，而 `:712-723` 的"清除"逻辑**恒为真**，
  会把任何配置（包括 Debug 安装）的插件 PDB/dSYM 剥掉 —— 与同文件 `:740-743` 用 `$<CONFIG:…>` 的正确写法
  、以及"Debug 安装应带符号"的规格（`:162-166`）**互相矛盾**。
  当前所有 preset 都是单配置 Ninja（`CMakePresets.json:13,30,47,67`），所以**目前不触发**，属潜在缺陷。
- `:568-573`、`:645-648` 两条"`_lite_ort_relative` 为空"的 WARNING 是**不可达代码**（`:15-21` 已 FATAL，
  `:191` 取的是同一常量）——留着会误导维护者以为这里有兜底。

#### F-10（P2 → 已修）`TestAnalyzerPackages` 的 Windows 缺陷

**已于 2026-10-01 修复**：夹具改为"必然被拒绝的包 + `perms::none`"，断言改为无条件要求报出问题。详见 §4.5②
（含对上面那条因果链的补正）。**这是本轮唯一"测试本身有问题"的确认项**，修复后 lite 套件 73/73。

#### F-11（P2）`plugins/<lib>` 目录名三处写死

- `cmake/LiteBuildApi.cmake:294`（`plugins/${_library}`，`_library` 来自 `:241` 的 `ITEMS dsinfer wolf otter`）、
  `packaging/windows/build-installer.ps1:267-269`、`packaging/windows/build-portable.ps1:250-252`
  **各自独立**写死同一字面量。
- **讽刺的是**：`src/libs/SynthrtEngine/DeployLayout.h` 自称"部署布局的单一真相源"，但它**根本没有定义
  `plugins/<lib>` 这一层根名**（只有 `:23-38` 的类别目录与 `:41/:46/:49`）。
- **有限兜底**：configure 期只有 WARNING（`LiteBuildApi.cmake:281-287`），打包脚本缺目录会 throw
  （`:278`）——所以改名/增减库在**打包时**能发现，开发构建发现不了。
- **建议**：把 `plugins/<lib>` 也收进 `DeployLayout.h`，三处引用它。

#### F-18（P2，推断）CUDA 固定 `HEURISTIC`

synthrt `f2f0f8e` 把 cuDNN 算法选择固定为 `HEURISTIC`（`ExecutionProvider_CUDA.cpp:37-38`）。
好处是**行为确定、启动快**。代价是**放弃自动调优**，在 CUDA 机器上可能比默认慢。
本机没有 CUDA 环境，**未实测**，故只登记为推断，不作为确定的性能退步。建议在 CUDA 机器上做一次前后对比。

#### F-22（P2）lua 插件：装载期脚本没有打断路径

- **事实**：`lua/main.cpp:122` 在**装载期**（`create()` 路径）就执行语言包里的 Lua 脚本。沙箱的
  `interrupt()` 只有在一个**已经发布**的 Sandbox 实例上才有效（`LuaSandbox.cpp:291-297`），
  而此时它还没发布（`LuaSandbox.cpp:332`）。JIT 已关闭（`:322`，这本身是好事，见下）。
- **后果**：声明里的脚本只要有顶层 `while true do end`（或任何不返回的循环），**包加载会永久挂住**——
  没有超时、没有取消、没有错误。
- **性质**：事实（代码路径已核）。**建议**：装载期执行加时间/指令数上限，或把脚本执行推迟到执行期
  （那里已有取消路径）。

#### F-23（P3）manifest 里的脚本路径不限定在包内

- **事实**：`lua/main.cpp:89` 取 manifest 的 `file` 键作为脚本路径。路径解析只做分隔符归一化
  （`ManifestValues.cpp:75-84`），**不禁止 `..` 或绝对路径**。`multig2p` 同款（`Decoder.cpp:161`）。
- **后果**：一个（被信任的）语言包可以指向宿主机的**任意文件**，其内容被当作 Lua 代码读入并执行。
- **性质**：事实（代码已核）。严重性定 P3 是因为语言包本身就被当作可执行内容信任。
  但"路径不越包"这条约束是**可以零成本加上**的，建议加。

#### F-24（P3）自制的 `utf8` 库与自己头文件的声明不一致

- **事实**：`LuaSandbox.cpp:44-161` 用 C++ 重新实现了 `utf8` 的几个函数，但语义有出入：
  `char` 不校验码点范围（可产出非法 UTF-8）、`len` 忽略 `i`/`j` 参数、`offset` 的 `n==0` 语义不对、
  负数 `i` 直接报错。而 `LuaSandbox.h:19-21` 声称提供的是"完整标准接口"。
- **后果**：语言包按标准 `utf8` 的语义写脚本时，行为可能与预期不符（静默错结果，不是崩溃）。
- **建议**：要么补齐语义，要么把声明改成"提供以下受限子集"并列出差异。

#### F-25（P3）multig2p：具体错误被吞 + `maxLen` 无上界

- **事实**：`multig2p/main.cpp:146-153` 把 Decoder 返回的具体错误**丢弃**，只报一个笼统失败。
  整个 `inferenceinterpreters` 目录**没有任何一处**使用 `wolf::logCategory()`
  （仓内示例用法见 `WolfLinguistProvider.cpp:223`），所以排障时看不到细节。
  `main.cpp:222-227` 只校验 `maxLen` 为正，**不设上界**，极端值转 `int` 截断后会让全部词静默退化成
  `PhonemeGenerationFailed`（`:173-175`）。
- **建议**：透传具体错误。给 `maxLen` 设上界并在越界时报错而不是截断。

#### F-26（P3，未证实）otter `tifa/Decode.cpp` 的平局口径

- **背景**：这条与我此前那条"终态选择有问题"的旧断言**不是一回事**——旧断言即
  `docs/plans/tifa-align.md:666` 记录的 **C1（P0）**，经独立重审确认**在当前 HEAD 已经修好**
  （手算其反例得到参考答案，与 `test_Tifa.cpp:904` 的参考导出期望一致），**不要重开**（见 §8.1）。
- **残留疑点**：`Decode.cpp:109-130` 没有"终局三路比较"，而是固定从 `(frames, tokens)` 起回溯，
  三条入边靠**施加顺序 + 严格大于**定平局（WAIT `:109` → EXIT `:121` → SKIP `:126`）。
  可构造平局例：`decodeFrames({2.0f, 2.0f}, 1, 2, {0, 1})` 取 EXIT。
  而文档记载的参考规则是"平局取最小 state 序号"（`:666`、`:668`）。
- **为什么只算未证实**：参考实现（Python）不在本机可达范围，**跑一次参考即可在 5 分钟内结案**。
- **另记两条脆弱点（P3，非缺陷）**：① `EXIT` 只能在 `frame>=1` 发生，这一点靠 `gapBack` 第 0 行
  只含 `GAP_SKIP`/`GAP_WAIT` 来保证（`:70-83`、`:120-130`）——**一旦改动该初始化**，
  `tokenBack[(frame-1)*tokens+index]` 会在 `frame=0` 处算出巨大的 `size_t` 下标 → 越界，建议加断言。
  ② 函数不校验 `similarities.size() >= frames*tokens` 与 `groups.size() >= tokens`
  （生产调用点已满足：`tifa/main.cpp:646-659`、`:1210-1220`、`:1224-1234`、`:1117`），建议入口加校验。

#### F-27（P3）lua / multig2p 对"已知模式"的延续与偏离

此前只在三个插件里观察到的模式，已在 lua / multig2p 上核实：

- `static_cast<const Configuration*>(spec.configuration())` 无类型/空校验——**一致存在**
  （`lua/main.cpp:297-299`、`:346-348`，`multig2p/main.cpp:332-335`），但都位于
  interface/level/variant **全等门之内**，**当前不可达**。仓内还有**更严的反例**：
  `chain` 的 `ImportValidator`（`chain/main.cpp:418-426`）做了 interface/level/variant + 空指针三重校验。
  → 与台账 #9 的登记一致（属上游问题），本条**不单独排期**。
- "取消返回短结果且不报错"——lua 一致（`:163-175`、`:227-235`）。multig2p 是**等长但空**（`:100`、`:140-145`），
  比短结果更稳。
- **正面**：`chain/main.cpp:583-590` 确实没校验 `verifier->classify(texts)` 的长度，但
  `Verifier::classify` **非虚**且恒按 `words.size()` 构造（`Verifier.h:56`、`Verifier.cpp:142-143`）
  → **当前不可触发**。（这条此前被记为 P3 隐患，现降级为"不可能发生"，见 §7.1。）

---

## 七、对抗复核的结论

复核的方向是**找反例推翻**上面的结论（只读、不改仓库、不构建）。结果：**多条断言被推翻或降级、1 条被纠正了
归因**，另有一批新发现（已并入 §6 的编号条目）。逐条记录如下——这是"防重开"的依据。

### 7.1 被推翻 / 需要修正的断言（诚实清单）

| 原断言 | 复核结论 | 证据 |
| --- | --- | --- |
| 推理任务失败会"静默上报成功" | **推翻** | `InferAcousticTask.cpp:142-145` 在 `m_success.store(true)`（`:148`）前 `return`，`BaseInferState.cpp:116` 用 `task.success()` 转 `failed()` |
| 零长度写入 talcs（`TrackInferenceHandler.cpp:227`、`TrackSynthesizer.cpp:146`）会写 0 帧 | **推翻** | 落到 talcs 的区间长度已 `qMax(1, …)`（`DspxInferencePieceContext.cpp:71-72`，talcs `DspxNoteContext.cpp:167-168`）。且 `git diff c20f7a9a..HEAD -- src/app/Modules/Audio` **为空**——本轮根本没改这块 |
| otter pin 编不过是 **Windows 专属**问题 | **纠正归因** | 根因是 C++ 成员名隐藏，跨平台。见 F-1 |
| ORT payload 猜错会导致"运行期崩"（P1） | **降级 P2** | 当前硬编码路径与实际一致，加载失败只返回错误（`OnnxDriver.cpp:68`） |
| `synthrt/portfile.cmake:76-79` 删 `lib/cmake` 有后果（P2） | **降级 P3** | 实测 `share\synthrt\synthrtConfig.cmake` 与 `share\dsinfer\dsinferConfig.cmake` 都在，`lib\cmake` 已不存在。消费方走 `share`（`src/app/CMakeLists.txt:27-29`） |
| otter 快照 = `f4820d99..7ef8555`（48 提交） | **快照漂移** | 审计期间该分支被重写：先为 `f4820d99..8758fb2`（**5 提交**），定稿后又折入文档改动并复验 → **`f4820d99..d59ffc1`（仍 5 提交）**。与旧尖端的内容差异仅 2 个文件（`docs/plans/tifa-align.md`、`tifa/Grid.cpp` 各十余行），**被我审计的代码主体未变** |
| `chain/main.cpp:583-590` 不校验 `classify` 长度（P3 隐患） | **降级为"不可触发"** | `Verifier::classify` **非虚**且恒按 `words.size()` 构造（`Verifier.h:56`、`Verifier.cpp:142-143`）→ 当前不存在长度不符的路径。**不作缺陷计** |
| 我说的"otter tifa `Decode.cpp` 终态选择有问题" | **推翻（独立重审）** | 该断言即 `docs/plans/tifa-align.md:666` 记录的 **C1（P0）**，**在当前 HEAD 已经修好**。重审手算了它的反例并得到**参考答案**，与 `test_Tifa.cpp:904` 的期望一致。**不要重开**（详见 §8.1） |

> 记录这些不是为了自我批评，而是为了让读者知道**哪些结论已经过对抗检验、哪些还只是我的单方判断**。
> F-8（复核一度判为 P2、后澄清为**非缺陷，不要重开**）与两条"仍然未证实"的项见 §7.2。

### 7.2 复核补出的证据（§6 未展开的条目）

- **F-2 / F-11 / F-13 / F-14**：均确认。F-14 的权威来源是
  `scripts/vcpkg-ports/wolf-lang-packages/assets.cmake:7`、该端口 `vcpkg.json` 与 §4.3 的实测装入树。
- **F-4**：确认机制，并补出日志（`AppLogDirectory.cpp:14`）、崩溃转储（`CrashHandler.cpp:17`）两条同类路径，
  以及"缓存无跨进程锁""mkpath 失败不阻断"两个后果。
- **F-6**：确认 `stop()` 里是局部事件循环、**唯一退出条件**是 150 ms 定时器。后果是双向的
  （无条件 +150 ms，超过 150 ms 才发完的响应会被 `McpHttpServer.cpp:677-684` 切断）。
- **F-7**：确认无差别映射，并补出两个真实后果——自动化侧被改写成 `ModuleNotReady` +
  固定文案 "Package metadata service is unavailable"（真实 message 被丢弃，
  `PackageAutomationAdapter.cpp:167-173`）。UI 侧 `PackageManagerDialog.cpp:184-185` **静默 `return`**，
  用户只看到列表没变化。而 `srt::Error` 本身是带可分类 `code()` 的（`synthrt/include/synthrt/Support/
  Error.h:19-45,85-87`），**信息是有的，只是被丢了**。
- **F-8（已澄清为"非缺陷"，不要重开）**：`beginCommitting` 返回 `Expected<bool>`，值为 `false` 时 facade
  （`ExtractionAutomationFacade.cpp:508-515`、`:674-681`）只 `notifyFinished`、不重复落终态——这是对的：
  任务处于 `CancelRequested` 时，管理器**在返回 `false` 之前**就把状态置为 `Canceled`、`cancelable=false`，
  并**已经调用**了 `unsuccessful` 与 `terminal` 两个回调（`AutomationTaskManager.cpp:188-222`，回调在
  `:197-221`），返回 `true` 的分支才置 `Committing`。所以观察者**一定会收到终态**。唯一可挑剔的是
  **可读性**：该含义只能靠读 `AutomationTaskManager` 得到，建议在 `beginCommitting` 声明处
  （`AutomationTaskManager.h:101`）补一句注释（P3，建议类）。
- **F-9**（P2）：`LiteBuildApi.cmake` 的 `CMAKE_BUILD_TYPE` 与不可达 WARNING（详见 §6.2 F-9）。
- **F-12**（P3）：加载器其实读到了 `url`/`readme`（`PackageLoader.cpp:1165/1168` →
  `VoicebankCatalog.cpp:443` 的 `packageUrl`），但 `PackageManager.cpp:247-251` 构造 `PackageInfo` 时把
  readme/url 两个实参都传了 `{}`，且 `PackageInfo` 没有对应 setter（`PackageInfo.h:45-46/67-77`）→
  **编辑器永远不会显示声库主页/说明**。
- **F-19**（P3）：`convert-voicebank.py` 的 `is_language_path` 注释自称"与加载器同样、同序的校验"，
  但加载器 `readDisplayPath`（`wolf/src/lib/SVS/SingerContrib.cpp:122-167`）**还会因 NUL 拒绝**
  （`:125-128`）且对 map 的每个值做路径解析（`:149-160`）→ 含 `\u0000` 的 `avatar`/`demoAudio`
  会被脚本放行、被加载器整包拒绝（窄场景）。
- **F-20**（P3）：契约三元组 `(interface, variant, kind)` **没有单一真相源**——值在
  `scripts/convert-voicebank.py:58-65` 的表里（脚本不再复述匹配语义，只写指针指向 `SingerStages.h` 的
  `roleOf()`），`kind` 对应的阶段身份在 `SingerStages.h:23-37`，接口名在 wolf/synthrt 头文件。任一仓改名
  只会在**运行期**表现为"选不到解释器"。
- **正面发现**（值得记一笔）：wolf 的 `s2p`/`onset` 插件在收到取消时会返回**短结果且不报错**，
  下游靠长度校验兜住（`LinguistExecutiveImpl.cpp:328`）。`chain` 插件按 batchSize 切段、
  校验后端返回长度、把单步失败降级为词级错误而不中断整链 —— **这些设计是稳的**。
- **一致性确认**：构建树部署分支（`LiteBuildApi.cmake:193-204`）与
  `src/libs/SynthrtEngine/SynthrtEngine.cpp:281-287` **逐字一致**，CUDA 在 macOS 全排除也与
  `DeployLayout.h:43-46` 和 manifest 一致。
- **仍标「未证实」的两条**（复核提出、本报告未判定）：协程能否绕过沙箱 hook（取决于解释器是否真是
  LuaJIT）。`Executive` 持有 `const Configuration*` 是否安全（取决于 synthrt 的所有权契约——该仓源码在
  复核范围内不可达）。

---

## 八、未覆盖范围与后续建议

### 8.1 尚未覆盖（用之前先补）

| 范围 | 状态 | 建议 |
| --- | --- | --- |
| wolf `src/plugins/inferenceinterpreters/**` 剩余部分 | **大部分已补** | 已**通读 `lua` + `multig2p` 全文**（新增 F-22～F-27）。**仍未读**：`pinyin`、`stub` 正文，以及 `chain/main.cpp` 的 1-400 与 805-865 |
| otter tifa `Decode.cpp` 的"终态选择" | **已结案（非缺陷）** | 独立重审（通读全文 + 手算其反例）：该断言即 `docs/plans/tifa-align.md:666` 的 **C1（P0）**，**当前 HEAD 已修好**，回溯 walk 与前后向指针自洽，**未发现可证实缺陷**，**不要重开**。仅残留一条**平局口径**待参考实现判定（F-26） |
| Linux / macOS 部署分支 | 部分确认 | ① 安装期根目录与 macOS bundle 不同树（**推断未实测**）。② Linux/macOS **没有** Windows 那样的打包后布局校验（`packaging/` 只有 windows 4 个文件）。③ `scripts/deploy_linux_plugin_deps.sh:120` 遍历整个 `lib` 的 NEEDED 并复制到 `lib` 顶层，**可能把库放到 `DeployLayout` 约定之外**（未证实有害） |
| 打包安装器 | **前置检查已跑，完整打包未执行（本机缺件）** | 按技能 `windows-installer` 实测：① 不设 `QT_DIR` 时**直接失败**（本机 Qt 在 `<Qt 检出>`，脚本只认 `QT_DIR` 或 `C:\Qt\<版本>`，见 `build-installer.ps1:127`）。② 设好后 **VS 与 Qt 均识别成功**。③ 随后卡在 **VC++ 运行库**：`build-installer.ps1:383` 要求 `-VcRedistPath` 或 `VC_REDIST_X64`，本机**两者都没有**（`<VC 运行库目录>` 不存在）。故**完整打包未执行**——装包前须先备好官方 `vc_redist.x64.exe` |
| 2.3 → 2.4 声库转换 | **已验证（2026-10-03 实测，原"无样例"结论被推翻）** | 本机 OpenUtau 检出内的一个构建输出目录里有 **3 个真实 2.3 声库**——审计当时只扫了声库搜索根，故误判"无样例"。实测：`yousa@1.65.1.0` 与 `junninghua@26.1.5.0` 经脚本转换后与已发布 2.4 包**逐字节相同**（60/60、47/47 文件，目录与模式位亦相同）。`0902_wolf_club@1.0.0`（无同名 2.4 参照，`0913_wolf_club@1.0.0` 是另一版本，8 vs 4 个角色）转换后**真实加载并合成成功**（其自带 4 个 ONNX 会话真跑，导出 8.000 s float32/单声道/44100 Hz，发声区 0.9–3.0 s、f0 246–262 Hz）。**故 `schema`→`exports` 形状会被整包拒绝这一待验证点不成立**。**未覆盖/上限**：① 只覆盖正常路径。② 已发布包很可能由同一脚本产出，逐字节相同无法排除"两侧同源的同一处错误"。③ 脚本自身的错误路径缺陷另见下一行 |
| 2.3 → 2.4 声库转换（追加样本，2026-10-03） | **通过** | 用户另提供两个真实 2.3 声库：`qixuan@2.7.0.0.zip` 与 `zhibin@26.7.16.0.zip`（解包后 34 / 44 个文件，中英日粤四语言）。脚本对两者均 **0 error**（各产出 13 inference + 4 linguist + 1 singer），且**在编辑器里真实加载并合成**：`zhibin` 用 `zhibin-base` 说话人、`qixuan` 用 **null 说话人**（其 2.3 声明的 `schema.speakers` 为空数组，2.4 直接接受"无说话人"，见 `PublicAutomationRegistry.cpp:1028-1035`），各导出 8.000 s float32/单声道/44100 Hz（f0 240–271 / 253–263 Hz）。**因此审计原先担心的 `schema`→`exports` 形状问题在 4 个真实 2.3 声库上都不成立**。**注意**：上一行所用的那三个样例随后被用户从本机移除（同日），逐字节结论已记在上行。本机现存的 2.3 真实样例就是这两个 zip（解包副本为本机临时产物，未入库） |
| `convert-voicebank.py` 自身缺陷（2026-10-03 只读对抗复核） | **已修 6 项（全部）** | ① `--output` 对**已存在目录**无提示 `shutil.rmtree`（`:1167-1168`。守卫 `:1159-1166` 只拦"是包自身/包含包/在包内"，help `:1110-1111` 未提示会删除）→ 指向一个已安装声库即先删后拷。② `--in-place` 中途失败留 2.3/2.4 半成品（declaration 即时落盘、`desc.json` 最后写）。③ `relocate`（`:860-866`）无包内包含性/符号链接校验。④ 不查 NUL 与重复 contribution id（F-19 在脚本侧成立）。⑤ 类型未校验时抛未捕获 traceback 而非报告。⑥ docstring 称语言包缺失"报错并丢弃"，实际是 warning + 退出码 0。**反驳项**：我曾怀疑的大小写撞键不成立（`jpn/n` 与 `jpn/N` 不在同一文件）。**处置（同日）**：①–④、⑥ 已修，并配回归测试 `scripts/test_convert_voicebank.py`（7 用例，含 `--output` 拒绝覆盖、`--in-place` 失败不动原包、越界路径、重复/同 id）+ 机器本地守卫驱动 11/11（含两个真实 2.3 包的逐字节回归）。⑤ 类型防御也已补上（`read_object`/`read_array` 把类型错误报成 error 而不是 traceback，测试增至 10 用例）。同日还把包装脚本与参考脚本对齐：不再复制 `DEFAULT_OUTPUT_SUFFIX`/`MANIFEST_VERSION`、不再按行号引用参考脚本（那些行号已随参考脚本增长而失准），改为从载入的模块读取 |
| ~~`LITE_OTTER_FIXTURES`~~ | **已解决** | 已接上并跑通（§4.5③、§5.4）。建议固化进 CI 与本地测试约定 |
| `ExtractionTaskRegistry::beginCommitting` 语义 | **已解决** | 已读到实现，F-8 结案为**非缺陷**（§7.2） |
| 跨仓"多副本"收敛（2026-10-03 复核，修正回收文档的说法） | **已收敛（①②，2026-10-03）** | 回收文档把 lite 的按行号手抄与 otter/wolf 两处并列，复核后两处都要改写。**otter**：F0/NOTE/ALIGN 的 exports 键集确有三处载体——`docs/schemas/*-1-exports.schema.json`（声明式 `required`+`additionalProperties:false`）、`scripts/check-declarations.py:172-193` 的 `EXPORTS_KEYS`（lint 白名单）、`src/lib/Api/{F0,Note,Align}/1/*ApiL1.cpp`（过程式 `rejectUnknownKeys` + 逐键读取，键名不在任何常量表里，所以按键名 grep 找不到）。但**前两者早已被机器锁定**：`scripts/test_check_declarations.py:588-604` 的 `test_the_key_tables_mirror_the_schemas` 逐条比对 required、required∪optional、knobs 及种类、`sampleRate.maximum`、ALIGN 语言条目键集。真正没有检查的只有 C++ 读取器。三处的键集、required 归类、knob 种类**逐键一致**（6 组清单人工比对），缺省值载体不同（schema 注解 vs 头文件成员初值）而值相同。⇒ 没有"可删的副本"，三处职责不同（发布规范／lint 规则／装载期校验），只能**锁**（扩展现成镜像测试去解析 C++ 白名单）或**生成化**。**wolf**：`src/plugins/singerproviders/stub/main.cpp:45-49` 确实不判三元组，且是**全树唯一**不判的插件（其余 7 个插件与 dsinfer 的 `singerproviders/diffsinger` 都返回 `InvalidArgument`。回收文档写的 `:46-50` 差 1 行）。但"只靠 synthrt `validatePayloadIdentity` 兜底"**不成立**——第一道门禁是插件**元数据匹配**：`synthrt/lib/Core/ContribPluginFactory.cpp:20` 的 `metadataProvidesInterpreter` 读 `plugin.json` 的 `interpreters`，由同文件 `:75-78` 的 `findInterpreter` 调用。`PackageLoader.cpp:769` 调它，未命中即在 `:769-775` 报 `FeatureNotSupported: no interpreter provides the requested module triple`，声明写错在装载期即报 `FeatureNotSupported: no interpreter provides the requested module triple`。而 stub 把请求值原样回显，`validatePayloadIdentity` 对它**恒等通过**（没有第二份可比对的身份） |
| ①② 的处置结果（2026-10-03 收口） | **已完成并实测** | **wolf** `89ff8bb`：`src/plugins/singerproviders/stub/main.cpp` 新增 `INTERFACE`/`LEVEL`/`VARIANT` 常量与 `create()` 的三元组判定（+13 行，风格同 `linguistproviders/wolf`。实测 wolf 全部插件都已有同型检查，本处是补齐），构建干净、`ctest` **18/18 通过**。**otter** `12f35e1`：`scripts/test_check_declarations.py` 新增 `test_the_readers_whitelist_the_same_keys`，按调用消息锚点从三个 `*ApiL1.cpp` 解析 `rejectUnknownKeys` 的 exports 与 knobs 白名单（**恰好 1 份，否则失败**），与 `EXPORTS_KEYS` 的 required∪optional 与 knobs 逐键比对。otter Python **51/51**、`ctest` **12/12 通过**。⇒ 三处键集从此由机器锁定，不再依赖人工比对 |
| 文档路径卫生（2026-10-03 机械核查） | **已按口径 (b) 处置** | 本机脚本扫过 `docs/**/*.md` 的 283 处仓内路径引用，**29 处已不存在**（另有 26 处指向 wolf/otter/synthrt 的同名文件，属正常跨仓引用）。成因高度集中：三次库抽取重构（`7e22c39d`/`2fc44ff3` ProjectModel、`96a0403f` PackageManager、`d67bda14` ProjectConverters）把 `src/app/**` 的路径搬到 `src/libs/**`，两次删除（`6840e37a` 统一选项对话框、`c06fb36e` 波形几何共享），以及 `docs/plans/document-workflow-and-multi-format-import-plan.md` 转正为 design 文档（`91e3d21c`）。另有 `tests/TestSyllabification/main.cpp` 漏了 `src/` 前缀等少数真实笔误。命中几乎全在 `docs/design/**`（历史契约），口径二选一：(a) 逐条改成现路径（会改写历史记录）。(b) 保留原文，在 `docs/plans/README.md` 加一张"路径迁移对照表"（推荐） |
| 路径迁移对照表已落盘（2026-10-03） | **已完成** | `docs/plans/README.md` 新增「路径迁移对照表」（提交 `bd89798a`）：旧路径 → 现路径 + 搬运/删除提交，历史文档原文**未改**。同轮把迁移文档里指向已删除样例目录的"重建审计声库"配方改为现存两个 zip 的可用配方 |

### 8.2 与既有台账 `integration-pending-changes.md` 的关系

该文件是本仓**自己的待办台账**（9 条）。为避免重复登记，逐条对照如下：

| 本报告 | 台账中的对应 | 结论 |
| --- | --- | --- |
| **F-1** otter pin 编译不过 | 无 | **新增**，需处置（P0） |
| **F-2** 能力查询 vs 受理不一致 | #6 第二行（`SingerCapabilitySummary` 的能力字段恒为缺省值）**相关但不同** | #6 讲的是 UI 提示里显示缺省值，F-2 讲的是**自动化接口自相矛盾**。建议**另立一条**，或并入 #6 的"能力字段"主题 |
| **F-3** 推理失败无错误信息 | 无 | **新增** |
| **F-4** 数据根只隔离一半 | 无 | **新增**（影响全部测试可信度） |
| **F-5** ORT payload 靠猜 | #7（执行提供程序的其余字面量）**同族** | #7 管枚举字面量，F-5 管打包路径。主题都是"同一事实多处权威"，建议一并按"单一真相源"处理 |
| **F-11** `plugins/<lib>` 三处写死 | 同上 | 同族，建议并入 #7 |
| 复核轮 P3：wolf 插件里 `static_cast<const Configuration*>` 无校验 | **#9 已登记**（"`SYNTHRT_DECLARE_AS_METHODS` 的无检查下转型"，且明确注明**属上游问题、不在 lite 中修复**） | **已知项**，本报告只作为背景，**不重复排期**。遵守"缺陷归因在 synthrt 时源头修复"的约定 |
| **F-14** "13 门语言与 zxx"多算一门 | 无 | **新增**（文档勘误） |
| 台账 #9 的 `InferRetake` 帧/秒单位不一致（`GenericInferModel.h` 注释为帧下标，`convertInputParams()` 按秒传，实际等同于整段重算） | — | **本轮未覆盖**。这条是台账自己登记的已知缺陷，建议优先于本报告里多数 P3 处理（它会让所有重算退化） |

> 结论：本报告共 27 条编号（`F-8` 已澄清为**非缺陷**，实际缺陷 26 条）。其中只有 1 条（未校验下转型）
> 与既有台账重叠，且台账已明确"属上游问题、不在 lite 修复"。其余要么是新增，要么与台账条目
> **主题相邻但内容不同**。

### 8.3 处置顺序

P0/P1 的先后以 §1.3 的五件事为准（F-1 → F-4 → F-2 / F-3）。以下是 §1.3 之外的成组动作：F-10 让 Windows 上的
ctest 回到全绿（**2026-10-01 已修**，改掉了对平台权限模型的依赖）。F-5 / F-11 / F-9 一并处理（都关乎
`DeployLayout.h` 的单一真相源地位）。F-12 / F-14 / F-15 / F-19 / F-20 属文档与整洁性，随手清。

---

### 8.4 推送就绪评估（2026-10-03，只读，未联网）

**结论：三仓里只有 synthrt 无需推送。wolf 与 otter 的本地线都是"已推送线的重写版"，普通 push 必被拒。**

| 仓 | 本地 tip | 本机缓存的远端 | 关系 | 说明 |
| :-- | :-- | :-- | :-- | :-- |
| synthrt | `63bef25` | `origin/onnxruntime-builds-uptake` = `63bef25` | **0 / 0** | 与远端一致，**无东西可推**。lite 端口 pin `63bef253…` 正指向它 ✓ |
| wolf | `c6ae0f2`（7 笔） | `origin/linguistic-level-1-v2` = `876482e` | 远端独有 **4** / 本地 **7** | 远端那 4 笔（`8ef560b`…`876482e`）是**同一批工作压缩前**的版本，作者 `SineStriker <trueful@163.com>`（2026-09-30），非第三方。本地是重写线，`git merge-base --is-ancestor` 为假 ⇒ 非快进 |
| otter | `8285356`（8 笔） | `origin/analysis-level-1` = `f4820d9` | 远端独有 **4** / 本地 **8** | 同上（远端 4 笔同样来自 2026-09-30 的压缩前行）。本地线**与远端没有共同祖先**（根提交无父） |

**若真推，会带上去什么**：`git diff --stat <远端 tip> HEAD` 实测 wolf **191 文件 / +1835 −2372**、otter **112 文件 / +9738 −2436**——不只是重新切分，还含此后新增的测试、修复与"夹具改为生成"等实质改动。另有**作者身份差异**：本地 7/8 笔的作者是 `wolfgitpr <…@users.noreply.github.com>`，而远端线下由 `SineStriker <trueful@163.com>` 署名，推之前需要你决定是否改写。

**可选路径（都需要你发令，本仓禁 push、禁 `--force`）**：
1. **推新分支名**（如 `linguistic-level-1-v3` / `analysis-level-1-v2`）：非破坏，旧分支保留，最安全——**推荐**。
2. `--force-with-lease` 覆盖原分支：需你显式解除"禁 force"规则，且确认无人基于旧线工作。本地有锚点可回滚（`backup/pre-squash-20261003b-{wolf,otter}`，另 otter 中间态 `…b2-otter`）。
3. 不推：保持现状（lite 的 pin 继续指向旧线 tip，端口照样能装）。

**推送前必须做的核对（联网，需你确认才执行）**：
- `git ls-remote` 三仓目标分支：确认远端 tip 未再变化，并**确认 lite 的 otter pin `6f6b0a2f…` 仍可被远端某 ref 覆盖**——本机实测它**不在** `origin/analysis-level-1` 上、也不在任何本地分支上（`merge-base --is-ancestor` 为假、`branch -a --contains` 为空），若远端也没有它，装上 lite 的 `vcpkg install` 会在另一台机器上取不到源。
- 顺序 **上游先、lite 后**：synthrt 无需动 → wolf/otter → 更新 lite 三个端口的 `REF`（wolf `876482ec…`、otter `6f6b0a2f…`）→ 再推 lite。
- 推完删掉 lite `scripts/vcpkg-ports/otter/portfile.cmake:11-13` 的 `ROLLBACK 2026-09-30` 注释（它解释的是当时"分支 tip 在 Windows 上不构建"的降级，前提已被本轮改动改变）。
- **2026-10-06 更新**：上面两项与 lite 端口有关的动作**已完成**——`wolf` 端口的 `REF` 已随提交 `57781157`
  改为 `3a3f4a09064ca1a966c15466973fdeeb5b9772a9`，`otter` 端口的 `REF` 已随提交 `51cc9c7` 改为
  `e770ab94ac0a27a31c6cec502aeac765c9db70ef`（`ROLLBACK 2026-09-30` 注释亦随之删除），**现行值以
  `scripts/vcpkg-ports/*/portfile.cmake` 当前内容为准**，清单里其余核对（`ls-remote`、作者身份、推送路径）不受影响。

---

## 九、附录

### 9.1 快照与复现命令

```text
lite    <lite 检出>   97002f18c8d80d8f47b06930f10952e676d6a6dd  (origin/main c20f7a9a)
synthrt <synthrt 检出>          f2f0f8ee3669206ed90f951c17c397a23c5e4b6d  (origin/main 3c7549d)
wolf    <wolf 检出>             876482ec7033e8984c10b581f8358bcc435da9d7  (origin/main cecedba)
otter   <otter 检出>            审计时为 7ef8555（f4820d99 + 48 提交）；收敛后为 d59ffc1（+5 提交，已复验）
```

- 构建：preset `debug-mainline`（`CMakeUserPresets.json`，机器本地，未跟踪）。
  产物 `build\Debug\out\bin\DsEditorLite.exe`。
- 测试：本机临时脚本（未入库）两份，一份跑全量（已内置 `LITE_OTTER_FIXTURES`），
  一份只跑 TestOtterExtraction / TestAnalyzerPackages /
  TestVoicebankLanguages / TestAudioSlicer 四项。
  `LITE_OTTER_FIXTURES` 取值：otter 检出内的构建夹具目录。
- 无头：本机临时脚本（未入库）三份，分别负责起实例、跑命令、合成。
- GUI：本机临时脚本（未入库）三份，分别负责起实例、截图、MCP 调用。
- 波形分析器：本机临时产物（未入库，源码 `main.cpp` → `wav-inspect.exe`）。
- 权限探针：本机临时产物（未入库，源码 `main.cpp`）。

### 9.2 本机环境注意（踩过的坑）

- 本机 HTTP 代理会拦 `Invoke-RestMethod` 对 `127.0.0.1` 的请求 —— 必须用 `curl.exe --noproxy '*'`。
- DSH 宿主是 **Windows PowerShell 5.1**，环境块里有 3 组大小写重复的代理变量（`NO_PROXY`/`no_proxy` 等），
  `Start-Process` 会因重复键报错，需先 `Remove-Item Env:no_proxy` 等三个小写名。
- GUI 进程同时拥有**控制台窗口**和 Qt 主窗口，用 `MainWindowHandle` 截图可能抓到控制台。
  应按进程枚举可见窗口、取面积最大者（当时的截图脚本已按此后处理）。
- MCP 直连 `/mcp`：`initialize` 之后必须带 **`MCP-Protocol-Version: 2025-06-18`** 头，
  否则返回 `-32020`，`documents.save_as` **需要 `expected_revision`**（否则 `-32602` + `/expected_revision`）。

### 9.3 本次审计在仓库里留下的改动

- `scripts/vcpkg-ports/otter/portfile.cmake`：pin 回退（`f4820d99` → `6f6b0a2`）+ 说明注释。
  **已随 `bc4bd79b` 提交**（审计快照时尚未提交），该 `REF` 的位置与现行值以 `scripts/vcpkg-ports/otter/portfile.cmake` 当前内容为准。
- `docs/plans/README.md`：在索引表里加了本报告一行。**已随 `772da8a9` 提交**。
- `docs/plans/four-repo-integration-audit.md`：本报告（`772da8a9` 入库）。
- `CMakeUserPresets.json`（机器本地、未跟踪）：`LITE_WOLF_LANG_PACKAGES` 由陈旧的
  `<wolf 检出>/build/lang-packages-0.1.0.0` 改为 `""`（**陈旧值会让 4/4 声库全部加载失败**，见 §4.3），
  以及 `LITE_AUDIT_VOICEBANK` 指向本机的 yousa 2.4 声库。
- 其余产物全部为本机临时产物，未入库。其中测试脚本里的 otter 夹具路径已从旧构建树夹具目录
  改指**当前构建树的夹具目录**（前者已随 §4.2.1 的清理删除，改指后实测同一用例仍通过）。

> 审计快照（§2.1、§9.1 里 lite 的 `97002f18`）之后，本分支又新增 3 个提交：`c19447d2`、`bc4bd79b`、`772da8a9`。
> 本报告的行号引用以审计当时的快照为准。

---

## 十、附：增量提供、本仓暂无调用者的公开接口（登记，不删）

复核日期 2026-10-02。"调用者"只统计 `src/` 内的引用（含测试），逐符号 grep 复核过。

| 仓库 | 接口 | 声明位置 | 引用情况 |
| :-- | :-- | :-- | :-- |
| lite | `SynthrtEngine::hasInferenceBackend()` | `src/libs/SynthrtEngine/SynthrtEngine.h:121` | 只有声明与定义，无调用者 |
| lite | `SynthrtEngine::cancelConversions()` | `src/libs/SynthrtEngine/SynthrtEngine.h:257` | 只有声明与定义，无调用者 |
| lite | `SynthrtEngine::setReservedMarkers()` / `reservedMarkers()` | `src/libs/SynthrtEngine/SynthrtEngine.h:270-271` | 引擎级这一对无调用者，`LanguageBridge` 的同名对**被测试调用**（`src/tests/TestVoicebankAudit/main.cpp:521`、`:575`） |
| lite | `PackageManager::srtErrorToString()` | `src/libs/PackageManager/PackageManager.h:68` | 只有声明与定义，无调用者 |
| wolf | `LinguistCategory::linguists()` | `include/wolf/Linguist/LinguistContrib.h:64` | **被测试调用**（`src/tests/auto/Linguist/test_LinguistContrib.cpp:32`） |

**处置（用户决定，2026-10-02）：全部保留。** 它们是本增量新增的公开面，很可能是为后续或外部消费方准备的。
删除会缩小增量提供的契约，而保留一行未用接口的成本远低于事后补回。此表仅作**登记**，避免后续复核把它们误判为死代码。

> 更正记录：本表复核时推翻了"这五处全都零调用者"的初步说法——`LanguageBridge::setReservedMarkers/reservedMarkers`
> 与 wolf 的 `linguists()` 都已被测试实际调用。真正无调用者的是上表标注的三项（引擎级一对 + 两个独立方法）。
>
> **更正记录（决策反转，2026-10-04）**：用户当轮决定**删除** lite 侧的零调用 API，覆盖 2026-10-02 的"全部保留"。
> 实删（本轮对接层提交）：`SynthrtEngine` 的 `hasInferenceBackend()`、`cancelConversions()`、引擎级
> `setReservedMarkers()`/`reservedMarkers()`、`packageDirectory()`、`unit()`、`languagesOf()`、`setSingerPhonemes()`，
> 以及 `PackageManager::srtErrorToString()`。`unitIfReady()` 与 `LanguageBridge` 的同名方法保留（后者仍被测试调用）。
> 连带：`LanguageBridge::cancel()` 失去唯一调用者，已零调用但**本轮保留**（删它的唯一入口 `cancelConversions()`
> 即本次删除，是否一并删待定）。另 `unit()` 删除后，`PackageAutomationAdapter.cpp:217-221` 中引用它的注释已改写为
> 不含该名字的论证。本表其余登记项（wolf 的 `linguists()` 等）仍按原记录保留。

