# Automation Facade 行为测试清单

## 1. 审计口径

当前内部能力面由 `OperationIds::all()` 中的 **208** 个唯一 Operation ID 定义。Dispatcher 对受
支持 ID 使用显式类型化路由；领域 Facade 的 C++ 签名、DTO 和行为测试构成实现契约。

本清单索引测试职责，不复制 `OperationIds.h` 为第二份精确名册，也不维护
`OperationCatalog`、`OperationDescriptor` 或源码文本扫描门禁。最终执行结果、CTest 数量和耗时
只记录在 [test-report.md](test-report.md)，本文件不把静态存在性解释为本轮通过。

## 2. 内部能力域

| 域 | Operation 数 | 主要行为范围 |
|---|---:|---|
| application | 3 | 应用信息与内部生命周期请求 |
| audio_clips | 9 | 音频素材查询、导入、重定位和派生状态写回 |
| clips | 17 | 片段查询、几何、属性和声音继承 |
| documents | 10 | 文档生命周期、查询、保存和导入编排 |
| editor | 25 | GUI 状态、视口、选择、焦点和参数子区域 |
| exports | 7 | MIDI/音频能力、预览、任务和清理 |
| extract | 3 | 提取能力、Pitch 与 MIDI 任务 |
| formats | 2 | 格式列举和文件检查 |
| history | 3 | 状态、Undo 与 Redo |
| imports | 1 | 批量导入最终提交 |
| inference | 15 | 推理能力、状态、任务和阶段写回 |
| master | 6 | Master 查询、细粒度控制与兼容内部入口 |
| notes | 21 | 查询、创建、几何、歌词、语言、发音与音素 |
| packages | 6 | 包、搜索路径、刷新、验证与声音解析 |
| parameters | 12 | 曲线查询、采样绘制和锚点编辑 |
| playback | 10 | 播放状态、定位与工程循环 |
| project | 1 | 内部工程快照 |
| recent_files | 4 | 最近项目内部 CRUD |
| settings | 9 | 设置查询和分域更新 |
| speaker_mix | 17 | 固定/动态混合、关键帧和兼容内部入口 |
| speaker_mix_presets | 3 | 预设 list/save/delete |
| tasks | 3 | Task list/get/cancel |
| tempos | 2 | Tempo 设置与删除 |
| time_signatures | 2 | 拍号设置与删除 |
| timeline | 1 | 时间轴快照 |
| tracks | 16 | 轨道查询、细粒度编辑、语言与声音 |
| **合计** | **208** | **集中 ID、显式路由和领域行为测试** |

内部 208 项不等于公共 MCP 工具面。公共 Editor 工具为 175 项，其中
`tracks.get_voice_context` 与 `clips.get_voice_context` 不单独公开；相应声音上下文由
`tracks.get` 与 `clips.get` 返回。

## 3. 测试目标与职责

具体 CTest 注册数量以本轮配置为准。核心职责按行为分层：

| 目标/目标组 | 职责 |
|---|---|
| `TestAutomationCore` | `OperationIds::all()`、Dispatcher、Session、Facade、History、revision 和 Task 基础行为 |
| `TestAutomationIdempotency` | 显式 opt-in 的同步/异步重放、冲突、失败释放和 generation 键空间 |
| `TestAutomationTaskRaces` | Task 状态机、取消/提交点、重复完成、revision/对象/generation 竞态 |
| `TestAutomationDocumentLifecycle` | new/open/import/save、失败回滚、savepoint、generation 清理和错误优先级 |
| `TestAutomationEditingDomains` / `Dimensions` | 编辑域正常、no-op、非法输入、History/revision、Undo/Redo 和失败原子性 |
| `TestAutomationRuntimeDomains` / `Dimensions` | application、playback、editor、settings、recent、packages 和 presets 行为 |
| `TestAutomationL3ApplicationDomains` | 公开 L3 设置、包刷新与歌词规则所需的内部 Facade 行为 |
| `TestAutomationAsyncFileDomains` / `Dimensions` | inference、audio、import/export/extract、文件失败和 Task 终态 |
| `TestAudioAssetResolution` | 相对路径、source generation、解析/解码协议和晚到写回隔离 |
| `TestAutomationFileGuard` | canonical path、读写根、会话授权和实际 I/O 前复核 |
| `TestAutomationAdmission` | global 32 与 background 8 两个准入上限及计数释放 |
| `TestPianoRollNoteCommit` | GUI 音符插入/拆分的 created ID、revision 和失败无副作用 |

控制器层回归补充 GUI host 转发、可见状态和 History focus；它们不替代 Facade 行为断言。

## 4. 行为维度

### 4.1 Query

- 最小状态、有数据状态、空集合、边界值和 Unicode；
- 显式 DocumentId/WindowId、旧 generation、未知对象与 host unavailable；
- 返回 detached snapshot，不泄漏 Model/QObject/QWidget；
- 不修改 Model、History、revision、文件、Task 或通知状态。

### 4.2 同步 Command

- 正常提交、合法 no-op、非法输入和稳定错误优先级；
- 一次原子提交、适用时一条 History 和一次 revision；
- 失败不留下部分模型、持久化、文件或 GUI 状态；
- 只有明确支持的请求接受 `validate_only` 或 `idempotency_key`；不带幂等键的调用不哈希、不进入
  幂等存储；
- 创建操作返回稳定 `client_ref → object_id` 绑定，失败和预检不分配 ID。

### 4.3 异步 Command

- Queued、Running、CancelRequested、Committing 和稳定终态；
- 排队/运行取消、重复取消、不可取消提交点和一次最终提交；
- 捕获不可变输入，并在写回前复核 document generation、revision、对象与文件授权；
- Editor 重启、文档换代、对象删除或输入变化后，晚到结果不能写入错误目标。

## 5. 跨域边界

| 契约 | 验证重点 |
|---|---|
| 文档身份 | new/open 换代、旧 ID 拒绝、save/save-as 不换代、失败保留旧 Session |
| History/revision | no-op 不推进；编辑提交、Undo/Redo 与 savepoint 边界确定 |
| 原子批量 | 全量校验后单次提交；失败不分配 ID、不留下部分对象 |
| 显式幂等 | 仅 opt-in 工具进入键空间；重放、冲突、并发去重和失败释放 |
| Task 竞态 | cancel/commit、重复完成、对象删除、revision 前进和 generation 换代 |
| GUI 等价 | GUI 和自动化入口复用同一领域 Facade；可见状态在模型信号后同步 |
| 文件边界 | canonical path、读写根、覆盖策略、重新授权和失败清理 |
| 路由完整性 | `OperationIds::all()` 是能力来源；未知 ID 稳定失败，已支持 ID 具有显式路由和行为证据 |

## 6. 维护规则

- 新增、移除或重命名内部 ID 时，同步调整对应领域行为测试和本清单域计数。
- 不为元数据完整性复制一份 Descriptor 表；必要约束落在类型、显式路由和行为测试中。
- 测试结果只在最终候选上运行一次完整 CTest 后写入测试报告。
- 文档不记录用户名、本机绝对路径、真实端口、PID 或用户素材名称。
