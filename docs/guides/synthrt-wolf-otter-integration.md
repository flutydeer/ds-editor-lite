# 引擎接入、2.4 声库打包、ORT 配置与公共 G2P 携带指南

> 状态：**现行指南**（活文档）。
>
> 适用对象：在本仓接线 synthrt / wolf / otter、打包或校验 2.4 声库、调整 ONNX Runtime（下称 ORT）
> 部署，或把 wolf 公共语言包（下称公共 g2p）随产品携带的人。

## 〇、本文的权威来源清单

本文不复述会漂移的值（版本号、pin 的提交、端口号、包数、文件数）。需要具体值时，一律以**下表列出的
权威文件当前内容**为准。本文只写"为什么这样做、怎么用、到哪里看权威值"。

| 主题 | 权威来源（仓库相对路径） |
| :-- | :-- |
| 四个 overlay 端口：各自提供什么、构建开关 | `scripts/vcpkg-ports/{synthrt,wolf,otter,wolf-lang-packages}/portfile.cmake`、`scripts/vcpkg-ports/synthrt/vcpkg.json` |
| 依赖清单、feature、overlay 搜索顺序 | `scripts/vcpkg-manifest/vcpkg.json` |
| 语言包资产清单与 SHA512（每代 release 重新生成） | `scripts/vcpkg-ports/wolf-lang-packages/assets.cmake` |
| ORT 载荷与其 CMake 包的安装布局 | `scripts/vcpkg/ports/onnxruntime-builds/portfile.cmake` |
| 部署目录布局的**单一真相源** | `src/libs/SynthrtEngine/DeployLayout.h` |
| 部署期复制、语言包解析顺序、ORT 载荷落位、CUDA 闸门调用 | `cmake/LiteBuildApi.cmake`、`cmake/OrtRuntimeGate.cmake`、`cmake/LiteDsinfer.cmake` |
| 运行期装配（类别、驱动、runtimePath） | `src/libs/SynthrtEngine/SynthrtBootstrap.{h,cpp}`、`SynthrtEngine.{h,cpp}` |
| 语言域桥接（深度语义、保留音素） | `src/libs/SynthrtEngine/LanguageBridge.{h,cpp}`、`ReservedPhonemes.h` |
| 2.3 → 2.4 声库转换与打包产物形状 | `scripts/convert-voicebank.py`（及其测试 `scripts/test_convert_voicebank.py`） |
| 2.4 包格式规范（解包即安装、ZIP entry 规则、Package root） | `<wolf 检出>/docs/ds-spec-2.4.md` |
| 公共 g2p 的形态、切分、发布与端口拓扑 | `<wolf 检出>/docs/linguist-distribution.md` |
| 语言域 API Level 1 契约（深度、角色名） | `<wolf 检出>/include/wolf/Api/Linguists/Linguist/1/LinguistApiL1.h`、`<wolf 检出>/include/wolf/Api/Inferences/{Common,G2P,Onset,S2P}/1/*.h` |
| 四仓整合背景、迁移与部署形态 | `docs/archive/four-repo-integration-audit.md`、`docs/plans/synthrt-main-migration.md` |
| Windows 打包与 staging 校验入口 | `packaging/windows/README.md`、`packaging/windows/build-installer.ps1`、`packaging/windows/build-portable.ps1` |

**行号口径（重要）**：

- **本仓**（lite）的引用带 `文件:行号`，为撰写时的实测快照。改动后须按符号名重新定位。
- **wolf / otter / synthrt** 侧一律给**文件与标识符**（类型名、常量名、章节号），不给行号：本仓通过
  overlay 端口 pin 的是某个提交，而任何人手上的检出可能是另一代次，行号不并行维护。要按名 grep。
- 本文用 `<lite 检出>`、`<wolf 检出>`、`<otter 检出>`、`<synthrt 检出>` 指代各仓工作副本。

---

## 一、编辑器如何接入 synthrt + wolf + otter

### 1.1 四个端口各自提供什么

| 端口 | 提供物 | 形态与落点 | 权威位置 |
| :-- | :-- | :-- | :-- |
| `synthrt` | 合成引擎本体。开 `onnx` feature 时同时产出 **dsinfer** 与其 ONNX 驱动 | 链接目标 `synthrt::`、`dsinfer::dsinfer`。驱动与解释器作为**运行期插件**安装在 `lib/plugins/dsinfer/...` | `scripts/vcpkg-ports/synthrt/portfile.cmake:1-7,19-31,61-84` |
| `wolf` | 语言域（字 → 音素、起音层、语言模块提供者） | `lib/plugins/wolf/{inferenceinterpreters,linguistproviders}`，**不含**语言资源数据 | `scripts/vcpkg-ports/wolf/portfile.cmake:1-7,18-29,45-47` |
| `otter` | 分析域（音高、音符等分析器） | `lib/plugins/otter/inferenceinterpreters`，**不含**分析器模型权重 | `scripts/vcpkg-ports/otter/portfile.cmake:1-6,29-35,51-53` |
| `wolf-lang-packages` | **纯数据**：公共 g2p / 语言包 | 安装到 `share/wolf/packages`，并导出搜索根 | `scripts/vcpkg-ports/wolf-lang-packages/portfile.cmake:1-17,34-35,50-62` |

两条必须记住的架构事实：

1. **三个库共用一棵插件树**。每个库把自己的插件装到 `plugins/<库>/<类别>`，因此同一棵树里同时
   含有三家的插件。编辑器把这棵树整体部署，而不是分三处。
   （`src/libs/SynthrtEngine/DeployLayout.h:22-41`、`cmake/LiteBuildApi.cmake:154-159,256-318`）
2. **只有 synthrt 有 pin 的“家族来源”**。wolf 与 otter 端口都依赖 synthrt 端口产出的 `dsinfer`，
   而不是各自再带一份 pin（`scripts/vcpkg-ports/synthrt/portfile.cmake:1-3`）。所以 dsinfer 的
   代次由 synthrt 端口唯一决定。wolf / otter 的 `onnx` feature 只控制“本库是否构建依赖 dsinfer
   的那部分插件”（`wolf/portfile.cmake:23-29`、`otter/portfile.cmake:29-35` 均显式用
   `WOLF_DISABLE_DSINFER` / `OTTER_DISABLE_DSINFER` 关闭，而不依赖树上是否正好有 dsinfer）。

### 1.2 加载期依赖谁、推理期依赖谁

| 阶段 | 由谁负责 | 说明 |
| :-- | :-- | :-- |
| 构建/配置期 | CMake + vcpkg | `cmake/LiteDsinfer.cmake:13-19` 以 `find_package(dsinfer CONFIG)` 定位 dsinfer，**找不到即 FATAL_ERROR**（没有推理后端就没有可跑的模型）。该文件被每个链接 dsinfer 的目录 include——imported target 只在创建它的目录可见。 |
| 启动（Bootstrap） | lite 的 `SynthrtBootstrap` | 注册类别 → 设置包搜索路径 → 逐类别 `setPluginPaths` → 找驱动插件 → 初始化驱动 → 作为 **runtime service** 注册给 `SynthUnit`（`src/libs/SynthrtEngine/SynthrtBootstrap.cpp:45-111`）。 |
| 声库/包加载期 | synthrt 的 `SynthUnit` + 各库插件 | 只用**声明**与**解释器插件**，不加载模型权重。`VoicebankCatalog` 读声明与 exports 即可给出能力（`src/libs/SynthrtEngine/VoicebankCatalog.h:232-254`）。 |
| 推理期 | dsinfer + ONNX 驱动 + ORT + otter 解释器 | 合成、g2p、分析**共用同一个 ORT 实例**（驱动是整单元的 runtime service，`SynthrtBootstrap.cpp:79-83`），语言域不需要自带适配器去借驱动。 |

类别与目录的对应（`SynthrtBootstrap.cpp:45-58`，名字取自 `DeployLayout.h`）：

- `inference` 类别 ← `plugins/dsinfer/inferenceinterpreters`、`plugins/wolf/inferenceinterpreters`、
  `plugins/otter/inferenceinterpreters`
- `singer` 类别 ← `plugins/dsinfer/singerproviders`
- `linguist` 类别 ← `plugins/wolf/linguistproviders`
- 驱动 ← `plugins/dsinfer/inferencedrivers`

wolf 的类别注册依赖一个**必须在第一个 `SynthUnit` 之前调用**的符号引用：`wolf::linkLinguistCategory()`
（`<wolf 检出>/include/wolf/Linguist/LinguistContrib.h:17-25`，调用点
`src/libs/SynthrtEngine/SynthrtBootstrap.cpp:63-67`）。原因写在该函数的注释里：注册靠静态初始化，
而链接器（ELF `--as-needed`、MSVC 的未引用 import library）会丢弃无符号引用的依赖。
**在 lite 侧改动启动顺序时不要删掉这一行。**

### 1.3 部署布局只有一个定义处

`src/libs/SynthrtEngine/DeployLayout.h` 是部署目录的唯一真相源。CMake 与两个打包脚本用正则
**解析这个头文件**（`cmake/LiteBuildApi.cmake:3-28`），所以：

- 新增/改名一个类别目录，必须改 `DeployLayout.h`（保持 `inline constexpr char NAME[] = "value";`
  的单行形式，解析依赖这个形状）。
- 该文件同时被 `static_assert` 约束（`DeployLayout.h:57-59`：ORT 载荷必须位于驱动目录之下）。
- 打包脚本对 staging 的断言也读同一组常量（`packaging/windows/README.md:71-74`），部署侧与校验侧
  因此不会各写一份。

### 1.4 wolf 的**起音层 / 层数**语义

语言域是**三层**，由深到浅：

| 层 | 含义 | 角色名（wolf 侧） |
| :-- | :-- | :-- |
| `Pronunciation` | 歌词 → 发音（词典 / 模型 / 规则） | `linguist/g2p` |
| `Phonemes` | 歌词 → 音素 | `linguist/s2p` |
| `Onsets` | 音素 + 起音标记（`onsets`，与 `phonemes` **等长**） | `linguist/onset` |

- 契约定义：`<wolf 检出>/include/wolf/Api/Linguists/Linguist/1/LinguistApiL1.h` 的
  `API_LEVEL`、`ROLE_G2P/ROLE_S2P/ROLE_ONSET`、`Depth`、`LockedPhonemes`、`LinguistWordInput/Output`
  （含 `onsets` 字段）。**G2P 必需，S2P 与 Onset 可选，二者决定可达的最大深度**——这是该文件的原话级
  规则，不要再在别处复述一份。
- **深度是链式的**：缺 `linguist/s2p` 时深度停在 `Pronunciation`，此时**即使声明了 `linguist/onset` 也到不了
  `Onsets`**（`scripts/convert-voicebank.py:545-549` 的实测说明）。
- **请求更深层不报错**：请求超过组合能力时返回较浅的结果，且结果里**看不出**“这门语言没有起音层”与
  “没有音素开始这个音节”。因此宿主必须**转换前先查深度**——见
  `src/libs/SynthrtEngine/LanguageBridge.h:101-114` 的 `maxDepth()`（镜像 `canConvert()`。
  路线不可用时返回空，而不是折成 `Pronunciation`）。`LanguageBridge::Depth` 与 wolf 的
  `Depth` 一一对应（`LanguageBridge.h:66-74`）。
- **起音层是“部署组合”级的要求，不是“语言包”级的要求**，而且只看**被绑定的那个** linguist
  （`scripts/convert-voicebank.py:555-559`）：语言包声明了 onset、但绑定的是另一个不带 onset 的
  linguist 时，该语言仍然到不了起音层。
- 用户手改的音素要连同起音一起锁定：`LanguageBridge::Word.phonemes` 与 `Word.onsets` 成对出现
  （`LanguageBridge.h:46-51`），对应 wolf 的 `LockedPhonemes`（两者必须等长）。
- 保留标记（SP / AP 等）**绕过 g2p**，结果固定为“标记本身 + 单音素 + 单个 onset”
  （`LanguageBridge.h:122-134`，实测断言见 `src/tests/TestVoicebankAudit/main.cpp:872-896`）。
  声库可在 singer 类别里用 `reservedPhonemes` 声明自己的标记集。

### 1.5 otter 作为推理运行时插件的位置与装配

- otter **不是**第二套推理框架：它的分析器就是 `inference` 类别的模块，插件装在
  `plugins/otter/inferenceinterpreters`（`DeployLayout.h:34-35`），与 dsinfer、wolf 的解释器
  **并列在同一个 `inference` 类别的路径列表**里（`SynthrtBootstrap.cpp:48-50`）。
- 因此装配方式与其它解释器完全一致：**只设置路径，不手工创建实例**。哪个插件解释哪个模块由
  `plugin.json` 的 `interpreters` 与模块三元组匹配，未命中即装载期报 `FeatureNotSupported`。
- 分析器模型权重**不在端口里**（`scripts/vcpkg-ports/otter/portfile.cmake:5-6`）：权重随分析器包分发，
  由用户选择。`VoicebankCatalog` 层面把分析器与其它推理模块放在同一类别里枚举
  （`src/libs/SynthrtEngine/VoicebankCatalog.h:175-210` 的 analyzer 条目与语言解析）。

---

## 二、如何打包 2.4 声库

### 2.1 目录与清单要求（以规范为准）

规范章节：`<wolf 检出>/docs/ds-spec-2.4.md` 的「安装」与「安全解包」（§1 内），以及「推荐目录结构」。

- **解压即安装**：`dspk` 的“安装”就是在某目录解压它。Loader 只从**已安装完成的 Package 目录**读，
  不得把原始归档当候选或临时物化后加载。
- **Package root 必须恰好含一个 `desc.json`**（普通文件、名称大小写完全一致）。别处的同名文件不算描述文件。
- **ZIP entry 规则**（打包时按这些规则产出，校验时按这些规则检查）：entry 名必须是 UTF-8。
  路径分隔符必须是 `/`。**不得是绝对路径、带盘符或 UNC 路径**，不得含反斜杠、NUL，路径段不得为
  `.`/`..`/空（目录末尾的 `/` 除外）。只允许普通文件与目录（无符号链接/junction/设备等）。
- **先 staging 再原子移动**，失败不得留下半个已安装包。
- 声明文件里的资源路径**可以**是绝对路径或含 `..`（规范明确允许），所以“**不得有绝对路径条目**”
  这条约束的适用对象是 **ZIP entry 名**，不是声明里的资源路径——两者别混。
- 目录形状：根下 `desc.json`，推理模块放在 `inferences/` 的子目录中（每个子目录一个声明文件），
  共享资源放 `assets/`（可按歌手分子目录）。

> 规范里的推荐目录树与本仓转换脚本写出的目录**用复数形式**（`inferences/`、`singers/`），而声明里的
> 键与模块引用用单数（`inference`、`singer`）。判定以转换脚本与规范的当前内容为准，不要凭目录名猜角色。

### 2.2 模型与贡献的对应关系

2.4 包不直接“带模型”，而是**声明贡献（模块）并让其它模块按角色引用**：

| 角色 | 指向 | 谁声明 |
| :-- | :-- | :-- |
| `singer/<kind>`（`acoustic`/`duration`/`pitch`/`variance`/`vocoder` 等） | `:inference/<模型模块 id>` | 声库自建 singer 模块 |
| `linguist/g2p` | 公共 g2p 包里的模块 | 声库 linguist 或公共后端包 |
| `linguist/s2p`、`linguist/onset` | 声库自带的 `s2p-<lang>`、`onset-<lang>` 模块 | 声库自建 linguist 的 `imports` |
| `:linguist/<id>` | 该 linguist 模块 | singer 模块的 `reference` |

证据：`scripts/convert-voicebank.py:112-113`（两个角色名）、`:644`（`linguist/g2p` 指向被服务的公共包）、
`:697-735`（生成 `s2p-<lang>` / `onset-<lang>` 的 `inference.json` 与模块 id）、
`:758-760`（linguist 声明里的 `linguists` / `inferences` / `reference` 三项）、
`:1126`（`singer/<kind>` 引用 `:inference/<target>`）、`:1266`（`inferences` → `inference`、
`singers` → `singer` 的目录/键对应）。

一个 2.4 声库的**贡献面**典型形状（以已实测的 2.3→2.4 转换样例 `yousa-2.4@1.65.1.0` 为例）：
`inference/` 下的声学/时长/音高/方差/声码器五件套，加 `onset-{cmn,eng,jpn}`、`s2p-{cmn,eng,jpn}`，
加 `linguist/{cmn-pinyin,eng-arpabet,jpn-romaji}` 三个语言模块，加 `singer/yousa`。语言为
cmn/eng/jpn，5 个说话人。`assets/` 内放各语言词典与 `*_onset.json`。
**具体条目数、文件数、有无绝对路径条目等计数一律现场统计，不在本文复述**（见 2.4 的校验手段）。

### 2.3 `assets` 与语言声明的关系

- 2.3 的声明本来就已经引用了这些资源文件，转换保留其指向而不搬家
  （`scripts/convert-voicebank.py:37-38`）。词典与 `*_onset.json` 由声库**自带**，
  公共 g2p 只提供“共享的那一段”（通常是被绑定的 linguist 之外的其它步）。
- 语言由 **wolf 语言包 + singer 声明**共同决定：声库的 linguist 组合经 `languageMap` 与语言包的
  `handle`（ISO 639-3）与 `scheme` 绑定。同一个语言可因 `scheme` 不同而有不同条目。
- 声库还要声明自己的**保留音素**与音素表：保留音素缺失于模型时**整包拒绝加载**
  （实测断言 `src/tests/TestVoicebankAudit/main.cpp:898-938`，要求拒绝原因点名缺失的 token 与
  相关模型）。

### 2.4 打包规则（发布侧）

1. **zip 顶层即声库根**：解包后第一层就是含 `desc.json` 的 Package root，不要再套一层目录
   （规范：Package root 恰含一个 `desc.json`，解压即安装）。
2. **排除脏文件**：`*.log`、`*.tmp`、`Thumbs.db`、`.DS_Store`、`*.bak` 等操作系统/编辑器残留。
   ⚠️ **本条在本仓与规范中均未找到权威文件**（记为**未证实**的发布侧约定）：它是打包卫生要求，
   不属 spec 2.4 的强制规则。接入自动化打包时应把这份排除清单固化到**一处**（打包脚本），
   不要在多个文档里各抄一份。
3. **不得含绝对路径条目**（ZIP entry 层面，见 2.1 的 entry 规则）。
4. 用本仓的转换/打包链产出，而不是手搓 zip：`scripts/convert-voicebank.py`（2.3 → 2.4 转换，
   由 `scripts/test_convert_voicebank.py` 覆盖）。转换产物与已发布包应当**逐字节一致**时，
   才能说明打包链与发布侧同源。

### 2.5 校验手段（用现成用例，不抄阈值）

| 手段 | 用途 | 怎么用 |
| :-- | :-- | :-- |
| `TestVoicebankAudit` | 用**真实**声库跑完整链路，**双向**校验：链检查声库（发现转换缺陷）＋ 声库检查链（发现“路由通的音素模型里没有”，即无声失败）。另含保留音素拒绝、保留标记直通 | `src/tests/TestVoicebankAudit/main.cpp:1-11`、`:872-938`。CMake 侧用 `LITE_AUDIT_VOICEBANK` 与 `LITE_AUDIT_LANGUAGE_PACKAGES` 两个缓存变量指到目录，未设置时该用例**跳过**（`SKIP_RETURN_CODE`），见 `src/tests/TestVoicebankAudit/CMakeLists.txt:20-56` |
| `scripts/test_convert_voicebank.py` | 转换脚本自身的回归（不改代码时对同一输入应稳定） | 直接跑该脚本的测试 |
| `TestVoicebankLanguages` | 语言可用性 / 语言路由 | 同一套集成用例族（`src/tests/`） |
| 计数类校验（条目数、绝对路径条目数、脏文件） | 打包产物卫生 | 目前**没有**内置用例。需要时写成一次性只读脚本现场统计并留档，不要把数字写进文档 |

> 用例的阈值、期望条目数等**不在本文复述**：它们是用例自己的权威值，改一处即可。

---

## 三、编辑器怎么把 ORT 配置给 synthrt

一句话：**ORT 不是链接目标，而是一份由宿主部署、并在运行期把路径交给驱动的载荷**。

### 3.1 构建期：谁选、闸门在哪

1. **选择**：`onnxruntime-builds` 由 synthrt 端口的 `onnx`（及 `cuda12`）feature 引入，本仓 manifest
   只请求 `synthrt[onnx]`（`scripts/vcpkg-manifest/vcpkg.json:22-27`，CUDA flavor 走该 manifest 的
   `cuda12` feature，`:79-92`）。因此**不要在 lite 里再声明一份 ort 依赖**。
2. **载荷来源**：`scripts/vcpkg/ports/onnxruntime-builds/portfile.cmake` 把默认载荷装到
   `<port>/share/onnxruntime-builds/runtime/default/`（Windows 上是 DirectML），CUDA 载荷装到
   `runtime/cuda/`，PDB 在 `pdb/<flavor>/`，并导出 `ONNXRUNTIME_BUILDS_{INCLUDE,RUNTIME,CUDA_RUNTIME}_DIR`
   （同文件顶部布局常量与末尾的 `configure_file`）。**默认与 CUDA 载荷从不互相覆盖。**
3. **闸门**：`LITE_ENABLE_CUDA` 是唯一开关。`cmake/OrtRuntimeGate.cmake` 在**构建期与安装期各跑一次**
   （脚本模式，由 `cmake/LiteBuildApi.cmake:569-603` 调用）：
   - 期望 ON 而 `cuda/` 不在 → `FATAL_ERROR`（并给出修复指引）。
   - 期望 OFF 而 `cuda/` 在 → **删除 + WARNING**（清掉 vcpkg 树残留，避免“悄悄发布了一个 CUDA 包”）。
   - 一致 → STATUS。
   在 `LITE_ENABLE_CUDA=ON` 但端口没有声明 CUDA 载荷时，构建还会**先扫掉构建树里的陈旧 `cuda/`**
   再让闸门报错（`cmake/LiteBuildApi.cmake:507-527`），否则旧目录会被误当成“载荷齐备”。

### 3.2 安装期：DLL 与插件目录怎么布局

- 驱动插件在 `plugins/dsinfer/inferencedrivers/`，**ORT 载荷就落在驱动旁边的
  `.../inferencedrivers/onnx/runtime`**，CUDA flavor 在 `runtime/cuda`。
  这些都是 `src/libs/SynthrtEngine/DeployLayout.h:43-52` 的常量。
- 默认载荷的复制源是 `ONNXRUNTIME_BUILDS_RUNTIME_DIR`（`cmake/LiteBuildApi.cmake:196-201`）。
  复制命令在 `:463-494`：**载荷缺失在可安装树上是 FATAL_ERROR，在开发树上是 WARNING**（开发树
  允许“能列声库但不能合成”，可安装树不允许）。CUDA 载荷按 `LITE_ENABLE_CUDA` 复制到 `runtime/cuda`
  （`:496-528`）。
- **为什么必须“正好放在驱动旁边”**：主机把运行期路径**显式传给驱动**，不让驱动自己搜索。否则库搜索
  可能加载到机器上另一份 ORT（`cmake/LiteBuildApi.cmake:184-187`、
  `src/libs/SynthrtEngine/SynthrtEngine.cpp:336-341`）。
- PDB：install 层按 flavor 从 `share/onnxruntime-builds/pdb/<flavor>` 补齐符号，Release 安装包不带
  任何 PDB（`packaging/windows/README.md:96-103`）。flavor 列表与符号目录同样读
  `onnxruntime-builds` 的声明，而不是在 lite 里再写一遍（`cmake/LiteBuildApi.cmake:615-690`）。
- 打包脚本对 staging 的断言包含 `plugins`（含 `dsinfer`）与 flavor 一致性（多出或缺失
  `runtime/cuda` 都报错），见 `packaging/windows/README.md:71-74,136-141`。

### 3.3 运行期：怎么被加载

1. `SynthrtEngine::defaultPluginRoot()` 给出**包含** `plugins/` 的那一级目录
   （Windows：可执行文件所在目录，macOS：bundle 的 `Contents/PlugIns`，其它平台：`<exe>/../lib`），
   `defaultRuntimePath()` = `defaultPluginRoot()/plugins/dsinfer/inferencedrivers/onnx/runtime`，
   `defaultCudaRuntimePath()` = 前者 + `/cuda`，`defaultLanguagePackagePath()` = `pluginRoot/wolf/packages`
   （`src/libs/SynthrtEngine/SynthrtEngine.cpp:322-349`）。
2. `Bootstrap::create()` 把 `inferencedrivers` 传给 `InferenceDriverFactory::setPluginPaths`，按 API 名
   找到 ONNX 驱动。**找不到驱动不致命**（包仍能加载、声库仍能列出，只是不能跑模型）
   （`src/libs/SynthrtEngine/SynthrtBootstrap.cpp:82-94`）。
3. 初始化驱动时由**宿主**决定 `ep`（CPU / DML / CUDA / CoreML 映射见
   `SynthrtBootstrap.cpp:20-32`）、`deviceIndex` 与 `args.runtimePath`（`:96-101`）。
4. **ORT 实际是 `dlopen` 进来的，构建期不链接**：构建成功不代表运行期可用。三者齐备（dsinfer 库、
   驱动插件、部署后的 ORT 目录）宿主才注册 runtime service（`:102-109`）。
   同一结论的跨仓表述见 `<wolf 检出>/docs/linguist-distribution.md` §7.3。

---

## 四、怎么携带公共 g2p 给 wolf

### 4.1 公共 g2p 有哪些形态

`<wolf 检出>/docs/linguist-distribution.md` 是这一节的权威（本文只给指针，不复述它的清单数字）：

| 形态 | 作用 | 该文档的位置 |
| :-- | :-- | :-- |
| **语言包**（每语言一个 `wolf/lang-<iso>`） | 该语言的 linguist 声明 + 链（g2p / s2p / onset 步） | §2.1 |
| **共享后端包**（`wolf/g2p-multi`、`wolf/g2p-pinyin`） | 多语言共用的 g2p 引擎/模型后端，**不含 linguist**，由语言包在 `dependencies` 里依赖 | §2.2 |
| **直通包**（`wolf/lang-zxx`） | 纯直通（即原词兜底），供哼唱/纯音素素材 | §2.3 |

形态细节（词典 txt / json、带 `_onset` 变体的字典、`desc.json` + `linguists/` + `inferences/` 的
目录形状、各包带哪些载荷）一律以该文档各节的当前内容为准。转换管线“产物不进入 git”见其 §3.1，
打包校验开关见 §3.1.1。

### 4.2 放在哪、编辑器怎么带

- **端口**：`wolf-lang-packages` 是**纯数据端口**，不做编译。每个 feature 对应 release 里一个归档，
  下载后校验 SHA512 并解包到 `share/wolf/packages`（`scripts/vcpkg-ports/wolf-lang-packages/portfile.cmake:19-58`）。
  release tag 由 bundle 版本推导（`:23-30`），资产清单在**同一目录的** `assets.cmake`（每代发布重新生成）。
  该端口**刻意不含任何令牌处理**：release 必须匿名可达。离线环境改用本地已解包副本
  （`portfile.cmake:9-17`）。
- **本仓 manifest**：显式列出要携带的语言包 feature（`scripts/vcpkg-manifest/vcpkg.json:55-72`）。
  要加/减语言，改这一处。
- **部署**：`cmake/LiteBuildApi.cmake` 把每个包**逐个目录**复制到
  `pluginRoot/wolf/packages/<包目录名>`（`DeployLayout.h:54-55` 的 `wolf/packages`。
  复制命令 `LiteBuildApi.cmake:397-410`）。**不复制父目录**——父目录里同时有归档和第二份解包副本，
  复制它会让同一身份出现两个包（同文件 `:386-390`）。
- **来源解析顺序**（唯一权威，`cmake/LiteBuildApi.cmake:320-363`）：
  1. 缓存变量 `LITE_WOLF_LANG_PACKAGES`。
  2. 环境变量 `WOLF_LANG_PACKAGES_SOURCE`。
  3. 已安装的 `wolf-lang-packages` 包导出的 `WOLF_LANG_PACKAGES_DIR`（manifest 装有该端口时即此路）。
  4. 同级 wolf 检出的开发产物 `../wolf/build/lang-packages`——**默认关闭**，只有
     `LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK=ON` 时使用，不做“按目录名猜版本”。
  可安装树（`LITE_INSTALL`）没有语言包时 **FATAL_ERROR**，开发树降级为 WARNING
  （`:397-422`）。复制后还有一道**构建后校验**（`:424-461`），因为复制脚本对
  “配置期 glob 到、构建前被删掉”的目录会复制 0 个并仍然退出 0。

### 4.3 wolf 如何按语言 / 逻辑名找到它们

- **语言与记号（scheme）是唯一匹配键**：`LinguistSpec::language()`（ISO 639-3）与
  `LinguistSpec::scheme()`，两者都随清单解析、在 `DataOnly` 模式即可用
  （`<wolf 检出>/include/wolf/Linguist/LinguistContrib.h:27-42`）。同一语言的不同 `scheme`
  是**不同条目**，不可互换。
- **角色槽位**：g2p / s2p / onset 三个角色名在
  `<wolf 检出>/include/wolf/Api/Linguists/Linguist/1/LinguistApiL1.h` 里定义为常量。
  声明用 `imports[].role` + `ref` 绑定到具体模块（`:inference/...`、`:linguist/...`）。
  声库侧由 `scripts/convert-voicebank.py:644,709-711,733-735` 生成同形声明。
- **搜索路径优先规则**：**公共语言包路径必须排在声库内置包路径之前**
  （`<wolf 检出>/docs/linguist-distribution.md` §4.4，依据见该仓运行时文档 §9.3）。
  宿主设置包搜索路径的顺序因此是有语义的，不是随手拼列表。
- **依赖区间在提供方、目标点在依赖方**：语言包写 `compatVersion` 区间，声库写它要的版本。
  同身份重复目录属安装缺陷（同文档 §4.1、§4.3）。**编辑器不要替它们做版本猜测**。
- wolf 二进制侧的要求（安装哪些插件、缺哪个依赖会怎样、`libsynthrt-dsinfer` 与驱动的位置）见同文档
  §7.1、§7.2、§7.3。`cpp-pinyin` 词典**不随二进制部署**，它只是 `wolf/g2p-pinyin` 包的内容
  （§7.4）——所以在 lite 侧**不要**按 cpp-pinyin 端口的 usage 再往 bin 里放一份词典。

### 4.4 编辑器侧需要保证什么（检查清单）

1. **带上、且带全**：manifest 的 feature 列表就是携带清单。可安装树上缺包必须直接失败（见 4.2），
   不要用“运行期再提示用户装”。
2. **只带包，不带开发产物**：默认路径里同级 wolf 检出的 `build/lang-packages` 是关闭的，
   打包/CI 机器尤其不要打开——否则会发布一份“开发中的语言包”。
3. **顺序**：部署时公共包目录名即包身份，宿主把包搜索根传给 `SynthUnit`（
   `SynthrtBootstrap.cpp:72`）。若将来引入“声库内置包”目录，必须保证公共包路径在前。
4. **版本并存**：升级语言包是**新增目录**，不覆盖、不删除仍被在役声库依赖的旧版本。
5. **保留音素一致性**：`setReservedMarkers` 的默认集与声库声明的 `reservedPhonemes` 不一致时，
   现象是“覆盖率缺口”而不是静音（`LanguageBridge.h:116-134`），排查时先看声明再看词典。
6. **跨仓引用**：语言包一代次对应一次 release，端口资产随发布一起改。本仓**不要**在两个地方写
   同一份包清单（唯一处是 `assets.cmake` 与 manifest 的 feature 列表，语义不同、职责不同）。
