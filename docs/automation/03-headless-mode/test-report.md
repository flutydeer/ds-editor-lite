# 三期 Headless 与原生 JSON-RPC 最终测试报告

## 1. 最终结论

三期最终测试结论：**通过**。

测试源候选为 `headless` 分支 commit `871ed195`；其后的变更只回填正式报告，不改变受测产品代码
或测试。该候选保持项目 Debug build preset 的默认目标不变，通过项目脚本显式指定 `all` 完成
全目标构建，并执行 64 项串行 CTest，结果为 64/64、0 fail。Headless QCore 资格、Native
JSON-RPC、可选 MCP、Connector、GUI 回归、单实例、公开及控制台退出生命周期和资源清理均包含在
同一候选验证链中。

本报告不以 Native/MCP 对 151 项 operation 逐项重复调用作为结论依据。完整性由 Contract、Registry
binding、Host 集合与各业务域测试证明；真实进程使用有区分度的代表语料验证协议和宿主行为。

## 2. 候选与执行摘要

| 项目 | 最终记录 |
|---|---|
| 分支 | `headless` |
| 测试源候选 | `871ed195` |
| 平台 | Windows x64，build 10.0.26200.9168 |
| 编译工具链 | Visual Studio 2026 18.9.0；MSVC 19.51.36256；toolset 14.51.36231；Windows SDK 10.0.26100.0 |
| 依赖工具 | Qt 6.11.2；CMake 4.3.1-msvc1；Ninja 1.13.2 |
| Debug configure/generate | configure 4.7 秒；generate 1.3 秒；退出码 0 |
| Debug 全目标构建 | 项目脚本显式 `-Target all`，退出码 0；preset 默认目标未修改；`E-P3-FINAL-BUILD-011` |
| CTest 清单 | 文本与 JSON 清单均为 64 项，测试可执行文件完整；`E-P3-CTEST-LIST-006`、`E-P3-CTEST-JSON-006` |
| 完整串行 CTest | 64/64、0 fail、61.83 秒、退出码 0；`E-P3-CTEST-FULL-008` |
| 代表耗时 | `TestDsConnectorLite` 29.45 秒；GUI 真实进程 5.92 秒；Headless 真实进程 16.81 秒 |
| Computer Use | 0 次 |
| 控制台终止专项 | Windows `CTRL_BREAK_EVENT` 进程测试与真实 PTY `Ctrl+C` 均为脏文档、退出码 0；`E-P3-SIGNAL-PROCESS-001`、`E-P3-SIGNAL-PTY-001` |
| 最终审计与清理 | 通过；`E-P3-FINAL-AUDIT-008`、`E-P3-CLEANUP-001` |

正式候选使用项目标准 Debug preset wrapper，并只在测试命令中显式指定 `all`：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .agents/skills/scripts/run-cmake-preset.ps1 `
  -Mode ConfigureAndBuild -Preset debug -Target all
ctest --test-dir build/Debug -N
ctest --test-dir build/Debug --show-only=json-v1
ctest --test-dir build/Debug --output-on-failure -j 1
```

## 3. Contract、Registry 与 Host gate

最终候选公共集合快照为 Editor 176 项，其中 GUI-only 25 项、GUI/Headless both 151 项。数量仅作为
当前候选诊断快照，正式正确性由以下集合关系保护：

- Public Contract operation ID 唯一；
- GUI-only 与 both 互斥且并集等于权威 Contract；
- GUI-only 差集精确来自 `workspace`、`track_panel`、`clip_editor` 三个 GUI 域；
- Registry binding 集合等于 Contract，Host metadata 与 `value_sources` 一致；
- GUI/Headless 的实际目录先经过 Host gate，再经过 control level/Custom Policy；
- 已知 GUI-only operation 在 Headless 的 Schema、File Guard、Admission 和 handler 前返回
  `host_capability_unavailable`。

专项测试确认 176/25/151 集合关系、唯一性、binding 完整性、value source metadata 和 Host gate
顺序；Headless MCP 分页目录取得 151 个唯一 both operation。结果包含于
`E-P3-CTEST-FULL-008`，协议代表证据为 `E-P3-HEADLESS-001`。

## 4. QCore composition、身份与零窗口

最终真实进程测试使用隔离配置和故意无效的 GUI platform 配置启动 Headless，并确认：

- Headless 仍能建立 Native 服务，不依赖 Qt GUI platform plugin；
- 进程使用 `QCoreApplication`，未建立 MainWindow、ThemeManager、Widget 或平台窗口；
- `application.get_status` 报告 `host_mode=headless`、一个真实活动文档和 `windows: []`；
- 系统级顶层窗口枚举结果为零；
- 文档 operation 只使用文档身份与 revision，不要求或生成替代窗口身份；
- 带位置工程启动时，初始 open Task 在 Controller/listener ready 前到达终态；Primary 服务建立后
  立即转发的第二个工程也经 ACK-to-main 屏障进入同一队列，首次 status 对应最后转发工程，而非
  过渡空白文档或较早的位置工程；
- 最终屏障原子暂停 open/activate dispatch；屏障窗口内的新请求不 ACK，Controller ready 后才恢复
  为运行期请求，因此不存在屏障返回到 admission/readiness 之间的确认竞态；
- GUI-only operation 即使缺失或传入非法窗口参数，也先得到 Host capability 错误；
- dirty 默认退出返回 `busy`，显式 discard 后先返回接受结果再退出。

对应进程证据为 `E-P3-HEADLESS-001`；最终候选复验和退出后资源审计分别包含于
`E-P3-CTEST-FULL-008`、`E-P3-FINAL-AUDIT-008` 与 `E-P3-CLEANUP-001`。

## 5. Native JSON-RPC 与 HTTP

### 5.1 协议与业务路径

Native 直连确认：

- 首次 `application.get_status` 返回 Host、工具版本、控制层级、真实文档与 revision；
- 成功 `result` 直接使用公共 output object，不含 MCP `structuredContent`；
- 代表性轨道编辑、Undo、Redo 与 revision/History 状态形成闭环；
- 已知 GUI-only method 返回 `host_capability_unavailable`；
- `automation.discover` 返回标准 `-32601 Method not found`；
- dirty exit 与显式 discard exit 使用同一公共生命周期语义。

对应代表证据为 `E-P3-NATIVE-001`，最终候选复验包含于 `E-P3-CTEST-FULL-008`。

### 5.2 Envelope、限制与共享资源

组件测试覆盖单对象请求、字符串/安全整数 ID、params 省略、malformed JSON、Batch、notification、
未知 method、Invalid params、业务错误和 internal failure。共享 HTTP 测试还覆盖 Native 直接结果、
非 POST 拒绝、精确 `q=0` Accept、handler exception/非法响应、Native 超限响应、请求/响应上限
独立性、MCP→MCP 及 MCP→Native 全局 admission、有序停止和错误响应 ID 保留。

Host/Origin、Content-Type/Accept、body、JSON 深度/节点、响应体、deadline、安全响应头和 route 释放
矩阵通过。组件证据为 `E-P3-HTTP-002`，最终串行复验为 `E-P3-CTEST-FULL-008`。

## 6. 共享 listener、MCP 与 Bootstrap

真实进程测试确认：

- Headless `--no-mcp` 时 `/automation/v1` 可用；Bootstrap 报告 Headless Host、
  `server_disabled` 与空 MCP endpoint；
- Headless `--mcp` 时 Native 与 MCP 在同一 listener 上共存；
- MCP 完整分页目录为当前 Host/Policy 下的 151 个 both operation；
- Native 与 MCP 调用同一 Registry，并观察同一文档、revision、History 和 Task 状态；
- 端口冲突使 Headless 非零退出，并释放测试拥有的 listener、Primary 与 QLocal 资源；
- MCP route 动态启停不影响 Headless Native route 的持续可用性。

Connector 的固定桥接工具、实际 Headless 目录、GUI-only wrapper 错误、MCP disabled 状态以及
`editor_not_running`/`editor_not_connected` 边界通过。该域最终结果包含于
`E-P3-MCP-BOOTSTRAP-001` 与 `E-P3-CTEST-FULL-008`；`TestDsConnectorLite` 耗时 29.45 秒。

## 7. 生命周期、单实例与进程并发

统一串行进程 fixture 已确认：

- clean exit；
- dirty 默认 `busy` 且无模态窗口，显式 discard 后退出；
- Windows 独立隐藏控制台进程组的 `CTRL_BREAK_EVENT` 在脏文档下以正常退出码 0 完成，且 listener、
  Primary/QLocal 和进程均释放；Unix 测试分支在对应平台分别发送 `SIGINT` 与 `SIGTERM`；
- 前台 PTY 的真实 `Ctrl+C` 产生 `CTRL_C_EVENT`，同样丢弃脏文档并以退出码 0 完成清理；
- 原有 HTTP `application.request_exit(discard_changes=true)` 用例继续通过，证明信号与协议入口共用
  application Facade 生命周期；
- restart 复用当前 executable、参数和工作目录，并建立新的运行实例；
- GUI 已为 Primary 时 Headless 不成为第二 Primary，反向场景相同；
- GUI Primary 下 Headless Secondary 仍按既有语义成功转发并以 0 退出，Primary 保持 GUI；其
  `stderr` 明确提示请求已转发且未启动新的 Headless 实例；
- 启动位置工程和最终屏障前已确认的转发工程均先到达 Task 终态；屏障期间的新请求延迟 ACK 至
  ready 后，运行期位置工程请求保持单文档替换与队列顺序；
- 端口冲突和运行期致命配置错误非零退出；
- 测试结束后无测试拥有的孤儿进程、listener、QLocal、Primary、Task 或锁。

Headless 真实进程域耗时 16.81 秒，GUI 真实进程域耗时 5.92 秒；控制信号专项、最终执行与清理
证据分别为 `E-P3-SIGNAL-PROCESS-001`、`E-P3-SIGNAL-PTY-001`、`E-P3-CTEST-FULL-008` 和
`E-P3-CLEANUP-001`。

## 8. QCore 业务域资格

151 项 QCore 资格由 Contract/binding/Host 集合及各域既有 Facade 行为测试共同保护，未对全部
operation 做双协议逐项调用。真实进程和组件测试使用以下代表语料：

- Query：应用、文档、轨道/剪辑/音符、设置和能力；
- 同步 Command：编辑、Undo/Redo、播放和持久 loop；
- 异步 Command：文档、音频、导出、包刷新或推理的代表路径；
- 文件：自动生成的工程与音频 fixture 的 open/import/save/export；
- Task：accepted、list/get/cancel、终态和文档 generation；
- 环境能力：模型、codec 或设备不可用时返回真实 availability/error 事实。

Native、MCP、Connector 与直接 Facade 的代表调用在结果、错误、revision、History 和 Task 生命周期
上保持一致。代表语料与最终结果分别记录于 `E-P3-DOMAIN-001` 和
`E-P3-CTEST-FULL-008`。

## 9. GUI 与 Connector 回归

GUI 回归遵循“确定性测试与进程检查 → Editor MCP/DS Connector Lite MCP → 日志、QLocal、HTTP 和
状态回读”的控制优先级。全部必要断言均由前三类方式完成，Computer Use 使用次数为 0。

最终确认：

- GUI 仍创建 `QApplication`、一个真实 MainWindow 和真实窗口身份；
- L3 Editor MCP 实际目录等于权威 GUI 公共集合；
- status 为一个文档和一个真实窗口；
- document mutation、Undo/Redo、播放和保存正常；
- `workspace`、`track_panel`、`clip_editor` 各有代表 operation 与状态回读；
- MCP 启停、重启、单实例和 Connector 自动重连未回退；
- MCP 修改后的模型与 GUI 状态一致。

GUI 真实进程域耗时 5.92 秒，Connector 测试耗时 29.45 秒，结果包含于
`E-P3-GUI-001` 与 `E-P3-CTEST-FULL-008`。

## 10. 缺陷与修复闭环

| Failure ID | 根因与处理 | 闭环证据 |
|---|---|---|
| `E-P3-HTTP-001` | 共享 HTTP 改造中的 Qt HTTP response 所有权/API 使用问题；修复为唯一移动语义 | HTTP 组件复验 `E-P3-HTTP-002`，最终串行复验 `E-P3-CTEST-FULL-008` |
| `E-P3-BUILD-004` | 首次测试命令误以为不指定 Target 会构建全部测试；最终不修改 preset，而在测试命令中显式指定 `all` | 最终构建 `E-P3-FINAL-BUILD-011`、清单 `E-P3-CTEST-LIST-006`/`E-P3-CTEST-JSON-006`、全量 `E-P3-CTEST-FULL-008` |
| `E-P3-REVIEW-001` | Codex 审查发现 GUI 音频设备失败反馈被一并移除，以及响应上限会随请求上限被静默抬高；分别以 Host 条件反馈和独立响应上限修复 | 专项 2/2 与最终串行 64/64；`E-P3-REVIEW-001`、`E-P3-CTEST-FULL-008` |
| `E-P3-REVIEW-002` | Codex 审查发现启动位置工程的异步队列晚于 ready；改为先等待同一 open queue 的初始 Task 终态，再创建 Automation Controller | Headless 真实进程首次 ready/status 工程断言与最终串行 64/64；`E-P3-REVIEW-002`、`E-P3-CTEST-FULL-008` |
| `E-P3-REVIEW-003` | Codex 审查发现已获 ACK、尚待主线程投递的启动期单实例请求仍可能晚于 ready；Controller 前安装 handler 并以 IPC worker 屏障收齐已确认请求 | 真实进程与 ACK-to-main 回归通过；随后 `E-P3-REVIEW-004` 封闭屏障后的剩余窗口 |
| `E-P3-REVIEW-004` | Codex 复审发现一次性屏障返回到 Controller ready 之间仍可确认新请求；最终屏障改为原子暂停 open/activate dispatch，窗口内请求不 ACK，ready 后恢复 | 暂停/恢复组件断言、最终聚焦 2/2 与串行 64/64；`E-P3-REVIEW-004`、`E-P3-CTEST-FULL-008` |
| `E-P3-REVIEW-005` | Codex 建议因 Primary Host mode 不同而拒绝 Headless Secondary；该建议与既有单实例契约冲突，未采纳。按用户要求仅增加成功转发提示，不改变请求、ACK、Primary 或退出码 | GUI Primary/Headless Secondary 进程断言与最终串行 64/64；`E-P3-REVIEW-005`、`E-P3-MAIN-SYNC-FOCUSED-002`、`E-P3-CTEST-FULL-008` |
| `E-P3-MAIN-SYNC-001` | 合入 main PR #176 时，公开任务 continuation 与 busy admission 需要覆盖三期新增的 Native JSON-RPC；将内部来源泛化并保持 Native/MCP 共用 admission | `TestAutomationCore` 与 `TestMcpProcessIntegration` 2/2，最终串行 64/64；`E-P3-MAIN-SYNC-001`、`E-P3-MAIN-SYNC-FOCUSED-002`、`E-P3-CTEST-FULL-008` |
| `E-P3-SIGNAL-FIXTURE-001` | Windows 首版进程测试未让测试进程与目标控制台建立确定关联，API 返回成功但事件未送达；改为独立隐藏控制台并在发送时临时附加 | focused 进程测试通过、最终串行 64/64；`E-P3-SIGNAL-PROCESS-001`、`E-P3-CTEST-FULL-008` |

阶段提交链：

| Commit | 内容 |
|---|---|
| `415d8ce7` | `docs(automation): plan phase three headless delivery` |
| `506608e0` | `refactor(runtime): split core and gui host composition` |
| `3e616bc6` | `refactor(automation): remove window identity from application lifecycle` |
| `20a9d757` | `feat(automation): enforce public host capabilities` |
| `c5962c67` | `feat(automation): serve native json-rpc in headless mode` |
| `6e88b271` | `fix(headless): remove hidden gui runtime dependencies` |
| `2cbc9766` | `test(headless): cover qcore and native workflows` |
| `74915546` | `fix(build): include debug tests in preset build`（中间调整，最终已恢复） |
| `06c1a2e7` | `fix(audio): preserve gui device failure feedback` |
| `d0d64289` | `fix(automation): honor configured response limit` |
| `d852be21` | `fix(build): restore debug preset targets`（最终与基线一致） |
| `42a20353` | `fix(headless): delay readiness until startup open` |
| `1c635df6` | `feat(headless): handle console termination signals` |
| `a36a3f56` | `test(headless): cover console graceful exit` |
| `14bd71c6` | `fix(headless): drain forwarded opens before readiness` |
| `5ad8eab9` | `fix(headless): close readiness forwarding race` |
| `31681e78` | 合入 main PR #176 |
| `2f2021ea` | `refactor(automation): align public task admission` |
| `871ed195` | `fix(headless): report forwarded secondary launch` |

首次失败证据、修复提交、最小复验、所属域复验和最终串行 CTest 已形成闭环；最终候选无未关闭的
测试失败。

## 11. 数据安全、证据与清理

- 授权素材根在本轮完全未访问、未复制；测试只生成自己的 WAV/DSPX fixture；
- 因未使用任何授权原文件，前后 hash 不适用，原件也因未访问而未被修改；
- 原始请求响应、进程/监听/窗口快照、日志和运行期标识仅保存在仓库外私有归档；
- 报告只引用匿名 Evidence ID，不记录私有位置或运行期标识；
- `git diff --check` 与旧错误码 `host_unavailable` 的源码检索均通过；
- 最终审计确认无测试拥有的 Editor、Connector 或测试进程，无 listener、QLocal/Primary、Task、锁、
  临时目录残留；
- 测试前已运行的 Release Connector 进程不属于本轮测试所有，始终未被接管或改动。

最终匿名证据索引：

| Evidence ID | 内容 |
|---|---|
| `E-P3-CLEAN-001` | clean/build 前置记录 |
| `E-P3-HEADLESS-001` | QCore Headless、无效 GUI platform、零窗口、status 与生命周期 |
| `E-P3-NATIVE-001` | Native status、编辑、Undo/Redo、Host gate、无 discover 与退出 |
| `E-P3-HTTP-002` | 共享 HTTP/Native 组件复验 |
| `E-P3-MCP-BOOTSTRAP-001` | Headless Native/MCP 共存、Bootstrap 与 Connector 语义 |
| `E-P3-DOMAIN-001` | 编辑、History、文件、音频、Task、设置和错误的代表语料 |
| `E-P3-PROCESS-001` | QCore 零窗口、生命周期、端口冲突、单实例与进程清理 |
| `E-P3-GUI-001` | GUI 176 项集合、GUI-only 代表操作与 Connector/MCP 回归 |
| `E-P3-BUILD-004` | 测试命令未显式指定 `all` 导致目标缺失的首次记录 |
| `E-P3-FINAL-BUILD-004` | 控制台退出扩展前的最终 Debug 构建 |
| `E-P3-FINAL-BUILD-011` | 保持 preset 默认目标不变的 Debug 显式 `all` 构建 |
| `E-P3-CTEST-LIST-002` | 控制台退出扩展前的 64 项 CTest 文本清单 |
| `E-P3-CTEST-LIST-006` | 最终 CTest 文本清单与可执行文件核对 |
| `E-P3-CTEST-JSON-002` | 控制台退出扩展前的 CTest JSON 清单 |
| `E-P3-CTEST-JSON-006` | 最终 CTest JSON 清单 |
| `E-P3-CTEST-FULL-004` | 控制台退出扩展前的最终串行 64 项执行 |
| `E-P3-CTEST-FULL-008` | 控制台退出扩展后最终串行 64 项完整执行 |
| `E-P3-SIGNAL-FIXTURE-001` | 首版 Windows 控制台投递 fixture 失败与根因 |
| `E-P3-SIGNAL-PROCESS-001` | Windows `CTRL_BREAK_EVENT` 脏文档进程退出与资源释放 |
| `E-P3-SIGNAL-PTY-001` | Windows 前台 PTY 实际 `Ctrl+C`、信号日志和 Editor 退出码 0 |
| `E-P3-REVIEW-001` | Codex 审查问题、修复提交与专项回归链 |
| `E-P3-REVIEW-002` | 启动位置工程与 Automation readiness 时序修复及进程回归 |
| `E-P3-REVIEW-003` | 启动期已确认转发请求的 ACK-to-main 屏障、队列终态与 readiness 回归 |
| `E-P3-REVIEW-004` | 最终转发屏障的原子暂停、延迟 ACK、ready 后恢复及回归 |
| `E-P3-REVIEW-005` | Host mode 拒绝建议的契约判断、撤回记录与 Headless Secondary 提示 |
| `E-P3-MAIN-SYNC-001` | main PR #176 合并及 Native/MCP 公开任务 admission 整合 |
| `E-P3-MAIN-SYNC-FOCUSED-002` | 公开任务 admission 与 GUI Primary/Headless Secondary 提示的聚焦回归 |
| `E-P3-SCOPE-001` | 恢复项目 preset 并将 `all` 限定到测试命令的范围修正 |
| `E-P3-FINAL-AUDIT-004` | 控制台退出扩展前的最终审计 |
| `E-P3-FINAL-AUDIT-008` | diff、preset、控制台退出、进程与资源最终审计 |
| `E-P3-CLEANUP-001` | 测试拥有资源 cleanup manifest |

## 12. 最终通过清单

- [x] 测试源候选、分支阶段提交和最终产品代码已冻结。
- [x] Headless 确为 QCore、零 GUI 对象/窗口、一个真实文档。
- [x] 25/151 Host 差集、Registry binding、value source 和 Host gate 关系成立。
- [x] Native 固定 route、JSON-RPC envelope、无 discover 与统一错误通过。
- [x] Native/MCP 共用 listener、Registry、Admission 与有序停止通过。
- [x] Bootstrap、Connector、单实例、转发原子屏障/ready 顺序、退出/重启和致命启动失败通过。
- [x] Headless 控制台终止与公开 discard exit 等价；GUI 未注册该 handler。
- [x] 151 项 QCore 资格与真实文件/Task/业务代表路径通过。
- [x] GUI MCP/Connector 回归通过，Computer Use 使用符合最小必要原则。
- [x] Debug 显式 `all` 构建与一次最终串行完整 CTest 通过，preset 默认目标未修改。
- [x] 授权素材原件未修改，测试拥有资源已清理，Evidence 索引完整。

最终签署：**通过**。
