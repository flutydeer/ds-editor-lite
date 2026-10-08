# 迁移到 synthrt main 线 + wolf + otter：增量台账

> 状态：进行中。**本文是本分支相对 `main` 的增量事项唯一台账**：部署形态、模块映射、验证范围、
> 统一待办（§8）、已关闭记录（§9）、wolf 现状（§10）、推送与分支就绪（§11）、未证实清单（§12）、
> 复现命令（§13）与本地专属材料索引（§14）都在这里。原先分散的调研、缺陷档案与待改清单已并入本文，
> 其原文件已删除或归档（见 §1 与 [`README.md`](./README.md)）。
>
> 抽参域本身的设计见 otter 仓库的 `docs/otter-design.md`，与 lite 的对接见 otter 仓库的
> `docs/lite-integration.md`。wolf 语言就绪的对抗复核报告见
> [`wolf-language-readiness-adversarial-review.md`](./wolf-language-readiness-adversarial-review.md)（保留、续写中）。

> **2026-10-08 迁移轮更新（本节优先）**：本台账的基线已从 `synthrt/inferutil-binary-read` 换到迁移分支
> **`synthrt/spec2.4-uptake`**——它以当时的 `origin/main`（`33dadf958`）为基点，用 `git merge --squash`
> 把旧分支的并集算成**一条线性提交**（历史里没有 merge 记录），唯一冲突 `InferencePage.cpp` 以 main 版为准
> （main 已把歌手名本地化下沉到引擎层的 `localizedNames()`）。三个上游远端分支已统一改名为 `spec2.4-uptake`
> （synthrt 原名 `onnxruntime-builds-uptake`、wolf 原名 `linguistic-level-1-v2`、otter 原名 `analysis-level-1`。
> otter 的旧名是该仓**默认分支**，GitHub 拒绝删除，故 otter 现存双名指向同一提交），三个端口的
> `HEAD_REF` 与 `version-string` 随之改为 `spec2.4-uptake`，REF 仍是改名前后同一个提交。
> 因此本文中凡出现"暂不做上游化、不切 main"或把端口 `HEAD_REF` 记作旧名的句子，**一律已被本轮决策取代**。
> 另：新 main 自身的代码是照 refactor 线 synthrt 写的，而本分支 pin 的是主线，两线 API 不一致带来的
> 编译适配属于本轮工作（已定位并处理）。
>
> **本轮新增的已知降级（2026-10-08，集中在 `DeveloperPage.cpp` 的引擎状态视图）**：
> ① 歌手 **api level** 不再展示——`SynthrtEngine` 的 `SingerEntry` 快照不带该字段，为它去 walk
> `category("singer")` 拿借出指针，会让这个"上次扫描结果"页面重新耦合包生命周期，代价大于一列展示；
> ② 歌手 **architecture** 不再展示——主线 `ContribSpec` 没有 `className()`，且**不**用
> `interface()` / `variant()` 冒充（那是无法证实的语义替换）；
> ③ 每个 inference 的 name / class name / api level / path 不再逐条展示——主线要拿这些必须自己按 locator
> 在包树里两段查找解析，等于把包管理逻辑塞进状态视图页，改用 stages / speakers / languages 概览；
> ④ **未证实项**：`SynthrtEngine::instance()` 在找不到 owner 时会 `qFatal`（`SynthrtEngine.cpp:310`，
> owner 由 `AppContext` 注册），沿用既有的 `if (inferEngine)` 守卫，**该构造路径未实测**；
> ⑤ **未证实项**：该页**运行期渲染结果**（树里各节点的实际取值）未做端到端观察，本轮只验证到编译、链接与
> ctest 一层，GUI 里这张卡片的实际观感仍需人工过一眼；
> ⑥ 可清理项：`#include <synthrt/SVS/SingerContrib.h>` 在适配后已不被该翻译单元需要（不再用
> `srt::SingerCategory`），保留是为让本文件与 main 的 include 集合差异最小，删除不影响编译。
> 以上①②③ 取代 main 原先在此页展示的对应字段（既定取舍），④⑤ 为未证实项，⑥ 为可清理项，都不属待修缺陷。

## 0. 基线与快照口径

- **基线**：本分支 `synthrt/inferutil-binary-read` 相对 `main` 的增量提交（笔数与哈希**现场取**：
  `git rev-list --count origin/main..HEAD`、`git log --oneline origin/main..HEAD`）。撰写本台账时的
  快照为 2026-10-06、HEAD `489202eb`。引用前一律以现场输出为准，不复述。
- **只读盘点**：本文所有行号级断言都经只读盘点实测。引用**代码**时优先按符号名定位而非行号，
  引用**文档**时不写行号（文档增长即失准，本仓规约：按章节/符号定位）。
- **易变值**：端口 pin、依赖树版本、测试计数一律不复制，只写指针（§3、§6）。
- **上游快照**：synthrt / wolf / otter 各仓当时的分支尖与 pin 的历史对应关系，见归档审计
  [`../archive/four-repo-integration-audit.md`](../archive/four-repo-integration-audit.md) 的快照小节。
  其值为历史记录，**现行值一律以端口文件为准**。

## 1. 阅读顺序与文档规约

- 建议顺序：本文（台账与结论）→ [`wolf-language-readiness-adversarial-review.md`](./wolf-language-readiness-adversarial-review.md)
  （wolf 域的现状与消费约束）→ 归档审计（历史证据，仅供追溯，**不作为待办来源**）。
- 本目录的跟踪表与状态口径见 [`README.md`](./README.md)。`README.md` 的「文档清理规约」为硬约束。
- 状态行 `> 状态：` 是文档归档判据。「已完成／已实施」入 `docs/archive/`，「未实施／待实施／进行中」留本目录。
- **本地专属材料不入库**（由 `.git/info/exclude` 排除），索引见 §14。
- **不复制易变的值**（`docs/README.md` 的规约）：端口 pin 走指针，测试计数走现场输出。

## 2. 部署形态与唯一依赖树

### 2.1 构建入口

使用仓库自带的 preset。工具链与依赖树均位于仓库内，不使用系统上的 vcpkg：

```sh
cmake --preset debug        # 生成到 build/Debug
cmake --build --preset debug
```

preset 将 `CMAKE_TOOLCHAIN_FILE` 固定为 `${sourceDir}/vcpkg` 下的工具链，将 `VCPKG_INSTALLED_DIR` 固定为
`${sourceDir}/vcpkg/installed`，并关闭 manifest 模式。后两项由本分支新增（`main` 上的 preset 没有这两项，
依赖树由工具链自行定位）。该目录与安装路径、`AGENTS.md`、`packaging/windows/build-installer.ps1`
使用的是同一个依赖树，依赖只安装一份。

机器相关的路径（Qt、用于审计的声库与语言包、otter 夹具）写入 `CMakeUserPresets.json`，该文件不进入版本库：

```json
{ "version": 3,
  "configurePresets": [ { "name": "debug-mainline", "inherits": "debug", "cacheVariables": {
      "CMAKE_PREFIX_PATH": "<Qt>",
      "LITE_AUDIT_VOICEBANK": "<转换后的声库>",
      "LITE_AUDIT_LANGUAGE_PACKAGES": "<vcpkg/installed>/<triplet>/share/wolf/packages",
      "LITE_OTTER_FIXTURES": "<otter>/build/cmake/fixtures" } } ] }
```

依赖树（`vcpkg/installed`）由 vcpkg 安装：

```sh
CMAKE_PREFIX_PATH=<Qt> VCPKG_KEEP_ENV_VARS=CMAKE_PREFIX_PATH \
./vcpkg/vcpkg install --x-manifest-root=scripts/vcpkg-manifest \
                      --x-install-root=vcpkg/installed
```

构建环境的两项要求：

- **环境中必须有 Qt**，否则 `qbreakpad` 以 `Could not find a package configuration file provided by "QT"` 失败。
- 端口所固定的提交变更后，vcpkg 按新的端口内容重建。依赖的源码变更而端口不变时不重建，此时须删除整个
  依赖树后重新安装。

### 2.2 端口布局与依赖树唯一性

**四个端口位于本仓库的 `scripts/vcpkg-ports/` 下**，manifest 将该目录排在共享 overlay 子模块
`scripts/vcpkg` 之前：`synthrt` 取自 `diffscope/synthrt`，覆盖共享 overlay 中跟随 refactor 线的同名
端口，`wolf` 与 `otter` 分别取自 `diffscope/wolf` 与 `diffscope/otter` 的集成分支。
`wolf-lang-packages` 安装 wolf 发布的语言包数据。前三个端口按提交取源，不需要归档哈希。
三个仓库均已公开，安装时不需要凭据。

端口位于本仓库而非子模块，目的是使依赖只引用已推送的提交。子模块指向独立的 overlay 仓库。端口若写入该
仓库，必须先推送该仓库，干净克隆才能安装依赖，而未推送的 overlay 提交无法被他人安装。wolf 与 otter
各自的 `scripts/vcpkg-ports` 采用相同的做法。

otter 的模型包（`otter/rmvpe`、`otter/game`、`otter/hfa`）不由端口安装，由 otter 的 release
`models-v0.3.0.0` 提供。

**依赖树只有一个**：`vcpkg/installed`，与安装路径、打包脚本相同。共享 overlay 的 `synthrt` 端口在本仓库
中不参与构建，该依赖树中也没有 `srt-*` 布局，因此不需要第二个依赖树。两条线都安装 `lib/libsynthrt.so`
等同名产物，refactor 线的旧依赖树会与新树产生路径冲突，应当删除。

### 2.3 部署布局的单一真相源

三个库将插件安装到同一目录结构 `plugins/<库名>/<类别>` 下，部署保持该结构。Linux 的插件根目录是可执行
文件旁的 `lib`：

```
<exe>/../lib/plugins/dsinfer/{inferenceinterpreters,singerproviders,inferencedrivers}
<exe>/../lib/plugins/dsinfer/inferencedrivers/onnx/runtime/   ← ONNX Runtime
<exe>/../lib/plugins/wolf/{inferenceinterpreters,linguistproviders}
<exe>/../lib/plugins/otter/inferenceinterpreters
<exe>/../lib/wolf/packages/                                   ← 语言包
```

WIN32 仅根目录不同：插件直接位于可执行文件所在目录下（`cmake/LiteBuildApi.cmake` 的部署段在 WIN32 上
将插件目标目录设为 `.`，`bin\plugins\{dsinfer,wolf,otter}` 与 `bin\wolf\packages` 也正是
`packaging/windows/build-installer.ps1` 断言的路径），语言包同样相对于该目录。各类别插件目录、driver
目录、`wolf/packages` 与 ONNX Runtime 的相对路径仅在 `src/libs/SynthrtEngine/DeployLayout.h` 中定义一次：
`LiteBuildApi.cmake` 与两个 Windows 打包脚本用同一个正则表达式解析该头文件取值，引擎运行时读取同一
定义。configure 时若已安装的插件目录缺少头文件列出的类别目录，给出警告。

```
<exe>/plugins/dsinfer/{inferenceinterpreters,singerproviders,inferencedrivers}
<exe>/plugins/dsinfer/inferencedrivers/onnx/runtime/           ← ONNX Runtime
<exe>/plugins/wolf/{inferenceinterpreters,linguistproviders}
<exe>/plugins/otter/inferenceinterpreters
<exe>/wolf/packages/                                           ← 语言包
```

`SynthrtEngine::defaultPluginRoot()` 返回**包含 `plugins/` 的目录**，而不是 `plugins/` 目录本身。每个库的
CMake 包都导出 `<LIB>_PLUGINS_DIR`（`DSINFER_PLUGINS_DIR` / `WOLF_PLUGINS_DIR` / `OTTER_PLUGINS_DIR`）。

**插件依赖的共享库必须一并部署**（`scripts/deploy_linux_plugin_deps.sh`，在 rpath 归一化**之前**运行）。
vcpkg 在 Linux 上不提供 applocal 部署，即使提供也只部署可执行文件链接的库。插件在运行时按名称加载，
依赖可执行文件本身不链接的库。若不部署这些库，rpath 归一化会**移除原本有效的 rpath**，插件随后报告
缺少某个共享库，而不是报告插件加载失败。

### 2.4 wolf 语言包是数据，不是编译产物

`wolf-lang-packages` 端口将 wolf 的发行归档（tag 由 bundle 版本推导，版本以端口的 `vcpkg.json` 为准）
解压到 `share/wolf/packages` 并导出 `WOLF_LANG_PACKAGES_DIR`。本仓库在
`scripts/vcpkg-ports/wolf-lang-packages` 中保存该端口的副本（来自 wolf 仓库的同名目录，更换发行时须整体
重新复制）。

**计数口径（2026-10-06 复核，以该 portfile 当前内容为准）**：该端口的 `vcpkg.json` 当前定义
**15 个 feature ＝ 2 个共享 G2P 引擎（`multi`、`pinyin`）＋ 13 个语言包 feature（12 个语言码 ＋ `zxx`）**。
本仓 manifest 请求的正是这 13 个语言包 feature（`multi`/`pinyin` 由它们以 `default-features: false`
传递引入）。早先文档把「12 门语言 ＋ zxx」写成「13 门语言 ＋ zxx」或把 feature 总数写成 13，均为误记。
现以端口文件为唯一权威，`assets.cmake` 的 suite 清单是第二处对照。

`LITE_WOLF_LANG_PACKAGES`（或环境变量 `WOLF_LANG_PACKAGES_SOURCE`）可覆盖该路径。端口未安装时，只有开启
`LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK`（默认关闭）才回退到同级检出的 `../wolf/build/lang-packages`，
以免 CI 或打包机误用相邻检出中的开发产物。`LITE_WOLF_LANG_PACKAGES` 是缓存变量，旧构建目录中已缓存的
路径必须清除，构建才会改用端口。`LITE_INSTALL=ON` 的构建目录（两个打包脚本使用的 preset）解析不到语言包
时 configure 直接失败，开发构建目录只给出警告。语言包按 `*/desc.json` 逐个复制，不复制其父目录：该目录
是构建目录，包旁边还有同名 zip 与 `verify/` 下每个包的第二份解包副本，整体复制会使两个身份相同的包同时
出现在依赖解析路径中。

## 3. 端口 pin 与 overlay 遮蔽（只写指针，不抄值）

- **唯一权威**：四个端口的现行 pin 一律以各 `scripts/vcpkg-ports/*/portfile.cmake` 的 `REF` / `HEAD_REF`
  当前内容为准（版本字符串见同目录 `vcpkg.json`）。**本文与其余文档都不复述提交号**：此处曾出现五份平行
  副本与同一提交号的多处重复，2026-10-06 起收敛为这一处指针，历史段落只保留「当时快照（日期）」。
- **overlay 遮蔽**：manifest 的 `vcpkg-configuration.overlay-ports` 次序是 `"../vcpkg-ports"` 在前、
  `"../vcpkg/ports"` 在后 ⇒ 本仓端口遮蔽共享 overlay 子模块中的同名端口。共享 overlay 自带一份跟随
  **refactor** 线的 `synthrt` 端口（家族里的第三条 synthrt pin），对 lite **不参与构建**。它是否影响其他
  消费方未核（见 §12）。现行值见 `scripts/vcpkg/ports/synthrt/portfile.cmake`。
- **pin 前移的历史**（不含提交号）：2026-10-03 首次盘点时三个消费方 pin 都在 synthrt 的
  `onnxruntime-builds-uptake` 线。2026-10-04 与 2026-10-06 先后两次前移 lite 的 `wolf` / `otter` 端口，
  每次都以「真实 `vcpkg` 拉取 + 构建 + `ctest`」复跑作现场门禁，提交信息里记有当轮哈希（`git log`）。
- **上游推送的端口身份**：三个上游仓各自的 `scripts/vcpkg-ports/synthrt-main` 也描述同一条 pin，切主线时
  须一并复核（§11 的清单）。
- **可达性检查缺失**：lite 无 CI，甚至没有 pin 可达性的前置检查。历史上曾有 pin 在远端不可达而无人发现
  （该结论保留为历史，见归档审计）。此项登记在 §8 的 `IP-*` 与 `F-11` 条目下。

## 4. 模块映射与已知降级

| 原来（refactor 线） | 现在 |
| :-- | :-- |
| `srt::core::NO<T>` | 裸指针 / `std::unique_ptr` / `std::shared_ptr`，按所有权区分 |
| `srt::core::ErrorCode::*` | `srt::Error::*`，`result->error` 已删除（失败已由 `Expected` 表示） |
| `srt::Runtime` + `PluginFactory` + 两套驱动初始化 | `SynthrtBootstrap` |
| `VoicebankScanner` + 快照 + `SingerCapabilityReport` | `VoicebankCatalog` |
| `ds::session::ModelSetHandle` | `SingerPipeline` + `InferEngine::SingerPipelineLease` |
| `srt::g2p::LanguageService` / `LanguageRoute` / `Manager` | `LanguageBridge`（`G2pConvertRunner`、`G2pInputAdapter` 已删除） |
| `ds::bank::PackageManifest` / `SingerManifest` | `SingerEntry` + `SingerCapabilities` |
| `ds::bank::PackageValidator` | 加载一次后立即释放 |
| `srt::extract::{Pitch,Midi}Extractor` | otter 的 `F0Executive` / `NoteExecutive` + `AudioSlicer` + `AnalysisAudio` |
| `srt::audio::AudioPipeline` | talcs（`ExtractorUtils` 已删除） |
| `general.rmvpePath` / `general.gameDir`（文件路径） | `general.pitchAnalyzer` / `general.noteAnalyzer`（分析器引用） |

**`SingerPipelineLease`** 提供裸指针无法提供的两项保证：最后一个 lease 释放时丢弃 pipeline（关闭五个
模型，这是此处唯一占用大量内存的资源）。lease 记录取用时的 catalog generation，因此声库重新扫描后，
任务能够检测到所持指针已失效，而不是继续使用该指针。

**抽参设置项由路径改为引用**，原因有二：分析器是已安装包的贡献，包被安装到其他位置后路径即失效。校验也
从「文件是否存在」变为「该引用指向的分析器是否已安装、是否实现所需契约」。旧线允许将音高抽参指向音符
模型，直到实际运行才暴露错误。旧键不迁移，因为路径无法表示其所属的包与所实现的契约。

**已知降级**：

- **包校验结果不再分条**。加载器只返回一个 `Error`（带 cause 链），不提供 severity/recommendation 列表。
  换取的是**更强的检查**（每个解释器、每个 import validator 都实际运行），代价是报告只有一条。恢复分条
  报告需要加载器收集全部错误而不是在首个错误处返回。这是 synthrt 侧的改动，宿主无法自行重建。
- **G2P 预设控件不再解析任何内容**。主线没有可独立浏览的 G2P 模块注册表：语言由歌手导入的 linguist
  提供。该控件保留外观，不再执行查找。此处应显示的是「所选歌手的语言」，需要另一个控件实现。

## 5. 顺带修复的缺陷

refactor 线对接时暴露的三处缺陷已随本次迁移消失（旧线已不再对接，细节见 git 历史）：变速曲的曲线系统性偏移
（旧线按单一 `timeline.tempoAt(0)` 换算整段）、清浊标志语义与名称相反（otter 已改名并在出口处取反）、GAME 对齐
不可用（旧线封装丢弃了 `dur2bd` session，otter 已恢复）。

- 三处既有编译错误（均早于本次迁移）：`SpeakerMixList.h` 缺少 `class QLabel;`。`ProjectConverters` 只链接
  `ICU::uc`，而 `ucsdet_*` 位于 `icui18n`。`UpstreamMcpClient` 的 `m_endpoint = {}` 在 Qt 6.11 下有二义性。

## 6. 验证范围（分层，数字只留指针）

构建零错误（应用及全部库、工具、测试目标）。**测试项数与通过数以 `ctest` 现场输出为准，本文不复制**。
各次构建/复跑的具体读数见日志与提交信息。

全链路已**逐层实际运行**（而非仅核对声明），使用真实声库 yousa 1.65.1.0 与真实 GAME 模型：

| 层 | 结果 |
| :-- | :-- |
| 包加载 | 1 个包，0 个失败 |
| 目录 → 编辑器模型 | 1 歌手 / 5 说话人 / 3 语言 / 默认 cmn。音名 `C1..B7` → MIDI `24..107` |
| 语言链 | `你好` → `ni`/`hao` → `n i h ao` + onsets |
| 时长推理 | linguistic.onnx + dur.onnx 实际运行，4 个时长之和为 1.000s |
| 声学推理 | model.onnx 实际运行，mel 11008 = 86 帧 × 128 melChannels |
| 声码器 | vocoder.onnx 实际运行，44032 样本 = 86 × 512 hopSize = 0.998s @44.1k |
| 分析器发现与创建 | F0 与 Note 各一 |
| F0 推理 | 1 秒音频 → 100 帧 @ 0.01s |
| Note 推理 | A3/C4/E4 三音 → MIDI 57/60/64，边界位于整秒 |
| 部署目录 | 以本机部署目录而非 vcpkg 依赖树重新运行，全链路结果相同 |

`TestVoicebankAudit` 是该链路的回归测试，读取 `LITE_AUDIT_VOICEBANK` / `LITE_AUDIT_LANGUAGE_PACKAGES`，
未设置时跳过。已发布的声库体积过大，不进入仓库。`TestOtterExtraction` 使用 otter 构建生成的合成夹具
（`LITE_OTTER_FIXTURES` 指向含 `fixture-rmvpe` 与 `fixture-note` 的目录）实际运行 `ExtractPitchTask` 与
`ExtractMidiTask`，断言结果在工程时间轴上的位置（素材原点、裁剪后的可见区、clip 局部 tick）与取消路径。
缺少夹具、缺少已部署的 ONNX Runtime、或夹具与所链接的 otter 版本不兼容时跳过。

**部署缺口只有真正运行才能暴露**：本层全部为实测（含部署目录复跑），不以「链接通过」或「测试全绿」代替。

### 文档清理轮（2026-10-07，机器本地引用归零）

目标：`docs/` 里**只保留 git 追踪的东西**。第三方开发者从零复现时，文档里的每个路径要么能在本仓
（含子模块）里找到，要么在文内被定义为占位符。

- **改**：本机临时目录与产物名（`.tmp`、`instance`、本机日志/探针/脚本名）、本机自建构建树、
  盘符绝对路径、指向已删除台账的指针 ⇒ 中性表述（如「本机留档（未入库）」）。**结论、数字、指纹、
  提交号一律保留**，抽查办法是把删除行与新增行的数字/哈希抽出来做多重集比对。
- **不改**：main 分支既有的文档原样保留（`docs/design/**`、`docs/guides/**`、`docs/automation/**`），
  由已追踪配置定义的布局名（`build/Debug`、`vcpkg/installed`、`out/bin/wolf/packages`、`plugins/**`）、
  占位符（`<编辑器构建树>` 等）、子模块内已追踪文件（`scripts/vcpkg/**`，在根目录跑 `git ls-files`
  会把子模块路径全判成未追踪）。归档正文保留历史表述，含其残留的旧文件名引用，这属既定取舍。
- **顺带修**：与现状矛盾的断言（韩语文档的标题与结论节仍写「实测通过」、包装文档 §5 仍写「阈值未定」、
  一致性文档仍写「尚未定稿」），以及脚本收敛后失效的符号名与行号引用。

同批清理了 `.git/info/exclude` 里 4 条指向已删除文件的失效规则（落盘时这些文件已经不在）。

**行号引用的两种口径**（维护时必须先看口径，别一律对齐 HEAD）：

- [synthrt-wolf-otter-integration.md](../guides/synthrt-wolf-otter-integration.md) 按**当前工作树**维护
  （该文自己写明"改动后须按符号名重新定位"）⇒ 脚本一改行号就要跟着刷，本轮已刷 8 处。
- [wolf-language-readiness-adversarial-review.md](wolf-language-readiness-adversarial-review.md) 逐节
  **锚定当时的快照提交**（如 `b0bf3c9b`、`8a599701`、`6d575727`，`§11` 明写"以各段所述提交为准"）
  ⇒ 那些行号是该轮"逐条核对一致"的历史断言，**不要**改成当前行号，改了反而变成假陈述。
  要在当前树上核对这些证据，按符号名找（例：`reserved_phonemes()` 的四表取交集现在在 `:308-324`，
  onset 阶段块现在在 `:713-735`，`s2p and onset` 静默条件现在在 `:599-600`）。

**同一轮的只读复核与落地**（结论都已进文档）：

- **时效性审查（`docs/synthrt/**`，12 条）**：9 条已修——韩语文档标题与结论节仍写「实测通过」、包装文档 §5
  仍写「阈值未定／尚未定稿」、一致性文档仍写「尚未定稿」、包装文档引用了已被合并的符号名、README 的
  Python 脚本计数、「6 篇文档 + 3 个工具」计数、`PLAN.md` 的「进行中」状态行、发行历史的包计数（13→14，
  同表里 `wolf-lang-zxx` 版本号未动这一反例已写进正文）、`PLAN.md` 里对台账的行号引用改章节指针。
  另有 1 条属复核所用快照过期，落地为补上 `docs/README.md` 缺的 `docs/synthrt/` 索引行，以及一条复核
  未报而自查发现的过期标题（§4.2 仍写「未取样本」）。**2 条复核后判定不成立**（两条命令的 6 个必需参数
  一个不少，已用脚本逐个点名核过、拼音包版本没有说低一档），未据此改动文档。
- **对抗复核（两份归档，10 条）**：8 条已修，都是"替换把证据句柄抹平"造成的自我矛盾——「因为报告引用了
  它们」的因果不再成立、18 格证据栏同文化后丢了"哪些结论同源、哪条被两轮复现"（改为轮次表述）、把通配族
  写成「各一份」、台账成了无名对象、无头脚本丢了分工、本机 skill 名残留、相对路径少前缀、括号说明重复。
  2 条按归档「正文保留不改」保留（报告自指路径已迁到 `docs/archive/`、指向已并入台账的旧文件名）。
- **行号引用的漂移**：本轮的收敛会移动转换器里的行号，任何按行号引用它的文档都需要自行刷新。按符号名核对
  更稳（`declarations_from_model`、`overlapping`、`word_div` / `word_dur` 判据常量）。

## 7. 转换脚本三件套与回归三层

`LITE_AUDIT_VOICEBANK` 指向的 2.4 声库由 `python3 scripts/convert-voicebank.py <源声库> --output <目标目录>`
生成（目标目录须在源包之外，语言绑定另需 `--packages <已安装语言包目录>`，否则该语言会被丢弃并告警）。
转换成功以退出码 0 判定：脚本末行打印 `0 error(s), N warning(s)`，每个错误另以 `error: …` 写入 stderr，
出现任一错误即返回 1。两个安全默认：**已存在的目标目录会被拒绝**（要覆盖须显式 `--force`），`--in-place`
先把包转到旁侧 staging 目录、整包成功后才换入，失败时原包分毫不动。

面向发布的转换走**带校验的包装脚本**：`python3 scripts/convert-package.py <包> [<与上面相同的选项>]`。
它在读包之前先做目标目录检查，转换后把产物**读回来核对**（`$version`、id 与版本 grammar、每个贡献声明
存在、不残留 `class`/`$version`、2.3 的其它贡献类别若被原样带过即判失败），并可用 `--patch DIR` 产出
"只含差异文件 + `patch.json` + `DIR.zip`"的补丁包，避免把模型分发两次。规则实现只有一份：包装脚本从
`convert-voicebank.py` 载入模块并读取其中定义的值（`DEFAULT_OUTPUT_SUFFIX`、`MANIFEST_VERSION`），
不复制规则，也不再按行号引用它——早先按行号的副本在参考脚本增长后立刻失准，2026-10-03 已改掉。

| 脚本 | 用途 | 输入 → 输出 | 安全默认 |
| :-- | :-- | :-- | :-- |
| `convert-voicebank.py` | **参考实现**：2.3 声库 → 2.4，转换规则唯一一份 | 2.3 包目录 → 新包目录（或 `--in-place`） | 已存在的目标拒绝，`--force` 才覆盖。`--in-place` 先转旁侧 staging、整包成功才换入 |
| `convert-package.py` | 校验包装：把命令行原样转给参考实现，转换后**读回核对**，并可产出补丁包 | 同上 → 同上，另可 `--patch DIR` | 目标检查前移到读包之前，产物未通过核对即判失败 |
| `convert_phoneme_symbols.py` | 一次性助手：上游音素符号 YAML → 声库用的 `phonemeTypes` JSON（**不属于** 2.3→2.4 链路） | 符号 YAML → inventory JSON | 已存在的输出拒绝，`--force` 才覆盖 |

测试是同目录的 `test_*.py`，由 `python3 -m unittest discover -s scripts -p "test_*.py"` 一起跑。转换的回归
有三层，都在本机可跑，**通过与否以现场输出为准（本文不复制计数）**：

1. `python3 -m unittest discover -s scripts -p "test_*.py"`（`test_convert_voicebank.py` 现场生成最小 2.3
   包，覆盖正常转换与各守卫）。
2. 真实 2.3 声库转换后与已发布 2.4 包**逐字节**比较（2026-10-03 实测 yousa 与 junninghua 两例：文件集合、
   目录与模式位零差异，具体文件数不复述）。
3. 把转换产物放进编辑器的声库搜索路径，用无头自动化**实际加载并合成**（`voices.list` 认出、
   `tracks.set_voice` 通过、导出 8 s 波形）——这一层才是"2.4 形状确实能被加载器接受"的证据，前两层只说
   明"与已知可加载的产物一致"。

本机重建审计声库（2026-10-03 起）：本地保留两个真实 2.3 声库压缩包（`qixuan@2.7.0.0` 与 `zhibin@26.7.16.0`）。
解包后用上面任一脚本转换成 2.4，把转换结果**所在目录**作为无头编辑器的声库搜索路径交给
`LITE_AUDIT_VOICEBANK` 同类实测即可：`voices.list` 应认出两个 singer，`zhibin` 有两个说话人
（`zhibin-base` / `zhibin-pop`），`qixuan` **没有说话人**（2.3 声明的 `speakers` 是空数组，2.4 接受），
此时 `speaker` 必须传 **null**——传空字符串会被自动化接口的参数校验拒绝。

### 脚本收敛轮（2026-10-07，只读审查 → 分批施行）

对 `scripts/` 与 `docs/synthrt/tools/` 共 11 个脚本做了一轮只读收敛审查（快照即本轮开始时的树），
按「等价收敛优先」分批施行。**已施行项都在落地后过了门禁**：单测 38 项、6 个真实模型用例、工具的
PowerShell 解析检查，以及文档里 7 条命令与工具参数表的逐条核对。

| 项 | 位置 | 动作 |
| :-- | :-- | :-- |
| 声明判定合并 | `scripts/convert-voicebank.py` | `mode_to_declare` 与 `word_predictor_keys_to_declare` 合并为 `declarations_from_model`，一处读模型、一处调用 |
| 重叠守卫合一 | 两个转换脚本 | 包装器不再自带一份实现，改调权威模块的 `overlapping()` |
| 形状自查补齐 | `convert-voicebank.py`（G2P 导出的语言对） | 内联链式取值改走 `read_object` / `read_array`，异常形状报错而不抛栈 |
| 同步面注释校正 | `convert-package.py` | 注释不再断言「只有 PATCH_MANIFEST 是自有定义」，改为指向测试所守的契约 |
| Duration 常量 | `convert-voicebank.py` | `DURATION_KIND` 一处命名，契约表与声明步骤共用 |
| 测量工具身份参数必填 | `docs/synthrt/tools/measure-g2p-output.ps1` | 三个歌手身份参数不再写死本机声库，6 条文档命令同步补齐 |

**已否决（记录理由，防止重开）**：

- 把四个工具里的流式哈希与下载传输层抽成共享模块：`docs/synthrt/tools/` 的脚本是**交接给第三方的自包含工具**，
  README 明写「只用标准库、各自先跑一次自证」，共享模块会破坏「一个人从零复现」这条验收标准。
- `convert_phoneme_symbols.py` 去掉 PyYAML 依赖（用户 2026-10-07 裁定不动）：该文件首次提交早于本分支，
  手写窄子集解析会引入新的解析风险，且会影响测试在无 PyYAML 环境的可运行性。
- 权威转换器补 `allow_abbrev=False` 以统一缩写前缀行为（用户同轮裁定不动）：会改变对外 CLI 接受面，
  需要单独一轮裁定。

**未做（留待需要时）**：测试里 11 处硬编码 `linguist/s2p` / `linguist/onset` 改为引用权威常量（收益低，
且会让期望文案与常量同步漂移，需要先定文案断言口径）、把 `docs/synthrt/tools/` 两个脚本折到 100 列
（纯排版，`scripts/` 自身也有 3 行 101 列，仓库无强制约定）。

## 8. 统一待办台账（唯一编号）

编号规则：`MG-*`＝迁移剩余事项。`IP-*`＝原「对接层以外需要的改动」九条（保号）。`F-*`＝沿袭归档审计的
编号（**不重新编号**，以免历史引用断裂）。状态以本表为唯一权威，详情在对应小节或指针处。

### 8.1 迁移剩余事项

| # | 事项 | 状态 |
| :-- | :-- | :-- |
| MG-1 | dsinfer 不再手工定位：override 端口保留了 dsinfer 的 CMake 包（经 fixup 落到 `share/dsinfer`），`cmake/LiteDsinfer.cmake` 只调用 `find_package(dsinfer CONFIG)` | 已完成（保留登记，防止回退到手工定位） |
| MG-2 | **`um` 是声库自身的缺口**：yousa 的 cmn/jpn 两本词典都声明了该音素，而四个模型均不包含。该音素不能声明为 `reservedPhonemes`（整个包会被拒绝加载），也不能从 `exports.phonemes` 删除（删除后不再有任何声明表明该音素不可用）。需由声库作者决定 | 待上游/声库作者 |

### 8.2 对接层以外需要的改动（保号，每项含位置与建议）

对接层的范围：`src/libs/SynthrtEngine`、`src/libs/PackageManager`、`src/app/Modules/Inference`、
`src/app/Modules/Extractors`、`src/app/Automation` 中与抽参、推理、导出、声库相关的适配器、
`src/app/Model/AppOptions` 中与推理和分析器相关的选项、`cmake/LiteBuildApi.cmake`、`scripts/`、打包脚本、
对接层测试与 `docs/`。以下条目均在该范围之外，行号以撰写时为准，**改动前按符号名重新定位**。

| # | 主题 | 要点 |
| :-- | :-- | :-- |
| IP-1 | 保留音素 | 工程模型与编辑器仍只按 `SP`/`AP` 两个字面量判定，与推理侧「强制集 ∪ 歌手声明集」不一致：`Note::canEditPhonemes()`、`SingingClipSlicer`、`SingingClipPhonemeNormalizer` 三处。建议 `SingerInfo` 增加 `reservedPhonemes()`（对接层填），模型提供 `isReservedLyric(lyric, singer)` 并改调它。`Note` 不持有歌手信息，须由调用方传入集合或改为 `SingingClip` 的方法 |
| IP-2 | 零长音符与区间树 | `std::invalid_argument: Low border is not lower or equal to high border.` 来自 `lib_interval_tree::interval`。`OverlappableSerialList.h` 已钳位，**talcs 的 `IClipSeries` 不钳位**（`length <= 0` 抛）。写入点在 `TrackInferenceHandler::syncInferPiecePosition()` 与 `TrackSynthesizer::handleNotePropertyChanged()`。另有 `OverlappableSerialList::remove()` 按当前区间查找、失败后 `erase(end())` 的 UB。建议：写入前跳过/钳位、`Note::setLength()` 在 Release 亦拒绝 0、树在插入时记录区间并在查找失败时报错。**不改第三方 interval-tree** |
| IP-3 | 默认语言与语言回写 | 工程模型侧三条互不一致的回退链（`SingingClip::defaultLanguage()/effectiveDefaultLanguage()`、`AppModel` 载入时覆盖、`"unknown"` 哨兵约 10 处、`GeneralOption.h`/`AppGlobal.h` 硬编码语言表与默认歌词）。建议：定义 `kUnknownLanguage`，把「音符 → clip → 歌手包默认值 → 轨 → 应用默认值」实现为单一函数，载入时不再写应用默认值，下拉与默认歌词表改读 `SynthrtEngine::languagesOf()` 与语言包声明 |
| IP-4 | 参数曲线 5 tick 网格 | `DrawCurve.h` 的 `int step = 5;` 是网格实际来源，另有 `AppModelUtils::getResultCurve()` 与 `ParameterAutomationFacade.cpp` 硬编码。建议在工程模型定义 `DrawCurve::kDefaultStepTicks` 并让 `kParamCurveStepTicks` 等于它，届时删除 `InferParamCurve.h` 的独立定义 |
| IP-5 | 遗留的 G2P 标识管道 | G2P 标识已不存在（linguist 按语言句柄转换），但约 20 文件 / 120 处仍在用 `g2pId`/`G2pId`/`defaultDict`（`SingerInfo`、`SingingClip`、`Track`、`LanguageInfo`、多个 UI 控件与页面、`SettingsAutomationFacade::updateG2pLanguage`、`DspxProjectConverter`）。两步走：先停止按 g2pId 比较并删字段，再移除预设控件与设置项（工程文件旧字段只读不写）。MCP 的 `g2p_id` 在下一协议版本删除 |
| IP-6 | 设置页显示的空路径与恒缺省的能力字段 | `InferencePage` 显示从未赋值的 `InferEngine::configPath()`。`SingerInfo.cpp` 的歌手提示读 `SingerCapabilitySummary` 的一致性等级，而 `summaryOf()` 不填（主线不提供）。建议删该行与 `InferEnginePaths::config`，并从 `SingerCapabilitySummary` 删除这些字段 |
| IP-7 | 执行提供程序的其余字面量 | 对接层已统一为 `ExecutionBackend.h` 的枚举，`SettingsAutomationFacade.cpp`、`PublicToolContract.cpp`、`InferencePage.cpp`、`InferenceOption.{h,cpp}` 仍用 `"CPU"/"DirectML"/"CUDA"` 字面量。建议改用 `ExecutionProviderUtils::toString()/fromString()` 与 `availableInBuild()`，契约枚举由同一函数生成。CoreML 继续不对外暴露 |
| IP-8 | 导出长度 | `exports.audio` 取工程长度而非所选轨的内容长度（`AudioExporter.cpp`）。建议轨模式下以所选轨最后一个片段的结束为长度，或增加长度策略配置 |
| IP-9 | 对接层内已知而本轮未修改 | ① `InferRetake` 注释写帧下标而 `convertInputParams()` 按秒传给 dsinfer（各任务写 `retake.end = frames`，实际等同整段重算）。改口径需确认 dsinfer 对越界 `retake` 的处理，且会使全部推理缓存失效一次。② `H3` 的修复缺自动化测试（`HeadlessInferenceTask` 依赖真实 `InferController` 单例）。③ 上游问题不在 lite 修：synthrt `ITask` 析构契约（审计 H1）、`SYNTHRT_DECLARE_AS_METHODS` 的无检查下转型。otter 的 `AnalysisTask::start()` 已入口校验、`createAnalyzer()` 已改 `dynamic_cast`（审计 H2，otter 已修） |

### 8.3 归档审计 F 系列（沿袭原编号）

来源：归档审计 §6.2 与 §7.2。**除下表注明者外，其余条目的现状未在本合并轮逐条复核**，故按原分级登记。
引用前请先复核，或直接读归档审计的条目正文。

| # | 级 | 一句话 | 现状 |
| :-- | :-- | :-- | :-- |
| F-1 | P0 | otter 固定版本编不过（C++ 成员名隐藏，与平台无关） | **已关闭**（pin 前移 + 上游重写），见 §9 |
| F-2 | P1 | 能力查询说「可用」、发起被拒（`PublicAutomationHostAdapter` vs `ExtractionAutomationAdapter`），无统一收口 | 开放，最小落点＝把已有的 `configuredNote` 校验并入 `available` |
| F-3 | P1 | 推理失败可能没有错误信息，空 words 仍继续推理 | 开放（原「失败仍报成功」已被推翻，降级） |
| F-4 | P1 | 「隔离数据根」只隔离一半：缓存/日志/转储仍在共享位置 | 开放（机制确认，后果已补全） |
| F-5 | P2 | ORT payload 靠猜 + 只 WARNING，无下游兜底 | 部分已修（2026-10-03），余项与 `F-11` / `IP-7` 同族 |
| F-6 | P2 | MCP 关闭时固定等 150 ms：白等，且超时响应被切断 | 开放（双向后果已确认） |
| F-7 | P2 | 包扫描失败一律映射成「后端未初始化」，真实 message 被丢弃、UI 静默 return | 开放 |
| F-9 | P2 | 用 `CMAKE_BUILD_TYPE` 判配置（多配置生成器会剥 Debug 符号） | **降级**：当前 preset 全为单配置 Ninja，不可触发。见 §9 |
| F-11 | P2 | `plugins/<lib>` 目录名三处写死且不在 `DeployLayout.h` | **升级**为结构性缺口（类别目录 / ORT payload 无打包期门禁）：打包期必需路径清单加 ORT payload、断言改用已解析的类别目录，候选与验证方式见归档审计的交付面小节 |
| F-12 | P3 | 声库 `url`/`readme` 被静默丢弃，编辑器永不显示 | 开放（加载器其实读到了，构造 `PackageInfo` 时传空） |
| F-13 | P3 | `ONNXRUNTIME_ENABLE_DML` 定义了但无人读 | 开放 |
| F-16 | P3 | wolf `ExecutiveTask::start()` 无重复启动/状态前置校验 | 开放（未再复核） |
| F-17 | P3 | wolf 槽位借出/归还手工配对，非 RAII | 开放（未再复核） |
| F-18 | P2（推断） | CUDA 固定 `HEURISTIC`，放弃 cuDNN 自动调优 | 开放（未实测，推断项，不当既定缺陷排期） |
| F-19 | P3 | `convert-voicebank.py` 的路径校验与加载器不同构（漏 NUL 与 map 值路径解析） | 开放（窄场景） |
| F-20 | P3 | 契约三元组 `(interface, variant, kind)` 无单一真相源 | 开放 |
| F-21 | P3 | 音高回写只校验溢出与有限性，不校验段是否落在片段内/值是否为空/点数是否有界 | 开放 |
| F-22 | P2 | wolf lua 插件装载期执行脚本无打断路径（`while true do end` 可永久挂死加载） | 开放（wolf 域，另见 §10） |
| F-23 | P3 | wolf manifest 里的脚本路径不限定在包内 → 任意宿主文件被当 Lua 执行 | 开放（wolf 域） |
| F-24 | P3 | wolf 自制 `utf8` 库语义缺陷，与自己头文件声明矛盾 | 开放（wolf 域） |
| F-25 | P3 | wolf multig2p：具体错误被吞。`maxLen` 无上界、截断后静默「全部失败」 | 开放（wolf 域） |
| F-26 | P3（未证实） | otter `tifa/Decode.cpp` 平局口径疑与参考规则不符 | **未证实，禁止排期**（需真实 Python 参考）。见 §12 |

## 9. 已关闭 / 已推翻记录（防重开）

| 旧条目 | 处置 |
| :-- | :-- |
| **同名 API 三写矛盾**（归档审计一处写「保留」、另一处写「删除」，缺陷档案与调研报告写「零调用待清」） | **统一裁定：已于 2026-10-04 删除，源码 0 引用**。2026-10-06 复核 `src/`：`hasInferenceBackend`、`cancelConversions`、`srtErrorToString` 命中 **0**。引擎级 `setReservedMarkers`/`reservedMarkers`、`packageDirectory`、`unit`、`languagesOf`、`setSingerPhonemes` 亦已删除，仅保留仍被测试调用的 `LanguageBridge` 同名方法与 `unitIfReady()`。**不再有第二种说法** |
| **语言包计数「勘误」本身错一门**（审计 F-14 与本文旧句各写一种） | 已按 §2.4 的复核结果改写为「15 feature ＝ 2 引擎 ＋ 13 语言包（12 语言码 ＋ `zxx`）」，并注明以端口文件当前内容为准。**F-14 归档，不重开** |
| F-1 otter pin 编不过 | 已随 2026-10-04 的 pin 推进与上游重写关闭 |
| F-8 `beginCommitting` 返回 false 的收尾 | **非缺陷，不要重开**：管理器在返回 false 前已置终态并回调，观察者一定收到终态。可挑剔的只是可读性（建议在声明处补注释） |
| F-9 `CMAKE_BUILD_TYPE` 判配置 | 降级：当前 preset 全为单配置 Ninja，不可触发 |
| F-10 `TestAnalyzerPackages` 的 Windows 权限夹具 | 2026-10-01 已修（夹具改为「必然被拒绝的包」，断言改无条件） |
| F-15 无条件删 `lib/cmake` | 降级 P3：实测 `share/` 下两份 config 都在，`lib/cmake` 已不存在，消费方走 `share` |
| F-27 无检查下转型 | 与 §8.2 `IP-9` 重叠，**不单独排期** |
| `ROLLBACK 2026-09-30` 注释与事实相反 | 注释已随 pin 前移删除（现行值见端口文件） |
| 「已知项 3」`maxSegmentDuration` 静默关闭切片 | **已在源头闭环**（otter 侧装载期正数门禁 + 测试），lite 侧不该补 |
| 「已知项 5」hitSource 未透出 | 部分推翻：桥接层已透出，断点在消费侧（`InferController` 构造 DTO 不带 stage 等），已并入 §12 |
| `plugins/<lib>` 两处/三处写死 | 订正为**真副本 3 处**，并升级为结构性缺口（`F-11`） |
| `OverlappableSerialList::clear()` | 降调为「死代码 + API 不一致，无现实影响」（零调用、只清 2/4 成员） |
| 「清 redo 栈即泄漏」 | 反驳：`undo()` 会把对象挂回容器。真正的泄漏点是 `HistoryManager::reset()` 与析构（见 §12 的 M1） |
| 「导出每轮重触发推理是缺陷」 | 反驳：注释写明是有意设计（piece 分先后进入 lazy-acoustic） |
| 「clip/note 所有权与文档契约冲突」 | 订正为**契约缺口**（契约只覆盖 Track，与实现相符） |
| 「W1 会丢用户编辑」 | 反驳：编辑在 `Edited` 层，已有「仅改发音」的快路径。缺的只是失效键（§12 的 W1） |
| 「`probe()` 每次 stat 文件」「lite 并发进同一 wolf session 有竞争」 | 反驳：前者不碰文件系统。后者被 wolf 的线程安全声明与内部单锁支持 |
| 「otter pin 在 otter 主仓任何 ref 不可达」 | 保留为历史：该端口 pin 已前移。结论只作 2026-10-03 快照看待 |
| 「`ExtractMidiTask` 空指针解引用」 | 目前**不可达**（基类守卫 + 同一对象已证），只加注释 |
| 「`RuleParamCurve`/`Param` 浅拷贝导致双删」 | **未证实**（无调用点），不得排期 |

## 10. wolf 语言就绪与加载期合规现状

**决策台账（摘要，轮次 A~H）**：A 先去 synthrt 核实 spec 2.4 再谈「预检资源」，查询期 `verify()` 草案作废、
合规落点改为加载期。B 逐词 `DriverUnavailable` **保持现状**不标灰。C 只禁真坏的那一个语言、失败包照常
显示但标灰。D 其他严重事项只落盘（→ §8.2/§8.3）。E 先收敛本地能做的。F/F′ **2026-10-05 拍板：暂不改 wolf，
先把台账落盘**，V6/F5 是否必须改 synthrt 已给出可达性论证（**不必改**）。G 授权实施 lite 侧 D4~D8。H 修正
multig2p 可达性。完整台账与来源轮次见 wolf 报告的决策台账小节。

**加载期合规台账（摘要）**：F1 包内 ONNX 模型在 Commit 之后才验证/打开且**实际可达**（OOV 词静默降级，
语言不标灰）——判定「轻不合规＋wolf 内部政策不一致」，**修复第一顺位**。F2 驱动/EP 缺失只在运行期以逐词
报错、fallback 还会改写成成功——字面不合规但有明示设计理由，**需产品拍板**。F3 文档把「会话期执行体创建
失败」称作「加载失败」——措辞级，零代码风险。F4 无法绑定的 linguist import 只 warning ⇒ 声明与装配不一致
的包仍 Commit——弱不合规/取向差异，改动会改变存量包加载结果。**F5 ＝ V6 不可达**（当前单 linguist
provider），故不必改 synthrt。V 域：V1/V11 合规（正向）。V3 未证实/未来风险。V4/V7/V8/V9/V10 为取向差异。
逐条证据与判定口径见 wolf 报告 §10.1，抽验记录见其 §10.2。

**消费约束（事实）**：wolf 的改动要进 lite，需要 push wolf ＋ bump
`scripts/vcpkg-ports/wolf/portfile.cmake` 的 `REF` ＋重装该端口。端口经
`vcpkg_from_git(URL https://github.com/diffscope/wolf.git)` 取源。**代理不得执行 push**，改动待用户发令。

**lite 侧现状（事实）**：标灰只覆盖「未声明语言」与「引擎给出原因的声明语言」，即**路由级**不可用。
lite **从不调用 `warm()`**（仅三处注释说明「无预热」是有意设计）。`setSingerPhonemes` 无应用侧调用者
⇒ 覆盖率恒 `Unknown`。资源级单语言损坏在首次尝试前不可见（wolf 明文设计取舍），尝试一次后由失败缓存
让 `probe()` 报 `Unavailable`——**这条路径不需要上游新增 API**。缺的是「失败后 lite 不重算语言呈现」。
未证实与待拍板清单见 wolf 报告，本文不复制。

**lite 界面实测（2026-10-04）**：抽取设置页两个下拉框（`Note Analyzer` / `Pitch Analyzer`）正常渲染，
当前取值 `(none)`（该搜索路径下没有安装分析提供者）。同一对话框「推理」页的 `执行提供程序` 与 `GPU`
两个下拉框已实测展开。要让分析器列表非空，需把携带 `analysis` 声明的包放进包搜索路径。

## 11. 上游推送与分支就绪

- **synthrt 口径（2026-10-03 用户裁定，本文与此前文档一致）**：synthrt 继续定在
  `onnxruntime-builds-uptake`（＝ `main` ＋ 6 笔），**暂不做上游化、不切 `main`**。端口 `HEAD_REF` 仍指向
  该分支（现行值见端口文件）。因此「切到 `main` 会缺什么」的盘点**读作「若/当切到 `main` 时会缺什么」，
  不是待办缺陷**：缺 5 个符号（`SingerSpec::reservedPhonemes()`、`SingerSpec::languages()`、
  `SingerSpec::defaultLanguage()`、`SingerCategory::NAME`、`InferenceCategory::NAME`）＋构建与部署面的
  4 笔（ORT 取自 `onnxruntime-builds`、dsinfer 插件按 plain name 安装、按宿主布局加载 ORT payload、
  CUDA 用 `HEURISTIC`）。实测口径是「`main` 的头 ＋ 本仓真实编译命令」的单 TU 编译（lite / wolf / otter
  三个消费方各一），**不是**从源码完整 configure+build 一遍 synthrt `main`。
- **上游化的最小集合**：若将来要切，至少需要这三笔（singer 语言/保留音素声明、ORT 取自
  `onnxruntime-builds`、按宿主布局加载 payload）。六笔的主题清单见归档审计与 `git log`。**顺序与评审口径
  需人判**（是否按原样推，还是拆成「ORT 相关 / singer API 相关」两组）。
- **本地历史已压缩**：本分支的增量由 34 个过程提交压缩为 6 个主题提交，各按其主题归组，压缩后的树与
  压缩前逐字节一致（用 `git diff` 对过）。压缩前的历史只在本机备份分支上可达，未推送。文档里对旧提交号
  的引用属当时的陈述，不随压缩改写。
- **本分支推送状态**：本地增量**未推送**。远端同名分支是 2026-09-30 的**旧压缩**（4 个提交，与本分支
  共享更早的祖先），两边各自压缩后互不包含（非快进），且远端那份的三仓 pin 全是旧值。⇒ 将来若要推送
  须用**新分支名**（非破坏性）。**本会话禁 push**，任何 push 由用户发令。
- **pin 与上游可见性**：本地 port 的 pin 与上游仓远端跟踪 ref 一致（**推断已发布**）。远端可达性本身需
  `git ls-remote` 现场确认（本会话禁联网）。
- **CI 现状**：lite **完全没有 CI**（连 `.github` 都没有，其它常见 CI 文件逐个确认不存在）。wolf/otter 各有
  `.github/workflows/ci.yml`，两仓 CI 都通过 **in-repo 端口**取 synthrt ⇒ pin 随端口一起走，但**没有 pin
  可达性的前置检查**。最小 CI 的落点与验证方式（含「故意改坏一个端口 REF，L1 必须变红」）见归档审计与
  调研结论的交付面小节，主题并入 §8.3 的 `F-11` 族。

## 12. 未覆盖范围与未证实清单

**未覆盖（用之前先补）**：wolf `src/plugins/inferenceinterpreters/**` 的 `pinyin`、`stub` 正文，以及
`chain/main.cpp` 的首尾区段（其余含 `lua`/`multig2p` 已通读），otter 多个子系统的深读，共享 overlay 的
第三条 synthrt pin 是否影响其他消费方。`ls-remote` 类互网核对。

**未证实（禁止当缺陷排期，需实测/取证后才可升级）**：

- 内存/性能类（M1~M4、D1~D4、P1~P9 各条）的**真实量级**：`Track::clips()` 单次拷贝耗时与 42 处调用点的
  可见卡顿、导出轮询每轮成本、`DrawCurve::insertValues` 曲线规模-耗时、`updateOriginalParam` 的
  O(N²) 重建量、AI 路径 mut 的实际发生次数（只证到「按 piece 循环发请求」的结构）。
- `Param` 拷贝/移动赋值是否有真实调用点（双删风险）。`OverlappableSerialList::clear()` 未来是否有调用方。
  `HistoryManager` 泄漏在真实使用中的可观测频率。
- wolf 域：失败后 lite 是否**一定**不刷新语言呈现（静态调用图已确认，端到端未实测）。逐词失败消息最终
  在 UI 的呈现深度，`warm()` 的真实代价，覆盖率一族数据的真实分布，首次尝试前的资源级不可见。
- otter 域：`tifa/Decode.cpp` 的平局口径（需真实 Python 参考实现）。协程能否绕过 lua 沙箱 hook（取决于
  解释器是否真是 LuaJIT）。`Executive` 持有 `const Configuration*` 是否安全（取决于 synthrt 的所有权契约）。
- W1（推理侧重复跑整 clip G2P）的失效键设计：**已定为只落盘**——它需要「发音属于哪次歌词/语言」的
  失效键，属设计决策，不是随手可改的缺陷。

## 13. 复现命令与夹具

```sh
# 分支相对 main 的差量（本仓）
git rev-list --count origin/main..HEAD
git log --oneline origin/main..HEAD

# 上游：分支相对 main 的差量（`<synthrt 检出>` 为占位符）
git -C <synthrt 检出> log --oneline main..<端口 HEAD_REF>

# pin 可达性（`<端口 REF>` 从端口文件现取）
git -C <otter 检出> branch -a --contains <端口 REF>

# lite 是否有 CI
Test-Path .github

# 构建与测试（数字以现场输出为准）
cmake --preset debug && cmake --build --preset debug
ctest --test-dir build/Debug --output-on-failure

# 转换脚本回归
python3 -m unittest discover -s scripts -p "test_*.py"
```

**夹具**：`LITE_AUDIT_VOICEBANK`（转换后的 2.4 声库，§7）、`LITE_AUDIT_LANGUAGE_PACKAGES`（已安装语言包
目录）、`LITE_OTTER_FIXTURES`（otter 构建生成的 `fixture-rmvpe` / `fixture-note`）。语言就绪矩阵与
「尝试一次后再读就绪」的复现命令、包内资源损坏夹具说明见 wolf 报告的复现命令附录与夹具小节。三者都在
工作树的本地目录中，不进入版本库。

## 14. 本地专属材料（不入库）索引

**2026-10-07 状态**：本节原登记的三份材料（`four-repo-defect-scan.md`、`four-repo-increment-report.md`、
`2026-10-03-recovered-audits-and-hardening.md`）已按用户裁定从工作树移除。移除前按「文档清理规约」第 3 条
在**仓库外**打包并逐份哈希校验回读（不一致 0），该批次的归档包为
本机留存的当轮产物（未入库）。

同批复核时另发现两份被 `.git/info/exclude` 排除的旧方案（`lang-package-24-rebuild.md` 状态「待实施」、
`lyric-language-architecture.md` 状态「进行中方案」），经用户裁定一并移除，其 `.git/info/exclude` 条目已同步清理。
归档目录 `docs/archive/four-repo-integration-audit.md:17` 内对其中两份的引用按「正文保留不改」保留，
是本批之后**唯一**指向已移除文件的残留引用。

其余本地材料是工作树下的临时目录（已被 `.gitignore` 忽略）与 `CMakeUserPresets.json`（机器本地配置），
前者含复现脚本与夹具，删除或清理前先查引用（用户文档写明路径或指纹的目录视为用户资产，先问再删）。
