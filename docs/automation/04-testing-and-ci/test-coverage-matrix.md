# 四期产品行为与测试覆盖矩阵

## 1. 口径

本表按产品能力记录覆盖、缺口及历史用例去向。引用为 `src/tests/Test<名称>` 的程序目录；精确执行结果和行/分支覆盖率见[test-report.md](test-report.md)。矩阵不维护工具清单、Schema 镜像或断言数量指标。

## 2. 产品行为

| 功能域 / 关键行为 | 类别 | 现有覆盖与确认问题 | 本期处置 | 测试引用 | 运行条件 |
|---|---|---|---|---|---|
| 时间线、量化、曲线、锚点 | unit/domain | 已有数值与编辑回归，入口分散 | 按独立行为迁移，核对边界 | MusicTimeline、ParamResample、ParamSupport、CurveTrace、CurveTransform、AudioAnchor、AnchorEditController | 通用 |
| Speaker Mix、声音继承和推理输入 | unit/domain | 已有校验及转换，部分生产源重复编译 | 复用生产组件，保留独特语义 | SpeakerMix、SpeakerMixValidation、VoiceContext、InputConversion | 通用 |
| 歌词、音节、本地化、关联音素属性 | unit/domain/gui | 已有排序及属性级联回归 | 独立用例及必要数据驱动 | Syllabification、FillLyricTaggerOrder、WordPropertyCascade、LocalizedText、UiLanguage | 通用/offscreen |
| 配置、CLI、自有 ADT、缓存 | unit | Expected 仅展示结果；缓存/配置已有回归 | Expected 改真实断言；补控制端口类型/范围与权限持久化；缓存使用受控时间 | Expected、StartupArguments、InferenceOption、AutomationOption、SingerSessionCache、InferCache；ICU wrapper | 通用/平台 |
| 轨道、片段、音符、参数与历史 | domain | 巨型编辑测试共用入口，难定位和隔离 | 拆 fixture 和独立行为，检查功能缺口 | AutomationCore、AutomationEditingDomains、NoteTransfer | 通用 |
| 保存点与撤销分支 | domain/workflow | 原测试只检查redo清空，未验证丢弃已保存分支后的dirty | 补分支生命周期及地址复用用例；修复保存点引用已销毁条目 | DocumentWorkflow | 通用 |
| 钢琴窗剪贴板入口 | gui/domain | 仅有 payload 和 Facade 粘贴，未执行 ClipboardController | 补真实 copy/cut/paste：MIME、活动片段及播放位置、单步撤销、无效内容不修改工程；恢复原剪贴板 | PianoRollGuiIntegration、NoteTransfer | offscreen |
| 应用运行状态、设置、播放、包与规则 | domain/workflow | 服务替身和场景混在大入口 | 按状态所有者拆分 | AutomationRuntimeDomains、AutomationL3ApplicationDomains、AutomationOption | 通用 |
| 文档生命周期、文件转换与发布 | workflow | 已有保存/换代/原子写回，执行边界不统一 | 保留真实文件回归；补保存扩展名/Unicode/大小写与过期确认；独立 fixture | AutomationDocumentLifecycle、DocumentWorkflow、MidiImportAutomation、ProjectConverterAtomicWrite | 通用/GUI |
| DSPX 编辑内容往返 | workflow | 原文件测试多检查空工程、JSON头或对象存在，未核对编辑内容 | 补真实save/load的乐句、发音、参数、声线混合、tempo/meter保真 | ProjectConverterAtomicWrite | 通用 |
| 任务竞态、幂等及音频资产 | workflow/domain | 已有受控调度和晚到回调 | 提取复用、拆独立状态转换 | AutomationTaskRaces、AutomationIdempotency、AutomationAsyncFileDomains、AudioAssetResolution、AudioDecodingController | 通用 |
| 权限、路径、分页、准入 | protocol | 已有真实边界验证 | 保留实际拒绝和副作用断言 | AutomationFileGuard、AutomationAdmission、AutomationCursor | 通用/平台 |
| 公共接口及协议转换 | protocol | 数量和 Schema 镜像与行为测试混合 | 删除 Contract 镜像程序；真实无效输入归入 Registry 并检查无副作用；共享场景比较四种调用路径 | PublicAutomationRegistry、AutomationWire、McpHttpServer | 通用 |
| Connector 生命周期与 stdio | protocol/process | 长入口及手工子集分派 | 拆可定位用例，保留真实流行为 | DsConnectorLite | 通用 |
| Editor 启动、服务、单实例和退出 | process | Windows 数据根假设；Headless 混入 GUI 场景 | 跨平台沙箱，共用进程设施，分运行条件 | HeadlessProcessIntegration、McpProcessIntegration、SingleInstance | 通用/GUI/平台 |
| 钢琴窗、轨道编辑、快捷键和视口 | gui/domain | 既有几何/事件回归，部分仅测算法 | 新增实际钢琴窗鼠标/提交/撤销场景；快捷键建立真实可见owner与焦点；视口纯逻辑分离 | PianoRollGuiIntegration、PianoRollInteractions、PianoRollNoteCommit、TrackEditorInteractions、EditorShortcuts、EditorViewController、EditorViewportController、EdgeAutoScroll、UndoRedoController | 通用/offscreen/原生 |
| 布局、动画、主题和渲染 | gui/unit | Qt 平台有硬编码，混有实验 demo | 分隔条和菜单通过真实事件验证；动画直接调用生产组件；删除主题token镜像和固定几何数量 | OverlaySplitter、TwoLevelComboBox、ElasticAnimation、AnimationSettings、ThemeColors、ThemeIcons、EditorGlyphAtlas、EditorRhiGeometry、ScrollBarInterplay、PitchDisplayStrategy、WaveformRenderUtils | 通用/offscreen/原生 |
| 声库推理及音频导出装配 | workflow | 普通测试使用受控服务，未覆盖实际模型输出 | 新增显式资源用例；验证CPU推理后WAV能解码且有能量，不以未提供资源记通过 | HeadlessResources | 显式声库、语言、歌词；当前未执行 |

## 3. 旧演示入口

AnchoredCurve、NewStyle、OpenGLWidget、ParamEdit、InsertTable、StateMachine、Cascader 是无正式产品引用的实验实现，已删除。ElasticAnimation 保留名称，改测生产 ElasticAnimator 的收敛、目标替换与信号。SingerMenuDisplay 合并到 TwoLevelComboBox，保留实际菜单选择与显示检查。

## 4. 覆盖率驱动的补测

Linux 实际行/分支采样正在建设，尚无可引用数字。采样后按未执行的产品行为补充本表，不按百分比强行补边界。无窗口视口与 GUI 滚动条分开注册，HTTP/Connector 和进程长场景继续拆分，以便每个独立行为有可定位的结果。
