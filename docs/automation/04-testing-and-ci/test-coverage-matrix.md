# 四期产品行为与测试覆盖矩阵

## 1. 口径

本表按产品能力记录已知覆盖与缺口。不是工具清单或断言数量指标；运行结果见[test-report.md](test-report.md)。旧测试的去向随实现更新，以下为实施基线。

## 2. 产品行为

| 功能域 / 关键行为 | 类别 | 现有覆盖与确认问题 | 本期处置 | 测试引用 | 运行条件 |
|---|---|---|---|---|---|
| 时间线、量化、曲线、锚点 | unit/domain | 已有数值与编辑回归，入口分散 | 按独立行为迁移，核对边界 | MusicTimeline、ParamResample、CurveTrace、CurveTransform、AudioAnchor | 通用 |
| Speaker Mix、声音继承和推理输入 | unit/domain | 已有校验及转换，部分生产源重复编译 | 复用生产组件，保留独特语义 | SpeakerMix、SpeakerMixValidation、VoiceContext、InputConversion | 通用 |
| 歌词、音节、本地化、关联音素属性 | unit/domain | 已有排序及属性级联回归 | 独立用例及必要数据驱动 | Syllabification、FillLyricTaggerOrder、WordPropertyCascade、LocalizedText | 通用 |
| 配置、CLI、自有 ADT、缓存 | unit | Expected 仅展示结果；缓存/配置已有回归 | 补真实断言，复用受控时间 | Expected、StartupArguments、InferenceOption、SingerSessionCache、InferCache | 通用/平台 |
| 轨道、片段、音符、参数与历史 | domain | 巨型编辑测试共用入口，难定位和隔离 | 拆 fixture 和独立行为，检查功能缺口 | AutomationCore、AutomationEditingDomains、NoteTransfer | 通用 |
| 应用运行状态、设置、播放、包与规则 | domain/workflow | 服务替身和场景混在大入口 | 按状态所有者拆分 | AutomationRuntimeDomains、AutomationL3ApplicationDomains、AutomationOption | 通用 |
| 文档生命周期、文件转换与发布 | workflow | 已有保存/换代/原子写回，执行边界不统一 | 保留真实文件回归，统一任务验证 | AutomationDocumentLifecycle、DocumentWorkflow、MidiImportAutomation、ProjectConverterAtomicWrite | 通用/GUI |
| 任务竞态、幂等及音频资产 | workflow/domain | 已有受控调度和晚到回调 | 提取复用、拆独立状态转换 | AutomationTaskRaces、AutomationIdempotency、AutomationAsyncFileDomains、AudioAssetResolution、AudioDecodingController | 通用 |
| 权限、路径、分页、准入 | protocol | 已有真实边界验证 | 保留实际拒绝和副作用断言 | AutomationFileGuard、AutomationAdmission、AutomationCursor | 通用/平台 |
| 公共接口及协议转换 | protocol | 数量和 Schema 镜像与行为测试混合 | 删除镜像，保留代表路由和错误场景 | PublicAutomationRegistry、PublicAutomationContract、AutomationWire、McpHttpServer | 通用 |
| Connector 生命周期与 stdio | protocol/process | 长入口及手工子集分派 | 拆可定位用例，保留真实流行为 | DsConnectorLite | 通用 |
| Editor 启动、服务、单实例和退出 | process | Windows 数据根假设；Headless 混入 GUI 场景 | 跨平台沙箱，共用进程设施，分运行条件 | HeadlessProcessIntegration、McpProcessIntegration、SingleInstance | 通用/GUI/平台 |
| 钢琴窗、轨道编辑、快捷键和视口 | gui/domain | 既有几何/事件回归，部分仅测算法 | 区分算法与真实输入路径，补实际交互缺口 | PianoRollInteractions、PianoRollNoteCommit、TrackEditorInteractions、EditorShortcuts、EditorViewController、UndoRedoController | offscreen/原生 |
| 布局、动画、主题和渲染 | gui/unit | Qt 平台有硬编码，混有实验 demo | 适配运行环境，保留正式组件覆盖 | OverlaySplitter、AnimationSettings、ThemeColors、ThemeIcons、EditorGlyphAtlas、EditorRhiGeometry | offscreen/原生 |

## 3. 旧演示入口

AnchoredCurve、NewStyle、OpenGLWidget、ParamEdit、ElasticAnimation、InsertTable、StateMachine、Cascader 按产品引用及正式能力覆盖逐项处置。只有确认没有独特产品验证价值的实验实现才删除；结果随实施回填。
