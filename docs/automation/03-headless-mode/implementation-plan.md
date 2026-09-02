# 三期：真正 Headless 与原生 JSON-RPC 实施计划

## 1. 交付目标与边界

三期在[二期 MCP Server 与 DS Connector Lite](../02-mcp-server-and-connector/implementation-plan.md)
基线上完成同一产品进程的两种 Host composition，并为 Headless 提供确定、无发现步骤的原生
JSON-RPC 入口。业务语义继续由二期公共 Contract/Registry 与一期类型化 Facade 承载，不建立
平行的 Headless 业务 Runtime。

| 模式 | Qt Application | Native JSON-RPC | MCP | 产品身份 |
|---|---|---|---|---|
| GUI | `QApplication` | 不提供 | 按配置启用 | 1 document + 1 window |
| Headless | `QCoreApplication` | 固定提供 `/automation/v1` | 独立按配置启用 | 1 document + 0 windows |

三期的核心交付是：

- Headless 启动全程不创建 `QGuiApplication`、`QApplication`、MainWindow、对话框、
  ThemeManager、QWindowKit、`QWidget` 或平台窗口；
- 二期 176 个 Editor 公共 operation 中，除 25 个 GUI operation 外的 151 个 operation
  均取得 QCore Host 资格；
- Headless 只创建并返回真实 `document_id`，不生成、返回或要求 sentinel/null UUID
  形式的 `window_id`；
- Native、Editor MCP 与 Connector wrapper 共享 Registry、Host/Policy 门禁、File Guard、
  Admission、类型化 binding、Facade、错误、revision、History 和 Task 生命周期；
- GUI 的单文档单窗口行为、位置工程参数、单实例转发及 MCP 行为保持兼容。

本期明确不实现：

- 多窗口、多文档或 Document/Window Registry；
- 独立 Headless 可执行文件或为缩小安装包而拆分链接依赖；
- GitHub Actions、统一 CI 基建或 Skill 封装；
- Native push、订阅、传输取消、JSON-RPC Batch 或 notification；
- `automation.discover`、运行时 Operation Catalog、Schema digest 或动态脚本编排查询；
- `--control-port 0`、Ready File、`--automation-profile` 或协议互斥开关；
- 已移除的 `operations.*` 别名，异步执行继续使用 `tasks.*`。

旧 [Issue #96](https://github.com/flutydeer/ds-editor-lite/issues/96) 中与上述边界冲突的早期
设想视为已被当前权威设计覆盖，不作为三期兼容要求。

## 2. 基线、分支、文档与提交

### 2.1 执行基线

实施前执行以下基线检查：

1. 确认工作树干净，且不存在测试所有权之外的 Editor/Connector 进程。
2. `git fetch origin`，将本地 `main` fast-forward 到最新 `origin/main`。
3. 从该提交创建本地分支 `headless`。
4. 不从历史残留的本地 `mcp` 分支分叉、合并或挑选提交。
5. 不自动 push、创建 PR 或改写任何远端分支。

若已经存在同名 `headless` 分支，停止分支创建并核对其来源，不删除、不覆盖。正式实现以同步后的
`origin/main` 为唯一分叉基线。

### 2.2 正式文档

`docs/automation/03-headless-mode/` 包含：

- [实施计划](implementation-plan.md)；
- [Headless 契约矩阵](headless-contract-matrix.md)；
- [全量测试大纲](test-outline.md)；
- [测试执行计划](test-plan.md)；
- `implementation-report.md`，在实现与正式测试完成后回填；
- `test-report.md`，在正式测试完成后回填。

`implementation-report.md` 同时承担执行报告职责，不另建重复的 `execution-report.md`。仓库文档
只引用匿名 Evidence ID，不记录用户目录、素材名称、实际端口、PID、Document/Object/Task ID。

### 2.3 阶段提交

计划提交边界如下；测试发现的问题另用独立 `fix(scope): summary` 提交：

```text
docs(automation): plan phase three headless delivery
refactor(runtime): split core and gui host composition
refactor(automation): remove window identity from application lifecycle
feat(automation): enforce public host capabilities
feat(automation): serve native json-rpc in headless mode
fix(headless): remove hidden gui runtime dependencies
test(headless): cover qcore and native workflows
docs(automation): report phase three delivery
```

每个阶段提交保持职责单一，并在其可用范围内完成构建或最小保护测试；不提交构建产物、私有证据或
测试工作区。

## 3. 启动入口与 Host mode

### 3.1 预解析与完整参数校验

新增统一的 `HostMode { Gui, Headless }`。入口在创建任何 Qt Application 对象之前，只从原始
`argc/argv` 预解析 `--headless`，以决定构造 `QApplication` 还是 `QCoreApplication`。

预解析遵守以下约束：

- `--headless` 在 `--` 前生效；
- `--` 后的同名字符串仍是位置参数；
- 重复、冲突、缺值、非法端口和非法控制层级等完整规则不在预解析器中复制；
- Qt Application 创建后仍由唯一的 `StartupArguments` 完成全部 CLI 校验；
- 既有位置工程参数、`activate/openProjects` 解析、单实例转发、队列顺序和单文档替换语义不变；
- Headless 在 Automation Controller 构造前安装单实例请求处理器，并以 IPC worker 屏障收齐已确认
  的转发请求；位置工程与这些启动期转发工程均由同一 open queue 到达终态后才发布 ready。

最终 CLI 保持：

```text
--headless
--control-port <1..65535>
--mcp | --no-mcp
--control-level l1|l2|l3|custom
```

`--control-port` 不接受 `0`。MCP 与 Native 不互斥；Headless 的 Native route 不受
`--mcp/--no-mcp` 开关控制。

### 3.2 Application 环境

`AppEnvironment` 拆成 common 与 GUI-only 初始化：

- common：应用身份、路径、日志、translator、通用配置和事件循环所需初始化；
- GUI-only：高 DPI、样式、字体、主题及任何要求 `QGuiApplication/QApplication` 的初始化。

translator、Restarter 与其他通用逻辑使用 `QCoreApplication` API。任何在 Host mode 判定前调用
GUI 静态 API、创建平台资源或隐式初始化 Widgets 的路径均视为缺陷。

## 4. Core 与 GUI Composition

### 4.1 Core composition

GUI 与 Headless 始终创建同一套 Core composition：

- AppStatus、AppOptions、AppModel、ParamUtils；
- HistoryManager、PackageManager、Automation `CoreRuntime`；
- SynthrtEngine、InferEngine；
- AudioSystem、AudioDecoding；
- EditSession、Playback、ProjectPackageResolver、InferController；
- 公共模型默认值、包状态和音频模型 wiring。

Core composition 不得依赖 MainWindow 或从 GUI singleton 反向取得服务。Headless 在 HTTP Server
进入可用状态前，通过现有 Document Facade 的默认 draft/commit 路径建立第一个空白文档，保证
默认值、History、revision 和业务信号与 GUI 同源。

### 4.2 GUI composition

仅 GUI 创建 `GuiContext` 及以下对象：

- ThemeManager、MainWindow；
- LevelMeter、Clipboard、Track、Clip、EditorView、UndoRedo；
- GUI Pitch/MIDI 提取控制器；
- ProjectStatus、DirectManipulation；
- AppController、DocumentWorkflow；
- WindowPlacement、GUI ExternalOpenRequestQueue。

GUI 保持现有 MainWindow 创建后的模型初始化时序，避免视图错过初始模型信号。Host 拆分不得重新
设计 GUI 的文档替换、外部打开或交互式保存询问。

### 4.3 Headless 宿主适配器

Headless 对涉及用户交互的既有路径注入无弹窗宿主适配器。适配器必须显式返回接受、拒绝或
不可用事实，不得通过隐藏窗口、Toast、默认按钮或模态对话框替代决策。

工程加载、批量导入、音频导入、路径更新、导出和推理继续复用现有 Headless/Facade/Task 实现，
不增加第二套文件解析、任务或提交 DTO。

## 5. 无窗口 Runtime、身份与生命周期

### 5.1 Optional window

将 `SingleWindowContext` 改为显式 optional window：

- GUI 构造并持有真实 `WindowId`；
- Headless 始终保持 `std::nullopt`；
- 不生成 null UUID、固定 UUID 或 sentinel；
- `validateWindow()` 先判断 Host 是否具有窗口能力，再比较请求 ID；
- 文档路由仅依赖 `document_id` 与适用的 `expected_revision`，不经过 WindowContext。

`application.get_status` 在 Headless 返回一个真实活动文档和 `windows: []`，不得回退到
`runtime.windowId()` 或从文档身份推导窗口身份。

### 5.2 应用生命周期

退出和重启从 GUI context 中分离为：

- `ApplicationCommandContext`；
- `ApplicationMutationResult`；
- `dispatchApplicationCommand`。

公共生命周期调用规则：

- clean 文档默认接受；
- dirty 文档且未提供 `discard_changes: true` 时返回 `busy`，字段路径为
  `discard_changes`；
- 自动化路径不显示保存询问、Toast 或其他决策 UI；
- 接受后先返回 `{accepted, action, discard_changes}`，再由事件循环退出；
- restart 只复用当前可执行文件、原参数和工作目录。

GUI 菜单、窗口关闭和其他用户发起路径继续使用现有交互式 Prompt 语义。

### 5.3 Headless 控制台终止

仅 Headless 在默认文档建立后、处理启动工程队列前安装内部
`HeadlessTerminationHandler`；GUI 启动路径不创建或注册该组件。控制台事件在 Qt 主线程复用
`ApplicationAutomationFacade::requestTermination`，固定使用 `Exit`、`discard_changes=true`、
`InternalAutomation` 和 `client_id=console-signal`，不新增 CLI、配置项或公开 operation。

| 平台输入 | 系统事件 | Headless 行为 |
|---|---|---|
| Windows 前台 `Ctrl+C` | `CTRL_C_EVENT` | 优雅退出 |
| Windows `Ctrl+Break` | `CTRL_BREAK_EVENT` | 优雅退出 |
| Linux/macOS 前台 `Ctrl+C` | `SIGINT` | 优雅退出 |
| Linux/macOS `kill <pid>` 或常规容器 stop | `SIGTERM` | 优雅退出 |

Windows 原始 handler 只通知 auto-reset event，由 `QWinEventNotifier` 将事件送回主线程；Unix 使用
`sigaction` 和 non-blocking、close-on-exec `socketpair`，原始 signal handler 只执行
async-signal-safe `write()` 并保留 `errno`。RAII stop 会移除 Windows handler 或恢复原 POSIX
signal action。接受退出后吞掉重复事件，不实现第二次中断强退；若当前文档 busy，记录 warning、保持
运行并允许稍后重试。

该入口是本机进程控制能力，不经过 Registry、control level 或 File Guard；业务判断、退出码和
listener、QLocal、Task、音频及 Runtime 清理仍由既有 Facade 和事件循环负责。没有前台控制台的
进程继续使用 `application.request_exit`。`CTRL_CLOSE_EVENT`、注销/关机、`SIGKILL`、
`TerminateProcess` 与 IDE 强制 Stop 不承诺优雅清理。

## 6. Host capability 与统一错误

### 6.1 公共 Host 元数据

二期公共 Contract 的 Host metadata 校正为：

```text
GUI-only = workspace 2 + track_panel 7 + clip_editor 16 = 25
both     = Editor public 176 - GUI-only 25 = 151
```

数量只是当前候选快照。正式门禁使用[Headless 契约矩阵](headless-contract-matrix.md)定义的
Contract/Registry 集合关系，不将数量或手工列表作为第二权威源。所有 `value_sources` 的
Host metadata 必须与其 provider 和消费方关系一致。

### 6.2 Registry Host gate

Registry 新增独立、可测试的 Host gate，并固定执行顺序：

```text
known operation
→ host capability
→ control level / custom permission
→ input schema
→ file guard
→ admission
→ typed handler
```

因此，Headless 调用已知 GUI-only operation 时，在读取或校验 `window_id` 之前返回
`host_capability_unavailable`。未知 operation 仍返回 `tool_unavailable`；Native 的未知 method
映射为 JSON-RPC `-32601 Method not found`。

### 6.3 错误边界

| 错误 | 固定含义 |
|---|---|
| `tool_unavailable` | 工具不存在、未注册或上游没有该工具 |
| `host_capability_unavailable` | 工具存在，但当前 GUI/Headless Host 缺少执行能力 |
| `permission_denied` | 工具存在且 Host 支持，但当前控制策略禁止 |
| `editor_not_running` | 当前没有可发现的 Editor 实例 |
| `editor_not_connected` | 已知 Editor 但传输链路未连接 |

Connector 对外使用的 `host_unavailable` 迁移为既有的
`host_capability_unavailable`，不保留别名。Native、Editor MCP、Connector wrapper、
`editor.tools.describe`、`connector.get_status` 及业务错误详情使用同一值；连接与传输错误不混入
核心 AutomationError。

## 7. Native JSON-RPC v1

### 7.1 调用模型

Native v1 是面向已知、版本化脚本的直接协议：

1. 脚本使用配置端口或显式 `--control-port` 连接 `POST /automation/v1`。
2. 首次调用 `application.get_status`。
3. 校验 `host_mode=headless`、`toolset_version` 和控制层级，并取得
   `document_id/revision`。
4. 直接调用已知公共 operation。

Schema 由版本化文档或生成绑定提供。Native 不提供 discover/catalog；若后续出现真实的动态脚本
编排用例，再按实际需求设计窄查询。

### 7.2 请求与响应

- 使用 JSON-RPC 2.0，`method` 等于公共 operation ID；
- `params` 省略时等价于 `{}`，存在时必须为 object；
- `id` 只接受字符串或 JSON 安全整数，不接受缺失或 `null`；
- 每个 HTTP 请求只接受一个 JSON object；
- Batch Array 与 notification 均返回 `Invalid Request`；
- 未知 method 返回 `-32601 Method not found`；
- Schema 错误返回 `-32602 Invalid params`，并携带稳定结构化 details；
- 成功 `result` 直接使用公共 output Schema，不包 MCP `structuredContent`；
- 业务错误置于 `error.data`，保留 AutomationError 的 `code`、`message`、
  `operation_id`、`field_path` 和 `details`；
- `automation.discover` 不注册，调用结果必须是标准 `Method not found`；
- 异步 operation 返回既有 `task_id`，后续使用 `tasks.list/get/cancel`。

Native 不进入 MCP session、协议协商或取消扩展生命周期；URL 路径表示传输协议版本，
`application.get_status.toolset_version` 表示公共业务工具版本。

## 8. 共享 Automation HTTP Server

现有 MCP-only Controller/Server 泛化为进程级 Automation Controller/Server：

- 一个 `QHttpServer`；
- 一个 `127.0.0.1` listener；
- 一个端口；
- `/automation/v1` 与 `/mcp` 各自拥有 route dispatcher；
- 两条 route 共用一个 `PublicAutomationRegistry`。

共享边界包括 Host/Origin 校验、Content-Type/Accept 约束、请求体、JSON 深度/节点与响应体上限、
deadline、transport in-flight 限制、Registry admission、安全响应头和有序停止。MCP 保留既有
session、版本、header 与取消语义，Native 不模拟 MCP lifecycle。

| Host | MCP disabled | MCP enabled |
|---|---|---|
| GUI | 不监听 Automation HTTP | 只注册 `/mcp` |
| Headless | `/automation/v1` 保持监听 | 同时注册 `/automation/v1` 与 `/mcp` |

Headless `--no-mcp` 时，QLocal Bootstrap 仍按 Connector 语义发布：

```text
host_mode = headless
state = server_disabled
server_endpoint = empty
```

上述 `server_*` 字段只描述 Connector 所需的 MCP 服务，不描述 Native listener。Headless 的
Registry、访问根或端口绑定失败属于致命启动错误：写入 stderr，有序清理 listener、QLocal 和
单实例资源，并以非零状态退出；不得在无可用 Native 服务时继续空转。

## 9. 清除隐藏 GUI 依赖

实施必须处理以下已知边界：

- Settings adapter 不再无条件取得 ThemeManager；
- Headless 的主题设置仅读写配置，不创建主题系统，query 通过既有 availability 字段表达当前
  界面不可用；
- AudioContext 设备失败不调用 QMessageBox；
- AudioDecoding 不通过 singleton fallback 创建 DocumentWorkflow，不显示 Toast；
- 工程路径从 Runtime/DocumentSession 获取，GUI workflow/notifier 作为可选注入；
- Playback 在没有 `QGuiApplication` 或 screen 时使用稳定 60 Hz fallback；
- UiLanguageManager 使用 `QCoreApplication` translator API；
- Restarter 使用 `QCoreApplication` 静态 API；
- Headless 文件、推理和 Task 路径不反向创建 GUI Controller 或窗口。

无效 `QT_QPA_PLATFORM` 的真实 Headless 进程测试和 Windows 顶层窗口枚举共同作为隐藏 GUI
依赖的外部资格证据；源码类型关系与 deterministic composition 测试提供内部保护。

## 10. 实施顺序

1. 提交本计划、契约矩阵、测试大纲与测试执行计划。
2. 引入 HostMode 预解析，拆分 common/GUI Application 环境与 AppContext composition。
3. 将窗口身份从 Core/application lifecycle 中改为 optional，并建立 Headless 初始文档。
4. 校正 25/151 Host metadata、`value_sources` 和 Registry Host gate/错误优先级。
5. 泛化 Automation HTTP Controller/Server，实现 `/automation/v1` codec 与 dispatcher。
6. 保持 MCP route、QLocal Bootstrap、单实例和 Connector 语义，并完成错误名迁移。
7. 清除 Settings、Audio、Playback、translator、Restarter 与文件任务中的隐藏 GUI 依赖。
8. 增加 Headless 控制台终止桥，并使其复用 application Facade 生命周期。
9. 增加单元、组件和精简真实进程测试，完成 Native/MCP/Connector/Facade 等价回归。
10. 完成 GUI 回归、完整 Debug 构建和同一候选的一次串行全量 CTest。
11. 回填实现报告、测试报告、failure ledger 与匿名 Evidence 索引。

## 11. 实现与验收门禁

- Headless 进程的 Qt 根对象确为 `QCoreApplication`，且没有 GUI 对象或顶层窗口。
- Headless 初始状态为一个真实 document、零 windows，任何请求和结果均不伪造 WindowId。
- 权威公共 Contract 的 GUI-only 与 both 集合互斥且并集完整；Registry binding 与 Host gate
  满足同一集合关系。
- `/automation/v1` 在 Headless 固定可用，`/mcp` 可独立启停并与 Native 共存。
- Native 首次 `application.get_status` 即提供确定脚本所需状态，不存在 discover/catalog。
- Native、MCP、Connector 与直接 Facade 的代表语料在业务结果、错误、revision、History 和
  Task 上等价。
- GUI/Headless 共享单实例身份，位置参数、文档替换、Connector Bootstrap 和 GUI MCP 不回退。
- Headless 的 `Ctrl+C`/`Ctrl+Break` 与 POSIX `SIGINT`/`SIGTERM` 使用和显式 discard exit 相同的
  Facade 与清理顺序；GUI 不注册控制台终止处理器。
- Headless 启动致命错误非零退出，全部测试拥有的 listener、QLocal、Task、锁和进程可清理。
- Debug 全目标构建、正式进程资格、GUI 回归和同一最终候选的一次完整串行 CTest 完成。
- 六份正式文档与仓库外私有证据归档形成可追溯闭环。
