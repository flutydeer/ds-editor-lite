# 一期 Automation Facade 全量测试大纲

## 1. 执行边界

本大纲用于实现完成后的全量验证。交付本大纲前先完成实现级保护测试、完整构建和
当前全部 CTest；待用户完成 GUI 基本功能冒烟并明确批准后，才由 Codex 使用
Computer Use 启动全量 GUI 回归并形成正式报告。

测试范围是 `OperationIds::all()` 中的 208 个 operation。测试按业务语义选择适用维度，重点验证
可达路由、结果、错误、原子提交与可观察副作用；不为每个 operation 机械复制同一组用例，也不以
固定场景数或断言数代替行为覆盖。

## 2. 测试环境与证据

正式报告记录以下不可省略的信息：

- git commit、分支、工作树状态、submodule 状态；
- Windows、MSVC、Qt、CMake、Ninja、vcpkg triplet 和关键依赖版本；
- Debug/Release 配置及完整命令；
- Operation ID 集合与 Dispatcher 显式路由快照；
- 每个测试目标的命令、耗时和结果；
- 真实测试文件、codec、声库、推理模型和音频设备资格；
- 首次失败、复现步骤、根因、修复 commit、回归结果；
- 未执行项、环境限制和残余风险。

确定性测试使用固定 seed、隔离临时目录、受控时钟/Task executor 和 fake host。涉及
真实文件、设备或模型的资格验证单独统计，不混入确定性通过率。

## 3. 共享基础维度

### 3.1 Query

Query 的共享所有者层按风险覆盖：

1. 最小有效状态与有数据状态的 DTO 完整性；
2. 返回值是快照，不泄漏 Model/QObject/QWidget 指针；
3. 未知/旧 DocumentId、未知 WindowId、模块或宿主不可用；
4. 空集合、边界数值、Unicode、长文本和可选字段；
5. 查询不改变 Model、History、revision、幂等缓存、文件和通知计数；
6. 代表性 Dispatcher 路由进入正确类型化 handler，错误和副作用语义与领域契约一致。

### 3.2 Command

Command 的共享所有者层按参数形状和状态转换覆盖：

1. 正常变更及 DTO/affected-object/通知结果；
2. 合法 no-op：`changed=false`，History/revision/业务通知均不变；
3. `validate_only`：完整验证并返回预测，不分配 ID、不占幂等键、无副作用；
4. 参数缺失、非法枚举、NaN/Inf、上下界、空集合、重复对象和领域冲突；
5. DocumentId → revision → 对象 ID/类型 → 领域约束的固定错误优先级；
6. 成功提交最多一个 History entry、revision 恰增一次，undo/redo 可逆；
7. handler/宿主/I/O 失败时不产生半提交；
8. 显式支持幂等且携带 `idempotency_key` 时的重放、冲突和并发去重；不带 key 时不计算指纹；
9. GUI-only 命令的有效/未知 WindowId，且不把 WindowId 混入文档身份。
10. 创建类命令的 `client_ref → object_id` 有序绑定、跨层唯一性、重放一致性，以及
    `validate_only`/失败请求不分配绑定。

### 3.3 异步 Command

异步 Command 在共享 Task/生命周期测试中覆盖：

- `Queued → Running → CancelRequested → Committing → terminal` 的允许迁移；
- 排队取消、运行取消、重复取消、终态后取消；
- 不可取消提交点，以及只产生一次最终提交；
- base revision 后有其他编辑、new/open 替换 generation、目标对象被删除；
- 同幂等请求只启动一个后端任务并返回相同 TaskId；
- 失败/提交前取消不占幂等键，已接受任务在 generation 内可查询；
- 任务捕获不可变输入，不依赖跨线程裸 Model/Track/Clip/Note 指针提交。

## 4. 单 Session 核心专项

### 4.1 生命周期与身份

- 启动 untitled Session 具有非空 DocumentId、revision 0；
- new/open 成功一次性替换，DocumentId 轮换且 revision 重置为 0；
- prepare/parse/用户取消/提交前 revision 冲突/宿主失败均保持旧 Model、History、路径、
  loop、DocumentId、revision 和幂等缓存；
- save/save-as 不轮换 ID/revision，只更新路径、名称和 savepoint；
- import/edit/undo/redo 只在真实改变时单调增加 revision；
- 旧 DocumentId 即使对象整数 ID 与新工程巧合相同也返回 `document_changed`；
- 替换时排队命令、导出、提取、解码和推理写回均不能进入新工程。

### 4.2 Resolver 与显式目标

- Dispatcher 双 fake resolver：两个 ID 分别命中相应 fake，未知 ID 不读取 global current；
- 生产 `SingleDocumentSessionResolver` 只暴露一个 Session；
- GUI selection、active clip、焦点和活动窗口不成为文档命令的隐式目标；
- 生产 CoreRuntime 无多 Session 装配路径，测试 fake 不被误认为产品支持。

### 4.3 Revision、History 与原子性

- 单命令、ActionSequence、通用 batch、批量导入各自的 revision/History 次数；
- validation 后、execute 前不存在可失败步骤；失败 Action 构建不触碰 Model；
- batch all-or-nothing，`client_ref` 在请求内唯一且正确绑定，错误项不留下部分 ID；
- `keep_successes` 文件导入先完成预处理，再以成功集合单次原子提交；
- 空 undo/redo 使用定稿语义，focus/reveal 失败不破坏已经完成的 History 状态。

### 4.4 幂等 generation

- 不带 `idempotency_key` 的请求不计算指纹、不访问幂等存储；只有显式 opt-in 的请求进入以下键空间；
- 键空间严格为 `(document_id, operation_id, idempotency_key)`；
- 公开 key 拒绝超过 128 个字符的输入，每个 generation 的成功结果缓存最多保留最近 256 个键；
- 同键同规范化输入在 revision 前进后仍返回首次结果；
- 同键异参数、异 operation 或异原 expected revision 返回 `idempotency_conflict`；
- 代表性并发重放只有一次执行，其余等待同一结果；
- validation 失败、validate-only、提交前取消不占键；
- save、焦点和 GUI 状态不清缓存，new/open 成功清除旧 generation；
- new/open、应用设置和 GUI 状态拒绝文档级幂等键；
- TaskId 与终态记录在 generation 内保留，替换后不可从新文档查询。
- 应用级活动任务不受终态历史清理影响；终态记录超过 128 项时按完成顺序淘汰最旧项。

## 5. 分域测试矩阵

| 域 | Operation 数 | 重点组合 |
|---|---:|---|
| application | 3 | info、WindowId、validate-only、宿主拒绝、退出/重启只调用一次 |
| documents | 10 | new/open/import/save、路径、savepoint、失败回滚、generation |
| tracks/clips/audio/import/project | 44 | 强类型 ID、细粒度编辑、声音上下文、批原子、解码/哈希竞态 |
| notes | 21 | 重叠/边界、量化、resize/split、歌词、语言、发音、音素和复制保真 |
| parameters/speaker_mix | 29 | 有界曲线、采样/锚点、继承、固定/动态混合、关键帧和运行期 ID |
| timeline/master | 11 | tick/bar 0 锚点、排序、拍号增删后的派生位置上界、Master 查询与细粒度控制 |
| history | 3 | 空栈、undo/redo、savepoint、focus、revision、分支截断 |
| inference | 15 | capabilities/status/start、stage、对象删除、revision 重基、cache 与持久化边界 |
| extract | 3 | capabilities、pitch/MIDI 后端、取消、TaskId、原子写回、旧 generation |
| exports/formats | 9 | 格式检查、能力、预览、排他覆盖策略、取消/generation 发布门、cleanup 和任务失败 |
| tasks | 3 | get/list/cancel、过滤、稳定终态、未知/旧 TaskId |
| playback | 10 | 状态/seek 不增 revision；loop 区间、启用/清除；拖动只预览且松手单 History/revision |
| editor | 25 | WindowId、selection、焦点、面板/子区域视口、量化、auto-page 和 view restore |
| settings/recent/search paths | 15 | 每个设置域、no-op、持久化次数、路径规范化、Unicode、清空 |
| packages | 4 | 模块状态、刷新、坏包、声音解析、缺失声库、文档版本 |
| speaker_mix_presets | 3 | CRUD、重复/缺失 ID、归一化、工程序列化隔离 |

## 6. 跨域、竞态与迁移缺陷集

跨域与竞态场景至少包括：

- new/open 与提取、导出、解码、哈希、包解析、推理完成同时发生；
- cancel 与 revision 前进、对象删除、进入 Committing 同时发生；
- selection/reveal 与轨道、片段、音符删除及 undo/redo 交错；
- save 与设置变更、loop 变更、History savepoint 的边界；
- Track/Clip/Note/Speaker Mix 跨域 ActionSequence 的通知次序，以及同步 Model signal
  启动异步推理时捕获提交后 revision；
- 大型多轨工程中同一 base revision 的兄弟分段写回可依次提交且不重启/闪烁，其他
  DocumentId、目标 clip revision 或输入签名变化仍稳定丢弃；短音符立即撤销后不残留
  空推理分段或红色失败状态；
- 复制/粘贴、导入、undo/redo 后 edited parameters、语言、声线和音素保真；
- 文件名大小写、Unicode、只读目录、已存在文件、临时文件清理和磁盘失败；
- `OperationIds::all()`、Dispatcher 可达路径和行为集合不一致或存在重复 operation ID；
- 关键边界的编译期与行为回归，不以源码文本扫描或 Descriptor 镜像替代。

## 7. GUI 冒烟与真实环境资格

### 7.1 用户 GUI 冒烟（启动全量测试前）

建议至少验证：启动、新建、打开 DSPX、导入 MIDI/工程、插入/移动/删除轨道和音符、
歌词与音素编辑、参数曲线、声线/Speaker Mix、tempo/拍号、undo/redo、selection 与
reveal、量化/auto-page、loop、播放/暂停/停止、保存/另存为、退出/重启路径。若有可用
声库，再验证合成波形和播放。用户反馈失败时先修复并重跑实现门禁，不进入全量阶段。

### 7.2 Computer Use 全量 GUI 回归（需用户批准）

在用户完成手工冒烟并明确批准后，Codex 使用 Computer Use 按分域用例逐项操作真实
Windows GUI。覆盖文档生命周期、轨道、片段、音符/歌词/音素、参数曲线、声线与
Speaker Mix、时间线、History、选择与视图、loop/播放、设置、导入导出、任务取消和
应用生命周期。每个场景记录前置工程、操作步骤、可见结果、保存/重开结果、截图或
日志证据及清理结果；危险退出、覆盖文件和大规模生成使用隔离临时目录及专用工程。

Computer Use 回归不以内部 DTO 断言代替 GUI 可见结果，也不把手工冒烟结果重复计为
自动化通过。遇到崩溃、数据损坏或基础路径失败时立即停止该域，保留证据并先修复。

### 7.3 自动化真实资格

- DSPX round-trip 与外部格式导入；
- MIDI 导入/导出 round-trip；
- WAV/FLAC/MP3 等当前注册 codec 的解码与导出；
- 至少一个可用声库的解析、短句推理、波形缓存和播放准备；
- 有/无音频设备、设备切换和初始化失败；
- 包扫描、坏包和搜索路径持久化。

环境缺少 codec、声库、模型或设备时标记为“环境未具备”，不得记录为通过，也不得
降低确定性测试分母。

## 8. 执行顺序与门禁

1. Debug configure/build，运行 `TestAutomationCore` 和受影响领域回归；
2. 完整构建 `DsEditorLite` 并执行当前全部 CTest；失败则修复并重跑，不交付大纲；
3. 提交本大纲，等待用户 GUI 冒烟和对 Computer Use 全量 GUI 回归的明确批准；
4. 按领域审查 208 个 Operation ID 的可达路径与适用行为维度；
5. 确定性单元/契约/集成测试在最终候选上完整运行一次；
6. 竞态测试使用受控调度并进行高迭代压力运行；
7. 按获批大纲执行 Computer Use 全量 GUI 回归和真实环境资格验证；
8. 修复失败后先重跑最小复现，再跑所属域，最后重跑一次完整 CTest；
9. 输出正式测试报告、Operation ID/路由快照、GUI 证据索引和残余风险。

通过标准：适用的确定性场景 100% 通过，`OperationIds::all()` 与 Dispatcher 行为覆盖完整，
应用和测试完整构建与一次完整 CTest 通过，用户 GUI 冒烟通过。任何 skipped/环境未
具备项目都必须逐项解释，不能计入通过率。
