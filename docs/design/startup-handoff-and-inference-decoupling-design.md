# 启动卡住与单实例请求吞失的结构性修复（设计契约）

## 1. 背景

2026 年 8 月的现场分析确认了一个结构性缺陷，而不是一次崩溃。主实例在 `main.cpp` 很早的位置就拿到单实例锁并启动 IPC 线程。IPC 线程在把请求投递给主线程之前立即回复 `accepted=true`。次实例据此 `return EXIT_SUCCESS`，表现为"一启动就闪退，退出码 0"。如果主实例恰好卡在 `AppContext` 构造链的推理初始化阶段，它就没有主窗口，但仍持有锁，IPC 线程仍能正常应答。此后所有新启动都被静默转发进黑洞。

当年杀掉僵尸进程后重启即恢复，因此根因侧的 ORT 或 GPU 驱动挂起属于偶发事件，本次调查没有复现真实驱动挂起，也不证明 CUDA 到 DML 的切换与那次挂起有因果关系。本轮修复的是"请求被吞掉且用户零反馈"这条结构性链路，以及"后台推理初始化阻塞 GUI"这条已确认的等待路径。

## 2. 契约定义

### 2.1 交接成功

"交接成功"表示请求已由**主线程及其有界工作队列接管**，取值为 `Taken`。它不表示工程已加载完成，也不保证操作系统授予前台焦点。IPC 线程收到字节只产生 `Received`，这个值绝不是成功终态。

状态机为 `Received → Pending → Taken / Rejected / Cancelled`。`Unknown` 用于超出终态保留期或无法确认结果的情形。本实现不承诺跨进程崩溃的持久化 exactly-once，也不自动重放工程打开命令。

### 2.2 协议与版本

外层 `protocolVersion` 保持为 1，`automation.discover` 与 `automation.watch` 契约不变。交接能力单独用 `handoffVersion = 1` 版本化。新增命令 `application.handoff`、`application.query`、`application.cancel`。

响应携带请求 ID、现有实例 UUID、PID、HostMode、启动阶段、主线程心跳年龄、请求状态和交接版本。复用 `SingleInstanceAutomationStatus::editorInstanceId` 作为实例 UUID，不建立第二套身份来源。

新客户端遇到不支持交接扩展的旧主实例时，收到旧格式响应，判定为 `Incompatible` 并给出明确提示。它不会退回不可靠的旧 ACK，也不会另起一个主实例。锁文件名 `instance-v1.lock`、服务名派生方式、数据目录身份全部保持不变，避免双主实例。

旧的 `activate` 与 `openProjects` 保持原有响应格式，但成功 ACK 延后到主线程接管之后。旧客户端可能因原有短超时报告失败，但不会再误把 IPC 收件当作成功。

### 2.3 主线程接管边界

`SingleInstanceCoordinator` 默认关闭接管。只有 `enableAdmission()` 被调用之后，IPC 线程才把请求投递到主线程。

GUI 在窗口显示、处理器安装完成，且事件循环实际执行过之后开放接管。实现上用 `QTimer::singleShot(0, ...)` 把开放动作排到事件循环里。Headless 在处理器和队列可用后即开放接管。健康 Headless 没有窗口不是故障，也不把 MCP Ready 当成应用接管条件。既有启动请求排空与自动化 readiness 屏障保留。

请求处理器返回 `AdmissionResult`。关闭已提交、队列容量不足、命令无效时明确拒绝，不再先确认再静默丢弃。`ExternalOpenRequestQueue` 与 `HeadlessOpenRequestQueue` 各有 64 条工程路径的有界队列。

### 2.4 账本、去重与取消

IPC 线程维护请求账本。同一 UUID 且摘要相同的重复提交只返回已有状态，不重复入队。相同 UUID 而摘要不同则拒绝。

取消与接管的状态转换全部发生在 IPC 线程内，用 `dispatchCommitted` 单一标志位解决竞态。取消只能撤销尚未投递给主线程的请求。已投递或已认领的请求不能谎报取消成功。断连会取消尚未认领的请求。已接管但终态响应丢失时，次实例通过 `application.query` 恢复结果。

在途上限 64，终态记录上限 256，保留 10 分钟。在途记录不会因缓存淘汰而失去所有权。超出保留期或无法确认状态时返回 `Unknown`。

### 2.5 心跳与阶段

心跳由主线程每秒驱动。主线程卡在同步初始化时心跳年龄增长，次实例可以据此报告"主线程暂未响应"，同时不把"正在启动"误报为永久卡死。启动阶段为 `Starting`、`Initializing`、`WindowShown`、`Ready`、`Stopping`。阶段日志带实例 UUID 与请求 ID，读取不依赖推理长锁。

### 2.6 推理门面与后台工作

`InferEngine::initialize()` 不再持 `m_engineRwLock` 写锁跨 GPU 枚举、插件加载和 ORT 会话建立。该锁只用于短时发布 `m_paths`。`initialized()` 与 `disposed()` 改为原子读。

启动配置在应用线程捕获为不可变快照 `InferEngineInitInput`，后台任务只读这份快照，不再直接读 `AppOptions`。CPU 回退修正仍在应用线程执行，且会检查用户在初始化期间是否已经改过设置，旧回调不能覆盖新选择。

完成回调不再持写锁，并在关闭开始后拒绝发布过期 Ready。`SynthrtEngine::m_sessionInitialized` 改为 acquire/release 门闩，关闭状态检查下发布。

GUI 侧的会话保留只发布最新选择与 generation。`retainSingerSessions()` 不再取引擎读锁。后台卸载先取选择快照再释放 `m_singerSessionSelectionMutex`，长耗时的 `unloadVoicebank()` 不在该锁内执行。过期 generation 不提交新的卸载。已开始的后台操作不强行终止，活跃句柄继续保护使用者。

开发者页在引擎未就绪时显示"正在初始化，运行时信息不可用"，不再借用裸 Runtime 遍历正在初始化的内部对象。

`SynthrtEngine` 的 `RuntimeOperationLease`、初始化与关闭 lifecycle 锁、DirectML 串行化、全局任务 drain、包含 warmup 的线程池等待与维护池 join 全部保留。本轮没有用取消等待代替真实退出，没有 detached 初始化，也没有 `QThread::terminate()`。

### 2.7 次实例反馈与安全恢复

`SecondaryHandoffController` 不依赖 `AppContext`、推理服务或音频服务。连接预算默认 3 秒，等待超过 500 毫秒显示交接进度，10 秒后提供恢复操作，非交互预算默认 30 秒。状态区分正在启动、主线程暂未响应、正在关闭、不兼容、已接管。GUI 请求交给健康 Headless 时明确说明不会打开窗口。失败或结果未知时输出到 stderr 并非零退出。

恢复操作不自动杀进程，不删除活进程持有的锁。Windows 路径要求用户两次确认，随后校验锁文件 PID、可执行文件路径、进程创建时间，并用同一个固定进程句柄完成最终校验与结束。身份或权限不足时只提供手动恢复。等待旧进程真正退出后重新参与正常抢锁，若其他实例已经获胜则向新主实例交接。

## 3. 代码位置

| 职责 | 文件 |
| --- | --- |
| 协议、状态、摘要、结构化结果 | `src/app/Bootstrap/SingleInstanceProtocol.{h,cpp}` |
| 账本、生命周期、心跳、接管边界 | `src/app/Bootstrap/SingleInstanceCoordinator.{h,cpp}` |
| 次实例反馈、取消、查询、安全恢复 | `src/app/Bootstrap/SecondaryHandoff.{h,cpp}` |
| 主线程有界队列 | `src/app/Bootstrap/{External,Headless}OpenRequestQueue.{h,cpp}` |
| 启动阶段、接管开放、次实例入口 | `src/app/main.cpp` |
| 推理门面 | `src/app/Modules/Inference/InferEngine.{h,cpp}` |
| 会话门闩 | `src/libs/SynthrtEngine/SynthrtEngine.{h,cpp}` |
| 开发者页诊断 | `src/app/UI/Dialogs/Options/Pages/DeveloperPage.cpp` |
| 回归测试 | `src/tests/TestBootstrap/tst_single_instance.cpp` |

## 4. 本轮范围与边界

本轮已完成协议、账本、接管边界、次实例反馈、安全恢复、推理门面解耦、旧断言更新与新增回归用例，并通过增量构建与 `TestBootstrap` 全量测试。

以下内容属于方案设想但本轮**没有**实现，不应被误认为已完成：

- 完整推理子进程隔离。方案本身把它列为后续工程，不是修复请求吞失的前提。
- 音频 QObject 树的线程迁移。本轮只增加阶段观测与故障反馈，没有在线程约束验证前改动音频架构。
- 配置、推理各子阶段、音频输出、MIDI、窗口与事件循环的逐项耗时埋点。本轮实现了主链路阶段日志，未做全量细分。
- 完整兼容性矩阵与恢复安全的进程隔离测试。本轮只覆盖协议层、账本、接管与取消查询的确定性用例，没有操作用户实际主实例，也没有用测试自有进程验证错误 PID、路径变更、创建时间变化和权限不足。
- 模型 factory 暂停、维护定时器暂停、初始化失败与 Loading 期间退出等生命周期用例，以及 GUI 与 Headless 四种组合的进程级冒烟。
- 自动强杀。恢复始终是用户显式确认的手动操作。

超时本身不证明进程永久挂死。驱动调用不可中断，本实现不声称超时能取消它。

## 5. 验收

- 按项目 MSVC 与 CMake preset 增量构建 `DsEditorLite` 成功。
- `TestBootstrap` 全量 48 个用例通过，含改写后的 `coordinator`、`queuedStartupConnectionIsAcknowledged`，以及新增的 `handoffProtocolRoundTrips`、`handoffWaitsForMainThreadTakeover`、`handoffLedgerDedupeCancelAndQuery`、`handoffCancelAfterDispatchIsNotConfirmed`、`handoffTerminalLedgerEvictsPredictably`、`handoffExpiredTerminalQueryReturnsUnknown`、`handoffPollsHeartbeatWhilePending`。
- `TestVoiceAndInference` 全量 59 个用例通过，含新增的 `singerSessionCacheRejectsStaleRetentionSnapshot`。
- `TestProcessIntegration` 全量 16 个用例通过。
- 原先把"提前 ACK"视为成功的断言已改为验证"先收件、后接管"，底层收件与应用交接保持区分。

## 6. 后续建议

按方案剩余部分补进程级兼容性与恢复安全测试，补生命周期与模型维护暂停用例，然后在目标机器做 Windows 窗口冒烟。若后续要引入推理子进程隔离，交接协议和账本已经提供了不依赖窗口的就绪语义，可以作为独立工程继续推进。

## 7. 评审修复记录

首轮提交 `57382a85` 经评审发现实现缺陷，本轮做聚焦修正，未推倒重来。

### 7.1 P1

- **账本淘汰悬空引用**：`pruneTerminalRecords()` 原先以 `const auto &` 绑定 `QQueue::head()`，随后 `dequeue()` 使引用失效仍继续使用，属于未定义行为。改为按值保存 ID。新增 `handoffTerminalLedgerEvictsPredictably()`，连续提交超过 `maxTerminalRecords` 的请求，验证最老记录被淘汰为 `Unknown`、窗口内记录仍为 `Taken`。
- **取消被谎报为已取消**：原客户端把取消响应中除 `Taken` 外的全部状态都当作 `Cancelled`。抽出纯函数 `SingleInstanceProtocol::cancelDisposition()`，只有显式 `Cancelled` 才是确认取消，`Taken` 为已接管，`Rejected` 为拒绝，`Received/Pending/Unknown` 为 `NotConfirmed` 并继续观察原交接，不再退出。新增 `handoffCancelAfterDispatchIsNotConfirmed()`，用独立线程发取消，精确复现"已投递、主线程尚未接管"窗口，验证服务端返回 `Pending + request_already_dispatched`，且该状态不得判为取消成功。
- **GUI 被后台模型工厂卡住**：`retainOnly()` 与 `evict()` 原先仍在 GUI 线程同步取缓存锁，而该锁被后台 `acquire()` 的整个 model factory 持有。现在 GUI 只发布选择、策略快照与 generation，`scheduleSessionMaintenance()` 把维护合并到串行释放池执行，10 秒定时器也不再同步取缓存锁。generation 在维护任务末尾复核，变化则再排一次合并后的维护。

### 7.2 P2

- **退出状态被迟到结果覆盖**：`InferEngine` 新增短 `m_stateMutex`，`initialize()` 发布 Ready 与 `dispose()` 清除状态在同一临界区内。`SynthrtEngine` 发布 session ready 与 `shutdown()` 的 `aboutToQuit + clear` 共用 `m_stateMutex`，避免 `refreshVoicebanks()` 看到已拆除的会话。
- **generation 过期后仍发起新卸载**：`releaseDeselectedSingerSessionsAsync()` 改为在每次新的 `unloadVoicebank()` 准入前重新读取 generation 与最新选择，过期即停止循环。
- **Windows 恢复路径**：统一 `QueryFullProcessImageNameW` 的原生分隔符与 `canonicalFilePath()` 的 Qt 分隔符（`QDir::fromNativeSeparators` + `cleanPath` + 小写），`OpenProcess` 补 `SYNCHRONIZE` 使 `WaitForSingleObject` 不再 `WAIT_FAILED / ERROR_ACCESS_DENIED`。
- **确认期间目标被替换**：确认前先固定锁所有者身份（PID、归一化路径、创建时间），两次确认后重新读取并比对。同时检查原交接是否已结束或已产生终态，任一变化都中止恢复，不结束任何进程。最终仍由同一固定句柄完成身份复核与终止。

### 7.3 交互缺口

- GUI 请求被健康 Headless 接管时，除 stderr 外增加可见信息框，覆盖快速成功时进度窗口尚未出现的情形。
- 新 handoff 路径恢复 `AllowSetForegroundWindow` 前台授权，在连接建立后、发送交接前授予锁所有者，避免 Windows 拒绝主实例的 `activateWindow()`。

### 7.4 Codex 自动审查（PR #206）

`chatgpt-codex-connector` 对 `a7d0ca12` 给出 3 条 P2，均核实成立并修复：

- **query 不淘汰过期终态**：`handleQuery()` 原先只在 `finalize()` 时顺带剪枝，安静的主实例会让超过 10 分钟保留期的记录仍返回 `Taken`，违背 §2.4。现在 `handleQuery()` 与 `handleCancel()` 在读账本前调用 `pruneTerminalRecords()`，并对查到的终态再校验一次 `finishedMs`，超期返回 `Unknown + result_unknown`。保留期改为 worker 可注入值（生产仍用协议默认），测试用 `setLedgerRetentionForTest()` 缩短窗口，新增 `handoffExpiredTerminalQueryReturnsUnknown()`：窗口内 query 得 `Taken`，期间不产生任何新的 finalize，等待后再 query 得 `Unknown`。
- **Pending 期间心跳年龄不刷新**：`m_heartbeatAgeMs` 原先只在 `onFrame()` 写入，而进度定时器是 singleShot 且只启动一次；收件后主线程卡住时界面永远停在"正在启动"。新增每秒一次的 `application.query` 轮询（`m_statusPollTimer`），查询响应统一经 `applyQueriedState()` 刷新实例信息、启动阶段、心跳年龄与状态文案，终态才结束，非终态继续观察；查询失败视为瞬时故障，由预算定时器最终裁决。`HandoffResult` 增加 `status` 与 `statusPollCount` 诊断。新增 `handoffPollsHeartbeatWhilePending()`：停掉主实例心跳且不开放接管，验证状态从"正在启动"变为"主线程暂未响应"、心跳年龄超过 3 秒阈值、轮询次数不少于 2。
- **缓存变更前未复核 generation**：`runSessionMaintenance()` 取完快照后可能在 `retainOnly()` 内等待 model factory 锁，期间 GUI 发布的新选择会被旧快照覆盖，导致刚重新选中的歌手模型被释放。`SingerSessionCache::retainOnly()` 增加在缓存锁内执行的准入谓词与 `applied` 标志；维护任务把 generation 校验推进到缓存变更的准入点，被拒绝时不动缓存、不消费 eviction 请求、重新排队一次合并维护。新增 `singerSessionCacheRejectsStaleRetentionSnapshot()` 验证过期快照只返回 `applied=false` 且缓存与句柄完全不变，当前快照仍按原语义生效。

### 7.5 仍未覆盖

进程级恢复安全测试（错误 PID、路径变更、创建时间变化、权限不足、确认期间目标替换的真实进程验证）与 GUI/Headless 四组合矩阵仍未做。本轮修复的是代码路径与协议层确定性用例，不能替代真实 Windows 恢复路径的端到端验证。
