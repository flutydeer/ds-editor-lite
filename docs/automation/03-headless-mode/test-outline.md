# 三期 Headless 与原生 JSON-RPC 全量测试大纲

## 1. 目标与分母

本大纲验证三期从 Host composition、公共契约到真实 Headless/GUI 进程的完整链路。正式结论只从
同一最终候选的构建、确定性测试、真实进程资格、GUI 回归和一次串行全量 CTest 产生。

当前公共能力候选快照为：

```text
Editor 公共 Contract：176
GUI-only：25
Headless both：151
Connector 桥接工具：6
```

数量用于描述候选，不作为硬编码正确性证明。正式分母由权威 Public Tool Contract 生成，并验证
Contract、Registry binding、Host 分类、MCP 实际发现与 Connector 实际目标之间的集合关系。
真实进程不对 151 项做 Native/MCP 双协议逐项重复调用；完整性由 Contract、binding、Host gate
和 QCore 资格测试负责，产品进程按 Query、同步 Command、异步 Command、文件、设置、生命周期
和错误类型选取代表语料。

## 2. 测试原则与层次

测试控制方式按以下优先级使用：

1. 确定性 CTest、命令行进程测试及操作系统级进程、listener 和窗口检查。
2. Editor 直连 MCP 与 DS Connector Lite MCP。
3. 日志、QLocal、HTTP、任务和模型状态回读。
4. 只有前三类证据无法完成必要 GUI 断言时，才使用最小范围 Computer Use。

默认预期 Computer Use 使用次数为零。GUI smoke 技能中的交互建议不构成调用 Computer Use 的
理由；声库选择、音符创建、播放、保存和 GUI operation 若能由 MCP 与状态回读完成，不再用鼠标
重复。

| 层次 | 验证对象 |
|---|---|
| 静态契约 | Host metadata、ID、Schema、`value_sources`、错误值和 binding 集合 |
| 单元 | HostMode/CLI、WindowContext、Policy、Host gate、JSON-RPC codec、生命周期 adapter |
| 组件 | Automation HTTP route、Native/MCP dispatcher、限制、停止与混合 admission |
| 进程 | QCore composition、无窗口、Native、MCP、QLocal、单实例、退出/重启与端口冲突 |
| 业务域 | 151 项 QCore 资格、Facade/History/revision/Task/File Guard 代表语义 |
| Connector | Headless 实际工具、固定 wrapper、状态、错误和 Bootstrap |
| GUI 回归 | QApplication/真实窗口、176 项目录、代表 GUI operation 与既有 MCP 生命周期 |
| 资格 | 真实工程、音频、导出、推理/不可用事实、资源和清理 |

## 3. 启动参数与 HostMode

### 3.1 `--headless` 预解析

- `--headless` 在选项区出现时选择 `QCoreApplication`。
- 重复 `--headless` 的最终校验语义稳定。
- `--` 后的 `--headless` 保持位置参数，不改变 Host mode。
- 参数中包含同名子串、工程名或路径时不误判。
- 预解析只选择 Application 类型，完整错误仍由唯一 `StartupArguments` 返回。
- 无 `--headless` 时保持 GUI `QApplication` 路径。

### 3.2 完整 CLI

- `--control-port` 覆盖 `1`、`65535` 与正常中间值；拒绝 `0`、负数、越界、缺值和非整数。
- `--mcp` 与 `--no-mcp` 的合法、重复和冲突组合按既有 CLI 契约处理。
- `--control-level l1|l2|l3|custom` 的合法值、大小写和非法值覆盖。
- CLI override 只作用于当前运行，不改写持久设置。
- 既有位置工程参数、多个 open request、`activate/openProjects`、单实例转发顺序与单文档替换
  行为在 GUI/Headless 保持一致。

## 4. QCore composition 与无 GUI 资格

### 4.1 对象装配

- Headless 根 Application 的动态类型为 `QCoreApplication`，不存在
  `QGuiApplication/QApplication` 实例。
- Core composition 包含 AppModel、History、Package、Automation、Audio、Playback、文档、
  推理和 Task 所需对象，并完成与 GUI 同源的默认模型 wiring。
- GuiContext、ThemeManager、MainWindow、QWindowKit、GUI Controller、Clipboard、LevelMeter、
  Widget view/controller 和 GUI 提取控制器均不创建。
- Headless listener ready 前通过 Document Facade 默认 draft/commit 创建一个空白文档；存在位置工程
  参数时，同一 open queue 必须先等其 Task 终态，首次 ready/status 不得暴露默认空白文档。
- GUI 仍在 MainWindow 创建后的既有时序初始化文档，视图可收到初始模型信号。

### 4.2 隐藏 GUI 依赖

- 故意设置无效 `QT_QPA_PLATFORM`，Headless 仍可启动并服务 Native。
- Windows 按进程 PID 枚举顶层窗口，Headless 全生命周期数量为零。
- 监测 QObject 类型、日志和平台资源，确认未间接创建 QMessageBox、Toast、ThemeManager、
  DocumentWorkflow 或 QWidget。
- Settings 主题 query/update 仅读写配置，返回当前界面 availability，不创建主题系统。
- Audio 设备初始化失败返回稳定能力/错误事实，不弹窗。
- AudioDecoding 从 DocumentSession/Runtime 取得工程路径，不通过 singleton fallback 创建 GUI。
- Playback 无 screen 时使用确定的 60 Hz fallback。
- translator 与 Restarter 在 QCore 下完成装配和调用。

## 5. 身份、Host gate 与错误

### 5.1 Document/Window 身份

- Headless 初始状态恰有一个非空真实 `document_id` 和准确 revision。
- `application.get_status` 返回 `host_mode=headless`、一个 document 与 `windows: []`。
- status、请求、成功结果、错误 details 和 Task 均不生成或回退到伪造 `window_id`。
- 文档 Query/Command 只按显式 `document_id/revision` 路由。
- GUI status 继续返回一个真实 WindowId；GUI-only operation 校验该真实 ID。

### 5.2 Contract 与 Host 集合

- Public Contract operation ID 完整且唯一。
- GUI-only 集合精确等于 workspace 2、track_panel 7、clip_editor 16；其余均为 `both`。
- GUI-only 与 both 互斥且并集等于权威 Contract。
- Registry binding 等于 Contract；每项 binding 的 Host metadata 一致。
- 所有 `value_sources` 的 provider 存在、控制层级可达且 Host metadata 合法。
- `both` operation 不因模块/设备暂不可用而被误分为 GUI-only。

### 5.3 Gate 顺序与错误值

- 未知工具返回 `tool_unavailable`；Native 未知 method 返回 `-32601`。
- 已知 GUI-only operation 在 Headless 返回 `host_capability_unavailable`。
- Host gate 在 control/custom、Schema、File Guard、Admission 和 handler 前执行。
- 构造缺失/非法 `window_id` 的 GUI-only Native 请求，仍先得到 Host capability 错误。
- Host 支持但控制策略禁止时返回 `permission_denied`。
- `editor_not_running` 与 `editor_not_connected` 只描述实例/连接事实。
- Native、Editor MCP、Connector wrapper、describe/status 和测试 golden 中不再出现对外
  `host_unavailable`；不保留别名。

## 6. Native JSON-RPC 与 HTTP

### 6.1 首次调用与成功路径

- `POST /automation/v1` 的首次 `application.get_status` 返回脚本继续执行所需的 Host、工具版本、
  control level、DocumentId 和 revision。
- `params` 省略与 `{}` 对无参方法等义。
- Query 直接返回公共 output Schema，不含 MCP `structuredContent`。
- 同步 Command 返回与 Facade 相同的 Mutation、revision、created/affected object。
- 异步 Command 返回现有 TaskId，并可由 `tasks.list/get/cancel` 观察到终态或取消。
- 字符串 ID 与 JSON 安全整数 ID 原样回显。

### 6.2 JSON-RPC envelope

- malformed JSON 返回 Parse Error。
- 顶层标量、`null`、Array/Batch、空数组和 notification 返回 Invalid Request。
- 缺失、`null`、布尔、小数、超出 JSON 安全整数范围及非法形状的 `id` 被拒绝。
- `jsonrpc` 缺失、非字符串或非 `"2.0"`，`method` 缺失/非字符串，未知顶层字段的行为稳定。
- `params` 省略或 object 合法；`null`、array、scalar 非法。
- 未知 method 返回 `-32601 Method not found`。
- `automation.discover` 明确返回 Method not found。
- Schema 失败返回 `-32602 Invalid params` 和稳定字段路径/details。
- 业务失败放在 `error.data`，保留 code、message、operation_id、field_path 和 details。

### 6.3 HTTP 与安全限制

- route 只接受规定的 HTTP method、Content-Type 和 Accept。
- listener 只绑定 `127.0.0.1`；Host、Origin、DNS rebinding 与远端地址拒绝矩阵覆盖。
- 请求 body、JSON 深度/节点、响应体和 deadline 的最小、边界与超限路径覆盖。
- 安全响应头在成功、协议错误、业务错误与 transport 拒绝中一致。
- Native 不建立 MCP session，不接受 MCP header/lifecycle 替代 JSON-RPC envelope。
- 一个 HTTP 请求最多承载一个请求对象，不因连接复用形成隐式 batch。

## 7. 共享 Automation Server 与路由生命周期

- GUI MCP disabled 时不监听 Automation HTTP；enabled 时只注册 `/mcp`。
- Headless MCP disabled 时仍监听 `/automation/v1`，`/mcp` 不可用。
- Headless MCP enabled 时两条 route 在同一 listener/端口同时可用。
- Native 与 MCP 共用 PublicAutomationRegistry、Host/Policy、File Guard、Admission 和业务状态。
- MCP 既有协议版本、session、header、notification/cancel 和结果塑形不回退。
- Native 请求不进入 MCP session 数量、淘汰或初始化状态。
- Native/MCP 混合在途请求共同遵守 transport 与 Registry admission 硬上限。
- 成功、拒绝、deadline、断开、route disable 和 shutdown 均释放配额。
- 有序停止先关闭 admission，再完成或终止在途请求，最后释放 listener 和 route 状态。
- Registry、访问根或端口绑定失败使 Headless 输出 stderr、清理全局资源并非零退出。

## 8. QLocal Bootstrap 与单实例

- GUI 与 Headless 使用相同产品身份、Primary 锁和 QLocal 服务名。
- GUI 已运行时 Headless 不成为第二 Primary；Headless 已运行时 GUI 同样不能成为第二 Primary。
- Headless `--no-mcp` 的 Bootstrap 为 `host_mode=headless`、`state=server_disabled`、
  `server_endpoint` 空。
- 上述 MCP disabled 状态不影响 Native listener 的实际可用性。
- Headless `--mcp` 时 Bootstrap 状态只在 MCP route 接受请求后进入 `server_ready`。
- Connector 只根据 `/mcp` 与 Bootstrap 工作，不连接或披露 Native route。
- instance ID、PID、MCP endpoint 和 Host mode 变化触发 Connector epoch 切换。
- 异常退出、启动失败、restart 和正常退出后 Primary、QLocal、watcher 与端口均释放。

## 9. 应用生命周期

- clean 文档的 exit 默认接受，响应先于事件循环退出完成。
- dirty 文档且缺少/为 false 的 `discard_changes` 返回 `busy`，字段路径准确。
- dirty 拒绝后进程、Native/MCP、文档和 History 保持可用，不出现模态窗口。
- dirty 文档传 `discard_changes: true` 后先返回接受结果，再退出。
- restart 复用当前 exe、原参数和工作目录，产生新的 instance ID。
- Headless restart 后仍为 QCore Host，Native 固定可用，MCP 开关保持当前启动语义。
- Windows `CTRL_C_EVENT`/`CTRL_BREAK_EVENT` 与 Unix `SIGINT`/`SIGTERM` 在 Qt 主线程请求固定 discard
  exit，脏文档也正常退出且退出码为 0。
- 接受控制台终止后重复信号被合并；busy 拒绝时进程保持可用并允许后续信号重试。
- GUI 不注册 Headless 控制台 handler；无前台控制台时仍使用公开退出 operation。
- GUI 菜单/窗口关闭继续使用交互式保存 Prompt；公共调用仍无弹窗。
- exit/restart 不重复派发，不留下孤儿 Editor/Connector、listener、QLocal、锁或 Task。

## 10. 151 项 QCore 业务资格

### 10.1 确定性完整性

- Contract/binding 集合证明每个 `both` operation 具有类型化 handler 和 QCore Host 资格。
- 按域执行已有 Facade 行为测试，覆盖正常、no-op、拒绝、History、revision、文件与 Task 的
  独特语义。
- 没有 handler 的能力不得通过 metadata 冒充 `both`；偶然 GUI 依赖必须在唯一实现路径消除。
- application-scoped 操作和 Task 不伪造 document/window；document-scoped 操作保持 generation。

### 10.2 真实进程代表语料

- Query：status、documents、轨道/剪辑/音符、能力或设置查询。
- 同步 Command：代表性编辑、Undo/Redo、播放状态或设置更新。
- 异步 Command：文档 open/import、音频、导出、包刷新或推理中的可用代表路径。
- 文件：从授权只读副本打开，在测试工作区 save/save-as/import/export，验证 File Guard。
- Task：接受、list/get、进度/终态、取消和文档换代边界。
- 模块不可用：无声库、模型、codec 或设备时保存稳定 supported/available/error 事实，不伪造通过。
- 代表性 Native 与 MCP 调用比较结果、AutomationError、revision、History 和 Task 状态。

## 11. 精简真实 Headless 进程场景

进程测试统一设置 `RUN_SERIAL TRUE` 和 `RESOURCE_LOCK editor_primary`，隔离
APPDATA/LOCALAPPDATA、访问根和端口，并只管理测试拥有的 PID/process handle。

关键场景：

1. 无效 `QT_QPA_PLATFORM` 下 Headless 仍成功启动。
2. Windows 窗口枚举确认该 PID 无顶层窗口。
3. `--no-mcp` 下 Native 正常，Connector 准确报告 MCP disabled。
4. `--mcp` 下 Native 与 MCP 同时工作。
5. status 返回真实 document 与空 windows。
6. Native 完成代表性查询、编辑及 Undo/Redo。
7. 只读素材副本完成文档打开、保存和 Task 终态。
8. 覆盖音频导入、导出、推理或稳定模块不可用事实。
9. GUI-only operation 返回 `host_capability_unavailable`。
10. GUI/Headless 竞争同一 Primary。
11. 端口冲突导致 Headless 非零退出。
12. clean/dirty/discard/restart 生命周期闭环。
13. Windows 独立控制台进程组用 `CTRL_BREAK_EVENT`，Unix 分别用 `SIGINT`、`SIGTERM`，验证脏文档
    discard exit、退出码 0 与资源释放。
14. Windows 前台 PTY 发送实际 `Ctrl+C`，不使用 Computer Use。
15. 最终无 listener、QLocal、Task、锁或进程残留。

每个 timeout/crash 场景使用 watchdog，先保存日志、进程、端口和窗口快照，再精确结束测试拥有的
进程；不使用宽泛进程名终止用户进程。

## 12. Headless MCP 与 Connector

- Headless enabled MCP 的 `tools/list` 最大实际集合为当前 151 项 both 候选，并受控制层级/Custom
  继续过滤。
- Native 与 MCP 的代表 Query、同步 Command、Task、文件和错误语料等价。
- Connector 固定六个桥接工具与二期已知类型化 wrapper 面保持契约。
- `editor.tools.list/search/describe` 只返回 Headless 当前实际工具。
- GUI-only wrapper 的实际调用返回 `host_capability_unavailable`，不误报工具不存在或权限不足。
- Connector status 准确区分 Headless Host、MCP disabled、Editor 未运行和未连接。
- GUI/Headless restart 或 Primary 切换后，Connector 不复用旧 instance 的请求、Task 或目录缓存。

## 13. GUI 回归

- GUI 仍创建 `QApplication`、一个 MainWindow 和一个真实 WindowId。
- L3 Editor MCP 的实际目录保持完整权威公共集合候选 176 项。
- `application.get_status` 返回一个 document 和一个真实 window。
- 代表性 document mutation、Undo/Redo、播放及保存正常。
- workspace、track_panel、clip_editor 各选代表 operation，验证真实 GUI 状态与 query 回读。
- MCP 启停、换端口、退出、重启、单实例和 Connector 自动重连不回退。
- MCP mutation 后 GUI 模型立即一致；GUI mutation 后 MCP 查询与 revision 一致。
- GUI/offscreen 组件轮显式设置有效 Qt platform plugin 路径。
- watchdog、日志、窗口枚举和测试进程句柄负责处理 Debug Error、模态窗口和阻塞；只有仍存在
  无法自动断言的必要缺口时才使用最小 Computer Use。

## 14. 构建、CTest 与通过标准

正式候选使用项目标准 DevShell/preset 脚本并显式指定 `all`，完成 Debug 全目标配置和构建，不修改
项目 preset 的默认目标。随后：

```powershell
ctest --test-dir build/Debug -N
ctest --test-dir build/Debug --show-only=json-v1
ctest --test-dir build/Debug --output-on-failure -j 1
```

CTest 数量以本轮配置结果为准。失败必须保存首次证据，修复后依次运行最小复现、所属测试域、
适用的 Native/MCP/Connector 或 GUI 等价回归，并在同一最终候选上重新执行一次完整串行 CTest。

通过标准：

- Host Contract/Registry/value source 集合关系成立，25/151 候选差集无未解释漂移。
- Headless 确为 QCore、零 GUI 对象、零窗口，并具有一个真实文档。
- Native 固定 route、可选 MCP route、共享业务门禁及致命启动失败语义满足契约。
- Headless 控制台事件和公开 discard exit 复用同一生命周期，GUI 启动路径不受影响。
- Native JSON-RPC envelope、错误映射、安全限制和无 discover/catalog 边界完整。
- QCore 业务资格、真实进程代表路径、Connector 与 GUI 回归形成同一候选证据。
- 完整 Debug build 与一次最终串行全量 CTest 完成，所有测试拥有的资源和产物受控清理。
