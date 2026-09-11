# 四期产品行为与测试覆盖矩阵

## 1. 口径

本表按产品能力记录覆盖、缺口及历史用例去向。测试引用采用程序名的简写，通常对应 `src/tests/Test<名称>`；两个 Provider 程序共用 `TestInferenceProvider` 目录，ICU 使用 `IcuWrapperTests`。本期验收结论见[test-report.md](test-report.md)，逐例结果和覆盖率数值查看测试产物。矩阵不维护工具清单、Schema 镜像或断言数量指标，不随每次提交刷新运行结果。

目标按组件职责、fixture 和进程运行条件划分；新增行为默认增加所属 suite 的 Qt Test slot 或数据行。源文件可按职责分开，但逐例发现和执行由同一次 `qExec` 完成，不以程序数量或文件数量作为建设成果。

用例实现为 `tst_<snake_case>.cpp`；多文件套件使用 `test_main.cpp` 入口及 `tst_<domain>.h` 声明，单文件套件可在 `tst_` 文件内保留 `QTEST_MAIN` 或自定义入口，fixture 辅助保留语义名称。完整职责边界见[大纲](test-outline.md#9-套件职责与程序划分)。本表的“通用”表示可在适用平台执行；各平台的实际执行结论见测试报告和产物，运行条件本身不代表测试通过。

## 2. 产品行为

| 功能域 / 关键行为 | 类别 | 现有覆盖与确认问题 | 本期处置 | 测试引用 | 运行条件 |
|---|---|---|---|---|---|
| 时间线、量化、曲线、锚点 | unit/domain | 已有数值与编辑回归，入口分散且保留旧布尔包装；领域曲线局部修改仍需验证保留范围 | 迁为直接 Qt Test slot，保持原严格相等和容差；补 overlay/replace 绘制、擦除及 no-op，检查局部样本、保留锚点、预览和撤销重做；跨越中间曲线的锚点合并拒绝且无副作用 | MusicTime、Parameters、ProjectEditing::drawAndErasePreserveOtherParameterCurves、nonAdjacentAnchorMergePreservesDocument | 通用 |
| Speaker Mix、声音继承和推理输入 | unit/domain | 已有校验及转换，部分生产源重复编译并依赖空 stub；继承缺少实际轨道/片段行为 | 统一归入 VoiceAndInference，共用 EditorInferenceCore 并删除空 stub；声线组件使用真实 ProjectModel 验证继承、独立声线、切换及通知 | VoiceAndInference | 通用 |
| 歌词、音节、本地化、关联音素属性 | unit/domain/gui | 已有排序及属性级联回归；首次采样中 TextSplitter 未执行 | 规则、音节与文字级联归入 Lyrics；增加混合文字保真、规则启用/优先级及空匹配回归，修复空匹配重复文字；本地化数据和语言设置分别归入基础/配置职责 | Lyrics、Foundation、Preferences | 通用/offscreen |
| 配置、CLI、自有 ADT、缓存 | unit | Expected 仅展示结果；缓存/配置已有回归 | Expected 改真实断言；补控制端口类型/范围与权限持久化；缓存使用受控时间 | Foundation、Bootstrap、InferenceProviderDefault、InferenceProviderCuda、Preferences、VoiceAndInference；IcuWrapperTests | 通用/平台 |
| 轨道、片段、音符、参数与历史 | domain | 巨型编辑测试共用入口，难定位和隔离 | 拆 fixture 和独立行为，检查功能缺口 | AutomationRuntime、ProjectEditing | 通用 |
| 保存点与撤销分支 | domain/workflow | 原测试只检查 redo 清空，未验证丢弃已保存分支后的 dirty | 补分支生命周期及地址复用用例；修复保存点引用已销毁条目 | DocumentIO | 通用 |
| 钢琴窗剪贴板入口 | gui/domain | 仅有 payload 和 Facade 粘贴，未执行 ClipboardController | 补真实 copy/cut/paste：MIME、活动片段及播放位置、单步撤销、无效内容不修改工程；恢复原剪贴板 | ApplicationGui、ProjectEditing | offscreen |
| 参数曲线与轨道片段的真实编辑接线 | gui | 数值和 Facade 用例未进入 CommonParamEditorView 与 TracksGraphicsView 的实际鼠标提交路径 | 补参数绘制、Shape/Scale 和 Legacy 音高调制，后者以真实推理基音验证选区结果；检查预览、范围外保留、一次提交、取消及撤销。轨道视图检查片段选择与拖动 | ApplicationGui 的 tst_parameter_editing.cpp、tst_pitch_modulation.cpp、tst_track_editing.cpp | offscreen；音高调制默认内置声库；无需设备 |
| 整片段复制、跨轨粘贴与参数保真 | domain/gui | 首次采样中 ClipsInfo 未执行；序列化遗漏片段参数 | 复用曲线编码及粘贴准备逻辑，补参数/声线/发音保真和批量撤销；轨道右键 Copy/Paste 经真实菜单检查悬停预览、Escape 清理、提交位置与预览一致及撤销 | ProjectEditing、ApplicationGui::wholeClipClipboardUsesSelectedTrackAndPreservesCurves、trackContextMenuPastePreviewCancelsAndMatchesCommittedClip | 通用/offscreen |
| 应用运行状态、设置、播放、包与规则 | domain/workflow | RuntimeDomains/L3ApplicationDomains 按历史阶段分开，设置及包 fixture 重复，歌词替身复制排序逻辑 | 合并到 ApplicationServices；Host、播放、编辑状态、设置、包、歌词规则及预设按源文件和独立行为组织；共用 Harness，规则使用明确 Host 快照和实际持久化断言 | ApplicationServices、Preferences | 通用 |
| GUI Host 无设备时的公开播放失败 | process/gui | Linux 实际 MCP 调用超时；无设备时错误弹窗阻塞调用 | 仅 TrustedGui 调用显示设备错误弹窗；公开调用检查失败、状态不变及后续查询；GUI fixture 关闭自身设备确定性验证无弹窗 | ProcessIntegration、ApplicationGui | offscreen |
| 文档生命周期、文件转换与发布 | workflow | 已有保存/换代/原子写回，执行边界不统一 | 保留真实文件回归；补保存扩展名/Unicode/大小写与过期确认；独立 fixture | DocumentIO | 通用/GUI |
| 新建前的保存决策 | workflow | 底层保存/换代不能验证 DocumentWorkflowController 的实际状态机接线 | 外部 UI 提供取消、放弃、取消路径、保存和失败后取消的回答；生产状态机与保存器检查 busy、工程/历史保留或替换、实际文件及最近列表 | ApplicationGui::newDocumentHonorsTheSaveDecision | offscreen 程序承载；属于文件工作流，不代表真实弹窗输入 |
| 普通音频片段导入及 WAV 导出 | workflow/process | AudioExporter 此前只触达初始化代码，普通导出缺少真实执行 | 在现有进程目标补生成小 WAV、导入并等待任务完成、实际导出、解码检查采样率/声道/时长/有限非零内容及文档不变 | ProcessIntegration::audioImportAndWaveExport | 通用；无需声库/设备 |
| 音频导出配置、进度与关闭 | gui | 实际文件导出不能替代配置及进度对话框的事件接线验证 | 通过真实输入检查格式/采样率与预览、轨道选择/混音与文件计划、取消及重开恢复；复用小型 WAV，点击 Export 经生产导出器检查完成提示、进度达到 100%、Close 关闭、任务清理和进度窗口释放 | ApplicationGui::exportFormatUpdatesFileNamePreview、exportSourcesAndMixingUpdateFilePlan、canceledExportConfigurationDoesNotPersist、audioExportProgressCompletesAndCloses | offscreen；无需声库/设备 |
| 外观设置即时保存与重开 | gui | 原生覆盖采样显示普通设置页缺少实际输入验证 | 通过侧栏进入页面，真实切换动画开关及键盘提交时长，检查运行时值、配置文件和重开后的控件；恢复用例原设置 | ApplicationGui::appearanceInputsPersistAcrossReopening | offscreen；隔离配置 |
| 声线混合来源、权重及确认/取消 | gui | 领域数值测试未进入声线混合对话框 | 真实标签移除来源并保留剩余比例，拖动权重分隔线；确认返回编辑结果，取消后重开恢复初始草稿，预设和文档不被意外修改 | ApplicationGui::speakerMixSelectionAndDrag | offscreen；仅需声线元数据 |
| 离线导出后的混音器状态恢复 | workflow | 实际 Linux 导出流程在恢复初始关闭的混音器时调用 open(0,0)，触发重采样比率断言 | 复用 Headless AppContext 测试目标，新增原先打开/关闭两行回归；按原 isOpen 恢复 open/close，已打开时保留原缓冲及采样率 | ApplicationWorkflows::offlineExportRestoresMixerState | 通用；fixture 关闭自身设备 |
| DSPX 编辑内容往返 | workflow | 原文件测试多检查空工程、JSON 头或对象存在，未核对编辑内容 | 补真实 save/load 的乐句、发音、参数、声线混合、tempo/meter 保真 | DocumentIO | 通用 |
| 任务竞态、幂等及异步服务 | workflow/domain | 已有受控调度和晚到回调，入口按历史功能拆散 | 运行时状态/准入/幂等归入 AutomationRuntime；文件、导出和提取服务的受控提交边界归入 ApplicationServices | AutomationRuntime、ApplicationServices | 通用 |
| 终态任务历史与后台许可释放 | workflow/domain | macOS 的移动后回调可能继续捕获许可，导致任务结束后新请求仍 Busy | 转移完成回调时显式清空源；在既有生命周期中验证终态历史可查询、回调资源已释放，并检查取消后的实际准入 | AutomationRuntime、AutomationProtocol | 通用；macOS 实际复现 |
| 推理初始化失败后的包扫描与退出 | workflow/process | Windows 无 GPU 早退未通知共享运行时，包任务等待会话并阻塞退出 | 复用已有 Provider/设备查询配置在独立子进程中制造真实失败，检查任务错误和正常析构；统一完成与关闭通知。普通进程 fixture 显式选择 CPU | ApplicationWorkflows::failedInferenceInitializationReleasesPackageWaiters、ProcessIntegration | 通用；失败复现不要求实际 GPU |
| 音频资产解析、来源换代和解码通知 | workflow | 路径/哈希与解码控制分散，解码测试替写 AudioContext、DocumentWorkflowController 等生产方法；公开路径更新缺少实际准备与提交接线 | 合并 AudioAssets 并复用真实运行时；公开确认/重定位经 Registry 和 Host 准备，检查路径、格式、哈希、真实解码、单次提交及撤销，损坏文件和提交前取消无副作用 | AudioAssets、ApplicationWorkflows::publicAudioPathUpdatesPrepareCommitAndUndo | 通用；小型 WAV；无需播放设备 |
| 推理结果与当前文档、输入及编辑会话匹配 | workflow | 首次采样中 InferenceApplyGate 未执行；输入转换测试不能代替完成门控 | 新增真实任务快照到门控的 Apply/Drop/Defer 行为，覆盖四阶段输入变化、文档/对象消失、无关 revision 变化和编辑冲突 | ApplicationWorkflows | 通用；无需模型输出 |
| 真实读音/音素完成后的暂存与应用 | workflow | 快照门控不能验证语言任务完成、pending 存储及 flush 接线 | 内置声库执行实际任务，编辑期间结果不修改工程，结束后应用且不新增撤销；文档换代或片段删除后丢弃待应用结果 | ApplicationWorkflows::clipInferenceResultsRespectEditSession | 默认内置声库；CPU；无需播放设备 |
| 运行中重新推理与任务队列释放 | workflow | 真实声库执行暴露旧流水线销毁后完成回调消失，替换任务停留在队列 | 受控暂停真实 duration worker 后重启，检查替换任务终态及清理；先取消所属片段任务再销毁旧流水线，复用已有安全取消路径 | ApplicationWorkflows::restartInferenceReleasesReplacedTask、ModelResources | 受控回归通用；完整输出需声库 |
| 关闭自动推理后的手动完整推理 | workflow | 手动请求未携带声学许可，停止播放时停在 Acoustic.Awaiting | 为目标流水线保留本次请求许可，声学缓存探测与 variance 更新共用准入判断，Ready 或取消时清除；验证后台等待、手动放行及完成后恢复原策略 | ApplicationWorkflows、ModelResources | 受控状态验证通用；完整模型输出需声库 |
| 公开推理状态与任务作用域关联 | workflow/protocol | 完整模型输出不能证明状态查询关联了正确的公开任务 | Registry/Host 发起真实任务并暂停 Pitch worker，检查当前及后续阶段关联同一任务，已完成阶段和其他片段不误关联；实际成功后清除活动任务关联 | ApplicationWorkflows::publicInferenceStatusAssociatesTasksWithTheirScope | 默认内置声库；CPU；无需播放设备 |
| 权限、路径、分页、准入 | protocol | 已有真实边界验证，分页游标独立目标与 Wire 职责重叠 | 保留实际拒绝和副作用断言；准入/文件授权归入 AutomationRuntime，Cursor/Wire 归入 AutomationProtocol | AutomationRuntime、AutomationProtocol | 通用/平台 |
| 公共接口及协议转换 | protocol | 数量和 Schema 镜像与行为测试混合 | 删除 Contract 镜像程序；真实无效输入归入 Registry 并检查无副作用；共享场景比较四种调用路径 | AutomationProtocol | 通用 |
| 公共参数查询范围与输出预算 | protocol | 完整快照不能验证有界查询和曲线数据保真 | 检查时间范围裁剪、绘制曲线降采样、锚点原样保留、点数预算不足拒绝及查询无副作用 | AutomationProtocol::parameterQueryBoundsSamplesAndPreservesAnchors | 通用 |
| GUI 编辑模式设置与查询 | protocol | 参数 Shape/Scale 和音高调制未接入公开转换，实际状态被回报成默认模式，设置请求被拒绝 | 补齐输入/输出模式与转换；既有接口场景验证到达服务的枚举、状态读回及工程版本不变 | AutomationProtocol::routing(guiBindings) | 通用 |
| Connector 生命周期与 stdio | protocol/process | 长入口及手工子集分派；可执行后缀和阻塞接收端依赖 Windows | 拆可定位用例，保留真实流行为；CMake 提供可执行路径，测试自身提供跨平台接收端；大帧验证不依赖工具总数 | Connector | 通用 |
| Connector 分页缓存与离线调用 | protocol | 缓存与本地调用混入无 SLA 的循环次数和速度门槛 | 检查查询不额外请求上游、快照变化引发刷新，以及删除/保留工具和离线状态的实际结果；保留协议自身截止时间 | Connector | 通用 |
| Editor 启动、服务、单实例和退出 | process | Windows 数据根假设；Headless 混入 GUI 场景；Linux 替代进程未就绪且存活/所有权/清理实现缺失 | 跨平台沙箱，共用进程设施，分运行条件；重启输出改为沙箱文件，补跨平台替代进程管理 | ProcessIntegration、Bootstrap | 通用/GUI/平台 |
| 退出响应和有界停止 | protocol/process | 实际退出竞争暴露未发送响应被销毁；原停止测试仅覆盖空闲与处理超时 | 先排空响应再释放连接，全部连接共用截止时间；验证完整大响应和不读取客户端的有界停止，保留跨进程响应及退出断言 | AutomationProtocol、ProcessIntegration | 通用/GUI |
| 钢琴窗、轨道编辑、快捷键和视口 | gui/domain | 既有几何/事件回归，部分仅测算法；首次采样显示已有音符交互不足；编辑视图与撤销控制分散 | 补实际钢琴窗绘制/提交/撤销、已有音符拖动提交与 Escape 取消；快捷键建立真实可见 owner 与焦点；控制器、视口、输入与滚动统一归入 EditorInteraction，生产应用完整接线留在 ApplicationGui | ApplicationGui、EditorInteraction、ProjectEditing | 通用/offscreen/原生 |
| 布局、动画、主题和渲染 | gui/unit | Qt 平台有硬编码，混有实验 demo；颜色/图标目标共用主题职责 | 分隔条和菜单通过真实事件验证；动画直接调用生产组件；主题与菜单归入 GuiComponents，原生布局归入 NativeDesktop，绘制组件归入 EditorRendering；删除主题 token 镜像和固定几何数量 | NativeDesktop、GuiComponents、EditorRendering、EditorInteraction、Parameters | 通用/offscreen/原生 |
| 声库推理及音频导出装配 | workflow | 普通测试使用受控服务，资源客户端曾偏离实际协议，并缺少异步分段准备条件 | 与常规 Headless 共用 Native 传输，使用明确语言与任务 scope，按模型目标就绪条件等待 G2P/分段；保留实际 CPU 手动推理和 WAV 解码、有限非零样本检查，资源运行结果由报告与产物记录 | ModelResources | 默认内置声库，也可显式配置；无需播放设备 |
| 实验 RHI 编辑后端 | gui | 几何与字形不能替代真实控件；实际析构暴露私有状态释放后的事件重入 | Null 后端检查音符绘制/移动/裁边/分割、连续擦除、内联文字和右键目标，音高绘制/描摹/擦除及锚点插入与取消，以及片段跨轨拖动和裁边；验证预览、一次提交/取消、命中、撤销重做及帧，保留正常析构 | EditorRendering、NativeDesktop 的 tst_rhi_editor.cpp、tst_rhi_tracks.cpp | 原生窗口；Linux Xvfb；无需物理 GPU；不验证像素 |
| 输出设备配置与实际播放 | workflow | 受控回调和无设备失败不能替代真实输出设备路径 | 自动探测或按名称指定设备，验证重开、缓冲配置落盘，静音素材的公开播放/暂停/停止及回调推进，恢复自身配置 | NativeDesktop::availableAudioDeviceRunsPublicPlayback | 无可用设备明确 QSKIP；已枚举或指定设备的失败为 FAIL |
| MIDI 输入至实时合成器 | workflow | 设备异常处理不等同于真实消息输入 | 使用已配置专用回环端口，检查生产设备选择、配置落盘、note-on/off 接收、有效 PCM 及释放后归零；清理自己打开的端口 | NativeDesktop::configuredMidiLoopbackFeedsLiveSynthesizer | 未配置双端口 QSKIP；部分配置或指定后的执行失败为 FAIL |

## 3. 目标收敛与历史入口去向

| 原入口 / 内容 | 最终归属 | 保留的职责及边界 |
|---|---|---|
| Expected、LocalizedText | Foundation | 自有基础类型与本地化文本断言 |
| MusicTimeline、AudioAnchor | MusicTime | 音乐时间与音频锚点的纯行为 |
| ParamResample、ParamSupport、CurveTrace、CurveTransform、AnchorEditController、PitchDisplayStrategy | Parameters | 曲线、重采样、锚点编辑和音高显示策略按组件分文件 |
| Syllabification、LyricRules（原 FillLyricTaggerOrder）、WordPropertyCascade | Lyrics | 歌词、规则和文字属性级联 |
| SpeakerMix、VoiceContext、InputConversion、SpeakerMixValidation、SingerSessionCache、InferCache | VoiceAndInference | 真实声线模型、输入与缓存共用生产实现，移除空 stub |
| AutomationOption、UiLanguage、通用 InferenceOption | Preferences | 配置与语言行为复用 EditorPreferencesCore |
| InferenceOption 的默认/CUDA 编译分支 | InferenceProviderDefault、InferenceProviderCuda | 同一套源码验证编译时 Provider 选择；不代表实际 GPU 推理 |
| ICU wrapper | IcuWrapperTests | 保留项目依赖的 ICU 包装验证 |
| AutomationCore、AutomationIdempotency、AutomationTaskRaces、AutomationAdmission、AutomationFileGuard | AutomationRuntime | 调度、任务、幂等、准入与文件授权；文件 fixture 每例独立 |
| AutomationEditingDomains、NoteTransfer、PianoRollNoteCommit | ProjectEditing | 核心编辑、数据转移和控件提交桥接；保留独立 slot 及数据行 |
| AutomationApplicationDomains（原 RuntimeDomains/L3ApplicationDomains）、AutomationAsyncFileDomains | ApplicationServices | Host、设置/播放/规则/包及异步服务按职责分文件，共用必要 Harness |
| DocumentWorkflow、AutomationDocumentLifecycle、ProjectConverterAtomicWrite、MidiImportAutomation | DocumentIO | 生命周期、历史、真实文件往返和发布；用例边界重置历史，删除空 AppContext specialization |
| AudioAssetResolution、AudioDecodingController | AudioAssets | 路径/哈希/来源与解码控制；后者改用真实 Headless AppContext，移除生产方法替写 |
| ApplicationWorkflows | ApplicationWorkflows | 保留共用 Headless 环境中的推理完成、重启、声学许可及离线导出回归 |
| AutomationWire（含 Cursor）、PublicAutomationRegistry、McpHttpServer | AutomationProtocol | Wire、注册与分派、HTTP 生命周期使用一个 suite，保持协议行为边界 |
| DsConnectorLite 测试 | Connector | 真实 Connector、连接与 stdio 行为 |
| StartupArguments、SingleInstance | Bootstrap | 参数与单实例身份、协议及平台行为 |
| HeadlessProcessIntegration、McpProcessIntegration、跨 Host 进程场景 | ProcessIntegration | 共用进程与数据隔离设施，按入口及平台条件保留行为 |
| HeadlessResources | ModelResources | 默认内置声库和 CPU 模型执行，可显式覆盖资源 |
| Theme（原 ThemeColors/ThemeIcons）、TwoLevelComboBox、ElasticAnimation | GuiComponents | 主题、菜单和组件动画；SingerMenuDisplay 的有效行为继续保留 |
| OverlaySplitter、AnimationSettings | NativeDesktop | 需要原生窗口系统的布局与动画设置 |
| EditorControllers（原 EditorViewController/UndoRedoController）、EditorViewportController、EdgeAutoScroll、PianoRollInteractions、TrackEditorInteractions、ScrollBarInterplay、EditorShortcuts | EditorInteraction | 控制器、视口、输入、焦点与滚动按组件分文件 |
| EditorGlyphAtlas、EditorRhiGeometry、WaveformRenderUtils | EditorRendering | 字形、几何及波形组件，保留实验后端范围说明 |
| ApplicationGui（原 PianoRollGuiIntegration） | ApplicationGui | 共用实际应用环境，保留钢琴窗、轨道、参数、剪贴板、导出配置并补外观设置与声线混合交互 |

这些归并将原测试主体变为同一 Qt Test 类的领域 slot；需要资源、原生桌面或编译宏变体的入口按实际条件独立。fixture 的共享不等于共享每例状态，文档、历史、临时文件、任务与信号连接必须在用例边界复位。

GuiComponents、NativeDesktop、EditorInteraction 和 EditorRendering 与应用共用生产 `EditorGuiCore`；配置、领域和推理分别复用 Preferences、Automation 和 Inference 组件，不再在各测试中重复编译相同生产源。完整应用接线继续由 ApplicationGui、AudioAssets 和 ApplicationWorkflows 的真实 Runtime 验证。

## 4. 旧演示入口

AnchoredCurve、NewStyle、OpenGLWidget、ParamEdit、InsertTable、StateMachine、Cascader 是无正式产品引用的实验实现，已删除。原 ElasticAnimation 的有效行为在 GuiComponents 中验证生产 ElasticAnimator 的收敛、目标替换与信号；SingerMenuDisplay 的实际菜单选择与显示检查也归入该套件。

## 5. 覆盖率驱动的补测

GCC/gcovr 采样发现整片段剪贴板、推理完成门控、歌词拆分未执行，以及已有音符交互覆盖不足。统计口径与补测依据见[测试报告](test-report.md)；逐文件明细、分母和覆盖数值由每次 coverage 产物提供。

上述行为归入对应领域和共用 GUI 目标；CI 另外发现的退出响应竞争归入协议生命周期。实际执行结论见测试报告，原始覆盖明细留在产物。原生渲染、声库/设备及主观观感按运行条件记录，不为降低未执行行数扩展像素基线或模型矩阵。

使用独立 `coverage` preset 隔离插桩构建。Linux CI 的独立 Coverage 步骤通过 GCC/gcovr 提供行/分支统计；Windows MSVC 原生静态插桩与 macOS LLVM 采集保留为按需本地入口。Windows/LLVM 共用生产源码过滤及逐行 OR 去重逻辑，提供行覆盖及逐文件明细，不使用 MSVC 采集格式中的占位分支值。

CI 使用 Linux x64、Windows x64、macOS arm64 的 `latest` runner，统一 Qt 6.11.2；各平台结果分别验收，全部适用测试均包含内置声库和 GUI。覆盖率仅由 Linux CI 生成，Windows/macOS 使用普通 Debug `tests` 构建和完整测试集合；此前取得的跨平台专项覆盖证据仍保留。不同平台编译进来的源码、编译器插桩及资源集合会影响分母，不能直接以总百分比判断覆盖增减。以功能缺口和逐文件未执行路径决定补测，外部真实声库执行的附加结果单独说明。

GCC/gcovr 启用 `merge-lines` 合并同一源码行的模板实例；既有数据重算属于口径修正，不计为新增命中。应测逻辑以接近或超过 90% 为方向，按行为和未覆盖行补齐，不通过排除整目录、Schema 镜像或无语义断言达标；条件用例未执行与尚未实现的测试分别记录。

## 6. 普通工作流与内置资源补测

以下补测承接覆盖率中发现的正常行为缺口，继续使用既有程序；实际通过结果与统计由测试报告及产物给出。

| 功能域 / 关键行为 | 类别 | 原有缺口与本期处置 | 测试引用 | 运行条件 |
|---|---|---|---|---|
| 批量锚点、曲线合并、动态声线关键帧 | domain | 单点与整体替换不能覆盖批量命令；补插入、移动、删除、插值/权重及一次撤销，冲突失败不落半成品 | ProjectEditing::batchAnchorsCommitAndUndoTogether、adjacentAnchorCurvesMergeWithoutLosingNodes、dynamicSpeakerKeyframesEditAndUndo | 通用 |
| 批量轨道顺序、片段裁边、音符搜索与切分 | domain | 补正常编辑结果、原音符保持、无匹配及撤销；搜索用少量有语义的数据行 | ProjectEditing::batchTrackOrderAndClipTrimming、noteSearch、splitAtPreservesPhraseAndUndo | 通用 |
| LRC 与填词分行 | unit | 补秒/小数时间、重复标签、元数据、定位、重新加载和分隔模式；修复时间换算与失败残留状态 | Lyrics::lrcTimestamps、lrcMetadataRepeatedLinesAndSeeking、lrcFailedReloadClearsPreviousDocument、lyricSplittingModesPreserveLines | 通用 |
| DSPX 声线及音素往返 | workflow | 扩展已有完整乐句场景，验证轨道固定混合、片段动态混合/旁路/预设来源，以及原始与编辑音素/偏移；包不可用时保留内容，已解析声库减少来源时保留剩余比例 | DocumentIO::dspxRoundTripPreservesEditedPhrase | 通用 |
| 实际批量文件导入 | workflow/gui | 真实 MIDI/DSPX 经生产 Host Adapter 和加载器验证一次提交、失败回滚及允许部分成功；轨道视口的真实拖入/离开/放下事件经 DocumentImportController 将两份 WAV 导入现有及新增轨道，检查预览无修改、一次提交、撤销重做和视图恢复 | ApplicationWorkflows::projectBatchImportUsesRealLoaders、ApplicationGui::droppingAudioFilesCommitsOneBatchToTheSelectedTracks | 通用/offscreen；临时文件 |
| 实际音频批量任务 | workflow | 有效/损坏素材经过实际解码，检查原子失败无修改、部分成功的警告及属性/元数据/一次撤销；取消后同一幂等键可重新执行 | ApplicationWorkflows::audioBatchFailurePolicy、audioBatchCancellationReleasesRetry | 通用；临时音频；无需设备 |
| 公共批量、计划复验及填词 | protocol | 检查策略与选项传递、文件授权、源文件变化及授权撤销后的计划复验；填词检查拆分、连音跳过、语言选择、实际结果及撤销，无效语言无副作用。真实解码与批量提交由工作流层承担 | AutomationProtocol::batchImportRouting、batchImportPlanRevalidation、fillLyricsOptions、fillLyricsUnavailableLanguage | 通用；临时文件和声线元数据 |
| MIDI 选择预览与片段声线上下文 | protocol | 公共能力与预览检查整轨/片段并集、音频排除、选项和覆盖提示，预览不执行导出或修改文件。片段查询检查继承、独立声线、恢复继承及语言来源，读取不改变工程和历史 | AutomationProtocol::routing 的 midiPreviewSelection、voiceAndSpeakerMix 数据行 | 通用；临时文件和声线元数据 |
| 预设和歌词规则的生产持久化 | workflow | 旧服务替身不能验证 AppOptions Adapter/Store；补创建、更新、重开、删除及规则的实际语言结果 | ApplicationWorkflows::speakerMixPresetPersistsThroughTheProductionStore、lyricRulesUseTheProductionRuntimeAndPersistence | 隔离配置 |
| 设置、缓存清理及交互导入 | gui | 使用实际页面和输入，验证访问根、推理选项、缓存确认、轨道/时间线选择、预览、取消及撤销 | ApplicationGui 的 tst_settings_pages.cpp、tst_project_import.cpp | offscreen；临时文件 |
| 自动化权限和连接配置页面 | gui | 自定义权限的分组、单项、折叠与预设导入经真实输入保存并在重开后恢复；复制的 Connector 参数保留所选权限；HTTP 配置跟随实际运行端点，错误状态消除后页面同步恢复 | ApplicationGui::automationCustomToolsetInputsPersistAndExportPermissions、automationConnectionCopyFollowsTheRuntimeEndpoint | offscreen；恢复应用运行状态属性及选项 |
| 音频/MIDI 设置与合成预览 | gui/workflow | 无播放设备时经真实输入检查增益/声像同步、生成器/包络和参考音高、关闭提交及配置重开；空 driver 只禁用依赖设备的控件。预览组件用真实 mixer 的受控回调检查采样率变化后 attack/decay/release 时长、有效 PCM、停止归零和音源释放 | ApplicationGui::audioPageInputsPersistWithoutPlayback、midiPageSynthInputsPersistWithoutPlayback、settingsSynthPreviewKeepsEnvelopeDurationsAcrossSampleRates | offscreen；无需音频/MIDI 硬件；设备选择另按条件执行 |
| 填词预览、编辑与规则 | gui | 实际声库参与歌词转换，经过真实控件拆分、修改预览、导入音符、取消及撤销；规则编辑验证实际预览和保存 | ApplicationGui 的 tst_fill_lyric.cpp | offscreen；默认内置声库 |
| Tagger 规则编辑与稳定身份 | gui | 创建、修改语言/正则/标签、启停、删除后 Apply 检查实际 TextTagger、落盘和重开；错误正则不改变已应用规则。新草稿分配稳定 ID，Splitter 同时保留详情编辑和 DTO 转换中的 ID | ApplicationGui::taggerRuleInputsApplyPersistAndReopen、invalidTaggerRegexPreservesAppliedRules、lyricRuleEditingChangesThePreviewAndPersists | offscreen；共用应用与声库；每例恢复规则和配置 |
| 主窗口面板及嵌入设置 | gui | 实际按钮、片段双击、分离窗口关闭和菜单输入验证面板恢复、分离/重新嵌入后的编辑上下文、视图状态复原，以及嵌入设置对后台快捷键的阻断与焦点恢复 | ApplicationGui 的 tst_main_window.cpp | offscreen；不代表各窗口管理器或多屏行为 |
| 主窗口文件拖入 | gui/workflow | 未保存工程经真实保存提示取消或放弃后打开；工程与音频混合拖入整批拒绝，再次单独拖入音频正常提交；检查工程身份、路径、轨道控件、播放位置锚定及一次撤销，释放临时音频 | ApplicationGui::projectDropCanCancelThenOpenTheDocument、mixedFileDropRejectsAtomicallyAndAllowsTheNextImport | offscreen；临时 DSPX/WAV；无需音频设备 |
| 日志接收、筛选和复制 | gui | 真实 LogBus 包含跨线程追加，经过控件过滤级别/标签/文本，检查显示顺序复制与清空，文档和历史不变 | ApplicationGui::logWindowFiltersLiveMessagesAndCopiesDisplayedOrder | offscreen；真实总线；无需设备 |
| 音素、搜索、动态声线与编辑边界 | gui | 真实对话框和输入验证音素确认/取消/重置、边界拖动及撤销；波形加载使用实际推理音频，受控延迟检查切换片段后丢弃过期结果。内联歌词/读音检查双击、提交/取消和导航，保留搜索、动态关键帧、音符缩放及时间尺循环行为 | ApplicationGui 的 tst_note_dialogs.cpp、tst_dynamic_mix.cpp、tst_piano_roll.cpp、tst_track_editing.cpp | offscreen；音素和波形使用内置声库 |
| 歌词整体后移与钢琴键盘 | gui/domain | 实际菜单检查连续选区变连音、整套语言/读音/候选后移、手工音素清理及撤销，非连续选择禁用；键盘按压/滑奏/隐藏检查 note-on/off 和释放 | ApplicationGui::movingLyricsBackwardUsesTheSelectedWordRange、pianoKeyboardGlissandoAndHideReleasePressedNotes | offscreen；无需音频或 MIDI 硬件 |
| 钢琴窗粘贴预览与边缘滚动接线 | gui | 完整菜单检查非零片段起点下的相对时间、手工文字、预览取消/提交和对象清理；真实计时器在鼠标停止移动后继续滚动视口与音符预览，取消/释放后停止并可一次撤销 | ApplicationGui 的 tst_piano_clipboard_scroll.cpp | offscreen；无需声库或设备 |
| 轨道头部编辑与颜色预览 | gui | 实际名称编辑检查提交/取消，静音/独奏按钮检查模型和撤销同步；颜色菜单悬停不产生历史，Escape 恢复原色，点击后单次提交并可撤销重做 | ApplicationGui::trackHeaderInputsCommitAndUndo、trackColorMenuPreviewsAndCommits | offscreen；无需声库或设备 |
| 公共数值控件的输入与提交 | gui | SeekBar 检查实时/释放提交、键盘步进及复位；Fader/Pan 检查预览信号、释放提交和随后外部更新，避免拖动状态残留 | GuiComponents::seekBarTrackingControlsWhenDraggedValuesCommit、seekBarKeyboardStepsClampAndDoubleClickResets、mixerSliderReleaseEndsPreview | offscreen；无需设备 |
| 音频输出及循环播放回调 | workflow | 生成短素材检查全工程时长、混音、静音、自定义来源及 WAV/FLAC；实际导出中途取消，检查旧文件、暂存清理和混音器恢复。受控回调验证循环及缓冲等待/恢复；修复 Talcs 位置同步并保留 PCM 断言。循环区导出尚未实现，不在测试中补建 | ApplicationWorkflows 的 tst_audio_workflows.cpp | 通用；不需要播放设备 |
| 解码峰值与波形缩放采样 | workflow/unit | 实际 WAV 检查多声道与文件尾部的瞬态不丢失，概览/细节/逐样本采样保持幅度和时间定位，速度或来源变化后刷新；修复解码帧数误按声道缩减及缩略峰值漏块 | AudioAssets 的 tst_waveform_sampling.cpp | 通用；临时 WAV；无需窗口或设备 |
| 自定义导出预设实际使用 | workflow | 创建、同名更新和落盘重读后按预设真实导出，检查整数 WAV 及有效样本，再验证删除和配置恢复；公开配置复制/赋值在私有类型完整处定义，允许调用方仅依赖公开头；不枚举内置预设 | ApplicationWorkflows::customExportPresetPersistsAndProducesIntegerWave | 通用；小型 WAV；不需要模型输出或播放设备 |
| 多语种、多声线及推理重算 | workflow/process | 内置微型计算图走正常包、语言、推理和导出路径，明确检查实际音素；验证缓存复用、切换声线和 BPM 后的输出变化 | ModelResources::voicebankInferenceAndWaveExport | CPU；默认内置声库，可显式换真实资源 |
| 声线预设 GUI 生命周期 | gui | 实际保存/取消、下拉选择、修改后 dirty 标记和删除/取消；搜索窗口销毁后保存预设揭示工具栏私有对象残留，将其归属视图并同步解除通知；持久化业务规则由应用工作流承担 | ApplicationGui::speakerMixPresetsFollowSaveSelectAndDeleteInputs、lyricSearchNavigatesTheActualEditorAndHandlesNoMatches | offscreen；临时配置 |
| 包查找与详情、缺失音频重定位 | gui | 搜索已加载包、选择详情及无匹配恢复；缺失音频经 Qt 文件选择取消或重定位后真实解码，验证状态和撤销重做 | ApplicationGui 的 tst_resources.cpp | offscreen；默认内置包、临时 WAV |
| 歌词网格和声音上下文编辑 | gui/domain | 跨行选择、行移动、删除及拆行后的继续编辑；清空轨道/片段声音上下文和读音恢复；检查归属、继承、通知及撤销 | ApplicationGui 的 tst_lyric_grid.cpp；ProjectEditing | offscreen/通用 |
| 轨道和规则完整拖放 | gui | Qt QDrag 经过真实事件执行轨道首尾移动及取消，检查头部/片段映射和一次撤销；规则排序保留未应用详情与 ID，并改变实际规则优先级，保存后重开 | EditorInteraction 的 tst_list_reordering.cpp | minimal；无需原生桌面 |
| 轨道菜单与音频文件选择 | gui/workflow | 新建轨道/片段、剪切和删除的撤销链；实际文件选择确认/取消、Unicode 路径解码及一次提交 | ApplicationGui 的 tst_track_menus.cpp | offscreen；临时 WAV |
| 动态声线编辑与导航 | gui | 区间/菜单删除和首帧保护；旁路、恢复、停止/取消及撤销后的控件状态；非零片段起点下关键帧导航的播放位置和视口 | ApplicationGui 的 tst_dynamic_mix.cpp | offscreen；声线元数据；无需设备 |
| 音频并发与有损导出 | workflow | 普通/Future 片段读取期间范围更新须等待，修改后 PCM 正确；MP3/Ogg 经实际编码解码验证有限非零采样、时长与格式 | ApplicationWorkflows 的 tst_audio_workflows.cpp | 通用；受控读源；无需设备 |
| 音频驱动延迟启动与释放 | workflow/native | 停止或销毁发生在延迟启动前，或与线程启动相邻时，不再执行悬空回调或丢失停止请求；独立进程退出验证未初始化的 ASIO 驱动不会释放其他组件的 COM 环境 | NativeDesktop::audioDriverStartupCanBeCanceled | 可用音频后端；逐例条件跳过；同一程序的独立子进程 |
| RHI 音高调制与音频时间锚定 | gui/workflow | 真实声库给出推理片段基线，检查局部调制、取消及单次撤销；双声道 WAV 实际解码后经鼠标裁边和跨轨移动，保留毫秒锚定、材料长度及命中 | NativeDesktop::rhiPitchModulationUsesTheInferredBaseline、rhiAudioClipTrimAndMovePreserveTimeAnchors | 原生窗口；调制默认内置声库；音频无需播放设备 |
| 锚点事务的重试与整批拒绝 | edit | 创建预览后提交和重试保持同一曲线身份，改变输入或目标拒绝复用请求；插入/移动跨曲线重叠，以及删除/插值批次包含已删除锚点时，所有曲线、版本及历史保持不变 | ProjectEditing::anchorCreationRetriesKeepTheCommittedIdentity、rejectedAnchorBatchPreservesEveryCurve | 通用；无需声库 |
| 实际推理的参数依赖和声线变化 | workflow | 表达力度、音高和 gender 分别重算所属下游阶段，保留时长及无关片段；固定混合更新已有分段，同声线跨轨移动保留分段，继承另一声线时重建；撤销恢复输入和声线 | ApplicationWorkflows 的 tst_clip_inference.cpp | 默认内置声库；混合及不同声线场景需要至少两条声线 |
| RHI 轨道与完整编辑器接线 | gui/workflow | 完整 TrackEditorView 使用 Null RHI 画布，菜单粘贴预览/取消/提交、框选、焦点定位、双击新建和文件拖入现有/追加轨道均经实际 Qt 事件验证，并检查轨道控件及撤销 | NativeDesktop::rhiTrackMenuPasteAndSelectionUseTheFullEditor、rhiTrackFileDropImportsAtTheChosenSlot | 原生窗口；生成小 WAV；无需播放设备 |
| 保存决策期间的音频完成回写 | workflow | 用受控调度暂停真实解码任务，在生产文档状态机等待保存决策时释放；完成结果保持托管且不写入忙文档，取消新建后应用波形，放弃原工程后丢弃旧结果，保留对应历史边界 | AudioAssets::decodeCompletionWaitsForTheSaveDecision | 通用；小 WAV；仅保存提示回答使用替身 |
| RHI 钢琴窗的完整编辑与选区 | gui | 完整 PianoRollView 的菜单粘贴保留手动读音，悬停预览及取消不改文档，提交可撤销；音域定位、焦点恢复、隐藏/显示同步视口状态。框选、区间选择与成组拖动检查批量提交和取消；调制拖动两端选区边界和倍率手柄，分别检查核心区、过渡区及范围外样本 | NativeDesktop::rhiPianoMenuPasteAndVisibilityUseTheFullEditor、rhiMultiNoteSelectionAndMoveCommitAtomically、rhiPitchModulationUsesTheInferredBaseline | 原生窗口；Null RHI；调制默认内置声库 |
| DSPX 音素互操作与 MIDI 导出 | workflow | 外部标准音素修改/清除优先于旧私有快照，兼容旧 workspace 音素；实际 MIDI 文件保留跨片段及裁剪后的音符时间、音高和 Unicode 文本，按选项保留或省略歌词、速度和拍号，源模型不变 | DocumentIO::dspxPhonemeInterchangeRespectsExternalChanges、midiExportPreservesProjectTimingAndOptionalMetadata | 通用；小型临时文件 |
| 初始化期间关闭与任务回收 | workflow/native | 立即销毁引擎不能丢失已完成初始化任务的清理；普通用例复用进程级应用、重建文档并等待任务完成，G2P 不进入销毁后的降级状态 | NativeDesktop::audioDriverStartupCanBeCanceled；GuiAppFixture、GuiDocumentFixture | GUI 运行时；驱动子场景按后端条件执行 |
| 音符创建与跨片段复制的请求重试 | domain | 创建音符返回稳定 clientRef/对象身份；复制与剪贴板粘贴连同参数曲线仅提交一次；同键改动手工读音、选区、内容或目标位置被拒绝，原结果及撤销边界不变 | ProjectEditing::insertingNotesCanRetryWithStableCreatedIdentities、transferringNotesCanRetryWithoutDuplicatingEdits | 通用；直接领域调用 |
| 钢琴窗锚点创建、框选与合并 | gui | 完整 Graphics View 编辑器验证单锚点与连线预览尚未提交、Escape 取消、补足曲线后提交；框选成组拖动、插值/删除菜单、跨曲线连接均检查模型、预览绘制变化和一次撤销 | ApplicationGui::pitchAnchorCreationPreviewsBeforeCommitting、pitchAnchorRangeEditsUseTheViewAndMenu、pitchAnchorMergePreviewCommitsAndUndoes | offscreen；真实控件事件；无声库和设备要求 |
| 播放窗口中的声学推理调度 | workflow | 内置声库形成当前、临近、已播放及远处片段；当前片段优先，暂停撤回排队任务并保留运行任务，继续及跳转后仅新窗口内任务启动，受控 PCM 回调推进播放 | ApplicationWorkflows::playbackWindowPrioritizesAndSuspendsAcousticInference | 默认内置声库；受控真实任务；无需音频设备 |
| 外部 DSPX 标准声源混合 | workflow | 无 DS 私有 workspace 的嵌套固定/动态混合展开权重并合并同一声线；已安装声库使用真实元数据，未安装时保留待解析引用；保存重开保留声线与关键帧 | DocumentIO::dspxStandardSingerSourcesLoadNestedMixes | 通用；临时文件；元数据解析替身 |
| 固定声线列表排序与来源替换 | gui | 实际 QDrag 前后保持每条声线的非均分比例，取消保持原顺序；下拉框替换声线保留比例、同步标签并禁用已占用来源；接受后重开保留自定义混合 | EditorInteraction::speakerMixDragKeepsWeightsWithTheirSources、speakerMixSourceChoicePreservesWeightsAndUpdatesTags | minimal；真实控件与拖放循环 |
| 音素时长重置的相邻词确认 | gui | 从真实音符菜单发起重置，未选中邻词将受影响时展示名称并等待决定；取消不改任何音符，确认后一起重置且一次撤销恢复；已选中全部受影响音符时直接执行 | ApplicationGui::phonemeDurationResetConfirmsAdjacentChanges | offscreen；临时文档；无需声库或设备 |
| 固定/动态声线模式与轨道继承 | domain | 公开领域入口验证轨道固定混合、片段继承、动态首帧初始化、关键帧重试及冲突、停用后固定值恢复；重复设置不产生新历史，撤销恢复继承，片段独立混合不改变轨道 | ProjectEditing::speakerMixModeTransitionsPreserveTrackInheritance | 通用；声线元数据；无需模型或设备 |
| 主菜单音符编辑及语言菜单 | gui | 量化通过真实对话框选择网格、起点/长度、选区/全片段和取消；菜单全选及八度移动检查模型和撤销。语言子菜单验证指定语言、跟随声库与 Unknown，仅改变选中词并清理旧手工读音，一次撤销恢复 | ApplicationGui::mainMenuQuantizationUsesTheChosenScopeAndOptions、mainMenuOctaveEditsFollowThePianoSelection、noteLanguageMenuChangesOnlyTheSelectedWords | offscreen；语言使用内置声库；指定语言场景需要第二语言 |
| 音符两侧裁边与取消 | gui | 在既有右侧拉伸用例增加左侧拉伸和 Escape 取消，预览期间模型不变，提交后的时间/长度与视图一致且一次撤销恢复 | ApplicationGui::resizingANotePreviewsAndCommitsItsBoundary | offscreen；无需声库或设备 |
| 声音导出等待推理时取消 | workflow | 实际声库任务进入导出准备阶段后取消，旧文件不被覆盖且不开始渲染；后台任务排空后再次导出成功，文件可解码、样本有限且非零，无暂存残留 | ApplicationWorkflows::cancelingVoiceExportDuringPreparationAllowsAnotherExport | 默认内置声库；独立缓存；无需音频设备 |
| 公共曲线替换与音素名称转换 | protocol | 混合绘制/锚点曲线经公开 JSON 输入保存位置、步长、值和插值，连续替换可撤销至原曲线身份；音素名称按音符显式语言或片段继承语言补全，清除失效手工偏移并可撤销恢复 | AutomationProtocol::parameterReplacementDecodesDrawAndAnchorCurves、phonemeNamesUseTheEffectiveLanguageAndResetOffsets | 通用；真实领域实现；无需声库 |
| 撤销快捷键的编辑位置定位 | gui | 离屏片段、隐藏轨道面板及钢琴窗顶端音符先显示编辑位置，再按一次才撤销，重做恢复并定位；首轨和顶端音符的边框超出场景曾导致定位误报失败，统一按编辑对象的主体矩形计算焦点范围 | ApplicationGui::undoShortcutRevealsTheTrackEditBeforeChangingIt、undoShortcutRevealsThePianoEditBeforeChangingIt | offscreen；完整 MainWindow、真实快捷键及历史 |
| 混合 MIDI/音频的批量导入选项 | gui/workflow | 两个 MIDI 和一个 WAV 经真实画布拖放共用一次编码/时间线决策；取消整批无修改，确认分别验证首个 MIDI 速度优先、保留当前拍号或速度、已有轨道复用、歌词保真、音频时间锚定及整批撤销重做 | ApplicationGui::droppingMidiAndAudioFilesUsesOneBatchDecision | offscreen；小型临时 MIDI/WAV；无需模型或设备 |
| 参数变换边界与倍率手柄 | gui | 拖动过渡区及核心区边界，保持未提交模型和已有过渡宽度；倍率手柄改变核心值，过渡区平滑衔接且范围外保持，预览图形更新，单次提交可撤销恢复 | ApplicationGui::parameterTransformHandlesControlTheTransitionRange | offscreen；实际参数控件及输入；无需声库 |
| 文件菜单的打开、另存为及保存 | gui/workflow | 真实 Qt 文件选择框处理 Unicode 路径；取消打开不换文档，取消另存为保留路径和未保存历史，确认后更新保存点/最近列表，后续 Save 使用新路径；独立重读磁盘验证新文件更新且原文件不被覆盖 | ApplicationGui::fileMenuOpensAndSavesThroughTheActualPicker | offscreen；临时 DSPX；Qt 文件选择框 |
| 公共单文件导入与计划接线 | workflow/protocol | 从公开格式检查取得计划，再经 Registry 和实际 Host 加载器追加 DSPX；读取计划不改工程，导入保持原文档及已有轨道身份，保存新轨道/片段/音符内容，一次撤销恢复原工程 | ApplicationWorkflows::publicSingleProjectImportUsesThePreparedPlanAndKeepsTheDocument | 通用；临时 DSPX；实际应用运行时 |
| 公共声线预设与已应用混合的生命周期 | workflow/protocol | 通过实际声库解析公开预设的保存、查询、同名更新和应用；删除预设后，已应用混合保留，失效引用返回错误且不改变工程或历史，撤销恢复原声线 | ApplicationWorkflows::publicSpeakerMixPresetsResolveAndPreserveAppliedVoices | 默认内置声库；至少两条声线；隔离配置 |
| 文件打开失败与最近项目管理 | gui/workflow | 损坏工程经过真实错误提示后保留当前文档、未保存编辑及历史，修复源文件后可再次打开；最近项目子菜单移除失效文件、打开有效文件并清空列表，持久化列表与界面一致 | ApplicationGui::failedProjectOpenPreservesTheDocumentAndRecovers、recentProjectsMenuRemovesMissingFilesAndClearsTheList | offscreen；临时 DSPX；真实 MainWindow 和菜单 |
| 音频设置保存失败与重试 | workflow | 在隔离配置路径上制造可移除的文件写入障碍，验证设置、运行时音量/声像和热插拔策略共同回滚；恢复路径后同一修改成功落盘，文档和历史不受影响 | ApplicationGui::audioSettingsSaveFailureRestoresRuntimeAndAllowsRetry | 通用 GUI 环境；无需音频设备 |
| 混合片段正常复制 | domain | 歌声和音频一起复制到原轨道或指定轨道，保留相对时间、真实音频时长、歌词/发音、曲线及动态声线；新片段、音符、曲线、锚点和声线帧使用独立身份，重试不重复创建，一次撤销/重做恢复整批 | ProjectEditing::duplicateClipsPreserveContentAndCreateIndependentObjects | 通用；真实模型；无需文件解码、模型或设备 |

本次还移除无产品实例化入口的旧 G2P/伪声设置页和 `TrackSynthesizer` 及其空容器引用。清理改变统计分母，报告中与新增测试命中的贡献分开说明，不通过排除仍有效的生产文件提高比例。
