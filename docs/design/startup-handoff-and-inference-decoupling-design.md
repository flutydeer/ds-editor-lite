# 启动与单实例交接契约

## 1. 背景

主实例会在窗口可用之前持有单实例锁，并启动 IPC 线程。IPC 线程能够收件时，主线程仍可能在初始化、繁忙或退出。次实例依据应用接管状态完成交接，等待期间通过主线程心跳反馈进度。推理初始化与 GUI 启动分别跟踪。

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

请求处理器返回 `AdmissionResult`。关闭已提交、队列容量不足或命令无效时明确拒绝。GUI 和 Headless 的外部打开队列各接收至多 64 条在途工程路径。本地初始命令行请求不受外部在途上限约束，若提交失败则记录警告。

### 2.4 账本、去重与取消

IPC 线程维护请求账本。同一 UUID 且摘要相同的重复提交只返回已有状态，不重复入队。相同 UUID 而摘要不同则拒绝。

取消与接管的状态转换发生在 IPC 线程内。`dispatchCommitted` 标记已进入主线程投递阶段的请求，只有投递之前可以取消。取消响应须明确返回 `Cancelled` 才代表取消成功。请求仍为 `Received`、`Pending` 或 `Unknown` 时，次实例继续观察交接状态。断开连接同样只能取消尚未投递的请求。已接管却丢失终态响应时，可通过 `application.query` 恢复结果。

在途请求上限为 64，终态记录上限为 256，最长保留 10 分钟。终态淘汰不影响在途记录。查询和取消在读取账本前清理超期记录，超出保留期或无法确认状态时返回 `Unknown`。控制响应允许跨多次读取到达，接收端会在截止时间内累积字节并尝试解码完整帧。

### 2.5 心跳与阶段

心跳由主线程每秒驱动。次实例在交接等待期间每秒查询，刷新主实例的启动阶段和心跳年龄。主线程长时间未处理心跳时可显示“主线程暂未响应”。启动阶段为 `Starting`、`Initializing`、`WindowShown`、`Ready`、`Stopping`。阶段日志用于定位所处环节，状态查询不依赖推理长锁。

### 2.6 推理门面与后台工作

`InferEngine::initialize()` 在后台枚举 GPU、加载插件和建立 ORT 会话。`m_engineRwLock` 用于短时发布 `m_paths`，长耗时工作在锁外执行。`initialized()` 与 `disposed()` 可原子读取。

启动配置在应用线程捕获为不可变快照 `InferEngineInitInput`，后台任务读取该快照。CPU 回退修正在应用线程执行，只有选项仍与启动快照一致时才写回，避免覆盖初始化期间的选择。

就绪发布与关闭状态变更在各层的状态锁内协调，关闭开始后不发布迟到的 Ready。`SynthrtEngine::m_sessionInitialized` 通过 acquire/release 同步会话就绪状态。

GUI 只发布会话选择、维护策略和 generation。后台串行处理保留与缓存淘汰，获取缓存锁后复核 generation。快照过期时保留待处理的维护请求，再按当前选择调度。后台卸载在每次调用 `unloadVoicebank()` 前检查最新选择，长耗时调用不持有选择锁。活跃句柄保护使用中的资源。

开发者页在引擎未就绪时提示运行时信息不可用。

`SynthrtEngine` 通过 `RuntimeOperationLease` 管理进行中的运行时调用。初始化和关闭互斥，DirectML 工作按后端约束串行。关闭等待后台任务、预热与维护工作结束。交接超时只反映等待结果，无法中断正在执行的驱动调用。

### 2.7 次实例反馈与安全恢复

`SecondaryHandoffController` 无需构造应用服务。连接预算默认 3 秒，等待超过 500 毫秒显示交接进度，10 秒后提供恢复操作，非交互预算默认 30 秒。状态区分正在启动、主线程暂未响应、正在关闭、不兼容、已接管。GUI 请求交给健康 Headless 时提示不会打开窗口，包括进度窗口尚未出现的快速交接。失败或结果未知时输出到 stderr 并以非零状态退出。Windows 次实例会在连接后、发送交接前向锁所有者授予前台窗口权限。

Windows 的进程恢复由用户显式触发并两次确认。确认前记录锁所有者的 PID、规范化可执行文件路径和进程创建时间，确认后复核身份和交接状态。任一身份变化或交接产生终态时取消恢复。最终校验、结束和等待使用同一进程句柄。身份或权限不足时只提供手动恢复。旧进程退出后重新参与正常抢锁，若其他实例已获得锁则向新主实例交接。

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
| 启动与交接测试 | [`tst_single_instance.cpp`](../../src/tests/TestBootstrap/tst_single_instance.cpp) |
