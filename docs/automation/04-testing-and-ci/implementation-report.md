# 四期实现报告

## 1. 最终交付

本期将随功能零散增长的测试整理为按产品职责维护的 Qt Test 套件，使用 CTest 统一执行，建立三平台完整产品 CI，并通过真实工作流补测修复生产问题。本报告按相对 main 的最终代码差异说明交付，不把仅新增测试的既有能力列作产品修复。

设计原则见[实施计划](implementation-plan.md)，覆盖内容和程序分工见[测试大纲](test-outline.md)，测试入口见[覆盖矩阵](test-coverage-matrix.md)，实际验收及遗留范围见[测试报告](test-report.md)。

## 2. 测试结构和生产复用

### 2.1. 套件与执行入口

原大 main、布尔用例包装及按历史功能拆分的程序收拢到领域套件。用例由 Qt Test slot 和数据行独立报告，一个程序执行一次 qExec。CTest 注册程序级标签、环境、超时和资源锁，不维护函数镜像。历史入口及演示程序的去向集中在[覆盖矩阵](test-coverage-matrix.md#3-目标收敛与历史入口去向)。

tests 和 coverage preset 使用独立构建目录。lite_tests 聚合全部测试，tests build preset 同时构建 Editor、Connector 和测试。ICU wrapper 纳入统一发现和执行。用例统一采用 tst_ 文件名，多文件套件共用入口和声明头。

### 2.2. 共用组件与必要重构

| 组件或重构 | 最终职责及影响 |
|---|---|
| EditorAutomationCore | 模型、领域编辑、Facade、歌词规则和剪贴板等共用实现 |
| EditorInferenceCore | 推理输入、音符快照、音节、混合校验和缓存辅助 |
| EditorPreferencesCore | 配置、推理选项、语言与客户端设置 |
| EditorGuiCore | 视图、控制器、视口、菜单、绘制组件和资源 |
| EditorRuntime | 完整应用装配，供 Editor 和实际应用测试复用，入口 main 保持独立 |
| DSPX 内存编码 | 保存和测试内容快照共用编码器，替代遗漏片段内容的测试快照。文件校验及原子发布仍属于保存流程 |
| 参数与剪贴板 | 共用曲线编码、几何/值域/拓扑校验及粘贴准备。参数整体替换复用已有事务入口 |
| 两种渲染器 | 共用视口缩放约束和锚点插入布局。预览使用局部节点副本，不临时修改真实尾节点 |
| 动态声线 | 模型、自动化插入、推理采样和界面共用分段插值及关键帧表示 |
| 查询与界面服务 | 公开查询复用 Expected 结果组合，搜索复用领域查询，文本菜单共用语义转换，音素波形复用正式解码任务 |
| 外部依赖控制 | GPU 枚举、Tap Tempo 单调时钟、电平保持/衰减时间可受控，默认仍使用生产实现和原默认参数 |

无产品调用入口的旧 G2pPage、PseudoSingerPage、TrackSynthesizer 及其空引用已移除。其余经确认暂保留的清理候选见[统计边界](test-outline.md#10-业务逻辑覆盖统计边界)，不为覆盖率继续删除。

### 2.3. 隔离与素材

ProcessFixture 统一数据根、配置、访问根、临时文件和子进程所有权。实际运行时夹具共用应用生命周期，每例复位文档、历史、任务和通知。完整内容、文件故障、线程池时序、剪贴板及输入辅助按组件复用。

测试构建支持 DSEL_TEST_DATA_ROOT 和 DSEL_TEST_PLUGIN_ROOT，配置、缓存、日志、单实例身份和插件部署能够隔离。正式构建不识别这些测试覆盖入口，不增加公开工具或产品 CLI。

仓库提供独立身份的微型声库 ZIP，支持中英文、两条声线和完整推理阶段。确定性 ONNX 图通过正式加载器及执行器生成可验证音频，不含真实训练权重。另有可加载但执行失败的微型声码器素材，用于真实失败恢复。说明见[测试资源](../../../src/tests/resources/README.md)。

## 3. 生产问题修复与行为变化

下表只列最终差异中的生产改动。测试如何覆盖这些行为由覆盖矩阵负责，本节不重复逐次执行结果。

### 3.1. 文档、历史和数据保真

| 原有问题 | 最终行为 | 主要实现 |
|---|---|---|
| 丢弃已保存的 redo 分支后，失效保存点可能误判 dirty | 删除该历史条目时同时使保存点失效，新分支正确保持未保存状态 | HistoryManager |
| 文档换代保留旧轨道/片段选择，导致新工程快捷键粘贴失效 | 文档恢复 Active 后通过既有 Facade 清理过期选择 | DocumentWorkflowController |
| 整唱段复制遗漏参数内容 | 音符和整片段复用曲线编码，保留各参数层及音符范围外曲线 | ClipsInfo、ParameterCurvesJson |
| 损坏参数标识、样本、锚点、重复组或重叠曲线被错误接收，部分坏组被静默截断 | 严格读取整数并复用完整曲线校验。直接编辑原子拒绝，剪贴板舍弃整个受损组并保留其他组及音符，首个完整合法组保留 | ProjectAutomationDtos、ParameterCurvesJson、ProjectAutomationFacade |
| 粘贴相对时间在检查前溢出 | 使用宽整数计算并检查目标范围，任一无效草稿拒绝整批准备 | ClipsInfo::preparePaste |
| 搬迁 DSPX、批量导入和未保存工程丢失音频定位基础 | 每个音频片段保留来源工程目录，解析、剪贴板、DTO 和撤销快照共同传递 | AudioClip、ProjectLoadSessionBase、ResolveAudioPathTask |
| 另存到其他目录后，缺失音频无法从原位置恢复 | 保存时保留来源路径及回退定位基础，不修改内存来源 | DspxProjectConverter |
| 音频重链接撤销增加原本不存在的格式字段，重做丢失后来完成的摘要 | 历史动作保留字段存在性、来源目录及同路径已完成摘要 | EditAudioClipPathAction |
| 经文件快照加载的工程名称带临时文件前缀 | 转换及加载保留原始文件身份，标题使用原始名称 | IProjectLoadSession、ProjectLoadSessionBase |
| DSPX 写入提交失败只有文件路径，缺少底层原因 | 返回 QSaveFile 的实际错误信息 | DspxProjectConverter |
| MIDI 导出重复计算片段偏移，裁剪时间和元数据选项不正确 | 使用局部音符时间及正确片段几何，关闭速度/拍号导出时不再由中间格式补回默认数据 | MidiConverter |
| MIDI 拍号分母白名单窄于编辑器时间线能力 | 共用时间线投影校验，接受产品支持的拍号并拒绝无效位置和值 | MidiTrackGenerator |
| 声库暂不可用或来源减少后 DSPX 混合信息丢失 | 未解析资源保留保存的引用，过滤已解析来源时按原来源数恢复、筛选和归一化权重 | DspxProjectConverter |

### 3.2. 歌词、声线与参数编辑

| 原有问题 | 最终行为 | 主要实现 |
|---|---|---|
| 歌词拆分遇到空匹配重复前缀 | 回退保留原文本，不重复累积已匹配内容 | TextSplitter |
| LRC 整秒、小数秒换算不一致，失败重载残留旧时长及读取位置 | 统一时间单位解析，清理旧文档和迭代状态 | LrcDecoder |
| 填词网格跨行移动后菜单仍指向旧行，选择和行序不稳定 | 单元格重新绑定目标行和样式，按稳定行序移动，释放场景抓取后确定最终选择 | CellList、LyricWrapView |
| Splitter/Tagger 再次 Apply 改变规则身份，排序丢失未应用详情 | 草稿创建时分配稳定 ID，详情和 DTO 保留 ID，排序前保存当前详情 | SplitterConfigTab、TaggerConfigTab |
| 界面与接口的歌词正则完全匹配分别漏报或误报 | 由领域查询统一编译完整匹配表达式，界面复用相同查询 | NoteAutomationFacade、SearchDialog |
| 动态混合编辑重建已有关键帧身份 | 草稿复用模型关键帧，只对新增节点产生新身份 | SpeakerMixEditorView |
| 非零起点唱段的上一/下一关键帧定位错误 | 查询时转换为片段局部时间，播放头与视口定位转换回工程时间 | ParamEditorView |
| 重开没有预设的混合对话框覆盖自定义比例 | 保留调用者混合，仅在没有有效来源时初始化等权值，预设菜单使用异步 popup | SpeakerMixDialog |
| 固定混合切换单声线后公开查询丢失主声线 | 正规化前保留有效主声线身份，轨道和片段共用解析结果 | PublicAutomationRegistry |
| 公开默认语言与模型实际继承语言不一致 | 快照单独记录有效语言，查询、默认歌词、填词及音素路径复用 | PublicAutomationRegistry |
| 点击已占用锚点时间仍产生编辑发布 | 先选择已有节点，只有实际新增节点进入事务和提交 | AnchorEditController |
| 锚点菜单弹出使画布失活，清空菜单操作需要的选区 | 菜单打开期间保留选区，其他失活路径仍取消编辑 | ParamEditorGraphicsView |
| 参数替换与转移各自维护校验和提交逻辑 | 共用参数变更及曲线校验，保留预验证、no-op、身份和失败原子性 | ParameterAutomationFacade、ProjectAutomationDtos |

### 3.3. 推理、任务和声库

| 原有问题 | 最终行为 | 主要实现 |
|---|---|---|
| 初始化提前失败或关闭时等待者未被唤醒，初始化任务清理随引擎丢失 | 各退出路径统一发布初始化完成，清理连接独立于引擎生命周期 | InferEngine、SynthrtEngine |
| 后台初始化与主线程注册服务并发访问单例容器 | 注册、查询和清空使用同一互斥锁，对象存活期仍由宿主负责 | SingletonRegistry |
| worker 在取消检查与订阅之间完成，队列失去接续通知 | 先订阅再观察完成状态，复用既有取消清理 | TaskQueue |
| 外线程尚在发出完成通知，任务已被清理删除 | 在 QObject 所属线程发布停止与完成，结果消费及清理显式排队 | Task、推理/音频/文件消费者 |
| 终态历史继续持有回调捕获的后台许可 | 转移完成回调时显式清空原值，在锁外调用 | AutomationTaskManager |
| 替换或取消流水线后旧状态转换继续写回，替代任务停在队列 | 先安装任务清理，再停止并移除旧状态机，任务入口检查停止状态 | InferController、InferPipeline、BaseInferState |
| 重复推理沿用旧 Ready，阶段重置后仍报告就绪 | 启动同步发布 Pending，重置相应阶段及声学就绪状态 | InferPipeline、InferenceAutomationAdapter |
| 自动推理关闭时显式手动请求停在声学等待 | 为本次请求传递声学许可，完成或取消后清除 | InferPipeline、声学准入状态 |
| 已接受推理在延迟启动前遇到无关版本变化 | 准入及终态接线完成后在同一事件轮次重置和入队，模型执行仍异步 | PublicAutomationHostAdapter |
| 删除目标后公开任务仍运行，轨道请求无法关联片段查询 | 按实际准入乐句关联任务，在完整编辑/撤销后验证目标，删除进入明确终态 | PublicAutomationHostAdapter |
| 取消后编辑参数、恢复播放或撤销删除无法继续推理 | 从正确依赖阶段恢复缺失流水线。播放按窗口恢复每个登记候选一次，不因连续位置通知反复重试失败推理 | InferController |
| 声学缓存命中绕过取消检查，语言服务失败仍保留待写回任务 | 统一结果发布前检查取消，语言失败清理暂存并取消读音及音素任务 | InferAcousticTask、InferController |
| 包刷新被拒绝时，SDK 已改变共享推理会话 | 将准入放到会发布会话的刷新调用之前 | PackageManager |
| 混合扫描有效和损坏声库时丢失损坏项原因 | 适配完整 RefreshResult，保留快照之外的刷新诊断 | PackageManager、SynthrtEngine |

### 3.4. 音频、播放和设备

| 原有问题 | 最终行为 | 主要实现 |
|---|---|---|
| 无设备的公开播放请求被模态错误框阻塞 | 公开调用返回失败，仅 TrustedGui 操作显示设备提示 | AudioContext |
| 暂停后停止不回起点，快速恢复播放被旧暂停通知打断 | 已暂停的停止请求直接按策略回位，消费通知时核对当前传输状态 | AudioContext |
| 主输出声像、静音只改变模型，没有影响实际音频 | 模型通知同步实际 mixer 的声像和静音 | AudioContext |
| 离线导出恢复原本关闭的 mixer 时调用 open(0,0) | 按原打开状态恢复，已打开时恢复原缓冲和采样率 | AudioExporter |
| 多声道解码误缩减帧数，缩略峰值漏掉分组尾块 | 按实际帧数遍历各声道，完整聚合峰值及尾部 | DecodeAudioTask |
| 读取错误或提前 EOF 被当作解码成功 | 返回明确失败并清空部分波形缓存，音素波形也复用该解码路径 | DecodeAudioTask、PhonemeView |
| 无 MIDI 服务导致启动异常，切换失败破坏当前设备 | 集中捕获枚举/打开失败，保留无设备或原设备状态 | MidiSystem |
| 采样率变化改变合成预览的 decay 时长 | 与 attack/release 一起重新换算 decay 样本数 | SettingPagesSynthHelper |
| 公开 Ogg/MP3 导出没有传递有损格式确认 | 显式格式选择转换为相应导出策略 | PublicAutomationHostAdapter |
| 导出完成后关闭只隐藏进度窗口，配置公开复制依赖不完整私有类型 | 终态接受关闭事件，复制构造与赋值在实现文件定义 | AudioExportProgressDialog、AudioExporterConfig |

### 3.5. 编辑视口、输入和界面反馈

| 原有问题 | 最终行为 | 主要实现 |
|---|---|---|
| 零长度唱段使 Legacy 缩放除零，后续正常工程也不可见 | Legacy/RHI 共用未缩放内容尺寸、填满下限和倍率限幅，空内容及失效倍率可恢复 | EditorViewportScale、TimeGraphicsView、EditorViewportController |
| 窗口增大只更新一轴填满约束，缩放后的动画逻辑视口偏移错误 | 分别调整两轴，在场景坐标中计算动画目标 | TimeGraphicsView |
| 首轨片段或顶端音符绘制边框导致历史定位误判 | 历史定位使用对象主体范围 | TrackEditorView、PianoRollGraphicsView |
| 隐藏当前编辑页后再次请求该页没有显示面板 | 页面选择同时恢复可见状态 | EditorAutomationFacade |
| 跨轨移动撤销后钢琴窗仍使用目标轨道颜色 | 根据片段模型通知重绑所属轨道颜色，移除鼠标提交专用刷新链 | ClipEditorView、ProjectAutomationFacade |
| 发音文字可被触控识别，但选择/拖动只命中音符主体 | 选择、按下和右键共用文字所属音符判断 | PianoRollGraphicsView、SelectNoteHandler |
| 笔擦除或窗口失活后遗留按下状态，下一笔无效 | 失活、触控取消和笔中断共用指针清理，恢复借用工具 | PianoRollGraphicsView |
| Windows 的非原生 Qt 后端忽略已有笔悬停事件 | 按实际 QPA 后端选择悬停实现 | EditorPenHoverWatcher |
| 触控转鼠标污染原触点，空触点取消被忽略，弹窗重开后首次轻触失效 | 独立重放设备状态，保留最近位置处理系统取消，隐藏下拉框时结束输入流 | TouchClaimFilter、ComboPopupTouchFilter |
| 到期长按的释放先于计时器回调，菜单丢失，重新接触仍继续惯性 | 释放前处理已经到期的长按，新按下立即停止惯性 | EditorTouchController、InlineTextEditOverlay |
| RHI 编辑目标删除后草稿悬留，析构事件访问已释放私有状态 | 目标失效时取消内联草稿，私有状态释放后事件交给基类 | PianoRollRhiWidget |
| RHI 回退后轨道滚动仍访问旧画布 | 滚动连接绑定实际画布生命周期 | TrackEditorView |
| 时间轴切换唱段丢失当前反馈，过渡结束后持续重绘 | 更换前解除旧订阅，运行分段和有效过渡统一决定计时器 | TimelineView |
| 图标透明像素在 RHI 图集里成为实心覆盖 | 图集保留源透明度并使用预乘遮罩 | EditorGlyphAtlas |
| Fader/Pan 释放后残留拖动，连续拖动被自制双击窗口误判复位 | 释放正确结束拖动和提交，复用 Qt 双击判定 | Fader、PanSlider |
| SeekBar 右键意外改值，普通键盘复位未执行 | 只让左键进入编辑路径，Return/Space 正常按下复位 | SvsSeekbar |
| 电平悬停读数依赖全局光标，循环快捷键只改变按钮，速度弹窗遗留旧显示 | 使用事件坐标，快捷键进入正式点击提交，编辑后恢复播放头当前位置的速度显示 | LevelMeter、PlaybackView |
| 日志等辅助窗口的快捷键被编辑器抢占 | 匹配前按活动窗口调整编辑器快捷键作用域 | EditorShortcutUtils |
| 文本菜单只读状态图标错配，目录取消编辑仍删除路径，外部 drop 被内部移动覆盖 | 菜单按动作语义转换，取消依据模型值，目录按复制引用处理并拒绝远程文件 URL | Menu、PathEditor、PathListWidget、FileSelector |
| 提取来源选择按 Escape 后仍保留旧选项 | 拒绝出口统一清理，菜单只在确认后执行 | ExtractPitchParamDialog、MainMenuView |

### 3.6. 设置、翻译和对象生命周期

| 原有问题 | 最终行为 | 主要实现 |
|---|---|---|
| 设置落盘失败，页面仍显示未提交值或提示重启 | 通用、外观、推理和开发者页面恢复正式值，失败不提示成功或重启，解除故障后可重试 | 各设置页 |
| 主题切换另走一套应用/保存逻辑 | 页面使用正式设置服务完成主题应用及持久化回滚 | AppearancePage |
| 非 Windows 构建仍可选择 DirectML | 界面及公开候选共用构建可用性判断 | InferencePage、AppOptionsAutomationAdapter |
| 无输出驱动访问空指针，切换驱动后列表订阅仍指向旧驱动，设备偏移错误 | 统一刷新设备控件并订阅当前驱动，按实际插入位置选择设备，无设备时保留普通设置 | AudioPage |
| 音频设置回滚在后端不可重开时提前返回 | 先恢复控制值及热插拔策略，再恢复设备 | AppOptionsAutomationAdapter |
| 中文切换依赖开发机 Qt SDK 翻译目录 | Qt 基础翻译与应用目录合并为内嵌资源，统一安装、移除及英文回退 | UiLanguageManager、应用 CMake |
| 外部主题错误指向内置清单 | 诊断使用实际选中的清单路径 | ThemeLoader |
| 主窗口、工具栏或设置页销毁后仍有裸指针/通知引用 | 默认对话框父窗口改弱指针，解除文档 UI，通知绑定接收者，工具栏私有对象和状态树模型有明确所有者 | Dialog、MainWindow、AppController、ClipEditorToolBarView、DeveloperPage |
| Toast 的延时回调访问旧窗口，替换工具提示泄漏标签 | 弱指针及上下文清理停止旧回调，释放被替换内容 | Toast、ToolTip |

### 3.7. 自动化传输与启动

| 原有问题 | 最终行为 | 主要实现 |
|---|---|---|
| GUI 支持的调制、Shape/Scale 被公开接口拒绝或错误读回 | 补齐输入/输出模式与实际枚举的双向映射 | PublicToolContract、PublicAutomationRegistryAdvancedGui |
| 音频确认/重定位返回任务 ID，却不能公开查询或取消 | 纳入统一公开任务类型，提交前重新验证文件读取授权 | PublicValueDefinitions、PublicAutomationHostAdapter |
| HTTP 退出在响应发完前断线 | 有序断开并在共同截止时间内排空已生成响应，慢客户端不无限阻塞退出 | McpHttpServer |
| 次实例误判同步写完的数据为失败，监听启动期间漏接已到达请求 | 按待发送字节决定等待，监听前连接接入处理，接入即读取缓存首帧，保留 ACK 诊断 | SingleInstanceCoordinator |
| POSIX stdout 断管直接触发 SIGPIPE，绕过正常错误反馈 | Linux 屏蔽写出线程信号，Darwin 在 stdout 描述符禁用该信号，经原 EPIPE 路径结束 | StdioTransport |

## 4. 上游库补丁

最终提交的第三方源码补丁仅位于[项目覆盖端口](../../../scripts/vcpkg-project/README.md)，优先于共享 overlay。每个库仍只链接一份，完整配方用于 vcpkg 构建和缓存追踪。

| 库 | 补丁 | 修复的上游行为 |
|---|---|---|
| talcs | fix-loop-position.patch | 循环跨块回绕后按剩余读取长度推进位置，避免跳采样 |
| talcs | fix-buffering-mode-position.patch | 预读切换时清理后台读取并恢复位置，直接读取同步推进位置，保证离线导出起点和后续采样连续 |
| talcs | fix-clip-series-read-serialization.patch | 片段读取与起点/范围修改使用同一互斥，覆盖普通和 Future 片段源 |
| talcs | fix-sdl-driver-startup-lifetime.patch | SDL 延迟启动绑定驱动生命周期，停止或销毁后不启动旧线程，不覆盖已发出的停止请求 |
| talcs | fix-asio-com-lifetime.patch | ASIO 只配对释放成功初始化的 COM，处理失败和重复初始化 |
| talcs | fix-output-device-release.patch | 释放设备前清空基类公开引用，切换驱动失败不遗留悬空设备 |
| RtMidi | fix-coremidi-exception-specification.patch | 移除 CoreMIDI helper 错误的 throw() 规格，使初始化错误可捕获，并释放失败路径的临时 CFString |

talcs 配方基于 v0.1.0，RtMidi 配方基于 6.0.0。Linux manifest 显式请求 RtMidi 的 ALSA 特性。共享端口或上游纳入等价修复后，须经应用回归验证再移除对应覆盖。SynthrtEngine 和 PackageManager 的变化是应用适配层修改，已知 synthrt 遗留问题见测试报告。

## 5. 构建、部署与 CI 行为

- CMake 最低版本统一为 3.24，Apple 显式启用 Objective-C++。生产库明确声明提取模块依赖，MSVC 大型注册源码在实际编译目录配置 /bigobj。
- Linux x64、Windows x64、macOS arm64 使用 latest runner，完整构建 Editor、Connector 和测试。Linux 独立 Coverage 步骤复用测试数据，Windows/macOS 使用普通 Debug 全量测试。
- macOS 先放齐插件和 FFmpeg 私有依赖。清单、词典、模型归入 Resources，以包内相对链接兼容加载器，交给 Qt 部署工具处理二进制依赖及签名，最后严格校验。旧手工 dylib 路径修补脚本移除。
- macOS 原生窗口效果限定 Cocoa。offscreen/minimal 统一使用兼容样式，独立运行时测试从实际 Editor 取得规则、插件和 Frameworks。
- Qt/vcpkg 缓存按平台、镜像、Qt、manifest 和端口区分，每轮核对 ABI，支持依赖更新后的增量准备。构建目录不缓存。
- 三平台保留环境、构建及测试日志、JUnit 和失败素材。macOS crash diagnostics 收集系统崩溃报告，作为诊断附件，不作为额外测试。
- Windows 原生静态插桩及 macOS LLVM 采集入口保留为本地能力。原始生产源码报告与业务边界核算分开，具体方法见[测试报告](test-report.md#51-统计口径)。

## 6. 取舍和交付边界

本期采用 Windows 完整环境业务覆盖 90% 为参考，三平台规定 CI 全部通过后验收。实际设备、GPU 和原生输入按条件报告，GAME/RMVPE 实际模型、主观视听及硬件组合不扩展。微型声库和服务替身验证生产接线，不替代真实模型品质与外部转换器算法。

六份文档按最终设计、能力、执行、实现和验证分别维护，适用至本期发布。逐轮日志、受测版本、覆盖明细及审查证据保留在本地产物和 [PR #187](https://github.com/flutydeer/ds-editor-lite/pull/187)，不再按测试轮次追加正文。
