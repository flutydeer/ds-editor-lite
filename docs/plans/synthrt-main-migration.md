# 迁移到 synthrt main + wolf + otter

lite 从 synthrt 的 refactor 线迁到 main 线，并接入 wolf（语言域）与 otter（抽参域）。**迁移已完成**，
本文是做完之后的记录：改了什么、怎么构建、验到了哪一步、还剩什么。

抽参域本身的设计在 otter 仓的 `docs/otter-design.md`，与 lite 的对接在 `docs/lite-integration.md`。

## 1. 怎么构建与运行

用仓库自带的 preset，工具链与依赖树都在仓库里，不碰系统上的 vcpkg：

```sh
cmake --preset debug        # 生成到 build/Debug
cmake --build --preset debug
```

preset 把 `CMAKE_TOOLCHAIN_FILE` 钉到 `${sourceDir}/vcpkg`，把 `VCPKG_INSTALLED_DIR` 钉到
`${sourceDir}/vcpkg/installed-main`——**这条是本分支加的**，没有它会回落到 `vcpkg/installed`，那棵树
没有 main 线的包。

机器相关的路径（Qt、wolf 语言包、用于审计的声库）放 `CMakeUserPresets.json`，它不进版本库：

```json
{ "version": 3,
  "configurePresets": [ { "name": "debug-local", "inherits": "debug", "cacheVariables": {
      "CMAKE_PREFIX_PATH": "<Qt>",
      "LITE_WOLF_LANG_PACKAGES": "<wolf>/build/lang-packages",
      "LITE_AUDIT_VOICEBANK": "<转换后的声库>",
      "LITE_AUDIT_LANGUAGE_PACKAGES": "<wolf>/build/lang-packages" } } ] }
```

依赖树（`vcpkg/installed-main`）由 vcpkg 装：

```sh
CMAKE_PREFIX_PATH=<Qt> VCPKG_KEEP_ENV_VARS=CMAKE_PREFIX_PATH \
./vcpkg/vcpkg install --x-manifest-root=scripts/vcpkg-manifest \
                      --x-install-root=vcpkg/installed-main
```

**三个 port 在 `scripts/vcpkg-overrides/` 里**，排在共享 overlay 之前：`synthrt` 钉在 `diffscope/synthrt`
的 `onnxruntime-builds-uptake` 分支，覆盖共享 overlay 里跟 refactor 线的同名端口；`wolf` 与 `otter`
分别钉在 `diffscope/wolf` 与 `diffscope/otter` 的集成分支。三者都按提交取源，不需要归档哈希。wolf 与
otter 是私有仓，安装时需要对 github.com 有读权限的 git 凭据。

**旧的构建目录不能复用**：两条线都装 `lib/libsynthrt.so`，不可能共存一棵已安装树。

三处容易踩的：

- **环境里必须有 Qt**，否则 `qbreakpad` 以 `Could not find a package configuration file provided by "QT"` 失败。
- 端口换钉提交后 vcpkg 会按新的端口内容重建；只改源码而不改端口时不会，那种情况要删掉整棵树重装。

## 2. 部署形态

三个包把插件装进同一棵树 `plugins/<库名>/<类别>`，部署保持这个形状：

```
<exe>/../lib/plugins/dsinfer/{inferenceinterpreters,singerproviders,inferencedrivers}
<exe>/../lib/plugins/dsinfer/inferencedrivers/onnx/runtime/   ← ONNX Runtime
<exe>/../lib/plugins/wolf/{inferenceinterpreters,linguistproviders}
<exe>/../lib/plugins/otter/analysisproviders
<exe>/../lib/wolf/packages/                                   ← 语言包
```

`SynthrtEngine::defaultPluginRoot()` 指的是**持有 `plugins/` 的那个目录**，不是树本身。每个库的
CMake 包都输出 `<LIB>_PLUGINS_DIR`（`DSINFER_PLUGINS_DIR` / `WOLF_PLUGINS_DIR` / `OTTER_PLUGINS_DIR`）。

**wolf 语言包由 `LITE_WOLF_LANG_PACKAGES`（或环境变量 `WOLF_LANG_PACKAGES_SOURCE`）指过来**，因为
wolf 的 port 不安装它们——它们由该仓脚本从资源生成，不是编译产物。按 `*/desc.json` 逐个拷贝，不拷
那个目录本身：那是个构建树，包旁边还有同名 zip 和 `verify/` 下每个包的第二份解包副本，整目录拷过去
等于把两个自称同一身份的包放进依赖解析路径。

**插件依赖的共享库必须一并部署**（`scripts/deploy_linux_plugin_deps.sh`，在 rpath 归一化**之前**跑）。
vcpkg 在 Linux 上没有 applocal 部署，且就算有也只部署可执行文件链的东西；插件是运行期按名字加载的，
链着可执行文件从不链的库。不部署它们时，rpath 归一化反而**把原本能用的 rpath 抹掉**，插件于是报
「某个库找不到」而不是「我加载不了」。

## 3. 各块落在哪

| 原来（refactor 线） | 现在 |
| :-- | :-- |
| `srt::core::NO<T>` | 裸指针 / `std::unique_ptr` / `std::shared_ptr`，按所有权分 |
| `srt::core::ErrorCode::*` | `srt::Error::*`；`result->error` 整个消失（失败已由 `Expected` 说了） |
| `srt::Runtime` + `PluginFactory` + 两套驱动初始化 | `SynthrtBootstrap` |
| `VoicebankScanner` + 快照 + `SingerCapabilityReport` | `VoicebankCatalog` |
| `ds::session::ModelSetHandle` | `SingerPipeline` + `InferEngine::SingerPipelineLease` |
| `srt::g2p::LanguageService` / `LanguageRoute` / `Manager` | `LanguageBridge`（`G2pConvertRunner`、`G2pInputAdapter` 已删） |
| `ds::bank::PackageManifest` / `SingerManifest` | `SingerEntry` + `SingerCapabilities` |
| `ds::bank::PackageValidator` | 直接加载一次再放掉 |
| `srt::extract::{Pitch,Midi}Extractor` | otter 的 `F0Executive` / `NoteExecutive` + `AudioSlicer` + `AnalysisAudio` |
| `srt::audio::AudioPipeline` | talcs（`ExtractorUtils` 已删） |
| `general.rmvpePath` / `general.gameDir`（文件路径） | `general.pitchAnalyzer` / `general.noteAnalyzer`（分析器引用） |

**`SingerPipelineLease`** 补的是裸指针给不了的两件事：最后一个 lease 释放即丢掉 pipeline（关掉五个
模型，这里唯一真占内存的东西），且 lease 记着取用时的 catalog generation——跨声库重扫后任务能知道
自己的指针已经没意义，而不是跟着它走。

**抽参设置项改成引用而非路径**：分析器是已安装包的贡献，路径的那天包被装到别处就失效；而且校验从
「文件存不存在」变成「这个引用指的分析器装没装、答不答这个契约」——旧线可以把音高抽参指向一个音符
模型，直到真跑起来才发现。旧键不迁移：一个路径说不出它来自哪个包、答哪个契约。

## 4. 顺带修掉的缺陷

- **变速曲的音符/曲线系统性偏移**：旧线取 `timeline.tempoAt(0)` 一个 tempo 套整段，跑得越久偏得越多。
  现在逐点过 timeline 换算。
- **清浊标志名实相反**（`uv` 注释写「true=浊音」而算法按「true=清音」用）。otter 已改名并在出口处取反。
- **GAME 的对齐能力用不上**：权重里有，refactor 的封装把 `dur2bd` session 丢了。otter 已补回。
- 三处既有编译错误（均早于本次迁移）：`SpeakerMixList.h` 缺 `class QLabel;`；`ProjectConverters` 只链
  `ICU::uc` 而 `ucsdet_*` 在 `icui18n`；`UpstreamMcpClient` 的 `m_endpoint = {}` 在 Qt 6.11 下二义。

## 5. 明知的两处降级

- **包校验不再分条**。加载器只返回一个 `Error`（带 cause 链），没有 severity/recommendation 列表。
  换来的是**更强的检查**（每个解释器、每个 import validator 都真跑），代价是报告只有一条。要恢复分条
  得让加载器收集而不是首错即返——那是 synthrt 的改动，宿主重建不出来。
- **G2P 预设控件不再解析任何东西**。main 线没有可独立浏览的 G2P 模块注册表：语言是歌手导入的
  linguist。控件保留形状不再查找；真正该放这里的是「所选歌手的语言」，那是另一个控件。

## 6. 验到哪一步

构建零错误（应用 + 全部库、工具、测试目标），**72/73 测试通过**。

全链路**逐层实跑**过一遍（不是读声明），用真实声库 yousa 1.65.1.0 与真实 GAME 模型：

| 层 | 结果 |
| :-- | :-- |
| 包加载 | 1 包 0 失败 |
| 目录 → 编辑器模型 | 1 歌手 / 5 说话人 / 3 语言 / 默认 cmn；音名 `C1..B7` → MIDI `24..107` |
| 语言链 | `你好` → `ni`/`hao` → `n i h ao` + onsets |
| 时长推理 | linguistic.onnx + dur.onnx 实跑，4 个时长和恰为 1.000s |
| 声学推理 | model.onnx 实跑，mel 11008 = 86 帧 × 128 melChannels |
| 声码器 | vocoder.onnx 实跑，44032 样本 = 86 × 512 hopSize = 0.998s @44.1k |
| 分析器发现/创建 | F0 与 Note 各一 |
| F0 推理 | 1 秒音频 → 100 帧 @ 0.01s |
| Note 推理 | A3/C4/E4 三音 → MIDI 57/60/64，边界落在秒上 |
| 部署树 | 用 `out/lib` 而非 vcpkg 树重跑，全链路照常通 |

`TestVoicebankAudit` 是这条链的回归测试，指 `LITE_AUDIT_VOICEBANK` / `LITE_AUDIT_LANGUAGE_PACKAGES`，
没有就跳过——已发布声库太大，不进仓库。

## 7. 剩下的

| # | 事项 |
| :-- | :-- |
| 1 | ~~RMVPE 尚未打包~~ 已打成 `openvpi/rmvpe`，与 `openvpi/game` 两个包都按 otter 现行的声明形状（契约事实在 `exports`，模型接线在 `configuration`）书写并用真实模型实跑过 |
| 2 | `ExtractPitchTask` / `ExtractMidiTask` 两个 Task 本身未端到端跑过（其下的分析器层已实跑）；设置页两个下拉框未实际显示过 |
| 3 | ~~`TestHeadlessProcessIntegration` 自重启一处失败~~ 已修：退出与重启请求的回复在 MCP 服务器关闭前排空，宿主忽略 SIGPIPE，单实例协调器识别持有者已死的陈旧锁并对正在退出的持有者有界等待，继任进程先等前任退出 |
| 4 | dsinfer 已不再手工定位：override 端口保留了 synthrt 安装的 `lib/cmake/dsinfer`，`cmake/LiteDsinfer.cmake` 只剩一句 `find_package(dsinfer CONFIG)` |
| 5 | **`um` 是声库自身的缺口**：yousa 的 cmn/jpn 两本词典都声明了它，而四个模型都没有。既不能声明为 `reservedPhonemes`（整包会拒载），也不能从 `exports.phonemes` 删掉（删了就再没人会说它不可用）。需声库作者决定 |

## 8. 两条值得记下的教训

**一套 dlopen 的插件体系，「链接通过」和「测试全绿」都不足以说明它装得起来。** 部署缺口只有在部署树里
跑真实应用才暴露：测试把 `pluginRoot` 指向 vcpkg 树，缺的库就躺在插件旁边。

**从实现生成的 fixture 等于让实现自己给自己打分。** otter 的 GAME fixture 按 provider 的假设声明了
`t: ["steps"]` 与 `[1]` 的旋钮，于是测试永远绿，而任何真实导出都跑不了——真实模型的 `t` 是批次维，
采样循环在模型外面。fixture 现在按真实导出的形状写。
