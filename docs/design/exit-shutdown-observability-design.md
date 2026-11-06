# 退出阶段的卡死观测与进程生命周期兜底

> 状态：已实现。本文说明编辑器一次退出由谁提交、谁负责等待、卡住时能留下什么证据，
> 以及超出预算后的失败退出边界。
> 实现位置：`Bootstrap/ShutdownMonitor.h`、`main.cpp`、`AppContext`、`MainWindow::closeEvent`、
> `CoreRuntime::commitShutdown`、`DocumentWorkflowController::commitShutdown`、
> `EditorMcpController::shutdown`、`TaskManager` 的 `BackgroundWorker::wait()`、
> `PublicAutomationRegistry` 的析构。

## 1. 背景

极低概率下，点主窗口关闭后会出现「正在退出…」对话框，然后窗口再也关不掉，只能结束进程。
触发条件跟时序有关，没有稳定复现路径。CI 上也见过一次：同一份源码（`3f7dd933` 与 PR 的
`b62d206` 树完全一致）一次挂、重跑就过，挂的那次日志在显示等待对话框之后直接结束，
看不出卡在哪一步。

退出是一串等待：窗口等后台任务池，任务池停了才关窗，关窗触发自动化服务停机，
再进入应用上下文的子系统析构，最后还有 Qt 的全局线程池与静态对象析构。只要这条链上
任何一个等待永不返回，用户就只能杀进程。

只给每处等待加时限解决不了问题。超时不代表任务已经停下，任务仍在引用的 runtime、
file guard 与模型对象马上就会被销毁。反过来，绕过线程池析构又会把冲突推迟到线程恢复的时刻。
所以这份设计只做三件事：**一个正式的退出提交**、**一个覆盖整个进程的绝对预算**、
**低层只负责取消与安全等待**。

## 2. 退出契约

一次正常退出分两段。第一段由用户与文档决定：保存、放弃或取消。第二段从
「文档去留已定，进程确定要走」开始，直到操作系统回收进程。本文只描述第二段。

第二段只有两个不变量：

1. **退出提交只发生一次，且不可逆。** 提交之后文档进入 `Closing`，不再接受任何修改、
   换代与任务续写。
2. **预算从提交时刻开始计算，一次退出只有一个预算。** 每进一个子系统不重新计时，
   也不会因为进入新阶段而延长。

硬退出是这条契约的失败出口，不是修复手段。真正该修的是让每个任务都响应终止请求，
那需要逐个任务核对，不在本次范围。

## 3. 观测与兜底：`ShutdownMonitor`

header-only 的进程内监视器，宿主与 `TestBootstrap` 的退出子进程测试共用同一份实现，
不需要为一个只有几十行的监视器再建库目标。

- `start()`：在正常启动阶段预先创建监视线程与诊断线程，早于任何可能卡住的子系统。
  创建线程本身也可能失败或变慢，所以它不能是卡住的退出路径上的第一个动作。
- `commit()`：正式提交退出。幂等，只写原子状态与绝对截止时间，**不写日志**。
  冻结完成后由宿主另记一行日志，这样阻塞在日志上的调用不会推迟冻结，也不会推迟预算。
- `setStage()`：各阶段写一行 `[shutdown] stage <名字> (+<毫秒>)`，同阶段重复设置只记一次。
  只有提交之后才有意义，提交前不输出。
- 没有 `finish()`。提交之后保护一直持续到操作系统回收进程，中途不停线程、不 join。
  最后一段（Qt 全局线程池、`Restarter`、静态析构）正是最容易卡住的部分，
  在返回前撤销保护等于放弃这一段的承诺。
- 状态对象是进程生命周期的：用 `new` 分配且从不销毁。静态对象的析构函数会 join 监视线程，
  而这恰好发生在它要保护的最后一段，所以这里刻意不析构，线程随进程结束。

### 硬退出路径

监视线程判定超预算时，只读时钟与两个预初始化值，然后 `std::_Exit(EXIT_FAILURE)`。
这条路径不碰 Qt、不写日志、不枚举线程、不分配内存、不新建线程——每一项都可能在已经卡住的
进程上再次阻塞。

诊断是启动期就预创建的另一条线程：监视线程只把耗时与阶段写进原子变量并递增一个序号，
不等待、不分配。真正格式化、枚举线程、拿日志互斥量、写日志的都在诊断线程上，
它卡住只会让后续报告停摆，不会影响退出预算。每次报告仍受「至少间隔 10 秒、最多 60 条」限制。

线程清单仍是 Linux 读 `/proc/self/task`、Windows 用 Toolhelp32 加 `GetThreadDescription`，
上限 48 条，超出记 `+N more`。

### 阶段名

| 阶段（日志里的名字） | 正在等什么 |
|---|---|
| `close-requested` | 请求终止后台任务，准备等待任务池 |
| `waiting-for-background-tasks` | 任务池 `waitForDone()` |
| `background-tasks-done` | `allDone` 回到主线程，准备关窗 |
| `closing-window` | `QMainWindow::closeEvent` 接受关闭 |
| `automation-stopping` / `automation-stopped` | 自动化 HTTP 服务的停机握手与线程 join |
| `context-teardown` | `AppContext` 析构各层子系统 |
| `context-tasks-drained` | 析构期间的任务池排空 |
| `context-teardown-done` | 剩余 L0 单例 |

## 4. 提交入口

`AppContext::commitShutdown()` 与 `DocumentWorkflowController::commitShutdown()`
是宿主级的提交入口，两者都先调 `ShutdownMonitor::commit()`，再冻结文档，并且都是幂等的。
接入点按「能多早、就必须多早」排列：

- **GUI**：`terminationApproved` 的直接主线程处理器。提交是处理器里的第一条语句，
  早于随后排队的 `close()`、早于任务池等待、早于任何同步日志。保存对话框里等待或选择取消时
  不会走到这里，因此不会启动倒计时，也不会把正常文档留在 `Closing`。
- **Headless**：`requestTermination` 的 busy/unsaved 校验通过之后、排队 `quit` 之前。
  `aboutToQuit` 回调比析构早得多，冻结必须在那之前完成。
- **composition root**：`main()` 里注册最早的 `aboutToQuit` fallback，早于 `InferEngine`、
  MCP 等可阻塞回调。回调按连接顺序执行，先提交就不会被后面的回调拖住。
- **析构入口**：`AppContext` 析构开头再提交一次，用于启动失败等没走正式入口的收尾路径。
  它在第一次阻塞式析构之前，且正式提交之后是空操作。

## 5. Closing 门闩：冻结修改与续写

`CoreRuntime::commitShutdown()` 置原子标志并让 `DocumentSession` 进入 `Closing`。

- `Closing` 是粘性的。`setLifecycleState()` 在提交之后忽略一切非 `Closing` 的更新，
  `replaceGeneration()` 也不再把它改回 `Active`。否则一个已经在飞的换代提交
  （`Replacing → Active`）会把门闩顶开。
- 所有 document 命令的闸门是 `AutomationDispatcher::resolveDocumentCommand()` 的
  `lifecycleState() != Active`，与调用来源无关：`Internal`、`TrustedGui`、
  `PublicTaskContinuation` 一律返回 `documentBusy`。mutation、control、
  不校验 revision 的变体、准入与查询都走这一个闸门。
- `documentWorkflowCommitContext()` 与 `derivedWritebackContext()` 在提交后直接返回
  `documentBusy`，避免把写许可继续传到更低层再去报冲突。
- 释放 workflow 的 busy 不影响 `Closing`：`setDocumentBusy()` 只改 busy 标志，
  不动 lifecycle。
- 工作流层再加一道窄闸：`begin()` 与 `requestTermination()` 在提交后直接返回，
  队列里已排队的打开请求不会把一个已冻结的文档重新打开。
- 自动化准入走宿主提供的窄谓词：`EditorMcpController` 把
  `AdmissionController::setAdmissionGate()` 绑到 `CoreRuntime::shutdownCommitted()`，
  于是提交瞬间就停止接纳新请求，既不需要主线程先跑完停服握手，也不需要准入策略
  依赖进程级单例。`EditorMcpController::stopAdmission()` 只负责关这扇门，
  不顺便同步停线程。
- 两个外部打开入口各自也查一次门闩：`ExternalOpenRequestQueue::dispatchNext()` 与
  `HeadlessOpenRequestQueue::dispatchNext()`。单实例协调器停止分发之后，
  已经排好的定时器回调仍会执行，只有提交状态这道闸能拦住它们。

## 6. 低层只等待，不决定杀进程

- **任务池**：`BackgroundWorker::wait()` 改为无参 `waitForDone()`。任务仍在运行时继续持有
  其依赖并安全 join，超时返回后照常析构是 use-after-free。
- **导出线程池**：`PublicAutomationRegistry` 析构同样改为无界 join。
- Tasking、导出与 HTTP 服务里原来的 `std::_Exit` 全部移除。通用任务库不引入应用层
  Bootstrap 头，也不维护自己的强杀政策。进程卡住由唯一的进程级看门狗结束。
- 不使用 `QThread::terminate()`，也不靠泄漏线程池换取退出。

代价是这些等待在实现上仍然无界。真正兜住进程的是第 3 节的预算，而不是各子系统自己的时限。

## 7. 环境变量

| 变量 | 默认 | 作用 |
|---|---|---|
| `DS_EDITOR_EXIT_FORCE_EXIT_MS` | 30000 | 提交之后允许退出的绝对预算，超时硬退出。**设为 0 表示禁用整套硬退出** |
| `DS_EDITOR_EXIT_WATCHDOG_MS` | 10000 | 提交之后卡住多久开始报「卡住」 |
| `DS_EDITOR_SHUTDOWN_TIMEOUT_MS` | 30000 | 运行期停服握手的阶段等待阈值，不是退出策略 |

`DS_EDITOR_EXIT_FORCE_EXIT_MS=0` 的语义是「这个进程不再有硬退出」，因此
`_Exit` 只剩看门狗这一处，禁用时不会留下几个局部的强杀点。单独的等待阈值或诊断阈值可以保留，
但不能暗中改变这个承诺。

## 8. 数据与日志承诺

正常路径优雅退出。超过预算后失败退出，优先避免无限等待与不安全析构，代价明确如下：

- 硬退出会中断进行中的导出、未提交的事务、配置与日志收尾。诊断尽力而为，不保证落盘，
  因为写日志的线程可能正卡在输出背压或日志锁上。
- MIDI 与 DSPX 写入使用 `QSaveFile`，这能保护已存在的目标文件不被部分覆盖，
  不代表进行中的导出已经完成，也不保证强杀会清理临时文件。
- 「保存已批准」本身不是 mutation fence。真正的冻结来自第 5 节的 `Closing` 门闩，
  它在保存成功或用户明确放弃之后才提交，取消不提交。

## 9. 验证

退出路径的回归测试是子进程测试，位于 `src/tests/TestBootstrap`（`tst_shutdown_monitor.cpp`）。
父进程只做两件事：等一个有界窗口，以及在「本应活着」的用例上回收子进程。

| 用例 | 断言 |
|---|---|
| `watchdogBoundsStalledExit` | 提交后卡死，进程在预算内以非零码结束 |
| `watchdogIgnoresIdleProcess` | 未提交时进程可以长期运行，看门狗不会误杀 |
| `watchdogCoversLateQtTeardown` | `QCoreApplication` 析构卡在全局线程池时仍有兜底 |
| `watchdogExitsWithoutLogging` | 日志互斥量被卡住的线程持有，提交后的日志调用阻塞，仍然按时退出 |
| `watchdogUsesOneAbsoluteBudget` | 持续进入新阶段不会延长预算 |
| `disabledHardExitIsLeftToTheParent` | `FORCE_EXIT_MS=0` 时子进程不会自行退出，由父进程回收 |
| `aboutToQuitFallbackCommitsBeforeBlockingCallbacks` | 先注册的提交 fallback 生效，后被阻塞的回调不影响预算 |

`FORCE_EXIT_MS=0` 的用例必须由外部父进程超时回收，否则测试进程会永久挂起。

## 10. 刻意没做的部分与已知限制

- `InferEngine::dispose()` 里释放 ONNX 推理 session 的线程池仍是无界等待。超时后继续往下走
  会在 session 还在销毁时关掉 runtime，风险比挂住更大，所以留着由看门狗兜底。
- 运行时停服（禁用或重配自动化服务）的握手是独立设计：它不能因为停不掉而杀掉整个编辑器。
  停服完成依赖 worker 的 ack，不依赖主线程在同步 join 里泵全局事件。
- GUI 里绕过自动化闸门、直接改模型的入口没有逐个加锁。实际编辑都经 `CommandCommitter`
  与 dispatcher，因而被 `Closing` 挡住，但这条结论依赖调用方都走 dispatch，
  新增直改模型的路径时要注意。
- 不宣称能处理进程被外部暂停、内核不调度等情况。那类保证需要外部 supervisor。
