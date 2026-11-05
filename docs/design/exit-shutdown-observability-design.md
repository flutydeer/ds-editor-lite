# 退出阶段的卡死观测与有界等待

> 状态：已实现。本文说明编辑器的退出顺序、卡住时能留下什么证据，以及各处等待的超时约定。
> 实现位置：`Bootstrap/ShutdownMonitor.h`、`MainWindow::closeEvent`、`EditorMcpController::shutdown`、
> `McpHttpServer::stop()`、`TaskManager` 的 `BackgroundWorker::wait()`、`AppContext` 的析构。

## 1. 背景

极低概率下，点主窗口关闭后会出现「正在退出…」对话框，然后窗口再也关不掉，只能结束进程。
触发条件跟时序有关，没有稳定复现路径。CI 上也见过一次：同一份源码（`3f7dd933` 与 PR 的
`b62d206` 树完全一致）一次挂、重跑就过，挂的那次日志在显示等待对话框之后直接结束，
看不出卡在哪一步。

退出是一串等待：窗口等后台任务池，任务池停了才关窗，关窗触发自动化服务停机，
再进入应用上下文的子系统析构。只要这条链上任何一个等待永不返回，用户就只能杀进程。
而原来的代码里，这些等待全部是无界的（`waitForDone()` 无参、`BlockingQueuedConnection`、
`QThread::wait()`、无人打断的 `QEventLoop::exec()`），且大部分位置没有任何日志。

于是这次改动只解决两件事：**卡住时能定位**、**不让等待无限延长**。

## 2. 退出阶段与各自的卡点

| 阶段（日志里的名字） | 正在等什么 | 卡住的表现 |
|---|---|---|
| `close-requested` | `TaskManager` 搬到新线程后 `waitForDone()` | 「正在退出…」对话框一直不消失 |
| `background-tasks-done` | `allDone` 回到主线程，准备关窗 | 窗口不关 |
| `closing-window` | `QMainWindow::closeEvent` 接受关闭 | 事件循环不退出 |
| `automation-stopping` / `automation-stopped` | 自动化 HTTP 服务的跨线程停机握手与线程 join | 事件循环不退出，GUI 可能仍在刷帧 |
| `context-teardown` | `AppContext` 析构各层子系统（音频、推理引擎、命令历史等） | 进程不退出 |
| `context-tasks-drained` | 析构期间的任务池排空 | 进程不退出 |
| `context-teardown-done` | 剩余 L0 单例 | 进程不退出 |

## 3. 观测：`ShutdownMonitor`

header-only 的进程内监视器，因为 `Automation/Mcp/McpHttpServer.cpp` 会被 `TestAutomationProtocol`
直接编译进测试目标，走独立库里加符号会让那个目标缺符号。

- `begin()`：在 `MainWindow::closeEvent` 首次请求终止时 arm（重复调用忽略），起一个看门狗线程。
- `setStage()`：各阶段写一行 `[shutdown] stage <名字> (+<毫秒>)`，同阶段重复设置只记一次。
- 超时后看门狗每 10 s 报一次 `[shutdown] still exiting after <毫秒>, stuck at stage <名字> (<线程清单>)`，
  上限 60 条，避免长期挂起把日志撑爆。
- `finish()`：在 `main()` 退出前收尾，记一行总耗时。
- 线程清单：Linux 读 `/proc/self/task` 拿 tid、名字与状态，Windows 用 Toolhelp32 加
  `GetThreadDescription` 拿 tid 与名字，其他平台只报阶段。清单最多 48 条，超出部分记 `+N more`。

三个环境变量可覆盖默认值：

| 变量 | 默认 | 作用 |
|---|---|---|
| `DS_EDITOR_EXIT_WATCHDOG_MS` | 10000 | 看门狗多久之后开始报「卡住」 |
| `DS_EDITOR_SHUTDOWN_TIMEOUT_MS` | 30000 | 退出路径上各处等待的超时 |
| `DS_EDITOR_EXIT_FORCE_EXIT_MS` | 30000 | 卡多久之后硬退出，设为 0 关闭兜底 |

## 4. 有界等待

- **任务池**：`BackgroundWorker::wait()` 改成 `waitForDone(timeout)`。超时打警告并照常发 `allDone`，
  让窗口能关掉而不是无限等待。
- **自动化服务**：`McpHttpServer::stop()` 原来用 `BlockingQueuedConnection` 叫 worker 停机。
  这条路径和请求处理互为反向等待：worker 线程在处理请求时会 `BlockingQueuedConnection`
  回到编辑器线程，若此时编辑器线程正阻塞等 worker，双方就永久互等。
  现在改成「排队投递 + `QSemaphore` 限时确认」，`m_thread->wait()` 全部带超时，
  异步停机那个 `QEventLoop` 也加了 deadline。
- **导出线程池**：`PublicAutomationRegistry` 的池子改成堆上持有（`std::unique_ptr`），析构时限时
  `waitForDone()`，超时就 `release()` 让它泄漏。原因是 `QThreadPool` 的析构函数会等它所有 runnable
  结束且没有超时，只把前面的 `waitForDone` 加上时限救不了紧随其后的成员析构。

### 全局线程池与硬退出兜底

`QThreadPool::globalInstance()` 的析构同样是无限等，而它发生在 Qt 的应用退出阶段，比上面任何一处
等待都晚，此时 `QApplication` 已经没了。也就是说，只要有一个 runnable 不听终止，进程最后一定会卡死
在那里，任何进程内的限时等待都拦不住。

因此看门狗在退出卡住超过 `DS_EDITOR_EXIT_FORCE_EXIT_MS`（默认 30 秒）后，先写下最后的诊断记录，
再 `std::_Exit` 直接结束进程。走到退出流程时文档的去留已由用户决定、需要保存的内容早就处理完，
所以硬退出不会丢用户数据，代价只是跳过窗口位置这类收尾写入。日志处理器按行 flush，因此那条记录一定落盘。

### 超时之后：`abandonServer()`

服务线程超时未退出时不去 `delete` 一个还在跑的 `QThread`，也不删它仍在使用的 worker 对象，
而是把指针清空、故意泄漏，让进程能退出。退出阶段挂死比泄漏严重得多，
这个取舍写在 `abandonServer()` 的注释里。

因为放弃之后那个线程仍可能执行早就排进它事件循环的回调，`requestStop()` 投递的回调不再读
成员变量：worker 指针按值捕获，`this` 用 `QPointer` 兜住，服务已销毁就直接返回。

## 5. 刻意没做的部分

- `InferEngine::dispose()` 里释放 ONNX 推理 session 的线程池仍是无界等待。超时后继续往下走会
  在 session 还在销毁时关掉 runtime，风险比挂住更大，所以留着由看门狗报阶段。
- 硬退出是兜底而不是修复：真正该修的是让每个任务都响应终止请求，那需要逐个任务核对，
  不在本次范围。
- 超时后带着仍在运行的任务退出，理论上可能在退出瞬间与后台任务竞争共享状态。这里选择
  「退出失败但可观测」而不是「永远不退出」，因为后者用户唯一的选择是杀进程。
- 没有做进程栈 dump。跨平台抓全线程栈需要额外依赖（gdb、DbgHelp），阶段加线程名
  已经能区分是任务池、自动化服务还是某层子系统在卡。
