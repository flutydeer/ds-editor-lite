# 迁移到 synthrt main + wolf + otter

本文评估把 lite 从 synthrt 的 refactor 线迁到 main 线，并接入 wolf（语言域）与 otter（抽参域）的
工作量，并给出实施方案。

抽参那一段的细节在 otter 仓的 `docs/lite-integration.md`，本文只收它的工作量与排序。

## 结论先行

**代码工作量比表面小得多，但有一个不在代码里的阻塞项。**

- 直接触及 synthrt 符号的只有 **282 行**，分布在 30 个文件、共 6366 行里。lite 早就有一层防腐层：
  `SpeakerInfo`(385 处)、`LanguageInfo`(128 处)、`ResolutionState`(53 处) 全是 lite 自己的类型，
  只在 `PackageManager.cpp` 的几行上与 synthrt 对接。迁移集中在边界，不会扩散到全仓。
- 推理契约的载荷几乎没变：`Common/1` 两条线逐字段比对下来只差三处（无作用域枚举变成
  `enum class`、`Speaker` 少一个 lite 没用的 `embedding`、命名空间）。四个推理任务主要是改名。
- **阻塞项：现有声库包是 spec 2.3 形状，main 线读不了。** refactor 读 `contributes`，main 读
  `contributions`，模块声明里 `class` 变成了 `interface` + `variant`，`exports` 是 2.4 新增的一层。
  两条线不可能共存（都装 `lib/libsynthrt.so`），所以这不是能灰度的事。见《阻塞项》。

## 1. 边界清点

30 个文件 include 了 synthrt/diffsinger 头，其中真正触及符号的行：

| 文件 | 边界行 / 总行 |
| :-- | --: |
| `libs/SynthrtEngine/SynthrtEngine.cpp` | 81 / 733 |
| `libs/PackageManager/PackageManager.cpp` | 36 / 548 |
| `libs/SynthrtEngine/SynthrtEngine.h` | 24 / 235 |
| `app/Modules/Inference/Tasks/InferAcousticTask.cpp` | 18 / 470 |
| `app/Modules/Inference/InferEngine.cpp` | 17 / 412 |
| `app/Modules/Inference/Tasks/InferTaskCommon.h` | 14 / 103 |
| `app/Modules/Inference/Tasks/GetPronunciationTask.cpp` | 12 / 278 |
| 其余 23 个文件 | 各 ≤ 6 |
| **合计** | **282 / 6366** |

`SynthrtEngine` 的外部门面只被 16 个文件调用，且调用点集中在 `session()`（5 处）与若干一次性的
生命周期查询。换掉门面内部不会牵动上层。

## 2. 按 API 面分类

### A. 纯改名（1 天）

| refactor | main | 处数 |
| :-- | :-- | --: |
| `srt::core::Expected` | `srt::Expected` | 20 |
| `srt::core::Error` | `srt::Error` | 12 |
| `srt::core::DisplayText` | `srt::DisplayText` | 3 |
| `srt::core::TaskInitArgs` | `srt::TaskInitArgs` | 2 |
| `srt::core::ITensor` | `ds::ITensor` | 4 |
| `srt::driver::InferenceDriver/Session` | `ds::InferenceDriver/Session` | 5 |
| `srt::svs::Api::*::L1` | `ds::Api::*::L1` | ~10 |
| `srt::svs::SingerSpec/SingerCategory` | `srt::SingerSpec/SingerCategory` | 2 |
| `GT_None` / `GT_Up` / `GT_Down` | `GlideType::None` 等 | 3 |
| include 路径 | | ~45 |

### B. `srt::core::NO` → 标准智能指针（1–2 天）

36 处。refactor 的 `NO<T>` 是侵入式引用计数；main 用 `std::shared_ptr`（张量、会话）与
`std::unique_ptr`（执行体）。每处要判断归属，但成簇出现。

### C. 错误码（0.5 天）

lite 把 `srt::core::ErrorCode::{InferenceNotInitialized, SvsSingerNotFound, Extract*}` 映射到自己的
枚举。main 的错误码是一小组通用值（`InvalidFormat`、`FileNotFound`、`FeatureNotSupported`、
`InvalidArgument`、`NotImplemented` 等），**没有这些具体码**。

lite 需要自己做一次分类映射。注意 `Expected::error()` 在成功时是未定义行为（读的是 union 的另一个
成员），迁移时顺手把这类写法查一遍。

### D. Runtime → SynthUnit（1–2 天）

`srt::core::Runtime`(12)、`srt::core::PluginFactory`(4)、`OnnxSetup`、插件路径与驱动初始化。
落点是 `SynthrtEngine::initializeRuntime` / `initializeExtractors` / `initializeG2pOnnxDriver`
三个函数约 130 行，换成：

```cpp
srt::SynthUnit unit;
unit.setPackagePaths(voicebankPaths);
unit.setPluginPaths("inference", pluginRoot / "dsinfer/inferenceinterpreters");
unit.setPluginPaths("singer",    pluginRoot / "dsinfer/singerproviders");
unit.setPluginPaths(wolf::LINGUIST_CATEGORY,  pluginRoot / "wolf/linguistproviders");
unit.setPluginPaths(otter::ANALYSIS_CATEGORY, pluginRoot / "otter/analysisproviders");

// ONNX 驱动是 RuntimeService，全 unit 共享，模块不自己加载驱动
ds::InferenceDriverFactory factory;
factory.setPluginPaths(pluginRoot / "dsinfer/inferencedrivers");
auto driver = factory.create(factory.find("onnx")).take();
ds::Api::Onnx::DriverInitArgs args;
args.ep = ...; args.deviceIndex = ...; args.runtimePath = <宿主部署的 ORT 目录>;
driver->initialize(args);
unit.addRuntimeService(std::move(driver));
```

这一段已经在 wolf 与 otter 的测试里写过并跑通，形状是确定的。

### E. 声库层重写（5–8 天，最大的一块）

要替换的类型：`ds::bank::{PackageManifest, SingerManifest, InferenceInfo, SingerCapabilityReport,
SingerSnapshot, SingerRef}`、`ds::session::{VoicebankSession, VoicebankSnapshot, ModelSetHandle}`、
`ds::infer::StageKind`。

规模的硬证据：已构建的 `DsEditorLite` 二进制里未定义符号按命名空间数，
**`ds::bank` 31 个、`ds::session` 23 个、`ds::infer` 3 个** —— 这 57 个符号是这一块要重新落地的全部
接触点。

main 侧的对应物：`srt::PackageHandle` + `srt::SingerCategory/SingerSpec` +
`ds::Api::DiffSinger::L1::DiffSingerPipelineExecutive`。

需要 lite 新写的：

1. **声库扫描。** main 没有 scanner，lite 自己枚举目录并逐个 `openPackage`。约 50 行。
2. **快照。** lite 已有 `SingerInfo` / `ResolutionState` 镜像类型，改为从 `SingerSpec` 的
   `imports()` 与各 inference 的 `exports()` 填充。
3. **能力报告。** `SingerCapabilityReport` 在 main 上不存在，需从 singer 的 import role 集合加各
   目标的 `exports` 推导。这是新逻辑，不是改名。
4. **模型集。** `ModelSetHandle::load(kind)` → `DiffSingerPipelineExecutive::create{Duration,
   Pitch, Variance, Acoustic, Vocoder}(options)`。**main 这一层更高**：import options 由管线内部
   解析，lite 的 `ActiveInference::Model{inference, importOptions}` 会塌缩成「一个执行体」。
   `InferTaskCommon` 的缓存结构随之简化。

### F. 语言层重写（4–6 天）

`srt::g2p::*`（约 30 个符号）与 `srt::s2p::LanguageResource` → `wolf::LinguistSession`。

调用点其实很集中：`convertG2p`×3、`convertS2p`×1、`resolveLanguageRoute`、`languageService()`，
外加两个枚举界面（`G2pInfoWidget`、`G2pListWidget`）走 `srt::g2p::Manager::instance()` 这个全局单例。

**wolf 更简单**：`LinguistSession` 自带 catalog / probe / warm / release / CancelToken，refactor 那套
Stage1/Stage2、`deferLanguageModels`、`warmUpLanguageModels` 多半可以整块删掉。所以这一块是
**净减代码**，不是净增。

要注意的一处：wolf 没有全局 `Manager`，语言目录是 per-session 的，两个枚举界面要改成问 session。

### G. 抽参（2–3 天，L1 已完成）

见 otter 仓 `docs/lite-integration.md`。L1（切片器 + talcs 音频准备 + 测试）已落地并通过。
余下 L2–L6：引擎挂 analysis 类别、两个 Task 改写、设置界面下拉框、自动化适配器。

### H. 构建与依赖（2–3 天）

- 链接面从 `srt::synthrt` / `srt::diffsinger` / `srt::audio` / `srt::extract` / `dsinfer::dsinfer`
  变成 `synthrt::synthrt` + `wolf::wolf` + `otter::otter` + 手工定位的 dsinfer。
- **`dsinfer::dsinfer` 与 `srt::diffsinger` 重复**。`synthrtTargets.cmake` 里
  `srt::diffsinger` 的 `INTERFACE_LINK_LIBRARIES` 是
  `srt-g2p;srt-ds-bank;dsinfer::srt-ds-infer;srt::session`，已经含了 `dsinfer::srt-ds-infer`；
  而 `dsinfer::dsinfer` 只是同一个目标的一层 INTERFACE 别名。两者都链等于说了两遍。
  全仓也没有一个 `#include <dsinfer/...>`，所以 `find_package(dsinfer)` 连头文件路径都不需要。
  迁移前就可以摘掉，摘掉后编译不受影响。
- `srt::audio` / `srt::extract` 只被抽参用到，随 G 一起消失。
- port：`scripts/vcpkg-overrides/{synthrt,wolf}` 已有，需加 otter，三者都钉到各自仓的提交。

### I. 验证（3–5 天）

lite 现有 67 个测试，其中 **7 个在迁移前就已失败**（4 个 Not Run、2 个 GUI 子进程中止、1 个未构建），
迁移的验收基线应以此为准而不是「全绿」。

需要新增的：
- 变速曲下的音符位置（这是迁移要修的缺陷，没有这条用例等于没修）
- 语言转换的端到端（wolf 那边已有同形用例可参照）
- 包加载：一个 2.4 声库装得上、一个坏包报得出原因

## 3. 阻塞项：现有声库不是 spec 2.4 形状

这是唯一一个不能靠写代码解决的问题。

**证据。** refactor 的 `lib/Core/Core/Runtime.cpp:252` 读 `contributes`；main 的
`lib/Core/PackageLoader.cpp:1195` 读 `contributions`。lite 自己的集成 fixture
（`src/tests/resources/ci-fixture@1.0.0`）是这样的：

```json
{ "id": "ci-fixture", "version": "1.0.0",
  "contributes": { "inferences": ["inferences/acoustic/config.json"],
                   "singers": ["characters/fixture/config.json"] } }
```

而 2.4 要求：

```json
{ "$version": "1.0", "id": "vendor/name", "version": "1.0.0.0", "runtimeLevel": 1,
  "contributions": { "inference": [{ "id": "acoustic", "path": "./inferences/acoustic/inference.json" }],
                     "singer":    [{ "id": "fixture",  "path": "./singers/fixture/singer.json" }] } }
```

模块声明差得更多：2.3 写 `"class": "ai.svs.AcousticInference"` 且把 `id` 写在声明里；2.4 写
`"interface": "org.openvpi.dsinfer.inference.Acoustic"` + `"variant": "onnx"`，`id` 由 Package 赋予、
声明里不得出现，并且新增了 `exports` 这一层 —— 2.3 放在 `configuration` 里的「支持哪些 speaker、哪些
variance 参数」在 2.4 属于 `exports`。

**影响。** 用户已安装的声库在迁移后一个都装不上。两条线不能共存（同名 port、同名
`lib/libsynthrt.so`），所以没有灰度空间。

**选项：**

| | 做法 | 代价 |
| :-- | :-- | :-- |
| **A** | 写一个 2.3 → 2.4 转换脚本，随编辑器分发或在首次启动时就地升级 | 映射基本机械，但要为每份契约知道哪些键该从 `configuration` 挪到 `exports`。参照 wolf 的 `convert-g2p-packages.py`。约 3–5 天，且需要真实声库样本验证 |
| **B** | 请声库发布方重新打包 | lite 侧零成本，但时间不由我们控制，且历史声库不会有人回头重打 |
| **C** | 给 main 的加载器加一个 2.3 兼容读取层 | 与「synthrt 尽量不动」相悖，也违背 2.4 有意做的断裂。**不推荐** |

**建议 A + B 并行**：转换脚本保证历史声库可用，同时推动新发布直接出 2.4。这件事**必须在迁移开始前
定下来**，因为它决定迁移完成后编辑器还能不能打开用户已有的工程。

## 4. 工作量汇总

| 区块 | 估算 | 把握 |
| :-- | :-- | :-- |
| A 纯改名 | 1 天 | 高 |
| B `NO` → 智能指针 | 1–2 天 | 高 |
| C 错误码 | 0.5 天 | 高 |
| D Runtime → SynthUnit | 1–2 天 | 高（wolf/otter 已跑通同形代码） |
| E 声库层重写 | 5–8 天 | 中（能力报告的推导是新逻辑） |
| F 语言层重写 | 4–6 天 | 中（净减代码，但枚举界面要重接） |
| G 抽参 L2–L6 | 2–3 天 | 高（L1 已完成并验证） |
| H 构建与依赖 | 2–3 天 | 中（port 推送时机不由我们定） |
| I 验证 | 3–5 天 | 中 |
| **代码合计** | **20–31 天** | |
| **阻塞项：声库转换** | **3–5 天 + 外部协调** | 低（需真实声库样本） |

估算是单人连续投入的工作日，不含等待外部（模型打包、port 推送）的时间。

不确定性最大的两处：声库层的能力报告推导（E3），以及转换脚本要面对多少种真实的 2.3 变体（阻塞项）。
两者都可以靠**先拿到一份真实声库**大幅收敛 —— 建议在动工前先做这一步。

## 5. 实施顺序

每一阶段都要能编译、能跑既有测试，不留半截状态。

| 阶段 | 内容 | 可独立验证 |
| :-- | :-- | :-- |
| **M0** | 摘掉残留的 `dsinfer::dsinfer` 依赖；把 `Expected::error()` 的误用查一遍 | ✅ 现在就能做，不依赖任何迁移 |
| **M1** | 声库转换脚本 + 一份真实声库转成 2.4，用 synthrt main 的加载器装上 | ✅ 完全在 lite 之外，可与 M2 并行 |
| **M2** | 切换依赖：port 换成 synthrt-main + wolf + otter，改链接面。**此时 lite 编译不过**，是预期状态 | ❌ 这一阶段刻意允许红 |
| **M3** | A + B + C + D：改名、智能指针、错误码、Runtime → SynthUnit | ❌ 仍编译不过（E/F 未做） |
| **M4** | E 声库层。做完 lite 应当能启动、能列出声库、能合成 | ✅ 第一个能跑起来的点 |
| **M5** | F 语言层。G2P/S2P 恢复 | ✅ |
| **M6** | G 抽参 L2–L6 | ✅ 含变速曲用例 |
| **M7** | I 验证补齐、文档、清理逃生口 | ✅ |

M2–M3 之间 lite 编译不过是**不可避免**的：两条线不能共存，切换是原子的。所以这两段要连着做，不要
在中间停下。M0 与 M1 不受影响，**现在就可以开始**，而且 M1 的产出（真实声库样本）正好把 E 与阻塞项
的不确定性压下去。

## 5.5 进度

| 阶段 | 状态 | 验证方式 |
| :-- | :-- | :-- |
| **M0** | **完成** | 摘掉 `dsinfer::dsinfer` 后编辑器仍完整编译、链接，无任何 `srt::`/`ds::` 未定义符号。`Expected::error()` 的 49 个调用点逐个查过，**无误用**（13 个疑似里 9 个是 `QFile`/`SndfileHandle` 的同名方法，其余 4 个都在 `if (!x)` 或 `else` 分支里） |
| **M1** | **完成** | `scripts/convert-voicebank.py`；转换后的 fixture 被 synthrt main 的加载器装上，5 个推理 + 1 个歌手的 `exports`/`configuration` 由真实解释器解析通过，歌手五个 `singer/*` role 校验通过；同一 fixture 未转换时被拒（`Package manifest version is missing`）。回归用例 `TestVoicebankConversion` |
| **M2** | **完成** | `scripts/vcpkg-overrides/otter` 新增；三个包装进 `vcpkg/installed-main`，插件树齐备（dsinfer 7 个 + wolf 7 个 + otter 2 个），ORT 1.24.4 就位；链接面切到 `synthrt::synthrt` / `wolf::wolf` / `otter::otter` / `lite::dsinfer` |
| **M3** | **部分** | A（约 100 处改名）与 Logger 完成；E1–E4 以 `VoicebankCatalog` + `SingerPipeline`、D 以 `SynthrtBootstrap`、F 以 `LanguageBridge` 落地并验证。**未做**：B（`NO` 36 处）、C（11 个错误码）、G（抽参 L2–L6）、H（部署机制）、I，以及把这四个零件接进 `SynthrtEngine` 门面 |
| **M4–M7** | 未开始 | |

### 恢复点

在 `synthrt-main-migration` 分支上。当前树**编译不过**，这是 M2–M3 的预期状态。

已验证可用的两个基点：

- `vcpkg/installed-main/x64-linux` —— main 线的完整依赖树（synthrt + wolf + otter + dsinfer + ORT）。
  装它时 **环境里必须有 Qt**，否则 `qbreakpad` 会以
  `Could not find a package configuration file provided by "QT"` 失败：

```sh
CMAKE_PREFIX_PATH=<Qt> VCPKG_KEEP_ENV_VARS=CMAKE_PREFIX_PATH \
vcpkg install --x-manifest-root=scripts/vcpkg-manifest --x-install-root=vcpkg/installed-main
```
- `src/libs/SynthrtEngine/` 下四个新文件 —— 引擎的替代零件，都不依赖 lite 其余部分，可以绕开整棵树
  单独编译验证：

  | 文件 | 替代 | 验证 |
  | :-- | :-- | :-- |
  | `SynthrtBootstrap` | `Runtime` + `PluginFactory` + 两套驱动初始化 | 四个类别齐备、驱动注册成功 |
  | `VoicebankCatalog` | `VoicebankScanner` + 快照 + `SingerCapabilityReport` | 从转换包推导出 2 speakers / 3 controls / 3 predictions / 2 transitions |
  | `SingerPipeline` | `ModelSetHandle` | 五个阶段真实创建（打开了模型），重复取回同一个 |
  | `LanguageBridge` | `LanguageService` + G2P `Manager` + `LanguageRoute` + S2P 资源 | 中→zhong、国→guo，经跨包 import 走完整条链 |


```sh
T=$PWD/vcpkg/installed-main/x64-linux
g++ -std=c++17 -o /tmp/t src/tests/TestLanguageBridge/main.cpp \
    src/libs/SynthrtEngine/{VoicebankCatalog,SynthrtBootstrap,LanguageBridge}.cpp \
    -Isrc/libs/SynthrtEngine -I$T/include \
    -DTEST_PACKAGE_DIR='"<包所在目录>"' -DTEST_PLUGIN_ROOT='"'$T/lib'"' \
    -DTEST_ONNXRUNTIME_DIR='"'$T/share/onnxruntime-builds/runtime/default'"' \
    -Wl,--no-as-needed -L$T/lib -lsynthrt-wolf -lsynthrt-otter -lsynthrt -lsynthrt-dsinfer \
    -lstdcorelib -lstdcorelib-plugin -Wl,-rpath,$T/lib
```

`--no-as-needed` 不是可选的：没有它，链接器会丢掉没被直接引用的 otter，`analysis` 类别随之消失。
走 `otter::otter` / `wolf::wolf` 目标时该选项自动带上。

**接续时建议延用这个做法**：把每一块新逻辑先写成不依赖 lite 其余部分的文件，配一个单独编译的测试，
再接进引擎。在整棵树编译不过的窗口里，这是唯一能让每一行都被验证的方式 —— 直接改 `SynthrtEngine.cpp`
那样的大文件，写出来的东西一行也检查不了。

### 实施中新发现的

- **lite 的 app 在迁移之前就编译不过**（两处，均已单独提交修复，与迁移无关）：
  `SpeakerMixList.h` 漏了 `class QLabel;` 前置声明；`ProjectConverters` 只链了 `ICU::uc`，而
  `ucsdet_*` 在 `icui18n` 里。
- **2.3 已经有 `schema` 顶层键，正是 2.4 的 `exports`**，所以转换比预估直接。转换器不移动任何文件，
  只改内容，`configuration` 里的相对路径一条都不用重写。
- **`ExecutionProvider` 枚举名两条线不同**（`DMLExecutionProvider` vs `DML`），D 阶段要注意。
- **fixture 里的 `.onnx` 是真图**（674 字节的桩，但输入输出名与 DiffSinger 完全对得上），所以模型集
  那一层能真正打开模型来验证，而不是只验到声明。
- **语言层不需要那 14 MiB 的模型也能验**：wolf 自己的 `lang-cmn` + `pinyin-engine` fixture 走词典与
  cpp-pinyin 引擎，不碰模型，而 pinyin 解释器插件 port 是安装的。
- **wolf 会校验声库的语言标识与 linguist 声明的语言一致**（把 `cmn` 绑到 `zxx` 的 passthrough 会被
  拒），所以转换时的 `--language` 绑定不能随便配。
- **依赖解析走的是 `setPackagePaths`，不是扫描目录**。两者不是一回事，缺了前者时报的错说的是引用而
  不是搜索路径，离原因很远。

## 6. 迁移会顺带修掉的缺陷

- **变速曲的音符位置系统性偏移**（抽参走单一 tempo 换算 tick）。otter 输出绝对秒，lite 逐点换算。
- **音高帧间隔在 lite 里写死 10ms**，换算法就错。改为从模块声明读。
- **GAME 的对齐能力用不上**：权重里有，refactor 的封装把 `dur2bd` session 丢了。otter 已补回。
- **清浊标志名实相反**（`uv` 字段注释写「true=浊音」而算法按「true=清音」用）。otter 已改名并在出口
  处取反。

## 7. 迁移会失去的东西

- **结构化的包诊断。** refactor 有 `ValidationReport` / `PackageStatus` / `ResolutionState`；main 的
  `openPackage` 只返回一个 `Error`（带 cause 链，可 `rootCause()`）。lite 的包管理界面要么降级为显示
  一条错误信息，要么自己重建诊断。**建议先降级**，有人抱怨再补。
- **ffmpeg 解码。** 抽参改走 talcs 后覆盖面看似变窄，实际不窄：抽参只能看到工程里已导入的剪辑，那些
  文件本来就得先过 talcs 的 `FormatManager`。

## 8. 真实声库过全链路（yousa 1.65.1.0）

拿一个**没有为这条线写过**的已发布 2.3 声库（541 MB，5 个说话人、
cmn/eng/jpn 三语）转换后跑整条链，两边互审。转换后的包
不进任何仓库。

### 转换器补上的「语言那一半」

原先 `--language HANDLE=REFERENCE` 只能表达「绑到某个现成 linguist」，而真实声库需要的不是这个。
一个 linguist 由 g2p / s2p / onset 三段组成，三段来源**逐语言不同**，这不是偶然：

- 音素集对每个声库都一样的语言（eng 的 ARPAbet），整个 linguist 装在语言包里；
- 音素集是声库内容的语言（cmn / jpn），语言包只出 g2p，s2p 词典与 onset 规则由声库自带 ——
  2.3 的声明本来就为同一个理由指着同样两个文件。

所以 `--packages DIR` 读已装语言包，**逐语言按现场证据决策**：声库带了音素段就在声库里合成
linguist（只有 g2p 伸出包外），没带就绑语言包自己的 linguist，都没有就丢弃并报告。合成的声明指向
声库**原来就有的资源文件**，不复制不改写，路径按新声明位置重新表达。

**共享引擎不是任何语言的入口**。`wolf/g2p-multi` 声明九种语言、`wolf/g2p-pinyin` 声明两种，按声明
挑会让引擎一口气「服务」九种语言，把 cmn 接到 `algo-pinyin` 引擎而不是 `lang-cmn` 的链上。区分两者
的不是变体名也不是包名，而是**位置**：引擎是被另一个 G2P 包起来的那个，语言入口是最外层那个。这条
从已装包集里读得出来，所以是读的不是假定的。只算 G2P→G2P 的 import —— linguist 导入 G2P 是常态，
算进去会把每个已经自带 linguist 的语言包全部排除（第一版就是这么错的）。

`TestVoicebankLanguages` 自建三种形态的语言包与声库覆盖这三条规则；两次变异测试（去掉排除、把排除
放宽到所有 import）都被它抓住。

### 互审结果

`TestVoicebankAudit`（指 `LITE_AUDIT_VOICEBANK` 与 `LITE_AUDIT_LANGUAGE_PACKAGES`，没有就跳过）：

- 包加载，`capabilitiesOf` 读出 5 说话人 / 预测 breathiness·tension·voicing / 控制同三项 /
  过渡 gender·velocity / 三语言默认 cmn；
- 五个模型**全部真正打开**（duration / pitch / variance / acoustic / vocoder）；
- 三条语言路由全部可用，把声学模型的 `phonemes.json` 喂给 `LinguistSession::setSingerPhonemes`
  之后，cmn 64 / eng 42 开集 / jpn 39，除 `um` 外全部覆盖（见下）；
- 六个词端到端转换，`你好` → `ni` `hao`、`hello world` → `hh ax l ow` / `w er l d`、`こん` →
  `k o` / `N`，音素与 onset 齐全，且**产出的每个音素声学模型都有**；
- 十四个保留音素**从歌手声明里读出**（不是传进去的），逐个验证「不进 G2P、原样返回、单音素、
  `onset = true`」；
- 把真实包镜像一份（模型软链、声明复制）后篡改出一个模型没有的保留音素，**整包拒绝加载**，错误
  逐一点名 duration / pitch / variance / acoustic 四个模块。

### 保留标记：`um` / `EP` 这类不是缺陷，是设计允许声库自带的

第一版审计把 `um` 报成「声库对不上自己」—— **那是转换器的错，不是声库的**。
声库可以自带保留标记（换气、喉塞、哼鸣），它们与音节同住一本词典、各自映射到自己。
wolf 的域契约 §4 早已规定：**保留音素不进 `exports.phonemes`**，因为那是**内容音素清单**；
会话层在派发前就把标记拦下（`mode=copy`、单音素、`onset=true`），根本不进 G2P
（`linguist-session.md` §7、A61）。转换器把词典里所有音素一股脑塞进清单，于是 `um` 被当成
「声库必须会唱的音素」去比对，报出一个并不存在的缺口。

修好的做法是**不猜**。哪些符号是标记只有声库作者知道，所以由 `--reserved` 指定（缺省取 wolf
会话自己的 `SP,AP`），清单里剔除。转换器会**指出候选而不擅自处理**：一条「自我映射、且该音素不
出现在任何其他词条里」的词条就是标记的形状（真音节如 `a` 也自我映射，但 `a` 会出现在 `ma → m a`
里，标记的不会）。对 yousa 这条规则精确报出 cmn 的 `EP GS um`、jpn 的 `EP GS cl um`，
一个不多一个不少 —— 但它只是提示，`--reserved` 才是决定。

按 `--reserved SP,AP,EP,GS,um,cl` 重转后，三条语言的封闭清单**全部 100% 覆盖**（cmn 63 / eng 42
开集 / jpn 38），六个标记逐个走 `LanguageBridge` 验证「原样返回、单音素、onset=true」。

### `configuration.reservedPhonemes`：歌手自己说，加载器逐模型核

靠宿主猜、或靠「词典里长得像标记」去推，都不够 —— 前者要求带外约定，后者会把
`um` 这种**词典里有、模型里没有**的条目也认成标记。所以歌手直接声明：

```json
"configuration": { "reservedPhonemes": ["AP", "CL", "CR", "EP", "GS", "SP", "VF", …] }
```

**校验才是声明它的意义**。保留音素绕开转换路径，于是**加载期是它唯一会被检查的地方**：没有这条
校验，一个声明了模型所没有的记号的歌手照样加载、照样路由、照样合成，只在人写下那个记号的位置
产出静音，中间没有任何一层分得出来。所以每个声明的记号要在**所有已导入且带音素表的模块**
（Duration / Pitch / Variance / Acoustic）的 `phonemes` 里都能找到，缺一即整包加载失败，错误
指出哪个模块缺哪个记号。

四个模块**分别**核而不是只看 Acoustic：音素表是四个独立文件，四者不一致的歌手，其记号会在音符
时长上成立而在声音上不成立。

**转换器不再需要被告知**。DiffSinger 音素表把属于某语言的音素写作 `<language>/<phoneme>`、不属于
任何语言的写作它自己 —— 于是**一个裸键就是声库在自己文件里说「这个符号不是言语」**。转换器取四
张表裸键的交集，`--reserved` 只是补充，两者都要过同一道模型核验。对 yousa 得到
`AP CL CR EP GS SP VF` 及其小写形，一个不多一个不少。只有单语声库（表里没有任何前缀）无从推导，
那时只取 `--reserved`。

三处口径因此对齐：

| 位置 | 东西 |
| :-- | :-- |
| 声明 | `DiffSingerConfiguration::reservedPhonemes`，加载期逐模型校验 |
| 语言 | 域契约 §4：保留音素不进 `exports.phonemes` |
| 运行 | `LinguistSession::setReservedMarkers()`，派发前拦截，`mode=copy` |

**向后兼容**：新键，旧包语义不变，不读它的宿主行为与今天完全一致。

lite 侧本轮只补**接口直通**，不含任何编辑器策略（不读工程、不特判、不碰 UI）：
`SingerCapabilities::reservedPhonemes`（从歌手配置读出）、
`LanguageBridge::setReservedMarkers()` / `reservedMarkers()` 与 `SynthrtEngine` 上的同名两项。
`LinguistSession::refresh()` 不清这份设置，所以重扫声库后不必重设。
**facade 不自动接线**：wolf 的标记集是会话级的、歌手声明是逐歌手的，替宿主挑一个就是替它做决定。
**读取与特殊处理由编辑器开发实施。**

### `um`：既不是保留音素，也唱不出来

这是真实声库审出来的实例。`um` 在 cmn 与 jpn 两本词典里各占一行、映射到自己，长得完全是标记的
样子 —— 但**四个模型一个都没有它**。于是：

- **不能声明**为 `reservedPhonemes`，声明了整包就不加载（这正是校验想拦的）；
- **也不能从 `exports.phonemes` 里删掉**，删了就再没有任何一层会说它不可用；
- 于是它留在清单里，宿主报「这个音素你的模型唱不出来」—— 它就是这个意思。

转换器在转换时就点名：「`um` 长得像保留记号而没有任何模型有它，既非保留也唱不出，留在清单里好让
宿主说出来」。审计把这类结论记为 **finding 而不是 failure**：failure 是两边对工具决定的事情不
一致、要改代码；finding 是声库对自己的陈述不成立 —— 拿真实声库跑全链路的意义正在于此，为它失败
只会让人不再跑这个测试。

### 另外两件

- **声库自带的 `dictionary-en.txt` 在这条线上没人读**。英文 G2P 现在来自 `wolf/lang-eng` 的
  cmudict 链。转换器仍从它取英文音素清单（那是这个声库对「英文工作在哪套音素上」的自述），但那
  13 万条词典本身不再参与。
- **`um` 不在声库自己的 `phonemes.json` 里**（有 AP/CL/CR/ep/gs/sp/vf）。作为保留标记这不构成
  语言侧的缺口，但一旦编辑器真把 `um` 当音素送进声学模型，模型没有它。**标记落到模型上怎么处置
  是编辑器的事**，本轮不碰。


## 9. 全流程编译通过

`build/main` 用 `vcpkg/installed-main` 配置，**68/73 个测试通过，应用与全部库、工具、测试目标均编译链接成功**。

```sh
cmake -S . -B build/main -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_INSTALLED_DIR=$PWD/vcpkg/installed-main -DVCPKG_MANIFEST_MODE=OFF \
  -DVCPKG_TARGET_TRIPLET=x64-linux -DCMAKE_PREFIX_PATH=<Qt> \
  -DLITE_BUILD_TESTS=ON \
  -DLITE_AUDIT_VOICEBANK=<转换后的声库> \
  -DLITE_AUDIT_LANGUAGE_PACKAGES=<wolf>/build/lang-packages
```

`TestVoicebankAudit` 在构建树里跑通真实声库全链路。应用启动日志可见
`Initialized ONNX Runtime 1.24.4` / `Successfully initialized InferEngine` / `Package scan completed`。

### 部署形态（M2 的 H 阶段，已完成）

三个包各装各的插件树，部署保持这个形状而不拍平：

```
<exe>/../lib/plugins/dsinfer/{inferenceinterpreters,singerproviders,inferencedrivers}
<exe>/../lib/plugins/dsinfer/inferencedrivers/onnx/runtime/   ← ONNX Runtime
<exe>/../lib/wolf/plugins/{inferenceinterpreters,linguistproviders}
<exe>/../lib/otter/plugins/analysisproviders
```

`SynthrtEngine::defaultPluginRoot()` 指的是**这三棵树的共同父目录**，不是其中之一。原先它返回
`lib/plugins`，而 `Bootstrap` 还要再接 `plugins/dsinfer/...`，整整深了一层 —— 迁移前无人发现，
因为应用根本没编译过。

### 逐块落点

| 原来 | 现在 |
| :-- | :-- |
| `ds::session::ModelSetHandle` | `SingerPipeline` + `InferEngine::SingerPipelineLease` |
| `srt::core::NO<T>` | 裸指针 / `std::unique_ptr` / `std::shared_ptr`，按所有权分 |
| `srt::core::ErrorCode::*` | `srt::Error::*`；`result->error` 整个消失（失败已由 `Expected` 说了） |
| `VoicebankSnapshot` + `ds::bank::PackageManifest` | `SingerEntry` + `SingerCapabilities` |
| `ds::bank::PackageValidator` | 直接加载一次再放掉 |
| `srt::g2p::LanguageService` / `LanguageRoute` / `Manager` | `LanguageBridge`（`G2pConvertRunner`、`G2pInputAdapter` 删除） |
| `srt::extract::{Pitch,Midi}Extractor` | otter 的 `F0Executive` / `NoteExecutive` + `AudioSlicer` + `AnalysisAudio` |
| `srt::audio::AudioPipeline` | talcs（`ExtractorUtils` 删除） |

**抽参顺手修掉的缺陷**：原先取 `timeline.tempoAt(0)` 一个 tempo 套整段，变速曲上跑得越久偏得越多，
曲线最后落在音符旁边而不是音符上。现在逐点过 timeline 换算。

### 明知的两处降级

- **包校验不再分条**。加载器只返回一个 `Error`（带 cause 链），没有 severity/recommendation 列表。
  换来的是**更强的检查**（每个解释器、每个 import validator 都真跑），代价是报告只有一条。要恢复分条
  得让加载器收集而不是首错即返 —— 那是 synthrt 的改动，宿主重建不出来。
- **G2P 预设控件不再解析任何东西**。main 线没有可独立浏览的 G2P 模块注册表：语言是歌手导入的
  linguist。这两个控件保留形状、不再查找，真正该放这里的是「所选歌手的语言」，那是另一个控件。

### 抽参设置项待改（下一步，未做）

`ExtractTask::Input::modelPath` 现在被当作**分析器引用**（`<package>:analysis/<id>`）传给
`SynthrtEngine::createAnalyzer()`，但写入它的仍是 `options->general()->rmvpePath` /
`gameDir` 两个**文件路径**设置项，设置页也仍然让人选目录。**设置模型与那一页的 UI 需要改成从
`SynthrtEngine::analyzers(interface)` 里挑一个分析器**，否则抽参在运行期会以「分析器不可用」失败。
编译与链接不受影响。

### 剩下 5 个测试失败，均与本次迁移无关

| 测试 | 原因 |
| :-- | :-- |
| `TestOverlaySplitter`、`TestAnimationSettings` | CMake 里写死 `QT_QPA_PLATFORM=windows`，Linux 上必然起不来 |
| `TestDsConnectorLite`、`TestMcpProcessIntegration`、`TestHeadlessProcessIntegration` | 找不到 connector 可执行文件（`out/bin/DsConnectorLite` 是有的，测试找的路径不对） |

顺手修掉的**三处既有编译错误**（都早于本次迁移，与 main 线无关）：`UpstreamMcpClient` 的
`m_endpoint = {}` 在 Qt 6.11 下二义（`QUrl` 同时有 `QUrl` 和 `QString` 赋值）；`TestCascader`
的 `CascaderPopup` 只有 friend 声明没有前置声明；`TestAudioDecodingController` moc 了
`DocumentWorkflowController` 却没有它的析构函数。
