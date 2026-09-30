# 对接层以外需要的改动

> 状态：待实施。本文记录 2026-09-29 四仓联合审计与 09-29 联测中，修复落在对接层以外的条目。对接层内的修复已随各自的提交落地；按用户要求，本文条目只记录、不修改代码，每项给出位置、问题与建议修法。行号以撰写时的 `HEAD` 为准，改动前须按符号名重新定位。

对接层的范围：`src/libs/SynthrtEngine`、`src/libs/PackageManager`、`src/app/Modules/Inference`、`src/app/Modules/Extractors`、`src/app/Automation` 中与抽参、推理、导出、声库相关的适配器、`src/app/Model/AppOptions` 中与推理和分析器相关的选项、`cmake/LiteBuildApi.cmake`、`scripts/`、打包脚本、对接层测试与 `docs/`。以下条目均在该范围之外。

## 1. 保留音素

对接层已改为按「lite 强制保留（`SP`、`AP`）∪ 歌手声明的 `reservedPhonemes`」判定（`src/libs/SynthrtEngine/ReservedPhonemes.h`、`SynthrtEngine::reservedPhonemesOf()`、`src/app/Modules/Inference/Utils/ReservedPhonemes.h`）。工程模型与编辑器仍只按 `SP`/`AP` 两个字面量判定：

| 位置 | 现状 | 后果 |
| :-- | :-- | :-- |
| `src/libs/ProjectModel/AppModel/Note.cpp` `Note::canEditPhonemes()`（约 195 行） | `m_lyric != "AP" && m_lyric != "SP"` | 歌词为声库保留音素（如 `br`）的音符仍可编辑音素，而推理侧按休止处理该音符 |
| `src/libs/ProjectModel/SingingClipSlicer/SingingClipSlicer.cpp`（约 57 行） | `lyric == "AP" \|\| lyric == "SP"` 判定休止 | 切片按两字面量分段，与推理输入的休止判定不一致 |
| `src/app/Model/AppModel/SingingClipPhonemeNormalizer.cpp`（约 22 行） | 同上 | 音素偏移归一化对声库保留音素按普通音节处理 |

建议：
1. `SingerInfo`（`src/libs/ProjectModel/Voice/SingerInfo.h`）增加 `reservedPhonemes()`，由 `PackageManager` 在构造 `SingerInfo` 时从 `SingerCapabilities::reservedPhonemes` 填入（该步骤属于对接层，届时一并修改）。
2. 工程模型提供一个函数 `isReservedLyric(const QString &lyric, const SingerInfo &singer)`，返回强制集（直接包含 `lite/SynthrtEngine/ReservedPhonemes.h` 中的 `FORCED_RESERVED_PHONEMES`；该头文件无依赖）与歌手声明集的并集；上述三处改为调用该函数。`Note` 不持有歌手信息，因此 `canEditPhonemes()` 须由调用方（持有 clip 的一侧）传入集合，或改为 `SingingClip` 的方法。

## 2. 零长音符与区间树

09-29 联测中的 `std::invalid_argument: Low border is not lower or equal to high border.` 来自 `lib_interval_tree::interval` 的构造函数。复核结果：

- `src/libs/ADT/OverlappableSerialList.h`（`Interval` 构造，约 16-19 行）已把上界钳到 `max(start, end - 1)`，零长区间不会在这里抛出。
- **talcs 的 `IClipSeries`**（`talcs/TalcsCore/private/IClipSeries_p.h` 的 `ClipInterval`）以 `position + length - 1` 为上界且不钳位，`length <= 0` 时抛出。lite 向它写入长度的两处：
  - `src/app/Modules/Audio/TrackInferenceHandler.cpp` `syncInferPiecePosition()`：`setLength(end - start)`，`end` 与 `start` 来自 `InferPiece::localEndTick()/localStartTick()`，可相等；
  - `src/app/Modules/Audio/TrackSynthesizer.cpp` `handleNotePropertyChanged()`：`setLength(note->length())`，零长音符直接写入。
  崩溃发生在声学结果写回之后，与第一处吻合。
- `OverlappableSerialList::remove()`（约 131-144 行）按条目**当前**的区间查找该条目；若条目在树中期间区间被修改（先改长度后移除），查找失败，随后的 `erase(end())` 是未定义行为。

对接层已在抽参入口过滤零长音符（`ExtractMidiTask`、`ExtractionAutomationFacade::completeMidiTask`）。建议：
1. 上述两处在写入 talcs 前跳过或钳位 `length <= 0` 的长度（钳到 1 tick，或不插入该片段），并记录日志。
2. `Note::setLength()`（`src/libs/ProjectModel/AppModel/Note.cpp`，只有 `Q_ASSERT(length >= 0)`）在 Release 下拒绝或钳位 0；音符编辑、粘贴、MIDI 导入与 `notes.*` 自动化入口统一校验 `length > 0`。
3. `OverlappableSerialList` 在插入时记录条目的区间，移除时按记录的区间查找；查找失败时报错，不再调用 `erase(end())`。
4. 不修改 interval-tree 本身（第三方端口）。

## 3. 默认语言与语言回写

对接层已让 MIDI 抽参在请求未给语言时使用音符分析器声明的默认语言，并把分析器实际使用的语言写入新建的音符、clip 与轨（`ExtractionAutomationAdapter.cpp` 的 `prepareMidi`）；`extract.get_capabilities` 的 `languages` 改为配置的音符分析器声明的语言。工程模型侧仍有三条互不一致的回退链：

| 位置 | 现状 |
| :-- | :-- |
| `src/libs/ProjectModel/AppModel/SingingClip.cpp` `defaultLanguage()` / `effectiveDefaultLanguage()`（约 214-240 行） | 空值时跟随轨，否则返回哨兵值 `"unknown"`；`effectiveDefaultLanguage()` 另在 clip 显式值与歌手包默认值之间选择 |
| `src/libs/ProjectModel/AppModel/AppModel.cpp`（约 195-210 行，载入工程时） | 轨与 clip 的语言为空或 `"unknown"` 时写入应用默认语言，覆盖了「跟随歌手包默认值」的语义 |
| `"unknown"` 哨兵值 | `SingingClip.cpp`、`AppModel.cpp`、`Modules/FillLyric/*`（`G2pService.cpp`、`LyricTab.cpp`、`TextTagger.cpp`、`LyricWrapView.cpp`）、`PublicAutomationRegistry.cpp` 约 10 处字面量 |
| `src/app/Model/AppOptions/Options/GeneralOption.h`、`src/app/Global/AppGlobal.h` | 语言表硬编码为 `cmn/eng/jpn/yue`，默认歌词硬编码为 `啦/la/ら`；可用语言应来自已安装的 linguist |

建议：定义常量 `kUnknownLanguage` 替换哨兵值；将「音符 → clip → 歌手包默认值 → 轨 → 应用默认值」的回退链实现为一个函数，由 `SingingClip` 提供，`AppModel` 载入时不再写入应用默认值；语言下拉与默认歌词表改为读 `SynthrtEngine::languagesOf()` 与语言包声明。

## 4. 参数曲线 5 tick 网格

对接层已统一为常量 `kParamCurveStepTicks`（`src/app/Modules/Inference/Models/InferParamCurve.h`）。以下位置仍硬编码 5：

| 位置 | 字面量 |
| :-- | :-- |
| `src/libs/ProjectModel/AppModel/DrawCurve.h`（约 21 行） | `int step = 5;`，即网格的实际来源 |
| `src/libs/ProjectModel/Utils/AppModelUtils.cpp` `getResultCurve(tickRange, …)`（约 204-208 行） | `MathUtils::round(…, 5)`、`i += 5` |
| `src/app/Automation/ParameterAutomationFacade.cpp`（约 1111 行） | `(first + 4) / 5 * 5` |

建议：在工程模型定义 `DrawCurve::kDefaultStepTicks`（或 `ParamInfo` 旁的常量），`DrawCurve::step` 的默认值与上表各处引用它；`kParamCurveStepTicks` 改为等于该常量，届时删除 `InferParamCurve.h` 中的独立定义。

## 5. 遗留的 G2P 标识管道

wolf linguist 按语言句柄转换，G2P 标识已不存在。对接层已让 `voices.describe` 的 `g2p_id` 显式返回空串并在契约中注明弃用（字段保留以兼容 MCP 客户端）。仍在使用的遗留管道（约 20 个文件、120 处 `g2pId`/`G2pId`/`defaultDict`）：

- `src/libs/ProjectModel/Voice/SingerInfo.{h,cpp}`：`g2pId()`、`defaultDict`；
- `src/libs/ProjectModel/AppModel/SingingClip.{h,cpp}`：`defaultG2pId()`、`updateDefaultG2pId()`，且 `setDefaultLanguage()`（约 209-212 行）把 g2pId 的变化算进 `bumpInferenceRevision()`；
- `src/libs/ProjectModel/AppModel/Track.{h,cpp}`、`src/libs/Language/Models/SingerG2pIdentifier.h`；
- `src/libs/ProjectModel/Voice/LanguageInfo.h` 的 `g2p/dict/s2pMode/onsetMode/s2pFile/onsetFile/g2pPackageVersion` 字段；
- `src/app/UI/Controls/G2pListWidget.*`、`G2pInfoWidget.*`、`src/app/UI/Dialogs/Options/Pages/G2pPage.cpp`、`Modules/FillLyric/*` 中的 G2P 预设控件；
- `src/app/Automation/SettingsAutomationFacade.*` 的 `updateG2pLanguage`（`InferController.cpp` 在语言模块出错时调用该函数清空设置）；
- `src/libs/ProjectConverters/DspxProjectConverter.cpp` 读写 g2p 字段。

建议分两步实施。第一步，`SingingClip::setDefaultLanguage()` 不再比较 g2pId（g2pId 恒为空，该比较无意义但无害），并删除 `SingerInfo::g2pId/defaultDict` 与 `LanguageInfo` 的 G2P 字段；第二步，移除 G2P 预设控件与设置项，工程文件中的旧字段只读不写。MCP 的 `g2p_id` 字段在下一个协议版本删除。

## 6. 设置页显示的空路径与始终为缺省值的能力字段

| 位置 | 现状 | 建议 |
| :-- | :-- | :-- |
| `src/app/UI/Dialogs/Options/Pages/InferencePage.cpp`（约 475 行）显示 `InferEngine::configPath()` | `InferEnginePaths::config` 从未赋值，页面恒显示空 | 删除该行，或改为显示语言包目录（`SynthrtEngine::defaultLanguagePackagePath()`）；随后删除 `InferEnginePaths::config` 与 `InferEngine::configPath()` |
| `src/libs/ProjectModel/Voice/SingerInfo.cpp`（约 250-253 行，歌手提示）读取 `SingerCapabilitySummary` 的一致性等级 | `PackageManager::summaryOf()` 不填写 `vocoderPitchControllable`、`effectivePhonemes` 与两个一致性等级（主线不提供这些信息），提示中始终显示缺省值 | 从 `SingerCapabilitySummary` 删除这些字段，提示改为显示真实可得的信息（说话人、语言、可预测参数） |

## 7. 执行提供程序的其余字面量

对接层已将执行提供程序统一为一个枚举与唯一一处字符串拼写（`src/libs/SynthrtEngine/ExecutionBackend.h`，应用侧 `ExecutionProvider` 是它的别名）。以下位置仍使用字面量 `"CPU"/"DirectML"/"CUDA"`：

- `src/app/Automation/SettingsAutomationFacade.cpp`（约 134-136、355-357、501-503、1021 行）的校验与候选值；
- `src/libs/AutomationWire/PublicToolContract.cpp`（约 2863 行）的枚举；
- `src/app/UI/Dialogs/Options/Pages/InferencePage.cpp`（约 87、104、119、122、261 行）；
- `src/app/Model/AppOptions/Options/InferenceOption.{h,cpp}` 的默认值与 CUDA 判定。

建议：改用 `ExecutionProviderUtils::toString()/fromString()` 与 `availableInBuild()`；契约枚举由同一函数生成。CoreML 目前不在任何构建中提供，改动时继续不对外暴露。

## 8. 导出长度

09-29 联测第 6 项：`exports.audio` 的导出长度取工程长度，而不是所选轨的内容长度（`src/app/Modules/Audio/AudioExporter.cpp`）。建议：`source_mode` 为轨时以所选轨最后一个片段的结束为长度，或在导出配置中增加长度策略。

## 9. 对接层内已知而本轮未修改的问题

- `InferRetake`（`src/app/Modules/Inference/Models/GenericInferModel.h`）注释为帧下标，而 `convertInputParams()` 按秒传给 dsinfer；各任务写入 `retake.end = frames`，数值远大于片段秒数，效果等同于「整段重算」。改为秒需要确认 dsinfer 对超出片段范围的 `retake` 的处理，且会使全部推理缓存失效一次，因此本轮未修改。
- `H3` 的修复没有自动化测试：`HeadlessInferenceTask` 依赖真实的 `InferController` 单例，而现有 Automation 测试设施使用替身任务，无法覆盖该路径。可基于 `TestHeadlessProcessIntegration` 的无头进程与一个最小声库编写集成测试。
- 上游问题（不在 lite 中修复）：synthrt `ITask` 析构契约（审计 H1）、`SYNTHRT_DECLARE_AS_METHODS` 的无检查下转型。otter 的 `AnalysisTask::start()` 已在入口校验载荷的接口与版本，`createAnalyzer()` 改用 `dynamic_cast`（审计 H2，otter 已修）。对接层读取导出与配置时已先比较接口名再下转型（`VoicebankCatalog.cpp` 的 `configurationOf()`、`analyzersOf()`）。
