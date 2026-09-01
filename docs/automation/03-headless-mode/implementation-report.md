# 三期真正 Headless 与原生 JSON-RPC 实现报告

## 1. 交付结论与最终口径

三期在一期 Automation Facade 与二期公共 Contract、Registry、Editor MCP、Connector 基础上，
完成同一产品进程的 GUI/Headless 双 Host composition。GUI 继续以 `QApplication`、一个真实文档和
一个真实窗口运行；Headless 改为 `QCoreApplication`、一个真实文档和零窗口，并固定提供
`POST /automation/v1`。

本期没有建立平行 Headless 业务 Runtime。Native JSON-RPC、Editor MCP 和 Connector 的实际调用
继续共用 PublicAutomationRegistry、Access Policy、File Guard、Admission、类型化 binding、
Automation Facade、History、revision 与 Task 生命周期。

测试源候选最终冻结于 `headless` 分支 commit `42a20353`；其后的变更仅回填正式报告，不改变受测
产品代码或测试。项目 Debug build preset 的默认目标保持不变，正式候选通过项目脚本显式指定
`-Target all` 完成全目标构建与串行 CTest，结果为 64/64、0 fail。最终构建、清单、全量执行与
审计证据分别为 `E-P3-FINAL-BUILD-004`、`E-P3-CTEST-LIST-002`、
`E-P3-CTEST-JSON-002`、`E-P3-CTEST-FULL-004`、`E-P3-REVIEW-001`、
`E-P3-REVIEW-002` 和 `E-P3-FINAL-AUDIT-004`。

## 2. 启动入口与 Host composition

### 2.1 Application 类型在创建前确定

启动入口通过 `StartupArguments::preparseHostMode()` 在创建任何 Qt Application 对象前读取原始
参数：

- `--headless` 只在 `--` 前改变 Host mode；
- `--` 后的同名文本继续作为位置参数；
- 预解析只决定创建 `QApplication` 还是 `QCoreApplication`；
- Application 创建后仍由既有完整参数解析器统一验证重复选项、MCP 开关、控制层级、端口和位置
  工程参数，不维护第二套 CLI 规则。

`AppEnvironment` 按 Host mode 拆分 common 与 GUI-only 初始化。应用身份、日志、配置与通用事件循环
初始化可在 QCore 下运行；高 DPI、样式、字体和其他 GUI 初始化只在 GUI Host 执行。翻译器和
Restarter 改用 `QCoreApplication` API。

### 2.2 Core 与 GuiContext

`AppContext` 现在始终装配同一套 Core composition，包括 AppStatus、AppOptions、AppModel、
ParamUtils、HistoryManager、PackageManager、Automation CoreRuntime、音频、Playback、文档、包解析、
推理和 Task 所需对象及 wiring。

窗口和视图对象收口到可选 `GuiContext`。仅 GUI Host 创建 MainWindow、ThemeManager 及 LevelMeter、
Clipboard、Track、Clip、EditorView、UndoRedo、GUI 提取、ProjectStatus、AppController 和
DocumentWorkflow 等 GUI controller。Headless 不通过 singleton fallback 反向取得这些对象。

Headless 在 Automation listener 对外可用前调用既有 Document Facade 默认 draft/commit 路径建立
空白文档；若启动参数包含位置工程，继续用同一 Headless open queue 等待其 Task 到达终态，再构造
Automation Controller、开放 admission 并发布 Bootstrap 状态。GUI 保持 MainWindow 建立后的原
初始化时序，避免改变视图接收初始模型信号的行为。

## 3. 文档、窗口与单实例身份

`SingleWindowContext` 改为持有 `std::optional<WindowId>`：

- GUI 创建并验证唯一真实 WindowId；
- Headless 显式持有 `std::nullopt`；
- 无窗口 Host 不创建全零、固定或由 DocumentId 推导的替代 WindowId；
- 文档 operation 只依赖 `document_id` 与适用的 `expected_revision`；
- GUI-only 路由在无窗口 Host 上返回 `host_capability_unavailable`。

`application.get_status` 因此可在 Headless 返回一个真实活动文档和 `windows: []`，不再使用默认
`runtime.windowId()` 回退。

GUI 与 Headless 继续使用同一产品单实例身份、Primary 锁和 QLocal 服务。Headless 外部工程请求由
`HeadlessOpenRequestQueue` 串行转交既有无交互文档打开 binding，保持单文档替换和 Task 终态顺序，
不创建 DocumentWorkflow 或第二套加载实现。启动请求在 listener ready 前先排空；运行期转发请求
继续异步串行执行。

## 4. 公共 Host capability 与错误边界

公共 Contract 的 Host metadata 已收口为当前候选差集：

```text
公共 Editor operation：176
GUI-only：workspace 2 + track_panel 7 + clip_editor 16 = 25
GUI/Headless both：151
```

数量只描述当前候选。长期门禁使用权威 Contract、Registry binding 与实际 Host/Policy 集合关系，
不维护第二份 176 项清单。`value_sources` 的 Host metadata 与同一公共声明同步。

Registry 的调用顺序调整为：

```text
known operation
→ host capability
→ control level / custom permission
→ input schema
→ file guard
→ admission
→ typed handler
```

这保证 Headless 调用已知 GUI-only operation 时，在读取或校验 `window_id` Schema 前返回 Host
capability 错误。当前对外错误边界为：

| 错误 | 含义 |
|---|---|
| `tool_unavailable` | operation 不存在、未注册或上游没有该工具 |
| `host_capability_unavailable` | operation 存在，但当前 Host 缺少执行能力 |
| `permission_denied` | Host 支持，但当前控制策略禁止 |
| `editor_not_running` | 当前没有可发现的 Editor 实例 |
| `editor_not_connected` | 已知 Editor，但传输链路尚未连接 |

Connector 的旧 `host_unavailable` 输出已迁移到 `host_capability_unavailable`，不保留别名；实例与
传输状态仍分别使用 `editor_not_running`、`editor_not_connected`，不与业务 Host capability 混用。

## 5. 无窗口应用生命周期

公共退出和重启从 GUI context 中分离为 `ApplicationCommandContext`、
`ApplicationMutationResult` 与统一 application command dispatch：

- GUI 用户路径仍使用交互式 Prompt 策略；
- Native/MCP/内部自动化的 clean 文档默认接受；
- dirty 文档在未指定 `discard_changes: true` 时返回 `busy`，字段为 `discard_changes`；
- 自动化路径不显示保存询问、Toast 或其他决策 UI；
- 接受响应先返回 `accepted`、`action` 和实际 `discard_changes`，再由事件循环退出；
- restart 只复用当前可执行文件、原参数与工作目录。

生命周期调用来源增加 `PublicJsonRpc`，因而 Native 与 MCP 使用完全相同的 dirty/accept 语义和
错误编码。

## 6. Native JSON-RPC v1

`NativeJsonRpcDispatcher` 将 JSON-RPC `method` 直接映射为公共 operation ID，并以
`InvocationSource::PublicJsonRpc` 调用同一 Registry。成功结果直接返回公共 output object，不包
MCP `structuredContent`。

当前协议固定实现：

- 路径为 `/automation/v1`，每个 HTTP 请求只承载一个 JSON-RPC object；
- `id` 只接受字符串或 JSON 安全整数，且必须存在；
- `params` 可省略，存在时必须是 object；
- malformed JSON、Invalid Request、未知 method、Invalid params 与 Internal error 使用标准
  JSON-RPC 错误码；
- 业务失败置于 `error.data`，保留 AutomationError 的 `code`、`message`、`operation_id`、
  `field_path` 与 `details`；
- 异步 operation 返回既有 Task 结果，并继续通过 `tasks.list/get/cancel` 管理；
- Batch、notification、push、订阅和传输取消均未实现；
- `automation.discover` 不注册，调用它得到 `-32601 Method not found`。

确定脚本首先调用 `application.get_status` 取得 Host、工具版本、控制层级、DocumentId 与 revision，
随后按版本化 Contract 直接调用已知 operation。本期没有增加 Native Catalog 或运行时发现机制。

## 7. 共享 Automation HTTP listener

现有 HTTP Server 被泛化为进程级共享 listener：同一个 `QHttpServer`、`127.0.0.1` 监听地址、端口和
PublicAutomationRegistry，根据 Host/configuration 独立启用 `/automation/v1` 与 `/mcp` route。

| Host | MCP disabled | MCP enabled |
|---|---|---|
| GUI | 不监听 Automation HTTP | 仅 `/mcp` |
| Headless | 仅 `/automation/v1` | `/automation/v1` 与 `/mcp` 共存 |

两条 route 共用 Host/Origin、method、Content-Type/Accept、body 与 JSON 资源上限、响应上限、deadline、
全局 transport in-flight、Registry admission、安全响应头和有序停止。MCP 原有 session、协议版本、
header、notification/cancel 与结果塑形保持独立；Native 不进入 MCP session 生命周期。

Headless 初次启动若 Registry、访问根、端口或 listener 不可用，入口在对外服务建立前清理 Server、
QLocal 和单实例资源并非零退出；运行期致命重配错误同样写入 stderr 后结束事件循环，不允许无 Native
服务空转。

## 8. Bootstrap 与 Connector 语义

QLocal Bootstrap 的 `server_*` 字段继续只描述 Connector 所需的 MCP route：

- Headless `--no-mcp` 发布 `host_mode=headless`、`state=server_disabled`、空
  `server_endpoint`，同时 Native listener 保持可用；
- Headless `--mcp` 仅在 `/mcp` 可接受请求后发布 `server_ready` 与 MCP endpoint；
- Connector 不把 Native route 当成 MCP endpoint，也不披露 Native 协议细节；
- Connector 固定桥接工具和二期 wrapper 契约保持不变，实际目录按当前 Headless Host/Policy 过滤。

## 9. 隐藏 GUI 依赖清理

本期在唯一既有实现路径上完成以下收口：

- Settings adapter 对 ThemeManager 使用可选注入；Headless 主题设置只处理配置，并以既有
  availability 字段表达当前界面不可用；
- AudioContext 在 QCore Host 的设备错误路径只记录日志且不创建 QMessageBox，GUI Host 保留既有失败
  对话框；
- AudioDecoding 从 Runtime/DocumentSession 获取工程路径，GUI DocumentWorkflow/notifier 为可选
  注入，不再用 singleton fallback 或 Toast；
- Playback 在不存在 `QGuiApplication::screen` 时使用稳定 60 Hz fallback；
- UiLanguageManager 与 Restarter 改用 QCore API；
- 文档打开、音频导入/路径更新、导出、包刷新、提取与推理继续复用二期 File Guard、不可变快照、
  Task 和提交门，不增加 Headless 专用 DTO 或文件后端。

真实 Headless 进程以故意无效的 Qt GUI platform 配置仍能启动，并由系统级窗口枚举确认零顶层
窗口；该进程资格随最终候选串行 CTest 一并通过，结果见测试报告。

## 10. 测试与长期保护

新增或扩展的保护覆盖：

- HostMode/StartupArguments、`--` 与端口/MCP/control-level 参数；
- Contract 的 GUI-only/both 集合、`value_sources`、Registry binding 与 Host gate 顺序；
- Native JSON-RPC envelope、错误映射、HTTP 限制与 Native/MCP 混合 admission；
- optional WindowContext、Headless status 与 application lifecycle；
- QLocal、Connector 错误迁移和 GUI/Headless 单实例身份；
- 真实 Headless 进程的无效 GUI platform、零窗口、Native、可选 MCP、文件/Task、端口冲突、
  启动工程 ready 顺序、clean/dirty/discard/restart 与资源清理；
- GUI MCP 目录、真实 WindowId、代表 GUI-only operation、编辑、History、播放和保存回归。

测试控制优先使用确定性 CTest、命令行进程、HTTP/QLocal/窗口枚举、Editor MCP 与 Connector MCP。
Computer Use 只在其他方式无法覆盖必要 GUI 断言时使用；本期最终执行次数为 0。

最终标准 Debug 构建通过项目脚本显式指定 `all`，退出码为 0，并保持 `CMakePresets.json` 与基线
一致。CTest 文本清单与 JSON 清单均为 64 项且测试可执行文件完整；最终候选串行执行 64/64、
0 fail，耗时 60.39 秒，退出码为 0。测试仅使用自动生成的 WAV/DSPX fixture，未访问或复制授权
素材，因此原文件 hash 不适用；最终资源清理和 Evidence 索引审计通过。

## 11. 明确未交付范围

三期没有实现多窗口、多文档、独立 Headless 可执行文件、CI/Skill、Native Batch/notification、
push/订阅/传输取消、Ready File、`--control-port 0`、`--automation-profile`、协议互斥开关、
`operations.*` 别名或 Native discover/catalog。后续只有在出现真实动态脚本编排需求时，才另行设计
窄查询能力。
