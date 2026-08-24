# 二期：MCP Server 与 DS Connector Lite 实施计划

## 1. 目标与决策来源

二期在一期 PR #161 已合入的 Automation Facade 基线上，交付运行中 GUI editor 的
MCP Server、公共 Wire Contract、三级 profile/Custom 权限基础、单实例发现与状态订阅、
以及面向 Agent Host 的 DS Connector Lite。Agent 会话可以先于 editor 启动；editor
稍后出现或在运行时开启 MCP 后，存活的 connector 能自动接入，无需重启 Agent 会话。

范围与语义以《DS Editor Lite MCP 与自动化体系设计》为准，并参考
[自动化体系五期建设路线图 #96](https://github.com/flutydeer/ds-editor-lite/issues/96)。两者冲突时
采用前者。公共工具的冻结分母见 [公共工具矩阵](public-tool-matrix.md)。

一期的 122 个 Catalog operation 是进程内能力，不等于 122 个 MCP 工具。二期公开
87 个 Meta/L1/L2 工具；它们可能直接绑定一期 handler、组合多个一期能力，或补充新的
细粒度 Facade。MCP Adapter 和 connector 均不得把内部提交/缓存写回 operation 直接公开。

## 2. 本期范围与延期范围

### 2.1 本期交付

- 将一期 `operations.list/get/cancel` 集中改名为 `tasks.list/get/cancel`，同步更新 ID、
  Catalog、Facade、测试和一期文档；不保留未公开旧名的兼容别名。
- 建立 editor 与 connector 共用的 Wire Contract、Schema 生成、Wire Binding Registry、
  Public Automation Manifest、版本/digest 和逐工具兼容判定。
- 补齐 87 个 Meta/L1/L2 公共工具所需的公开 DTO、细粒度领域命令、编排入口和能力查询。
- 建立 L1/L2/L3/Custom profile 基础、Custom 独立持久化、调用来源与客户端归因、
  Access Policy、File Guard 和 Admission Control。
- 在 GUI editor 内通过 `QHttpServer` 提供 MCP 2026-07-28 Streamable HTTP `/mcp`，
  仅绑定数值地址 `127.0.0.1`。
- 扩展现有全局单实例 `QLocalServer`，提供 `automation.discover` 和
  `automation.watch`，同时保持 `activate`、`openProjects` 兼容。
- 新增 DS Connector Lite：下游为 MCP stdio Server，上游为 editor Streamable HTTP
  MCP Client；实现固定工具面、exposure 裁剪、兼容交集、泛化转发、重连和状态报告。
- 增加 Automation 设置页、editor/connector CLI、运行时启停与换端口、多 connector、
  安全限制、测试和用户说明。

### 2.2 延期范围

- L3 仅建立 profile 枚举、descriptor 表达、持久化和 CLI 解析基础；本期 Public
  Automation Manifest、`tools/list`、类型化 connector 工具和泛化目标均不注册 L3 工具。
  因此本期选择 `l3` 时，实际公共业务集合与 L2 相同。
- 不实现 `--headless`、`QCoreApplication` Composition、Ready File、
  `POST /automation/v1` 或任何原生 JSON-RPC 行为。
- 不注册依赖未来多窗口/多文档 owner 能力的 `documents.list`、`windows.list`、
  `documents.close`，也不注册占位项。
- 不实现 MCP Resources、Prompts、Sampling、Elicitation 或长期 MCP 订阅；首版只提供
  Tools。profile/Manifest 变化由客户端轮询，connector 的 editor 状态变化由 QLocal watch
  获得。
- 不公开 `application.request_exit/restart`、`documents.commit_*`、`imports.commit_batch`、
  `inference.apply_*`、`audio_clips.apply_*`、`packages.resolve_document_voices`、
  `exports.audio.cleanup`、`editor.get_capabilities` 等内部 operation。

## 3. 目标结构与唯一调用路径

```text
GUI Controllers ───────────────────────────────┐
                                               ▼
MCP /mcp → MCP Adapter → Wire Binding Registry → Access Policy
                                               → File Guard
                                               → Admission Control
                                               → typed Domain Facade
                                               → AutomationDispatcher
                                               → Actions / History / Model / TaskManager

Agent Host → stdio → DS Connector Lite → Streamable HTTP → Editor /mcp
                         │
                         └─ QLocalSocket discover/watch → Instance Bootstrap
```

公共调用固定经过以下步骤：

1. transport/framing、MCP 元数据和消息形状校验；
2. 由 stable operation ID 查找 Wire Binding；
3. 严格输入 Schema 校验并转换为类型化 Wire DTO；
4. 按当前 profile/Custom、host availability 和 operation 状态做执行期权限检查；
5. 对路径字段执行 File Guard，对请求执行 Admission Control；
6. Wire Binding 调用对应领域 Facade；Facade 继续使用一期 Dispatcher、document/revision、
   History、幂等与 TaskManager 语义；
7. 将类型化结果转换为 Wire DTO，按输出 Schema 自检后编码为 MCP 结构化结果。

GUI 继续调用同一领域 Facade。MCP Adapter、Wire Binding 和 connector 不直接访问 Model、
Controller、History Action，也不创建第二套通用业务 Dispatcher。

## 4. 一期契约校正

### 4.1 Task 术语

`OperationId` 始终表示稳定能力定义；`TaskId` 表示一次异步执行实例。先完成以下原子整理：

```text
operations.list   → tasks.list
operations.get    → tasks.get
operations.cancel → tasks.cancel
```

同步更新 `OperationIds.h`、Catalog descriptor、`TaskAutomationFacade`、集中集合测试、行为测试、
测试场景名和一期文档。Catalog 总数保持 122，旧字符串在产品源码、测试预期和文档清单中均
不再出现。任务 DTO 的线协议字段固定为 `operation_id` 与 `task_id`。

### 4.2 公开审计

为 122 个内部 operation 建立三类显式审计结果：公开绑定、公共编排使用的内部步骤、仅
内部使用。审计集合与一期 Catalog 精确相等，且公开绑定集合与 87 工具矩阵精确相等。
内部 operation 不通过 `automation.get_options`、Manifest extensions 或 connector 泛化调用
泄漏。

## 5. Wire Contract、Binding 与 Manifest

### 5.1 单一 Wire Contract 来源

公共契约采用机器可读声明表或等价代码生成输入，editor 与 connector 共享同一生成产物。
每个绑定至少包含 stable operation ID、标题/说明、category、Query/Command、同步模式、
输入/输出 DTO、profile、document/revision/History/file/host/concurrency/conflict/safety policy、
`value_sources` 和版本元数据。

固定枚举、范围和步长只维护一份声明，并生成：

- C++ Wire enum、codec 与校验表；
- JSON Schema 2020-12 的 `$defs`、`enum`/`oneOf`、范围与必填字段；
- Public Automation Manifest 与 Schema digest；
- editor/connector 契约测试的有效和无效语料。

Wire 字段统一为 `snake_case`。输入和输出对象默认
`additionalProperties: false`；未知字段、未知枚举、非有限数字、越界整数和不合法 UUID
在进入 Facade 前拒绝。业务 Schema 不保留任意 `options`、`patch`、`scope` 对象或裸受控
字符串。仅 Manifest 根级版本化 `extensions` 和 `editor.tools.invoke.arguments` 为开放对象；
后者仍按目标真实输入 Schema 校验。

### 5.2 动态值与首次调用

- 固定值直接由目标 Schema 枚举，不经重复查询返回。
- 动态值由 `value_sources` 指向同 profile 或更低 profile、同 host 可用的查询。
- `automation.get_options` 只接受目标输入 Schema 的受控子集，并继承目标权限、Custom、
  host 和 connector exposure；不得借此探测被隐藏工具。
- `automation.get_status` 零参数返回 editor 实例、host mode、profile、Manifest 摘要、当前
  零或一个 document/window 的稳定 ID 与 revision，使首次业务调用无需预知 ID。
- 参数查询补齐稳定 `curve_id`、`anchor_id`；声音、格式、参数、导出、提取、推理和文件
  权限查询返回可直接用于命令的值。字体族不提供枚举查询。

### 5.3 Binding Registry

Binding 使用编译期类型或显式模板登记，不接受 `QString + QVariantMap` 式业务反射。
Registry 必须能由同一登记表派生：

- MCP `tools/list` 的确定顺序、input/output Schema 与 annotations；
- Public Automation Manifest；
- 输入解码、类型化 Facade 调用和输出编码；
- profile/Custom 发现过滤及执行期权限检查目标；
- connector 内置的已知类型化 wrapper 描述。

注册时校验 ID 唯一、Schema 可规范化、`value_sources` 可达、descriptor 字段完整、公开工具
不指向 InternalOnly handler。输出设置 `structuredContent`，并同时提供序列化 JSON 的
TextContent 兼容表示；输出 Schema 存在时，server 在返回前必须验证。

### 5.4 Public Automation Manifest 与版本

Manifest 根级包含 `toolset_version`、规范化内容的 SHA-256 digest、当前 profile/host、
operations、分页游标和预留 `extensions`；根级拒绝其他未知字段。每个 operation 至少包含
设计稿第 10.2 节列出的全部 descriptor 字段及输入/输出 Schema digest。

- `toolset_version` 从 1 开始，只增不减、不得复用；公共契约或公开策略变化时整体递增。
- 同一 Manifest 中所有工具的 `version == toolset_version`。
- `minimum_compatible_version` 默认 1，只能单调增加且不得大于当前 `version`；发生破坏性
  变化时提升到本次全局版本。
- `automation.get_manifest` 的最小响应包络和其
  `minimum_compatible_version == 1` 长期保持可解析。
- digest 用于缓存与漂移检测，不替代版本门槛；Schema 规范化必须稳定，不受对象键顺序、
  空白或生成机器影响。

类型化单工具兼容要求双方同名且同时满足：

```text
C >= editor.minimum_compatible_version
AND E >= connector.minimum_compatible_version
AND connector-input ⊆ editor-accepted-input
AND editor-output ⊆ connector-accepted-output
```

输入/输出 digest 相同可快速通过；不同时仅在受支持 JSON Schema 子集上做确定的方向性
证明，无法证明或遇到未知关键字即 `contract_incompatible`。只有一侧存在时为
`tool_unavailable`。整体有兼容交集但版本不同为 `compatible_subset`。

## 6. Profile、Custom 与公共工具面

Meta 工具是 MCP 启用后的最小握手/诊断面，不受业务 profile 或 Custom 开关影响；
`automation.get_options` 的目标与返回内容继承目标工具权限。L1 与 L2 是累积 preset。

持久化状态分离为：

```text
selectedProfile = l1 | l2 | l3 | custom
customPermissions[stableOperationId] = enabled | disabled
```

切换 L1/L2/L3 不复制、覆盖或重置 Custom；未选择 Custom 时忽略其值，切回时恢复上次
配置。Public Manifest 新增且无持久记录的 operation 在 Custom 中默认关闭。内部 operation
永不进入 Custom。CLI `--automation-profile custom` 使用持久化 Custom，但不回写。

同一 Policy 结果必须同时控制 editor `tools/list`、Manifest、`get_options`、connector 实际
目标缓存和执行期 Invocation Router。隐藏不是授权；每次调用在实际 dispatch 前重新检查。
Automation 自身的启用、端口、profile、Custom 和文件根目录不提供任何自动化写接口。

## 7. Editor MCP 2026-07-28 Transport

Editor 只提供：

```text
POST http://127.0.0.1:<port>/mcp
```

严格采用 MCP 2026-07-28：每个 JSON-RPC 消息使用独立 POST；请求体是单个 request 或
notification，不接受 Batch Array 或客户端 response。实现 per-request `_meta`，校验
`MCP-Protocol-Version`、`Mcp-Method`、适用时的 `Mcp-Name` 与 body 完全一致；不实现旧版
`initialize`/协议 session 兼容层，不签发或回显 `Mcp-Session-Id`，不提供 GET stream、DELETE
session 或 `Last-Event-ID` 恢复。GET/DELETE `/mcp` 返回 405。

首版处理强制发现入口 `server/discover`、`tools/list`、`tools/call` 和协议要求的最小错误
响应。`server/discover` 返回支持版本、tools capability、server identity 与缓存元数据；
`tools/list` 稳定排序、分页，
并返回严格的 input/output Schema。由于首版不建设 MCP 订阅，tools capability 不声明
`listChanged`；客户端通过轮询重新发现。

短请求默认返回单个 `application/json` 响应。若实现请求级 SSE，必须只承载该请求的进度
与最终响应，连接关闭即取消尚未提交的请求；已经返回 `TaskAccepted` 的后台任务使用
`tasks.cancel` 管理。旧版 standalone SSE 和跨请求推送均不实现。

## 8. Instance Bootstrap 与 QLocal discover/watch

从现有单实例实现提取 editor 与 connector 共用的稳定产品身份、应用数据目录和服务名计算。
connector 只能创建 `QLocalSocket` 客户端，不能尝试取得 `QLockFile`、创建同名单实例服务或
成为 Primary。`QLocalServer::UserAccessOption`、长度前缀 JSON、协议版本、请求 ID、最大帧、
Primary PID 和超时继续复用。

在兼容 `activate`、`openProjects` 的前提下增加：

- `automation.discover`：返回一次完整状态快照后关闭；旧 editor 不识别时报告能力缺失。
- `automation.watch`：立即返回完整快照并保持长连接；每次状态变化广播新的完整快照。
- 多帧解析、部分读写、写缓冲、每 watcher 背压上限、watcher 数量限制和异常断开清理。
- `starting → mcp_disabled/mcp_starting/mcp_ready/mcp_stopping/editor_stopping/error`
  状态机；只有 endpoint 已真实接受请求后才广播 `mcp_ready`。
- 每进程新的 `editorInstanceId`、规范化可执行路径、版本/build ID、host mode、PID 和 endpoint。

watch socket 断开后 connector 指数退避并带抖动重连；定期使用 discover 作为兼容回退。
QLocal 只承载实例和 MCP 状态，不承载 Manifest、Schema 或业务调用。

## 9. DS Connector Lite

DS Connector Lite 是独立可执行组件，不做单实例限制。它不查找/启动/退出/restart editor，
不保存预期 editor 路径，不连接非 discover 得到的本机 endpoint，也不充当任意 URL 代理。

### 9.1 MCP 到 MCP 的语义转接

下游 stdio Server 与上游 Streamable HTTP Client 是两条独立的 MCP 2026-07-28 连接语义。
connector 为每个请求重分配上游 ID并维护映射，分别校验两侧协议元数据，映射结构化结果、
错误、请求级取消、超时和断线。它不逐字节透传，也不复制 Facade 规则。

stdout 只能输出 MCP stdio 帧，启动诊断和日志全部写 stderr 或独立文件。editor 未运行、
starting、MCP disabled/starting 或上游未连接时，downstream 仍可初始化并保持固定工具集，
业务调用分别返回稳定 connector 状态错误。

### 9.2 固定工具面与 exposure

connector 始终发布六个桥接元工具：`connector.get_status`、`connector.reconnect`、
`editor.tools.list/search/describe/invoke`。它还内置构建时已知的 87 个公共工具描述，并在
进程启动时按 CLI exposure 裁剪；裁剪后的类型化 downstream `tools/list` 在该进程生命周期
不增删或改写。editor profile、Manifest 或连接状态变化只更新可用性/兼容缓存。

```text
--exposure-profile l0|l1|l2|l3     # 默认 l1
--include-tool <selector>           # 可重复
--exclude-tool <selector>           # 可重复
```

selector 仅接受 `id:name`、`category:name`、`prefix:text`，裸名按 `id:`。语法错误令进程
启动失败；语法正确但当前无匹配者保留为 pending。最终集合为
`(preset ∪ includeMatches) - excludeMatches`，exclude 优先。L0 不是 editor profile，默认
不公开业务目标，但保留六个桥接元工具。

同一 exposure 结果同时过滤已知类型化工具，以及 list/search/describe/invoke 可观察、可调用
的实际 editor 目标；被过滤统一返回 `connector_tool_filtered`。include 可扩大 connector
说明面，但不能越过 editor profile/Custom、host/file policy 或执行期 Access Policy。

### 9.3 Manifest 兼容与重连

连接到 editor 后取得当前 Manifest，逐工具计算版本与 Schema 兼容，分别报告
`compatible`、`contract_incompatible`、`tool_unavailable`、`profile_blocked`、
`host_unavailable`。不兼容已知 wrapper 不调用；Agent 仍可在 exposure 和 editor 权限允许时，
通过真实 Schema 的泛化 describe/invoke 使用新工具。

editorInstanceId 改变时清空 Manifest、Schema、document/window/task 句柄缓存，重新发现和
兼容握手，不自动重放响应前断线的有副作用 Command。无法确认结果时返回
`outcome_unknown`。多个 connector 独立 watch 和建立上游连接，任一退出不影响 editor 或
其他 connector。

## 10. Automation 设置页、CLI 与运行时启停

Automation 设置独立保存：MCP enabled、控制端口模式与具体端口、selected profile、Custom
权限、canonical 读/写根目录和必要的安全上限。安全默认值为 MCP 关闭、L1、随机端口模式和
空的额外文件根目录。随机模式仍保存并展示一个非零具体端口，可显式刷新；端口冲突不得连接
或复用其他实例。

Editor CLI：

```text
--mcp | --no-mcp
--control-port <random|1..65535>
--automation-profile l1|l2|l3|custom
```

CLI 优先于持久设置且不回写。冲突、缺值和非法 profile/端口在窗口启动前给出稳定诊断并
非零退出。设置页显示生效值、持久值和 CLI 覆盖来源；被覆盖项不可绕过 CLI。

运行时启用按 `mcp_starting → listen/register/ready → mcp_ready`；失败进入 `error` 且不发布
伪 endpoint。禁用按 `mcp_stopping → 停止新请求 → 短提交有限宽限 → 关闭 transport →
mcp_disabled`，已进入 TaskManager 的后台任务默认继续。启用中修改端口使用同一有序停启
流程，失败时保留可解释状态。所有状态变化向 watcher 广播。

设置页提供 L1/L2/L3/Custom 说明、Custom 分类/单项选择、读写根目录管理、当前 endpoint
与错误状态，并始终提供可复制的 stdio 和 Streamable HTTP 服务对象配置。InternalOnly、延期
和 host 不可用 operation 不出现在 Custom 列表。

## 11. 安全、文件与并发

### 11.1 HTTP 与本机边界

- 只绑定数值 IPv4 `127.0.0.1`，禁止 `0.0.0.0`、IPv6 wildcard、LAN 地址和名称解析绑定。
- 校验 Host 为实际本机 endpoint；Origin 缺失按非浏览器客户端处理，存在时只接受明确的
  本机 Origin allowlist，其他 Origin 返回 403；不发送宽松 CORS 头。
- 不使用 Bearer Token，并在设置与安全文档中声明同一用户环境本地进程可尝试连接的剩余
  风险。
- 限制 method、Content-Type、请求/响应体、JSON 深度/集合大小、分页、domain batch、超时、
  每客户端/全局并发和速率；超限返回 `too_many_requests` 或 `busy`，不得无限排队。
- 客户端 ID 只用于日志、归因、公平性和配额，不作为认证或任务 ACL。

### 11.2 File Guard

路径字段在进入文件系统后端前解析为 canonical absolute path，并按读/写目的分别检查允许
根目录或当前会话显式授权。处理 Windows 大小写、分隔符、`..`、相对路径、UNC、符号链接/
junction、非存在输出目标的最近存在父目录、保留名和根目录边界，禁止前缀字符串误判。
检查后的路径 DTO 传给 Facade，后端不得重新采用未经 Guard 的原始字符串。打开、导入、
音频重定位、包校验与导出分别声明 `file_access`。

`automation.get_file_access` 只读返回 canonical roots/session grants；MCP、connector 泛化调用
和 L2 工具不能修改 Guard 配置。

### 11.3 并发与公平

每个 MCP 请求分配逻辑客户端 ID；所有 Command 最终进入一期主线程 Dispatcher 串行提交。
文档写继续使用 `document_id + expected_revision`，connector 不自动刷新 revision 重放。
Admission Control 分别维护客户端在途数、全局在途数、每客户端令牌桶、全局后台任务数和
各并发域 busy 状态，并保证不同 connector 不被单一客户端长期饿死。

连接在 Command 响应前断开时不自动重放；有幂等键、TaskId 或可查询 revision 时由调用方
确认，否则返回 `outcome_unknown`。`tasks.list` 默认列出当前 document generation 的全部
任务，`created_by_client_id` 仅用于诊断，不建立伪 ACL。

## 12. 实施顺序与阶段提交

基线为 PR #161 合入后的 `main`，工作分支为
`codex/automation-phase2-mcp-server-connector`。复杂或可独立验证的工作包分别提交，每个
提交保持可构建并运行相应保护测试：

1. `refactor(automation): rename operation task controls`
2. `feat(automation): add public wire contract and schemas`
3. `feat(automation): add public bindings and l1 tools`
4. `feat(automation): add l2 workflows and file guard`
5. `feat(automation): add profiles custom policy and admission`
6. `feat(mcp): add editor streamable http server`
7. `feat(single-instance): add automation discovery and watch`
8. `feat(connector): add stdio bridge and exposure policy`
9. `feat(settings): add automation controls and runtime lifecycle`
10. `test(automation): cover phase two contracts and integration`
11. `docs(automation): report phase two implementation and tests`

实际提交可按依赖拆细或合并，但不得把 editor、connector、测试和文档全部压成一个无法审查
的提交。Editor 与 connector 可并行建设；共享 Wire Contract 和 Bootstrap identity 先冻结，
避免两侧手工复制。

## 13. 验收门禁与产物

- 一期 122 项 Catalog 在改名后集合完整，87 项公共矩阵、Binding Registry、Manifest 和
  connector 内置业务描述集合完全相等。
- 87 项工具均具有严格 input/output Schema、descriptor、profile、执行绑定与适用测试；
  InternalOnly/延期工具没有占位注册。
- Editor `/mcp`、DS Connector Lite stdio、QLocal watch、profile/Custom、File Guard、
  Admission Control、运行时启停与多 connector 测试通过。
- editor 直连和 connector 转接对相同工具得到等价业务结果、错误 code、History、revision
  与 Task 语义。
- Debug 完整应用、connector 和测试目标构建通过；全部 CTest 串行连续三轮无 flaky；GUI
  Automation 设置和真实 Agent 联调通过。
- 修复任何失败后，先跑最小复现、所属域、editor/connector 联调，再重新执行三轮全量。

规划与报告产物：

- [公共工具矩阵](public-tool-matrix.md)
- [全量测试大纲](test-outline.md)
- [测试执行计划](test-plan.md)
- `implementation-report.md`（实现完成后补充）
- `test-report.md`（最终测试完成后补充）
