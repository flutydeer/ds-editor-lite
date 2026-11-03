# synthrt、wolf 与 otter 接入指南

本文描述编辑器当前的模块分工与运行约束。
依赖范围见 [接入状态](../plans/synthrt-main-migration.md)，
声库声明要求见 [声库要点](../synthrt/packaging-voicebank-essentials.md)。

## 引擎分工

| 模块 | 职责 |
| --- | --- |
| `SynthrtBootstrap` | 注册贡献类别、插件搜索路径和 ONNX 驱动 |
| `VoicebankCatalog` | 扫描包，推导歌手、语言、声线和分析器能力 |
| `SingerPipeline` | 持有歌手包及各合成阶段的 executive |
| `LanguageBridge` | 将编辑器的语言操作映射到 wolf linguist 会话 |
| `SynthrtEngine` | 管理生命周期、目录快照、重载与宿主访问 |
| `InferStageModel` 与推理任务 | 组织阶段输入、模型调用和结果写回 |
| `ExtractTask`、`AnalysisAudio`、`AudioSlicer` | 准备分析音频、切片并调用 otter |

实现分别位于 [SynthrtEngine](../../src/libs/SynthrtEngine/)、
[Inference](../../src/app/Modules/Inference/) 和 [Extractors](../../src/app/Modules/Extractors/)。

## 包与歌手

包由 synthrt 解释，宿主根据已解析的贡献与导入关系提取能力，避免另读配置形成第二套判断。
声线对用户暴露歌手侧 ID，声学模型侧名称由导入映射解析。
未声明声线的模型可以使用默认声线，声明了声线却无法映射的情况需明确报告。

框架允许省略时长、音高或唱法参数阶段。编辑器当前要求五个合成阶段齐全，才将歌手标记为可合成。
任务持有的流水线保留包句柄，刷新不得销毁仍被任务使用的资源。

## 语言转换

| 深度 | 结果 |
| --- | --- |
| `Pronunciation` | 发音与候选 |
| `Phonemes` | 音素序列 |
| `Onsets` | 音素及等长的起音标记 |

可达深度由实际绑定的 linguist 组合决定。缺少 S2P 或 onset 层时，申请更深的结果不能
被当作完整音素或起音数据。宿主通过 `LanguageBridge::maxDepth()` 判断可用性，
不可用原因与语言可用状态分别保存。

用户锁定的音素与起音成对传递。单词转换失败保留原歌词，不能因一个词失败抹掉整批输入。
保留音素绕过普通 G2P，作为独立标记处理。模型未声明音素表与已声明表的交集为空是不同状态，
后者不能被解释为“覆盖率未知”。

## 刷新与缓存

目录刷新创建新的运行单元和快照。旧任务继续持有原有资源，新的任务从新目录取得流水线。
包身份包含版本，多个版本的同名歌手不能共用一个流水线。
扫描、语言刷新和返回数据准备完成后一起发布。准备过程抛出异常时返回错误并保留原有快照。

缓存键包含模型修订。读取缓存及交付延迟探测结果时复核目录代次，
同版本模型文件更新后也不能继续命中旧结果。并发刷新仅在路径与模式一致时合并。

## 音频分析与时间网格

音高、音符分析器是 `inference` 类别的包贡献。设置保存分析器引用，
界面从已安装包中提供选项，缺失的已选项保留原因提示。

分析音频必须完整解码，切片负责将模型局部时间换算回选区时间。
音高结果的非空清浊音数组需与频率数组等长，非空结果的帧间隔必须为正，时间和频率须为有限值。
音符结果在转为整数 tick 前检查有限性与范围，异常结果作为任务失败处理。

工程参数曲线的采样网格由
[InferParamCurve.h](../../src/app/Modules/Inference/Models/InferParamCurve.h)
中的 `kParamCurveStepTicks` 定义。推理输入、输出和提取音高写回应使用同一网格。

## 自动化与配置兼容性

| 接口或配置 | 当前契约与客户端处理 |
| --- | --- |
| 分析器设置 | 配置文件及内部 `GeneralSettingsDto` 使用 `noteAnalyzer`、`pitchAnalyzer`，值为 `包ID:inference/贡献ID`。旧 `gameDirectory`、`pitchModelPath` 路径不能直接作为分析器引用使用，需安装模型包并重新选择。这些字段不属于公开 `settings.query` 的字段集。 |
| 提取能力的 `models[]` | 按已安装分析器枚举，可能为空或包含多项。`id` 是上述贡献引用，不能硬编码为 `game` 或 `rmvpe`。传入 `options.model_id` 时须与当前配置的分析器相同，并检查能力与模型的 `available`。 |
| 歌手语言的 `g2p_id` | 保留兼容字段，但始终为空。客户端以歌手身份、`language_id` 和 `g2p_ready` 判断语言路由，不再从 G2P ID 推断可用性。 |
| 语言错误定位 | 语言选择错误使用 `language_id`，嵌套参数按实际字段路径定位。客户端应读取返回的错误字段，不再匹配旧的 `language` 名称。 |

`packages.refresh` 区分刷新过程失败与单个包加载失败。过程失败的任务终态为 `failed`。
扫描完成的任务为 `succeeded`，单包失败列在 `application_result.failures`，可同时存在成功加载的包。
这一区分在迁移前已经存在。客户端等待 `succeeded`、`failed` 或 `canceled` 任一终态，再检查结果中的
`failures`，不能以“是否存在坏包”预测任务终态。`packages.list` 也提供当前目录的失败列表。

## 插件与 ONNX Runtime 部署

[DeployLayout.h](../../src/libs/SynthrtEngine/DeployLayout.h) 是路径定义来源，
引擎、CMake 和 Windows 打包脚本共用它。

插件根在 Windows 为可执行文件目录，在 macOS 为应用包的 `Contents/PlugIns`，
在 Linux 为可执行文件旁的 `../lib`。其下包含：

- `plugins/dsinfer`：合成解释器、歌手 provider 和 ONNX 驱动。
- `plugins/wolf`：语言解释器与 linguist provider。
- `plugins/otter`：分析器解释器。
- `wolf/packages`：语言包及其依赖。

ONNX Runtime 位于驱动旁的 `plugins/dsinfer/inferencedrivers/onnx/runtime`。
CUDA 载荷使用其独立子目录，不能覆盖默认载荷。宿主将运行时路径明确交给驱动。

构建和安装按库替换插件树，避免已移除的插件残留。Linux 按当前依赖图复制传递依赖并清理已登记的旧库，
macOS 将依赖部署到应用包。语言包部署检查实际目录及安装规则，并替换版本不同的旧包。

Debug 和 RelWithDebInfo 安装保留依赖符号，其他配置去除符号。
三个 SDK 端口将 PDB 复制到普通 DLL 和插件 DLL 旁。多配置生成器依据本次构建或安装的配置选择符号。

## 错误与验证

找不到 ONNX 驱动时允许读取包元数据，但不能报告推理已经就绪。
语言、声库和分析器的失败原因需到达用户界面与自动化响应。
缺失资源不能被静默替换成另一位歌手或另一种语言。

回归入口和真实声库验收范围见 [验证指南](../synthrt/developer-repro.md)。
