# 四期产品行为与测试覆盖矩阵

## 1. 口径与索引

矩阵以产品能力为行，归并同一行为的存量测试、确认缺口及最终处置，不按开发轮次、工具 ID 或历史 bug 排列。只记录测试职责及入口，执行步骤见[测试计划](test-plan.md)，实际结果及剩余限制见[测试报告](test-report.md)，生产代码变化见[实现报告](implementation-report.md)。

测试引用省略 Test 前缀，通常对应 [src/tests](../../../src/tests/) 下的 Test<名称> 目录。Provider 两个变体共用 TestInferenceProvider，ICU 使用 IcuWrapperTests。程序内按 tst_ 文件和 Qt Test slot 定位，完整程序职责见[测试大纲](test-outline.md#9-套件职责与程序划分)。新增行为优先复用所属套件、场景和数据行。

“通用”表示适用平台均可运行，不表示已通过。ApplicationGui 的“应用 GUI”环境是 Windows/Linux offscreen、macOS 原生窗口。NativeDesktop 使用原生桌面或 Linux Xvfb。完整工程快照共用生产 DSPX 编码，身份、revision、历史及非持久状态另行断言。

## 2. 产品能力与最终覆盖

### 2.1. 基础数据与算法

| 功能域 / 关键行为 | 类别 | 原覆盖及确认缺口 | 本期处置与最终验证内容 | 最终测试引用 | 条件 |
|---|---|---|---|---|---|
| 基础类型、本地化、日志和单例 | unit | 混合演示和真实断言，注册容器有跨线程访问 | 重写 ADT 断言，合并本地化及日志，验证线程安全注册和查询 | Foundation，IcuWrapperTests | 通用 |
| 时间、拍号、量化和音频锚点 | unit | 数值用例分散在独立程序 | 合并时间换算、投影边界、拍号变化及音频时间锚定 | MusicTime | 通用 |
| 曲线、重采样和锚点布局 | unit/domain | 手写 main，控制器和渲染器重复插入算法 | 直接 Qt Test 数据行，共用插值与布局，验证局部保留、合并拒绝及重复点只选择不提交 | Parameters，ProjectEditing | 通用 |
| 声线继承、动态混合和输入转换 | unit/domain | 生产源重复编译且依赖空 stub，缺少真实继承 | 复用生产组件和模型，检查权重回退、端点/区间插值、连音、间隙、辅音提前量及输入保真 | VoiceAndInference | 无需模型执行 |
| 歌词规则、词典、拆分和 LRC | unit/domain | 规则排序已有，空匹配、有效词典及重载缺失 | 合并规则入口，补真实词条、重新初始化、空匹配保真、时间格式与失败重载清理 | Lyrics | 小型临时词典/文本 |
| 配置、语言和 Provider 编译条件 | unit | 配置测试分散，编译条件和持久化混杂 | Preferences 维护读写及语言，Provider 变体共用源码验证可用性、选择和回退 | Preferences，InferenceProviderDefault/Cuda | 通用，不要求 GPU |
| 推理缓存和会话缓存 | unit/workflow | 命中与失效有基础回归，实际应用未贯通 | 保留受控缓存检查，实际复用、损坏和清理归入应用流程 | VoiceAndInference，ApplicationWorkflows | 基础通用，实际模型用内置包 |
| worker 完成、取消及队列接续 | unit/workflow | 检查后订阅会漏通知，跨线程完成可能提前删除对象 | 受控时序覆盖正在结束、已结束和通知中的取消，后续任务继续，完成发布在所属线程 | Foundation::canceledWorkerCompletionAdvancesTheQueue、cancellationDoesNotDeleteAnEmittingWorker | 真实 Task/TaskQueue，无模型 |

### 2.2. 编辑与应用状态

| 功能域 / 关键行为 | 类别 | 原覆盖及确认缺口 | 本期处置与最终验证内容 | 最终测试引用 | 条件 |
|---|---|---|---|---|---|
| 轨道、片段和音符编辑 | domain | 巨型入口及局部快照难以定位缺失 | 按领域拆 slot，检查未选内容、批量原子性、预验证、no-op、revision、一次提交及完整撤销重做 | ProjectEditing | 通用 |
| 歌词、语言、读音及音素 | domain/protocol | 属性级联已有，公开投影与重置覆盖不完整 | 验证手工/自动来源、候选、未知语言、完整与部分偏移、名称修改和重置，复用历史回放 | Lyrics，ProjectEditing，AutomationProtocol | 通用，实际 G2P 另列 |
| 参数修改、替换、描摹及变换 | domain | 局部保留、失败时已有内容及预验证不足 | 共用变更入口，检查几何/值域/拓扑、范围外曲线、身份、拒绝后原历史及提交后可撤销 | ProjectEditing::parameterEditing、traceOriginalParameterPreservesGapsAndAnchors | 通用 |
| 音符及整片段复制粘贴 | domain/gui | 整片段遗漏参数，损坏曲线和位置溢出未拒绝 | 共用编码与准备，保留全部层、声线、发音、音频来源及相对位置，坏组整体舍弃，重复组保留首个合法值，越界整批拒绝 | ProjectEditing 的 tst_note_transfer.cpp，ApplicationGui 的剪贴板场景 | 通用/应用 GUI，小 WAV |
| 复杂插入与显式幂等 | domain | 简单对象重试不足以验证内容指纹 | 复用复杂片段，检查相同请求不重复创建，曲线/声线变化拒绝复用键，完整内容撤销重做 | ProjectEditing::insertingPreparedContentCanRetryWithoutDuplicatingEdits | 通用 |
| 轨道/片段声线及模式转换 | domain | 重复局部断言，缺少仅有动态权重时的回退 | 共用提交与历史场景，验证继承、独立来源、预设、单声线、固定/动态、首帧回退、旁路及清除 | ProjectEditing::speakerMixEditing、speakerMixModeTransitionsPreserveTrackInheritance | 声线元数据，无需模型 |
| 时间线、主输出和持久循环 | domain/protocol | 领域行为已有，公开转换不完整 | 速度/拍号增删改、静音/独奏、循环关闭与清除，保留乐句及逐步历史恢复 | ProjectEditing，AutomationProtocol::publicTimelineEditsPreserveThePhraseAndUndo | 通用 |
| 保存点、分支和文档换代状态 | domain/workflow | redo 清空未检查 dirty，旧选择影响新工程 | 验证保存分支丢弃、重复重置、布局偏好保留、换代后清理失效选择及继续编辑 | DocumentIO，EditorInteraction::documentScopedStateReset，ApplicationGui | 通用/应用 GUI |
| 编辑选择、活动对象及工作区状态 | domain/gui | 选择清除缺少预验证及重复请求，面板恢复未贯通 | 清除范围与活动片段按职责处理，不增加文档历史，公开请求和真实面板分别验证 | ApplicationServices::editorSelectionAndSnapshots，ApplicationGui | 通用/应用 GUI |
| 应用设置、规则、包和预设服务 | domain/workflow | 历史阶段入口重复，替身复制业务排序 | 合并 Harness，真实持久化验证稀疏更新、稳定身份、排序、失败重试和旧引用拒绝 | ApplicationServices，Preferences | 受控外部枚举，无设备要求 |
| 公开设备和计算 Provider 选择 | domain | 缺少外部候选、格式及 GPU 身份核验 | 预验证、提交及 no-op 共用场景，拒绝不可用选项而保留完整配置、文档和历史 | ApplicationServices::audioDeviceSettingsFollowAvailableCandidates、computeDeviceSettingsMatchAvailableGpuIdentity | 候选快照可控，真实设备另列 |

### 2.3. 文档、文件与异步工作流

| 功能域 / 关键行为 | 类别 | 原覆盖及确认缺口 | 本期处置与最终验证内容 | 最终测试引用 | 条件 |
|---|---|---|---|---|---|
| 新建、打开、保存和退出决策 | workflow | 底层保存测试未进入完整状态机 | 真实控制器处理取消/放弃/保存、路径取消、写入失败重试、忙碌及最近列表，外部回答替身与真实弹窗测试分开 | DocumentIO，ApplicationGui::newDocumentHonorsTheSaveDecision、rejectedProjectInputAllowsTheNextRequest | 临时文件，应用 GUI |
| 加载中编辑、取消、退出和声库等待 | workflow/gui | 就绪环境掩盖加载与提交边界 | 暂停实际解析 worker，验证再次确认、取消晚到结果、扫描失败的真实继续/取消按钮及后续打开 | ApplicationGui::pendingProjectLoadCanCancelOrRequestExit、projectOpenWaitsForPackageMetadata | 小 DSPX，受控任务/包状态 |
| DSPX 往返、时间线及资源身份 | workflow | 多数只检查文件头和对象数量 | 完整乐句、参数、声线、tempo/meter 往返，无效时间线不破坏模型，原始文件名、缺失声库和音频来源保留 | DocumentIO | 小型工程，无外部设备 |
| MIDI 导入、导出与原子发布 | workflow/protocol | 偏移、裁剪、可选元数据和实际轨道选择存在缺口 | 导出后重新解析，检查时间/歌词/所选轨道，覆盖写入、授权撤销、取消和并发目标保护 | DocumentIO，ApplicationServices::preparedMidiPublication，ApplicationWorkflows::publicProjectLoadChecksPlansAndParserFailures | 小 MIDI，真实转换器 |
| 单文件、批量和外部格式导入 | workflow | 计划预检不能证明加载失败，批量异常未贯通 | 实际 DSPX/MIDI/音频 loaders，原子/尽力模式、全来源缺失、解析失败、换代、幂等重试及完整历史。外部转换器只替代进程 | ApplicationWorkflows 的导入场景，ApplicationGui 的导入/拖入场景 | 临时文件，外部进程替身 |
| 音频来源、摘要和重定位 | workflow | 解码控制曾替写生产方法，来源目录及迟到结果丢失 | 复用真实 Runtime，验证摘要/快照、目录搬迁、逐片段来源、确认/重链接、授权撤销和音频格式历史 | AudioAssets，ApplicationWorkflows::publicAudioPathUpdatesPrepareCommitAndUndo，ApplicationGui | 小 WAV，自有临时目录 |
| 解码、峰值、错误和取消 | workflow | 多声道帧数、尾部峰值及部分缓存失败未覆盖 | 实际解码各种输出，失败清空部分缓存，删除目标取消，撤销重开恢复，旧文件句柄按平台能力验证 | AudioAssets::decodeBackendFailurePreservesTheDocumentAndAllowsReopen、removingAudioTargetsCancelsPendingDecode | 小音频，删除能力按平台 |
| 混音、编码、分轨与输出内容 | workflow/protocol | 导出器仅初始化，有损和量化格式缺少输出验证 | 真实 WAV/FLAC/Vorbis/MP3 再解码，检查采样格式、长度、增益/声像/静音、削波、范围、轨道分离、重名和取消 | ApplicationWorkflows::encodedAudioExportsProduceReadableFiles、audioExportRespectsRangeMixAndMute、separatedAudioExportRejectsCollisionsAndKeepsTrackSignals | 小 WAV，无播放设备 |
| 离线恢复、播放策略与循环 | workflow | 已关闭 mixer 恢复失败，旧暂停通知打断恢复播放 | 共用真实 mixer，验证原打开/关闭状态、实际 PCM、暂停/停止回位、窗口调度和跨块循环 | ApplicationWorkflows::offlineExportRestoresMixerState、controlledPlaybackLoopsAndBuffers | 回调由测试驱动，无设备 |
| 异步准入、幂等和终态资源释放 | domain/workflow | 程序按历史拆散，终态回调可能保留后台许可 | 归入统一运行时，检查接受前失败、成功回放、取消、回调释放及随后可准入 | AutomationRuntime，ApplicationServices，AutomationProtocol | 受控服务/时序 |
| 结果 Apply/Drop/Defer 与语言任务 | workflow | 输入转换不能证明门控和暂存写回 | 实际任务快照验证输入/文档/对象变化，真实 G2P 在编辑期间暂存、结束后应用，语言失败取消两类任务并恢复 | ApplicationWorkflows::clipInferenceResultsRespectEditSession、modelInferenceWaitsForEditingBeforeApplying | 内置声库，受控 worker |
| 推理重启、取消和手动声学许可 | workflow | 替代任务卡队列，关闭自动推理后手动请求不完成 | 暂停真实 worker，重启/采样设置修改/取消后验证新输入、清理、终态及一次手动许可。播放或参数编辑可恢复缺失流水线 | ApplicationWorkflows::restartInferenceReleasesReplacedTask、changingSamplingSettingsRestartsRunningInference、playbackRecoversAfterPublicInferenceCancellation | 内置声库，CPU |
| 公开推理作用域和目标生命周期 | workflow/protocol | 范围字符串误关联任务，删除后任务未终止 | 同事件轮次启动，按真实乐句关联轨道/片段查询，删除、移动、撤销、取消及重新请求保留正确终态和用户历史 | ApplicationWorkflows::publicInferenceStartsBeforeQueuedDocumentChanges、publicInferenceStatusAssociatesTasksWithTheirScope | 内置声库，受控 worker |
| 推理失败、缓存损坏和清理 | workflow | 成功路径不能证明执行失败或缓存恢复 | 无效音素、各阶段执行失败、声码器错误、截断/错误缓存、不可写路径及已完成探测后编辑，恢复后同进程重试，清理不删活跃缓存 | ApplicationWorkflows::inferenceFailureAndCancellationAllowRetry、acousticCacheWriteFailureCanBeRetried、cacheCleanupProtectsRestoredInference、queuedCacheProbeCannotRestoreAudioAfterAnEdit | 微型声库和错误 ONNX，自有缓存 |
| 包刷新、初始化和实际模型装配 | workflow/process | 失败原因及会话发布门控不完整 | 刷新拒绝不改 SDK 会话，保留损坏包诊断，初始化/关闭唤醒等待者。真实多语言/多声线推理、GPU 条件行及 WAV 输出 | ApplicationWorkflows，ModelResources | CPU 默认内置包，GPU 按条件 |
| 内置英语 G2P 与 ONNX 适配 | workflow | 词典素材不能证明产品 G2P 网络及张量接线 | 字典内外词和混合语言顺序，生产会话工厂的张量/掩码、错误重试与关闭重开 | ApplicationWorkflows::languageTasksKeepMixedResultsAligned、builtInG2pConvertsDictionaryAndUnlistedWords、g2pOnnxSessionTranslatesInputsAndRecoversAfterFailure | 产品 G2P 及微型模型，CPU |
| 音高/MIDI 提取通用流程 | workflow/protocol | 后端替身未覆盖来源准备，目标与重试不足 | 真实来源快照、摘要及授权，受控结果验证过滤、坐标、三类目的地、取消、同版本换源、完整撤销和幂等重试 | ApplicationServices 的提取场景，ApplicationWorkflows::extractionPreparationRejectsMissingResourcesAndChangedSources，AutomationProtocol | 小 WAV，GAME/RMVPE 实际模型除外 |

### 2.4. 自动化接口与 Connector

| 功能域 / 关键行为 | 类别 | 原覆盖及确认缺口 | 本期处置与最终验证内容 | 最终测试引用 | 条件 |
|---|---|---|---|---|---|
| 公共输入、错误及跨入口一致性 | protocol | 工具数量/Schema 镜像与行为混合 | 删除镜像目标，保留运行时校验。少量共享场景验证直接调用、Native、MCP 的实际结果及副作用 | AutomationProtocol::routing、sharedEditingScenario | 通用 |
| 编辑、设置及任务的公开转换 | protocol/workflow | 领域测试不能证明公开参数及结果接线 | 乐句、轨道、时间线、音素、声线及设置共用历史回放，补 GUI 模式、有效语言、主声线、音频任务和有损导出映射 | AutomationProtocol，ApplicationWorkflows | 领域边界不重复铺矩阵 |
| 分页、查询预算与目录变化 | protocol | Cursor 独立程序，续页及失效条件不足 | 工程和声库分页、版本/本地化、曲线裁剪/降采样/预算，修改后拒绝旧游标，查询不编辑 | AutomationProtocol::parameterQueryBoundsSamplesAndPreservesAnchors、routing | 通用，外部目录可控 |
| 权限、Host 和文件准入 | protocol/domain | 有边界基础，实际过滤及恢复缺失 | 检查授权拒绝/撤销及提交前复验、Headless 过滤 GUI 工具且不转发、GUI Host 恢复及权限更新 | AutomationRuntime，AutomationProtocol，Connector::headlessHostAvailability、editorPolicyRefresh | 临时访问根及回环服务 |
| HTTP 请求、失败和监听恢复 | protocol | 正常启停未覆盖在途请求及跨线程停止 | Native/MCP 路由开关、Accept 协商、处理器异常、输出上限、端口冲突，失败释放准入并可恢复 | AutomationProtocol::pendingRequestsRespectRouteAndServerShutdown、nativeRequestValidation、handlerResponseLimits、listenerLifecycle | 真实回环 HTTP，受控处理时序 |
| HTTP 退出响应 | protocol/process | 停止时已生成响应可能被截断 | 完整大响应、有界共同排空期限、不读取客户端，以及真实退出响应和进程终态 | AutomationProtocol，ProcessIntegration::gracefulExit | 通用/应用进程 |
| Connector 握手、目录和重连 | protocol/process | 长入口和速度循环，失败目录可部分发布 | 合并 Qt Test，用实际请求检查分页一致性、损坏/循环/重复页拒绝、缓存、权限及状态入口恢复 | Connector::paginatedHandshake、handshakeCoordination、editorPolicyRefresh | 受控上游及真实 Connector |
| stdio、异常响应和结果不明确 | protocol/process | Windows 路径假设，POSIX 断管绕过错误反馈 | 真实分帧/大帧/背压/EOF，损坏和截断 HTTP/SSE、错配 ID、会话更新。已送达命令不重试，SIGPIPE 转入可诊断错误 | Connector::stdioFraming、upstreamResponses、commandTransportOutcome | 当前平台，真实管道 |

### 2.5. 真实应用进程

| 功能域 / 关键行为 | 类别 | 原覆盖及确认缺口 | 本期处置与最终验证内容 | 最终测试引用 | 条件 |
|---|---|---|---|---|---|
| Editor/Connector 装配和文件闭环 | process | GUI/Headless 混杂，数据根仅适配 Windows | 共用跨平台进程沙箱，实际编辑、保存、重开、MCP/Native、音频导入导出及幂等 | ProcessIntegration | 小 WAV/DSPX，无设备 |
| 启动、单实例与转发 | process/protocol | 接入竞态、同步写完误判失败及启动恢复缺失 | 实际本地 socket 验证排队连接、请求 ACK、拒绝/断开、目录占用后恢复，严格区分进程所有权 | Bootstrap，ProcessIntegration | 当前平台，隔离身份 |
| 退出、重启和终止信号 | process | 替代进程输出/管理不完整，已退出误判等待失败 | 文件保存输出供重启继承，检查新身份、服务就绪、正常退出码和平台信号，只清理自有进程 | ProcessIntegration | 当前平台 |
| 插件失败和模型进程恢复 | process/workflow | 资源客户端偏离正式协议，部署失败未验证编辑保留 | 共用 Native 客户端，插件缺失时包任务失败但编辑可用，恢复目录后重启扫描，实际模型及导出成功 | ModelResources::runtimePluginFailurePreservesEditing、voicebankInferenceAndWaveExport | 内置或显式声库，GPU 按条件 |

### 2.6. 界面组件与交互

| 功能域 / 关键行为 | 类别 | 原覆盖及确认缺口 | 本期处置与最终验证内容 | 最终测试引用 | 条件 |
|---|---|---|---|---|---|
| 音符绘制、移动、裁边和选择 | gui/domain | 几何/Facade 不能证明实际输入 | 鼠标/笔/触控复用真实场景，选择框和预览、联合边界、取消、一次提交、完整历史及命中恢复 | ApplicationGui 的 tst_piano_roll.cpp，NativeDesktop 的 tst_rhi_editor.cpp，EditorInteraction | 应用 GUI/原生 RHI |
| 轨道片段、重排和拖入 | gui | 默认轨道输入、音频裁边和末尾区域不足 | 歌声/音频移动裁边、跨轨、文件落点、相对时间、真实重排和边缘静止滚动，取消/提交/撤销共用 | ApplicationGui 的 tst_track_editing.cpp，EditorInteraction，NativeDesktop 的 tst_rhi_tracks.cpp | 小 WAV，应用 GUI/原生 |
| 参数绘制、描摹、变换和锚点 | gui | 算法覆盖不能证明控件事务与接线 | 鼠标/触控/笔的局部绘制、擦除、Shape/Scale/Modulate、锚点插入及组拖动，范围外保留、预览取消、恢复输入 | ApplicationGui 的 tst_parameter_editing.cpp、tst_pitch_modulation.cpp，NativeDesktop | 调制用内置声库，其余通用 |
| 剪贴板、菜单和快捷键 | gui | 仅测 payload，主窗口及只读文本菜单未执行 | 实际 copy/cut/paste、悬停预览、Escape、歌曲/音频重链接、Delete 与工具栏，辅助窗口不抢快捷键，菜单按语义操作 | ApplicationGui，EditorInteraction，GuiComponents | 系统剪贴板由 fixture 恢复 |
| 文本编辑、发音和音素边界 | gui | 文字归属、触控接管、删除目标和边界不足 | 歌词/发音确认取消及 Tab，文字命中选所属音符，候选菜单、首尾/邻接音素、目标删除后丢弃草稿，撤销继续编辑 | ApplicationGui，NativeDesktop::rhiInlineTextEditingNavigatesCancelsAndUndoes | 音素实际结果用内置包 |
| 触控、笔和输入恢复 | gui/domain | 纯状态机不能证明实际配送，失活与空触点取消遗漏 | 共享意图测试，真实控件处理长按到期、双指接管、惯性、新接触停止、侧键/橡皮、窗口失活和工具恢复 | EditorInteraction 的 tst_touch_gestures.cpp、tst_pen_stroke.cpp，ApplicationGui 的 tst_pointer_delivery.cpp，NativeDesktop | 合成 Qt 输入，不代表物理驱动 |
| 触控滚动、下拉框及重排手柄 | gui | 仅安装过滤器或独立手写 main | 真实 QScroller、弹窗及列表区分选择/滚动，重复按下、关闭/取消后恢复，行重建后拖动接线和提示框位置 | GuiComponents 的 tst_touch_claims.cpp、tst_reorder_handles.cpp、tst_pointer_tooltips.cpp | offscreen，物理尺寸条件单列 |
| 视口、零长度、历史和跟随 | gui/domain | Legacy 与 RHI 保护不统一，逻辑视口换算及两轴下限错误 | 共用缩放约束，零长度到正常工程恢复，滚轮/捏合/动画中断、窗口尺寸、历史主体定位和播放翻页 | EditorInteraction，ApplicationGui，NativeDesktop | 对应后端，真实窗口/输入 |
| 填词网格与规则编辑 | gui | 对象归属、稳定行序、换行选择和草稿保存遗漏 | 跨行移动/删除/拆行、窄视口选区、主题切换保留草稿，规则创建/排序/Apply 保留 ID 与未应用详情 | ApplicationGui 的填词/规则场景，EditorInteraction 的拖放场景 | 应用 GUI，QDrag 用 minimal |
| 声线、动态关键帧和预设 | gui | 领域权重未验证对话框，已有关键帧身份丢失 | 真实来源选择、普通/Alt 权重拖动、主题、确认取消、预设增删，动态选择/旁路/停止/首帧保护及工程时间导航 | ApplicationGui 的声线/动态编辑场景 | 元数据，默认无需模型 |
| 推理诊断、时间轴及轨道颜色 | gui/workflow | 实际重叠诊断未贯通，切换唱段/跨轨撤销反馈过期 | 真实推理产生部分或整段失败，检查角标、原因、区间、Undo/Redo，切换订阅和过渡结束，颜色跟随模型归属 | ApplicationGui::inferenceErrorBadgesExplainOverlapsAndFollowUndo、timelineContextSwitchKeepsCurrentInferenceFeedback、activeClipMoveRebindsThePianoTrackColor，NativeDesktop | 诊断用内置包，CPU |
| 文件选择、导入配置和导出进度 | gui/workflow | 文件测试不能证明选择器、预演和进度窗口 | 真实菜单/文件选择，格式/采样率/轨道/模板、取消重开、Export、削波、错误及窗口关闭，文件和任务清理 | ApplicationGui 的导入、资源及导出场景 | 小 WAV/DSPX，无设备 |
| 设置、Provider 与服务重配置 | gui/protocol | 写入失败页面未回滚，异步枚举和服务启停不足 | 实际输入及重开，写入失败保留正式值且可重试，GPU 旧回复拒绝，自动化端口/权限实际生效，稍后重启不破坏工程 | ApplicationGui 的设置场景 | 设备发现可控，真实设备另列 |
| 语言、主题、菜单及绘制组件 | gui/unit | token 镜像与有效行为混杂，翻译依赖 SDK，图标透明度错误 | 删除清单镜像，检查定义/引用失败恢复、系统主题与显式选择、实际菜单重译、内嵌 Qt 翻译、图集覆盖和主题复用 | GuiComponents，EditorRendering，ApplicationGui | offscreen/应用 GUI，无像素基线 |
| 数值控件、电平和播放栏 | gui | 释放/双击、键盘复位、快捷键及实时反馈遗漏 | SeekBar/Fader/Pan 输入与提交，时钟控制 Tap Tempo，事件坐标电平，循环按钮正式提交和速度显示刷新 | GuiComponents，ApplicationGui，NativeDesktop | 普通反馈无设备，真实播放按条件 |
| 主窗口、设置、日志与生命周期 | gui/workflow | 独立组件不能发现全局父窗口悬空及销毁后通知 | 面板隐藏/最大化/分离/重嵌入、独立设置焦点、LogBus 过滤复制、窗口关闭及后续对话框，正常析构 | ApplicationGui，NativeDesktop | 应用 GUI/原生，日志跨线程 |
| RHI 实际画面和后端失败 | gui | 几何/Null 提交不证明 shader 或回退装配 | 平台后端同次小区域读回，移动/遮罩/撤销、缩窗/DPI 和命中，渲染失败通知合并并恢复 Legacy 后继续编辑 | NativeDesktop，EditorRendering | Null 通用，实际后端按可用性 |
| 外轨参考音符 | domain/gui | 主分支已有能力，需证明数据与两画布接线 | 排除宿主/同轨、绝对时间、更新与清理，显示开关和输入穿透，RHI 帧及 DPI 行，避免重复跨层矩阵 | EditorInteraction，ApplicationGui，NativeDesktop | Legacy/Null，像素行需平台后端 |
| 真实设备配置及播放 | gui/workflow | 受控回调不能证明设备打开和热插拔恢复 | 实际驱动/默认别名/缓冲/采样率及持久化，主窗口和公开播放控制，三种停止策略，MIDI 回环与真实 PCM | NativeDesktop::availableAudioDeviceRunsPublicPlayback、configuredMidiLoopbackFeedsLiveSynthesizer | 音频自动探测或指定，MIDI 显式配置 |

## 3. 目标收敛与历史入口去向

| 原入口 / 内容 | 最终归属 | 保留的职责及边界 |
|---|---|---|
| Expected、LocalizedText | Foundation | 自有基础类型与本地化文本断言 |
| MusicTimeline、AudioAnchor | MusicTime | 音乐时间与音频锚点的纯行为 |
| ParamResample、ParamSupport、CurveTrace、CurveTransform、AnchorEditController、PitchDisplayStrategy | Parameters | 曲线、重采样、锚点编辑和音高显示策略按组件分文件 |
| Syllabification、LyricRules（原 FillLyricTaggerOrder）、WordPropertyCascade | Lyrics | 歌词、规则和文字属性级联 |
| SpeakerMix、VoiceContext、InputConversion、SpeakerMixValidation、SingerSessionCache、InferCache | VoiceAndInference | 真实声线模型、输入与缓存共用生产实现，移除空 stub |
| AutomationOption、UiLanguage、通用 InferenceOption | Preferences | 配置与语言行为复用 EditorPreferencesCore |
| InferenceOption 与 ExecutionProvider 的默认/CUDA 编译分支 | InferenceProviderDefault、InferenceProviderCuda | 同一套 Qt Test 源码验证编译时 Provider 选择、设备缺失时回退及有效 Provider 发布。不代表实际 GPU 推理 |
| ICU wrapper | IcuWrapperTests | 保留项目依赖的 ICU 包装验证 |
| AutomationCore、AutomationIdempotency、AutomationTaskRaces、AutomationAdmission、AutomationFileGuard | AutomationRuntime | 调度、任务、幂等、准入与文件授权。文件 fixture 每例独立 |
| AutomationEditingDomains、NoteTransfer、PianoRollNoteCommit | ProjectEditing | 核心编辑、数据转移和控件提交桥接。保留独立 slot 及数据行 |
| AutomationApplicationDomains（原 RuntimeDomains/L3ApplicationDomains）、AutomationAsyncFileDomains | ApplicationServices | Host、设置/播放/规则/包及异步服务按职责分文件，共用必要 Harness |
| DocumentWorkflow、AutomationDocumentLifecycle、ProjectConverterAtomicWrite、MidiImportAutomation | DocumentIO | 生命周期、历史、真实文件往返和发布。用例边界重置历史，删除空 AppContext specialization |
| AudioAssetResolution、AudioDecodingController | AudioAssets | 路径/哈希/来源与解码控制。后者改用真实 Headless AppContext，移除生产方法替写 |
| ApplicationWorkflows | ApplicationWorkflows | 保留共用 Headless 环境中的推理完成、重启、声学许可及离线导出回归 |
| AutomationWire（含 Cursor）、PublicAutomationRegistry、McpHttpServer | AutomationProtocol | Wire、注册与分派、HTTP 生命周期使用一个 suite，保持协议行为边界 |
| DsConnectorLite 测试 | Connector | 真实 Connector、连接与 stdio 行为 |
| StartupArguments、SingleInstance | Bootstrap | 参数与单实例身份、协议及平台行为 |
| HeadlessProcessIntegration、McpProcessIntegration、跨 Host 进程场景 | ProcessIntegration | 共用进程与数据隔离设施，按入口及平台条件保留行为 |
| HeadlessResources | ModelResources | 默认内置声库和 CPU 模型执行，可显式覆盖资源 |
| Theme（原 ThemeColors/ThemeIcons）、TwoLevelComboBox、ElasticAnimation | GuiComponents | 主题、菜单和组件动画。SingerMenuDisplay 的有效行为继续保留 |
| OverlaySplitter、AnimationSettings | NativeDesktop | 需要原生窗口系统的布局与动画设置 |
| EditorControllers（原 EditorViewController/UndoRedoController）、EditorViewportController、EdgeAutoScroll、PianoRollInteractions、TrackEditorInteractions、ScrollBarInterplay、EditorShortcuts | EditorInteraction | 控制器、视口、输入、焦点与滚动按组件分文件 |
| DocumentScopedStateReset | EditorInteraction | 文档状态重置、保留布局偏好、实际通知和重复重置。文档提交顺序归入 DocumentIO 的现有生命周期场景 |
| EditorGlyphAtlas、EditorRhiGeometry、WaveformRenderUtils | EditorRendering | 字形、几何及波形组件，保留实验后端范围说明 |
| ApplicationGui（原 PianoRollGuiIntegration） | ApplicationGui | 共用实际应用环境，保留钢琴窗、轨道、参数、剪贴板、导出配置并补外观设置与声线混合交互 |

这些归并将原测试主体变为同一 Qt Test 类的领域 slot。需要资源、原生桌面或编译宏变体的入口按实际条件独立。fixture 的共享不等于共享每例状态，文档、历史、临时文件、任务与信号连接必须在用例边界复位。

GuiComponents、NativeDesktop、EditorInteraction 和 EditorRendering 与应用共用生产 `EditorGuiCore`。配置、领域和推理分别复用 Preferences、Automation 和 Inference 组件，不再在各测试中重复编译相同生产源。完整应用接线继续由 ApplicationGui、AudioAssets 和 ApplicationWorkflows 的真实 Runtime 验证。

## 4. 旧演示入口

AnchoredCurve、NewStyle、OpenGLWidget、ParamEdit、InsertTable、StateMachine、Cascader 是无正式产品引用的实验实现，已删除。原 ElasticAnimation 的有效行为在 GuiComponents 中验证生产 ElasticAnimator 的收敛、目标替换与信号。SingerMenuDisplay 的实际菜单选择与显示检查也归入该套件。

## 5. 未执行和遗留范围

设备、GPU、文件删除和图形后端的运行条件按测试计划报告，不能把跳过算作通过。GAME/RMVPE 实际模型、主观视听和确认无产品入口的旧实现保持约定边界，不为比例补测或删除。已复现但未修复的 synthrt 同版本显示元数据、跨包能力问题集中记录在[测试报告](test-report.md#6-遗留问题与明确边界)。完整统计边界见[测试大纲](test-outline.md#10-业务逻辑覆盖统计边界)。
