# 迁移到 synthrt main + wolf + otter

> 状态：进行中（迁移本体已合入；§7 列出仍开放的事项，对接层以外待改的部分见
> [`integration-pending-changes.md`](./integration-pending-changes.md)）。

lite 从 synthrt 的 refactor 线迁移到 main 线，并接入 wolf（语言域）与 otter（抽参域）。迁移本体已完成，
本文记录迁移后的状态：改动内容、构建方式、验证范围与剩余事项。

抽参域本身的设计见 otter 仓库的 `docs/otter-design.md`，与 lite 的对接见 otter 仓库的 `docs/lite-integration.md`。

## 1. 构建与运行

使用仓库自带的 preset。工具链与依赖树均位于仓库内，不使用系统上的 vcpkg：

```sh
cmake --preset debug        # 生成到 build/Debug
cmake --build --preset debug
```

preset 将 `CMAKE_TOOLCHAIN_FILE` 固定为 `${sourceDir}/vcpkg` 下的工具链，将 `VCPKG_INSTALLED_DIR` 固定为
`${sourceDir}/vcpkg/installed`，并关闭 manifest 模式。后两项由本分支新增（main 上的 preset 没有这两项，
依赖树由工具链自行定位）。该目录与安装路径、`AGENTS.md`、`packaging/windows/build-installer.ps1`
使用的是同一个依赖树，依赖只安装一份。

机器相关的路径（Qt、用于审计的声库与语言包、otter 夹具）写入
`CMakeUserPresets.json`，该文件不进入版本库：

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

**四个端口位于本仓库的 `scripts/vcpkg-ports/` 下**，manifest 将该目录排在共享 overlay 子模块
`scripts/vcpkg` 之前：`synthrt` 取自 `diffscope/synthrt` 的 `onnxruntime-builds-uptake` 分支，覆盖共享
overlay 中跟随 refactor 线的同名端口；`wolf` 与 `otter` 分别取自 `diffscope/wolf` 与 `diffscope/otter`
的集成分支；`wolf-lang-packages` 安装 wolf 发布的语言包数据（见 §2）。前三个端口按提交取源，不需要归档
哈希；具体提交以各端口的 `REF` 为准。三个仓库均已公开，安装时不需要凭据。

端口位于本仓库而非子模块，目的是使依赖只引用已推送的提交。子模块指向独立的 overlay 仓库；端口若写入该
仓库，必须先推送该仓库，干净克隆才能安装依赖，而未推送的 overlay 提交无法被他人安装。wolf 与 otter
各自的 `scripts/vcpkg-ports` 采用相同的做法。

otter 的模型包（`otter/rmvpe`、`otter/game`、`otter/hfa`）不由端口安装，由 otter 的 release
`models-v0.3.0.0` 提供。

**依赖树只有一个**：`vcpkg/installed`，与安装路径、打包脚本相同。本分支曾建立的第二个依赖树
（`installed-main`）已合并回该目录。共享 overlay 的 `synthrt` 端口在本仓库中不参与构建，该依赖树中也
没有 `srt-*` 布局，因此不需要第二个依赖树。两条线都安装 `lib/libsynthrt.so` 等同名产物，refactor 线的
旧依赖树会与新树产生路径冲突，应当删除。

构建环境的两项要求：

- **环境中必须有 Qt**，否则 `qbreakpad` 以 `Could not find a package configuration file provided by "QT"` 失败。
- 端口所固定的提交变更后，vcpkg 按新的端口内容重建；依赖的源码变更而端口不变时不重建，此时须删除整个
  依赖树后重新安装。

## 2. 部署形态

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
定义；configure 时若已安装的插件目录缺少头文件列出的类别目录，给出警告：

```
<exe>/plugins/dsinfer/{inferenceinterpreters,singerproviders,inferencedrivers}
<exe>/plugins/dsinfer/inferencedrivers/onnx/runtime/           ← ONNX Runtime
<exe>/plugins/wolf/{inferenceinterpreters,linguistproviders}
<exe>/plugins/otter/inferenceinterpreters
<exe>/wolf/packages/                                           ← 语言包
```

`SynthrtEngine::defaultPluginRoot()` 返回**包含 `plugins/` 的目录**，而不是 `plugins/` 目录本身。每个库的
CMake 包都导出 `<LIB>_PLUGINS_DIR`（`DSINFER_PLUGINS_DIR` / `WOLF_PLUGINS_DIR` / `OTTER_PLUGINS_DIR`）。

**wolf 语言包是数据，不是编译产物**：`wolf-lang-packages` 端口将 wolf 的发行归档（`lang-v<bundle 版本>`，
版本以端口的 `vcpkg.json` 为准）解压到 `share/wolf/packages` 并导出 `WOLF_LANG_PACKAGES_DIR`。本仓库在
`scripts/vcpkg-ports/wolf-lang-packages` 中保存该端口的副本（来自 wolf 仓库的同名目录，更换发行时须整体
重新复制），manifest 请求全部 13 门语言与 zxx，因此干净检出安装的是发布件。`LITE_WOLF_LANG_PACKAGES`
（或环境变量 `WOLF_LANG_PACKAGES_SOURCE`）可覆盖该路径；端口未安装时，只有开启
`LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK`（默认关闭）才回退到同级检出的 `../wolf/build/lang-packages`，
以免 CI 或打包机误用相邻检出中的开发产物。`LITE_WOLF_LANG_PACKAGES` 是缓存变量，旧构建目录中已缓存的
路径必须清除，构建才会改用端口。`LITE_INSTALL=ON` 的构建目录（两个打包脚本使用的 preset）解析不到语言包
时 configure 直接失败，开发构建目录只给出警告。语言包按 `*/desc.json` 逐个复制，不复制其父目录：该目录
是构建目录，包旁边还有同名 zip 与 `verify/` 下每个包的第二份解包副本，整体复制会使两个身份相同的包同时
出现在依赖解析路径中。

**插件依赖的共享库必须一并部署**（`scripts/deploy_linux_plugin_deps.sh`，在 rpath 归一化**之前**运行）。
vcpkg 在 Linux 上不提供 applocal 部署，即使提供也只部署可执行文件链接的库；插件在运行时按名称加载，
依赖可执行文件本身不链接的库。若不部署这些库，rpath 归一化会**移除原本有效的 rpath**，插件随后报告
缺少某个共享库，而不是报告插件加载失败。

## 3. 模块对应关系

| 原来（refactor 线） | 现在 |
| :-- | :-- |
| `srt::core::NO<T>` | 裸指针 / `std::unique_ptr` / `std::shared_ptr`，按所有权区分 |
| `srt::core::ErrorCode::*` | `srt::Error::*`；`result->error` 已删除（失败已由 `Expected` 表示） |
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
模型，这是此处唯一占用大量内存的资源）；lease 记录取用时的 catalog generation，因此声库重新扫描后，
任务能够检测到所持指针已失效，而不是继续使用该指针。

**抽参设置项由路径改为引用**，原因有二：分析器是已安装包的贡献，包被安装到其他位置后路径即失效；校验也
从「文件是否存在」变为「该引用指向的分析器是否已安装、是否实现所需契约」。旧线允许将音高抽参指向音符
模型，直到实际运行才暴露错误。旧键不迁移，因为路径无法表示其所属的包与所实现的契约。

## 4. 顺带修复的缺陷

- **变速曲的音符与曲线系统性偏移**：旧线以 `timeline.tempoAt(0)` 的单一 tempo 换算整段，时间越靠后偏差
  越大。现在逐点经 timeline 换算。
- **清浊标志含义与名称相反**（`uv` 的注释为「true=浊音」，算法按「true=清音」使用）。otter 已改名并在
  出口处取反。
- **GAME 的对齐能力不可用**：权重中包含该能力，refactor 线的封装丢弃了 `dur2bd` session。otter 已恢复。
- 三处既有编译错误（均早于本次迁移）：`SpeakerMixList.h` 缺少 `class QLabel;`；`ProjectConverters` 只链接
  `ICU::uc`，而 `ucsdet_*` 位于 `icui18n`；`UpstreamMcpClient` 的 `m_endpoint = {}` 在 Qt 6.11 下有二义性。

## 5. 已知降级

- **包校验结果不再分条**。加载器只返回一个 `Error`（带 cause 链），不提供 severity/recommendation 列表。
  换取的是**更强的检查**（每个解释器、每个 import validator 都实际运行），代价是报告只有一条。恢复分条
  报告需要加载器收集全部错误而不是在首个错误处返回；这是 synthrt 侧的改动，宿主无法自行重建。
- **G2P 预设控件不再解析任何内容**。main 线没有可独立浏览的 G2P 模块注册表：语言由歌手导入的 linguist
  提供。该控件保留外观，不再执行查找；此处应显示的是「所选歌手的语言」，需要另一个控件实现。

## 6. 验证范围

构建零错误（应用及全部库、工具、测试目标）。测试数与通过数以 `ctest` 实测为准，此处不复制。

全链路已**逐层实际运行**（而非仅核对声明），使用真实声库 yousa 1.65.1.0 与真实 GAME 模型：

| 层 | 结果 |
| :-- | :-- |
| 包加载 | 1 个包，0 个失败 |
| 目录 → 编辑器模型 | 1 歌手 / 5 说话人 / 3 语言 / 默认 cmn；音名 `C1..B7` → MIDI `24..107` |
| 语言链 | `你好` → `ni`/`hao` → `n i h ao` + onsets |
| 时长推理 | linguistic.onnx + dur.onnx 实际运行，4 个时长之和为 1.000s |
| 声学推理 | model.onnx 实际运行，mel 11008 = 86 帧 × 128 melChannels |
| 声码器 | vocoder.onnx 实际运行，44032 样本 = 86 × 512 hopSize = 0.998s @44.1k |
| 分析器发现与创建 | F0 与 Note 各一 |
| F0 推理 | 1 秒音频 → 100 帧 @ 0.01s |
| Note 推理 | A3/C4/E4 三音 → MIDI 57/60/64，边界位于整秒 |
| 部署目录 | 以 `out/lib` 而非 vcpkg 依赖树重新运行，全链路结果相同 |

`TestVoicebankAudit` 是该链路的回归测试，读取 `LITE_AUDIT_VOICEBANK` / `LITE_AUDIT_LANGUAGE_PACKAGES`，
未设置时跳过；已发布的声库体积过大，不进入仓库。`TestOtterExtraction` 使用 otter 构建生成的合成夹具
（`LITE_OTTER_FIXTURES` 指向含 `fixture-rmvpe` 与 `fixture-note` 的目录）实际运行 `ExtractPitchTask` 与
`ExtractMidiTask`，断言结果在工程时间轴上的位置（素材原点、裁剪后的可见区、clip 局部 tick）与取消路径；
缺少夹具、缺少已部署的 ONNX Runtime、或夹具与所链接的 otter 版本不兼容时跳过。

## 7. 剩余事项

| # | 事项 |
| :-- | :-- |
| 1 | ~~RMVPE 尚未打包~~ 已完成：`otter/rmvpe` 与 `otter/game` 按 otter 现行的声明形状（契约事实在 `exports`，模型接线在 `configuration`）编写，并已用真实模型实际运行；二者与 `otter/hfa` 一同由 otter 的 release `models-v0.3.0.0` 发布 |
| 2 | ~~`ExtractPitchTask` / `ExtractMidiTask` 未经端到端运行~~ 已由 `TestOtterExtraction` 用 otter 夹具实际运行（时间轴换算与取消路径）；设置页的两个下拉框尚未经界面实测 |
| 3 | ~~`TestHeadlessProcessIntegration` 自重启用例失败~~ 已修复：退出与重启请求的回复在 MCP 服务器关闭前发送完毕，宿主忽略 SIGPIPE，单实例协调器识别持有进程已终止的陈旧锁并对正在退出的持有进程进行有界等待，后继进程先等待前一进程退出 |
| 4 | dsinfer 不再手工定位：override 端口保留了 synthrt 安装的 `lib/cmake/dsinfer`，`cmake/LiteDsinfer.cmake` 只调用 `find_package(dsinfer CONFIG)` |
| 5 | **`um` 是声库自身的缺口**：yousa 的 cmn/jpn 两本词典都声明了该音素，而四个模型均不包含。该音素不能声明为 `reservedPhonemes`（整个包会被拒绝加载），也不能从 `exports.phonemes` 删除（删除后不再有任何声明表明该音素不可用）。需由声库作者决定 |

## 8. 经验总结

**对基于 dlopen 的插件体系，「链接通过」与「测试全部通过」都不足以证明其可部署。** 部署缺口只有在部署
目录中运行真实应用时才暴露：测试将 `pluginRoot` 指向 vcpkg 依赖树，所缺的库恰好位于插件旁边。

**由实现生成的 fixture 无法检验该实现。** otter 的 GAME fixture 按 provider 的假设声明了 `t: ["steps"]`
与 `[1]` 的旋钮，因此测试始终通过，而任何真实导出的模型都无法运行：真实模型的 `t` 是批次维，采样循环
位于模型外部。fixture 现已按真实导出的形状编写。
