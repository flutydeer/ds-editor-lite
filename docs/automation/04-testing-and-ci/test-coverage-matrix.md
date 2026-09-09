# 四期产品行为与测试覆盖矩阵

## 1. 口径

本表按产品能力记录覆盖、缺口及历史用例去向。引用为 `src/tests/Test<名称>` 的程序目录；本期验收结论见[test-report.md](test-report.md)，逐例结果和覆盖率数值查看测试产物。矩阵不维护工具清单、Schema 镜像或断言数量指标，不随每次提交刷新运行结果。

目标按组件职责、fixture 和进程运行条件划分；新增行为默认增加所属 suite 的 Qt Test slot 或数据行。源文件可按职责分开，但逐例发现和执行由同一次 `qExec` 完成，不以程序数量或文件数量作为建设成果。

## 2. 产品行为

| 功能域 / 关键行为 | 类别 | 现有覆盖与确认问题 | 本期处置 | 测试引用 | 运行条件 |
|---|---|---|---|---|---|
| 时间线、量化、曲线、锚点 | unit/domain | 已有数值与编辑回归，入口分散且保留旧布尔包装 | 迁为直接 Qt Test slot，值比较提供实际与期望值，保持严格相等和容差契约；辅助检查保留调用位置 | MusicTimeline、ParamResample、ParamSupport、CurveTrace、CurveTransform、AudioAnchor、AnchorEditController | 通用 |
| Speaker Mix、声音继承和推理输入 | unit/domain | 已有校验及转换，部分生产源重复编译并依赖空 stub；继承缺少实际轨道/片段行为 | SpeakerMixValidation 归入 InputConversion，共用 EditorInferenceCore 并删除空 stub；VoiceContext 使用真实 ProjectModel 验证继承、独立声线、切换及通知 | SpeakerMix、VoiceContext、InputConversion | 通用 |
| 歌词、音节、本地化、关联音素属性 | unit/domain/gui | 已有排序及属性级联回归；首次采样中 TextSplitter 未执行 | FillLyricTaggerOrder 归并改名为 LyricRules；增加混合文字保真、规则启用/优先级及空匹配回归，修复空匹配重复文字 | Syllabification、LyricRules、WordPropertyCascade、LocalizedText、UiLanguage | 通用/offscreen |
| 配置、CLI、自有 ADT、缓存 | unit | Expected 仅展示结果；缓存/配置已有回归 | Expected 改真实断言；补控制端口类型/范围与权限持久化；缓存使用受控时间 | Expected、StartupArguments、InferenceOption、AutomationOption、SingerSessionCache、InferCache；ICU wrapper | 通用/平台 |
| 轨道、片段、音符、参数与历史 | domain | 巨型编辑测试共用入口，难定位和隔离 | 拆 fixture 和独立行为，检查功能缺口 | AutomationCore、AutomationEditingDomains、NoteTransfer | 通用 |
| 保存点与撤销分支 | domain/workflow | 原测试只检查 redo 清空，未验证丢弃已保存分支后的 dirty | 补分支生命周期及地址复用用例；修复保存点引用已销毁条目 | DocumentWorkflow | 通用 |
| 钢琴窗剪贴板入口 | gui/domain | 仅有 payload 和 Facade 粘贴，未执行 ClipboardController | 补真实 copy/cut/paste：MIME、活动片段及播放位置、单步撤销、无效内容不修改工程；恢复原剪贴板 | ApplicationGui、NoteTransfer | offscreen |
| 参数曲线与轨道片段的真实编辑接线 | gui | 数值和 Facade 用例未进入 CommonParamEditorView 与 TracksGraphicsView 的实际鼠标提交路径 | 在共用 ApplicationGui 中补一种代表性参数的绘制、取消及撤销重做，补实际轨道视图中的片段选择、拖动预览、提交或取消及一次撤销；检查界面、模型和文档副作用 | ApplicationGui::parameterStrokeCommitsOnceAndUndoRestoresView、escapeCancelsParameterStrokeWithoutChangingDocument、trackClipDragCommitsOrCancels | offscreen；无需声库/设备 |
| 整片段复制、跨轨粘贴与参数保真 | domain/gui | 首次采样中 ClipsInfo 未执行；序列化遗漏片段参数 | 复用音符剪贴板曲线编码，补全部参数层、无音符曲线、声线/发音保真和跨轨批量撤销；生产控制器复用同一粘贴准备逻辑；补真实 GUI 入口 | NoteTransfer、ApplicationGui | 通用/offscreen |
| 应用运行状态、设置、播放、包与规则 | domain/workflow | RuntimeDomains/L3ApplicationDomains 按历史阶段分开，设置及包 fixture 重复，歌词替身复制排序逻辑 | 合并到 AutomationApplicationDomains；Host、播放、编辑状态、设置、包、歌词规则及预设按源文件和独立行为组织；共用 Harness，规则使用明确 Host 快照和实际持久化断言 | AutomationApplicationDomains、AutomationOption | 通用 |
| GUI Host 无设备时的公开播放失败 | process/gui | Linux 实际 MCP 调用超时；无设备时错误弹窗阻塞调用 | 仅 TrustedGui 调用显示设备错误弹窗；公开调用检查失败、状态不变及后续查询；GUI fixture 关闭自身设备确定性验证无弹窗 | McpProcessIntegration、ApplicationGui | offscreen |
| 文档生命周期、文件转换与发布 | workflow | 已有保存/换代/原子写回，执行边界不统一 | 保留真实文件回归；补保存扩展名/Unicode/大小写与过期确认；独立 fixture | AutomationDocumentLifecycle、DocumentWorkflow、MidiImportAutomation、ProjectConverterAtomicWrite | 通用/GUI |
| 普通音频片段导入及 WAV 导出 | workflow/process | AudioExporter 此前只触达初始化代码，普通导出缺少真实执行 | 在现有进程目标补生成小 WAV、导入并等待任务完成、实际导出、解码检查采样率/声道/时长/有限非零内容及文档不变 | HeadlessProcessIntegration::audioImportAndWaveExport | 通用；无需声库/设备 |
| 音频导出配置与取消 | gui | 实际文件导出不能替代对话框事件接线验证 | 在 ApplicationGui 共用环境中增加导出配置源文件；通过真实输入检查格式/采样率与预览、轨道选择/混音与文件计划、取消及重新打开后的配置恢复 | ApplicationGui::exportFormatUpdatesFileNamePreview、exportSourcesAndMixingUpdateFilePlan、canceledExportConfigurationDoesNotPersist | offscreen；无需声库/设备 |
| 离线导出后的混音器状态恢复 | workflow | 实际 Linux 导出流程在恢复初始关闭的混音器时调用 open(0,0)，触发重采样比率断言 | 复用 Headless AppContext 测试目标，新增原先打开/关闭两行回归；按原 isOpen 恢复 open/close，已打开时保留原缓冲及采样率 | ApplicationWorkflows::offlineExportRestoresMixerState | 通用；fixture 关闭自身设备 |
| DSPX 编辑内容往返 | workflow | 原文件测试多检查空工程、JSON 头或对象存在，未核对编辑内容 | 补真实 save/load 的乐句、发音、参数、声线混合、tempo/meter 保真 | ProjectConverterAtomicWrite | 通用 |
| 任务竞态、幂等及音频资产 | workflow/domain | 已有受控调度和晚到回调 | 提取复用、拆独立状态转换 | AutomationTaskRaces、AutomationIdempotency、AutomationAsyncFileDomains、AudioAssetResolution、AudioDecodingController | 通用 |
| 推理结果与当前文档、输入及编辑会话匹配 | workflow | 首次采样中 InferenceApplyGate 未执行；输入转换测试不能代替完成门控 | 新增真实任务快照到门控的 Apply/Drop/Defer 行为，覆盖四阶段输入变化、文档/对象消失、无关 revision 变化和编辑冲突 | ApplicationWorkflows | 通用；无需模型输出 |
| 运行中重新推理与任务队列释放 | workflow | 真实声库执行暴露旧流水线销毁后完成回调消失，替换任务停留在队列 | 受控暂停真实 duration worker 后重启，检查替换任务终态及清理；先取消所属片段任务再销毁旧流水线，复用已有安全取消路径 | ApplicationWorkflows::restartInferenceReleasesReplacedTask、HeadlessResources | 受控回归通用；完整输出需声库 |
| 关闭自动推理后的手动完整推理 | workflow | 手动请求未携带声学许可，停止播放时停在 Acoustic.Awaiting | 为目标流水线保留本次请求许可，声学缓存探测与 variance 更新共用准入判断，Ready 或取消时清除；验证后台等待、手动放行及完成后恢复原策略 | ApplicationWorkflows、HeadlessResources | 受控状态验证通用；完整模型输出需声库 |
| 权限、路径、分页、准入 | protocol | 已有真实边界验证，分页游标独立目标与 Wire 职责重叠 | 保留实际拒绝和副作用断言，Cursor 用例归入 AutomationWire | AutomationFileGuard、AutomationAdmission、AutomationWire | 通用/平台 |
| 公共接口及协议转换 | protocol | 数量和 Schema 镜像与行为测试混合 | 删除 Contract 镜像程序；真实无效输入归入 Registry 并检查无副作用；共享场景比较四种调用路径 | PublicAutomationRegistry、AutomationWire、McpHttpServer | 通用 |
| Connector 生命周期与 stdio | protocol/process | 长入口及手工子集分派；可执行后缀和阻塞接收端依赖 Windows | 拆可定位用例，保留真实流行为；CMake 提供可执行路径，测试自身提供跨平台接收端；大帧验证不依赖工具总数 | DsConnectorLite | 通用 |
| Editor 启动、服务、单实例和退出 | process | Windows 数据根假设；Headless 混入 GUI 场景；Linux 替代进程未就绪且存活/所有权/清理实现缺失 | 跨平台沙箱，共用进程设施，分运行条件；重启输出改为沙箱文件，补跨平台替代进程管理 | HeadlessProcessIntegration、McpProcessIntegration、SingleInstance | 通用/GUI/平台 |
| 退出响应和有界停止 | protocol/process | 实际退出竞争暴露未发送响应被销毁；原停止测试仅覆盖空闲与处理超时 | 先排空响应再释放连接，全部连接共用截止时间；验证完整大响应和不读取客户端的有界停止，保留跨进程响应及退出断言 | McpHttpServer、HeadlessProcessIntegration、McpProcessIntegration | 通用/GUI |
| 钢琴窗、轨道编辑、快捷键和视口 | gui/domain | 既有几何/事件回归，部分仅测算法；首次采样显示已有音符交互不足；编辑视图与撤销控制分散 | 补实际钢琴窗绘制/提交/撤销、已有音符拖动提交与 Escape 取消；快捷键建立真实可见 owner 与焦点；EditorView/UndoRedo 合并为 EditorControllers，无窗口视口仍按职责分开 | ApplicationGui、PianoRollInteractions、PianoRollNoteCommit、TrackEditorInteractions、EditorShortcuts、EditorControllers、EditorViewportController、EdgeAutoScroll | 通用/offscreen/原生 |
| 布局、动画、主题和渲染 | gui/unit | Qt 平台有硬编码，混有实验 demo；颜色/图标目标共用主题职责 | 分隔条和菜单通过真实事件验证；动画直接调用生产组件；主题颜色/图标合并为 Theme，删除主题 token 镜像和固定几何数量 | OverlaySplitter、TwoLevelComboBox、ElasticAnimation、AnimationSettings、Theme、EditorGlyphAtlas、EditorRhiGeometry、ScrollBarInterplay、PitchDisplayStrategy、WaveformRenderUtils | 通用/offscreen/原生 |
| 声库推理及音频导出装配 | workflow | 普通测试使用受控服务，资源客户端曾偏离实际协议，并缺少异步分段准备条件 | 与常规 Headless 共用 Native 传输，使用显式语言与任务 scope，按模型目标就绪条件等待 G2P/分段；已用本机 Qixuan 声库实际完成 CPU 手动推理和 WAV 解码、有限非零样本检查 | HeadlessResources | 显式声库、语言、歌词；无需播放设备 |
| 实验 RHI 编辑后端 | gui | 几何与字形组件已有覆盖，完整 RHI 控件交互、渲染及失败回退未实际验证 | 区别于默认 Legacy 真实输入路径记录范围；本期保留基础组件测试，不扩展实验后端的设备或像素基线 | EditorRhiGeometry、EditorGlyphAtlas；完整后端无用例 | 显式启用实验选项；实际绘制依赖图形后端 |

## 3. 目标收敛与历史入口去向

| 原入口 | 归属 | 保留的职责及边界 |
|---|---|---|
| AutomationCursor | AutomationWire | 分页游标的编码、上下文和错误行为，与协议值类型共用 suite |
| ThemeColors、ThemeIcons | Theme | 主题颜色解析与图标行为使用同一 Qt 环境，各自独立用例 |
| EditorViewController、UndoRedoController | EditorControllers | 编辑视图状态与撤销控制使用共同控制器 fixture，保留独立行为断言 |
| AutomationRuntimeDomains、AutomationL3ApplicationDomains | AutomationApplicationDomains | 同一 CoreRuntime 的应用 Host 与设置/规则/包等服务，按职责分源文件，不保留历史阶段边界 |
| SpeakerMixValidation | InputConversion | 输入转换与混合声线输入校验复用 EditorInferenceCore，不再用空 stub 链接生产代码 |
| PianoRollGuiIntegration | ApplicationGui | 同一真实 GUI 环境内按钢琴窗、轨道、参数曲线和音频导出配置组织源文件，后续行为默认增加 slot/data 行 |

VoiceContext 保留组件目标，改为直接验证 ProjectModel 的真实继承行为；其对象生命周期和纯模型运行条件与推理输入转换不同。ApplicationWorkflows 共用 Headless AppContext，承接推理完成门控、受控重启及离线导出等工作流。需要不同窗口或资源条件的程序保持相应注册，不为减少目标数量强行合并。

## 4. 旧演示入口

AnchoredCurve、NewStyle、OpenGLWidget、ParamEdit、InsertTable、StateMachine、Cascader 是无正式产品引用的实验实现，已删除。ElasticAnimation 保留名称，改测生产 ElasticAnimator 的收敛、目标替换与信号。SingerMenuDisplay 合并到 TwoLevelComboBox，保留实际菜单选择与显示检查。

## 5. 覆盖率驱动的补测

GCC/gcovr 采样发现整片段剪贴板、推理完成门控、歌词拆分未执行，以及已有音符交互覆盖不足。统计口径与补测依据见[测试报告](test-report.md)；逐文件明细、分母和覆盖数值由每次 coverage 产物提供。

上述行为归入对应领域和共用 GUI 目标；CI 另外发现的退出响应竞争归入协议生命周期。实际执行结论见测试报告，原始覆盖明细留在产物。原生渲染、声库/设备及主观观感按运行条件记录，不为降低未执行行数扩展像素基线或模型矩阵。

使用独立 `coverage` preset 隔离完整插桩构建。Linux GCC/gcovr 提供行/分支统计；Windows MSVC 以 `/PROFILE` 和原生静态插桩采集，由 `scripts/tests/collect-msvc-coverage.py` 按源码文件与行号跨模块 OR 去重，提供行覆盖及逐文件明细，不使用采集格式中的占位分支值。

Linux CI、Windows 本地通用/原生测试和实际声库测试的运行条件分别说明；不同平台编译进来的源码、编译器插桩及资源集合会影响分母，不能直接以总百分比判断覆盖增减。以功能缺口和逐文件未执行路径决定补测，将本地资源执行带来的推理/合成/导出覆盖与无资源集合区分记录。
