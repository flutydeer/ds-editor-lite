# Automation Facade 迁移矩阵

## 1. 口径

当前内部能力面包含 **208** 个集中 Operation ID。每项能力由类型化 C++ Facade、Dispatcher
显式路由和真实 handler 承载；`OperationIds::all()` 是运行时能力集合的唯一来源。

本矩阵记录产品入口如何收敛到领域 Facade，以及这些边界由哪些行为测试保护。它不复制
`OperationIds.h` 为第二份精确 ID 清单，不维护 `OperationCatalog` 或 `OperationDescriptor`。

创建类 DTO 可以携带请求内 `client_ref`；Facade 在实际分配对象 ID 后通过
`MutationResult.createdObjects` 返回有序绑定。该元数据不写入 Model 或工程文件，预检也不分配
绑定。

GUI 可以保留输入、绘制、hover、拖动预览和对话框；最终业务提交由对应 Facade 完成。

## 2. 产品入口、领域 Facade 与保护行为

| 域 | 产品入口/适配器 | 领域 Facade/运行时 | 关键保护行为 |
|---|---|---|---|
| 应用生命周期 | `AppController`、主菜单 | `ApplicationAutomationFacade` | info、WindowId、退出/重启宿主调用；生命周期 operation 保持 InternalOnly |
| 文档 | `DocumentWorkflowController` | `DocumentAutomationFacade` | generation 轮换、失败不替换、savepoint、revision、旧 ID |
| 轨道/片段/音频素材 | `TrackController`、`AudioDecodingController`、导入器 | `ProjectAutomationFacade` 及公共编排 | 对象解析、浅层创建、no-op、批提交、声音上下文和音频晚到写回 |
| 音符/歌词/音素 | `ClipController`、音符交互 Controller | `NoteAutomationFacade` | 增删改、量化、拆分、歌词/语言/发音/音素、单 History/revision |
| 参数与 Speaker Mix | 参数编辑器、Clip 工具栏、Track 控件 | `ParameterAutomationFacade` | 有界查询、采样曲线、锚点、固定/动态混合和关键帧 |
| 时间轴/Master | `AppController`、Tempo/拍号视图 | `TimelineAutomationFacade` | tempo、拍号锚点、Master 细粒度控制、no-op、Undo/Redo |
| History | `UndoRedoController` | `HistoryAutomationFacade`、`CommandCommitter` | 空栈、单次 record/revision、focus 回放 |
| 推理 | `InferenceAutomationBridge`、推理 adapter | `InferenceAutomationFacade` | capability/status/start、stage 分类、base revision、对象复检和原子写回 |
| 提取任务 | Pitch/MIDI Extract Controller | `ExtractionAutomationFacade` | TaskId、取消、提交点、旧 generation 丢弃 |
| 导出与格式 | 导出器、`AppController`、格式注册表 | `AudioExportAutomationFacade`、`FileAutomationFacade` | capabilities、inspect/preview/start、文件策略、任务和 cleanup |
| Task | 各异步领域 | `TaskAutomationFacade`、`AutomationTaskManager` | list/get/cancel、终态保留、generation 隔离 |
| 播放与 loop | `PlaybackController`、播放栏、时间线 | `PlaybackAutomationFacade` | 瞬时状态不增 revision；持久 loop 单 History/revision |
| Editor GUI 状态 | `EditorViewController`、Track/Clip Controller | `EditorAutomationFacade` | WindowId、DocumentId、视口、选择、焦点和面板所有权 |
| 设置/Recent/搜索路径 | 设置页、Audio、FillLyric、DocumentWorkflow | `SettingsAutomationFacade` | allowlist、稀疏更新、候选值、持久化回滚、路径规范化 |
| Speaker Mix 预设 | `SpeakerMixPresetStore` | `PresetAutomationFacade` | list/save/delete、重复 ID、运行期元数据不入工程 |
| 包 | 包管理器、`ProjectPackageResolver` | `PackageAutomationFacade` | list/refresh/validate/resolve、application task 和文档版本 |

行为与编译期测试守卫上述边界；`TestAutomationCore` 负责 Dispatcher、Session、显式幂等、Task、
`OperationIds::all()` 和各 Facade 的基础契约。领域、维度、文件、并发和 GUI 测试补充对应行为，
不依赖精确 Descriptor 镜像或源码文本扫描。

## 3. 当前内部能力域（208）

| 域 | 数量 | 域 | 数量 |
|---|---:|---|---:|
| application | 3 | audio_clips | 9 |
| clips | 17 | documents | 10 |
| editor | 25 | exports | 7 |
| extract | 3 | formats | 2 |
| history | 3 | imports | 1 |
| inference | 15 | master | 6 |
| notes | 21 | packages | 6 |
| parameters | 12 | playback | 10 |
| project | 1 | recent_files | 4 |
| settings | 9 | speaker_mix | 17 |
| speaker_mix_presets | 3 | tasks | 3 |
| tempos | 2 | time_signatures | 2 |
| timeline | 1 | tracks | 16 |
| **合计** | **208** |  |  |

内部能力和公共 MCP 工具面不是同一集合。内部提交、兼容入口与 GUI 生命周期 operation 可以保留
在 Facade 层而不公开；公共 Editor 工具为 177 项，另由 Connector 提供 6 项桥接工具。

## 4. 内部能力边界

以下内容不属于当前内部能力集合：

- 多真实 Session、DocumentRegistry、跨文档 batch/复制/拖放；
- 多窗口创建、关闭、绑定、切换及 WindowRegistry；
- 远程控制和任意进程启动；
- 音频导出中尚无真实后端的 loop/selected-range 等占位能力；
- 只有路线图/TODO、没有真实 handler 的能力；
- 被动绘制读取、hover、动画和拖动过程中的临时 preview。

新增真实能力时应同时增加类型化 handler、集中 Operation ID、显式 Dispatcher 路由和必要行为
测试；不为尚不可达的计划项制造占位 operation。
