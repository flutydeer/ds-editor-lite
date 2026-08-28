# 二期：MCP Server 与 DS Connector Lite 实施计划

## 1. 目标与决策基线

二期在一期 Automation Facade 基线上交付运行中 GUI Editor 的公共 MCP Server、公共 Wire Contract、实例发现与状态观察、DS Connector Lite，以及 GUI 进阶控制、允许公开的应用设置与歌词规则、包索引、设置页、CLI、安全和运行时生命周期。

设计语义以《DS Editor Lite MCP 与自动化体系设计》当前版为权威来源，并参考[自动化体系五期建设路线图 #96](https://github.com/flutydeer/ds-editor-lite/issues/96)。公共工具分母由[公共工具矩阵](public-tool-matrix.md)冻结：Editor 175 项，Connector 6 项，总计 181 项。

本期产品形态为：

```text
Agent Host
  ⇅ MCP stdio
DS Connector Lite
  ⇅ MCP Streamable HTTP
运行中的 GUI Editor
  ⇅ typed Automation Facade / 历史记录 / Model / TaskManager
```

Agent 会话可先于 Editor 启动。Connector 通过全局实例 Bootstrap 观察 Editor 的状态；Editor 出现、启用 MCP、换端口或重启后，Connector 自动完成新的上游握手。

## 2. 域优先原则

公共能力先按业务域建模，再映射到 Profile、MCP descriptor 和 Connector exposure。Editor 的 24 个域依次为：应用、文档与工程、格式、轨道、总线、片段、音频素材、声库、Speaker Mix、音符/歌词/语言/发音/音素、参数曲线与锚点、时间轴、历史记录、播放、导出、提取、推理、异步任务、工作区布局、轨道面板、片段编辑器、设置、包信息、歌词规则。

领域契约遵循以下规则：

- 历史记录决定 Command 粒度；只有能够整体撤销和重做的变化才组成一个编辑工具，同类多对象操作可以在完整预检后作为一条历史记录批量提交。
- 轨道、片段、总线的 rename/gain/pan/mute/solo，以及已有音符的歌词、语言和长度，使用各自的原子工具，避免通用属性 patch。
- 创建深度受限：轨道创建只接收空轨道的标题与颜色，歌声片段创建只接收空片段的位置、长度与名称；音符作为叶节点可在创建时携带完整初始歌词、语言、发音和音素。
- duplicate、move、resize、split 和参数锚点操作使用稳定对象 ID。
- 工具调用依赖显式 document/object 参数，业务结果不依赖 UI selection 或 focus。
- 可能随工程规模增长的 L1 查询必须提供范围、分页或固定上限；参数采样允许确定性降采样并报告原始/返回规模，锚点身份和拓扑不得静默丢失。
- GUI Editor 的领域默认值由服务端按同源规则解析；`notes.insert` 不要求 voice context，description 说明对应 GUI 动作、默认值、历史记录、文件和任务语义。
- voice selection 以 `{package_id, package_version, singer_id}` 组成的 singer 稳定引用；相同 package/singer ID 的并存版本必须精确区分，L1/L2 调用方可直接从 `voices.list/describe` 完成发现和选择，不依赖 L3 `packages.*`。speaker 可省略或为 `null`；零 speaker 声库保持空 speaker，单 speaker 声库自动解析唯一 speaker，多 speaker 声库要求显式选择；查询结果以 `speaker: null` 表达空 speaker。
- 同步 Command 在模型信号、历史记录与 revision 提交完成后返回；异步 Command 在最终写回与信号完成后进入成功终态。
- 自动化路径使用显式策略与稳定错误，不触发模态对话框。
- GUI 进阶控制以工作区、轨道面板和片段编辑器归域；钢琴与参数子区域共享时间视口，选择与焦点归入所属面板域。
- 应用设置使用明确 allowlist 与按小标题聚合的稀疏更新；自动化/MCP 自身配置和未列入设置不能由 MCP 修改。
- 应用级设置与歌词规则不进入文档 revision/history；包刷新使用 application-scoped Task，不伪造文档身份。
- `validate_only` 只开放给保存到新路径、批量文件导入、设备或包搜索路径设置，以及歌词规则创建/更新等确有复杂预检价值的操作；其他命令仍在提交前完成内部校验，不额外暴露预演参数。
- `idempotency_key` 只开放给能够以稳定结果安全去重的对象创建、批量导入和提取任务；文档导入、推理启动及普通属性编辑、移动/缩放、删除、历史记录和播放状态写入使用 revision/state version 或 Task 状态处理冲突，不额外要求 Agent 管理幂等键。

总线域的 descriptor category 为 `bus`，公开操作 ID 保持 `master.*`。历史记录是独立域，固定包含状态查询、Undo 与 Redo。

已冻结的跨域边界包括：`audio_clips.relocate` 与 `audio_clips.confirm_path` 在当前实现中同步完成校验、解码/hash 和最终写回，直接返回 Mutation；`playback.set_loop`、`playback.set_loop_enabled` 与 `playback.clear_loop` 修改工程持久状态，各自形成一条历史记录并递增文档 revision，而 `play/pause/stop/seek` 只修改瞬时播放状态。

## 3. 一期契约复用

异步执行实例统一使用 Task 术语：

```text
operations.list   → tasks.list
operations.get    → tasks.get
operations.cancel → tasks.cancel
```

`OperationId` 表示稳定能力定义，`TaskId` 表示一次异步执行实例。内部能力由
`OperationIds::all()` 提供权威集合，Dispatcher 使用显式类型化路由，不维护
`OperationCatalog` 或 `OperationDescriptor` 注册表。公开字段固定为 `operation_id` 与
`task_id`。

Editor MCP 的业务执行继续复用一期类型化 Facade、Dispatcher、CommandCommitter、历史记录、document/revision、idempotency、generation 和 TaskManager。新增公共语义时优先扩展现有领域 Facade；Transport、Wire Binding 和 Connector 只负责协议、权限、转换与转接。

Dispatcher 的幂等处理是显式 opt-in：只有调用方实际提供已获工具支持的
`idempotency_key` 时才计算请求指纹并进入去重存储；不带 key 的调用不做哈希，也不创建幂等记录。

## 4. 公共调用路径

```text
Streamable HTTP / stdio
  → MCP framing、版本与元数据校验
  → 公共 operation descriptor 与 input Schema
  → Profile / Custom Access Policy
  → 必要的 File Guard 与 Admission Control
  → typed Wire Binding
  → typed Domain Facade / host adapter
  → Dispatcher / TaskManager
  → 历史记录 / revision / Model / file backend
  → output 编码
  → MCP structuredContent + TextContent
```

每层职责单一。Editor MCP 与 Connector 都从同一公共契约表派生工具描述；业务提交只发生在领域
Facade 与其既有运行时路径中。动态候选由 `value_sources` 指向同层级可达的领域查询；正常
invocation 不自动回查 provider，调用参数以目标 input Schema 和领域 handler 为准。

## 5. Wire Contract 与 Binding Registry

### 5.1 单一契约来源

`AutomationWire` 集中维护：

- stable MCP tool name、operation ID、域、标题与说明；
- Query/Command、同步模式和最低 Profile；
- JSON Schema 2020-12 input/output Schema；
- 公共 enum、值域、范围、集合上限与稳定 codec；
- `value_sources` 及其上下文字段；
- 执行所需的 Profile、host、file access 和标准 MCP annotations；
- 全局 `toolset_version` 与每工具 `minimum_toolset_version`；
- MCP 2025-11-25 与 2026-07-28 两套主协议，以及 2025-06-18 兼容握手的消息、结果塑形和 header codec；
- exposure selector 与分页游标。

Wire 字段使用 `snake_case`。业务 object 使用封闭 Schema；未知字段、未知枚举、非有限数字、越界整数、非法 ID、超限集合和非法分页游标在进入 Facade 前失败。表示默认值、自动选择、无过滤或分页首页的可选字段采用显式白名单：字符串空值与省略等义，具有“全部”语义的集合空值与省略等义；必填标识、路径、查询词和编辑值仍保持严格校验。动态候选由 `value_sources` 指向同层级可达的领域查询，并继承该查询自身的 Profile、Custom 和执行权限。

### 5.2 Binding Registry

Registry 从同一工具声明建立 175 个类型化 binding，并派生：

- Editor `tools/list` 的确定顺序和 descriptor；
- input 解码、执行 handler 与 output 编码；output Schema 由确定性契约测试验证；
- Profile/Custom 的发现过滤与执行期授权；
- Connector 构建时已知的类型化 Editor 工具描述；
- 测试中的 ID、域、Profile、类型、Schema 和版本期望。

注册门禁校验 MCP tool name 与 operation ID 唯一且一一对应、Schema 有效、value source 可达、
binding 集合完整，以及工具描述与执行入口一致。执行时不再次访问 value source；同一版本下
Editor、Connector 或文档 Schema 不一致均按缺陷处理。

## 6. 工具集版本与兼容

标准 MCP `tools/list` 是运行时工具目录与完整 Schema 的事实来源；
`application.get_status` 返回 Editor 的 `toolset_version`、当前 Profile、host mode 及文档/窗口摘要。
本期工具集维持 v1：`toolset_version = 1`，181 个工具各自只持有
`minimum_toolset_version = 1`。

Connector 对同名类型化工具只检查双方工具集版本门槛：

```text
connector toolset_version >= editor minimum_toolset_version
AND editor toolset_version >= connector minimum_toolset_version
```

兼容判断不计算 Schema 方向性子集、Schema digest 或 `compatible_subset`。逐工具实际 Schema 仍以标准
MCP `tools/list` 为事实来源，泛化调用按 Editor 当前 Schema 发送；同一工具集版本下的 Schema
差异不是可协商兼容状态，而是应由 MCP 输入校验和契约测试修复的缺陷。Profile、Custom 和 host
availability 继续独立报告，不与契约版本混淆。

分页对调用方仍表现为 opaque cursor；内部编码只把 `context + snapshot + offset` 序列化为
base64url，并在下一页校验上下文和快照。游标不携带密钥、不计算 HMAC，也不承担认证职责。

## 7. Profile、Custom 与执行期授权

配置模型分离为：

```text
selectedProfile = l1 | l2 | l3 | custom
customPermissions[stableOperationId] = enabled | disabled
```

`L3` 的显示名称和能力含义统一为“进阶控制”（Advanced Control）：在 L2 之上仅增加经明确纳入的部分 GUI 自动化操作和设置项更改，不表示或承诺完全控制。自动化/MCP 自身的配置与运行生命周期不属于可由 MCP 修改的对象。

L0 工具构成固有能力面：所有 preset 和 Custom 都始终包含，不能通过任何权限配置禁用，也不显示在 Custom 工具列表中。L1～L3 使用最低 Profile 的累积关系；Custom 只为非 L0 工具使用稳定 operation ID 的显式集合。切换 preset 保留 Custom 配置，切回 Custom 恢复先前选择；新增非 L0 operation 在无持久记录时采用安全默认。

同一 `AutomationAccessPolicy` 同时控制：

- Editor `tools/list`；
- Registry 执行期 dispatch；
- Connector 的实际目标状态。

发现过滤与执行授权是两次独立门禁。每次调用都在 handler 前重新检查当前策略，避免列表缓存成为授权凭据。

## 8. Editor Streamable HTTP MCP

Editor 在数值 loopback 地址提供：

```text
POST http://127.0.0.1:<port>/mcp
```

协议层实现两套主协议契约：

- `2025-11-25`；
- `2026-07-28`。

2025-11-25 使用 `initialize → notifications/initialized` 生命周期；同一兼容入口接受客户端请求 `2025-06-18`，协商后以该版本完成 legacy 生命周期和后续工具调用。2026-07-28 使用无状态 `server/discover` 和逐请求 `_meta`，并对照 `MCP-Protocol-Version`、`Mcp-Method`、适用时的 `Mcp-Name` 与 body。2026-07-28 请求不经过 `initialize`。

两套主协议以及 2025-06-18 兼容会话共同处理 `ping`、`tools/list` 与 `tools/call`。结果按协商版本塑形：2026-07-28 响应携带现代结果与 server metadata；2025-11-25 和 2025-06-18 响应使用 legacy 兼容结构。每个 POST 承载一个 JSON-RPC request 或 notification，notification 接受后返回 HTTP 202。

HTTP 层实施：

- 仅监听 `127.0.0.1`；
- Host 与 Origin allowlist；
- POST、Content-Type 与 Accept 校验；
- 请求/响应字节、JSON 深度/节点和 deadline 上限；
- 全局最多 32 个在途请求；超限立即拒绝，不排队；
- 安全响应头与稳定 transport error；
- 有序停止、在途请求完成或超时、配额可靠释放。

## 9. QLocal 实例发现与状态观察

Editor 与 Connector 共用稳定产品身份、应用级服务名和长度前缀 JSON framing。在既有单实例命令基础上增加：

- `automation.discover`：返回当前完整快照；
- `automation.watch`：立即发送完整快照，并在状态变化时广播新快照。

快照表达 editor instance ID、应用版本/build、host mode、PID、MCP enabled、运行状态、endpoint 和错误。状态机覆盖启动、禁用、启动监听、ready、停止监听、Editor 退出和错误；`mcp_ready` 只在 HTTP endpoint 已接受请求时发布。

Watcher 具备最大连接数、最大帧、待写帧数、累计待写字节、读写 timeout、部分帧解析和异常断开清理。Connector 仅作为 `QLocalSocket` 客户端观察状态，使用 epoch/instance ID 丢弃旧 Editor 的晚到结果，并用有界退避与抖动重连。

## 10. DS Connector Lite

### 10.1 双向 MCP 转接

Connector 是独立多实例进程：下游为 MCP stdio Server，上游为 Editor Streamable HTTP Client。两侧分别维护 MCP 生命周期、request ID、取消、超时和结果验证。

上游优先按 2026-07-28 发起 `server/discover`，失败后使用 2025-11-25
`initialize/initialized`，并接受服务端协商到 2025-06-18 的 legacy 会话。协议握手成功后完整分页
读取 `tools/list`，再调用一次 `application.get_status` 取得 toolset version、Profile 与 host 摘要；
逐工具 Schema 和可用性以 `tools/list` 为事实来源，兼容性只计算全局版本与逐工具最低版本。
Editor instance 或 endpoint 变化会切换 handshake epoch、取消旧请求、清除旧缓存并建立新连接。

stdout 只写 MCP stdio 帧；诊断写 stderr。Reader 和 writer 都使用有界队列，覆盖部分读写、合并唤醒、EOF、broken pipe、backpressure 和停滞 deadline。

### 10.2 固定桥接面与 exposure

Connector 固定发布六个桥接工具：

```text
connector.get_status
connector.reconnect
editor.tools.list
editor.tools.search
editor.tools.describe
editor.tools.invoke
```

Connector 同时携带构建时已知的 175 个 Editor 类型化工具描述。进程启动时根据 exposure 生成固定 downstream 类型化工具集合：

```text
--exposure-profile l0|l1|l2|l3
--include-tool <selector>
--exclude-tool <selector>
```

Selector 支持 `id:`、`category:`、`prefix:`；L0 固有工具始终进入最终集合，不能被
`--exclude-tool` 去除；其余工具由 preset 与 include 的并集再应用 exclude。相同 exposure 结果同时过滤类型化 wrapper 和泛化
`list/search/describe/invoke`。Editor 上下线、Profile、工具目录和版本兼容变化更新状态缓存，
downstream descriptor 在该 Connector 生命周期内保持稳定。

### 10.3 状态、错误与多 Connector

`connector.get_status` 返回 Connector identity、Bootstrap、Editor、上游协议、工具集版本兼容、
exposure 与 pending selector 事实。稳定错误区分 Editor 状态、上游连接、工具过滤、工具可用性、
契约版本、timeout、取消和结果未知。

每个 Connector 拥有独立 QLocal watch、HTTP client、握手 epoch、工具目录缓存和 downstream
请求表。Connector 并发转发下游请求，不为 Editor 的 32 路上限增加串行队列。任一 Connector
的退出、慢读、触发全局准入上限或重连不会修改其他 Connector 的状态。

## 11. File Guard 与 Admission Control

### 11.1 File Guard

读根、写根、会话读授权和会话写授权分别维护。路径在调用文件后端前规范化为 canonical absolute path，并按访问目的处理：

- 根目录边界、相邻前缀与大小写；
- `..`、相对路径、drive-relative 与 UNC；
- symlink、junction 与 reparse point；
- 输出目标尚未存在时的最近存在父目录；
- authorize 后、实际 I/O 前的 reauthorize。

Editor 直连、Connector 类型化工具和泛化 invoke 进入同一个 Guard。`application.get_file_access` 返回当前授权事实。

### 11.2 Admission

业务 Admission 只维护全局 32 个在途请求与 8 个后台 Task 容量；HTTP Transport 只执行相同的
全局 32 路硬上限。不设置 client/peer/domain 配额、令牌桶或公平排队。超限请求立即得到稳定
`busy` 或 `too_many_requests`，不进入业务 handler；请求和 Task 终结时释放计数。

Command 使用显式 `document_id + expected_revision`；异步任务保留不可变执行快照，并在最终写回前复核 document generation、revision 和文件授权。断线时 Connector 不自动重放有副作用 Command，结果事实无法确认时返回 `outcome_unknown`，由调用方结合 revision、Task 和 idempotency 信息确认。

## 12. 设置页、CLI 与运行时生命周期

Automation 持久设置包含 MCP enabled、具体控制端口、selected Profile、Custom 权限、canonical 读写根。安全默认是 MCP 关闭、L1、持久化的非零本机端口和空额外文件根；端口只在配置首次建立时随机生成，随后保持不变，除非用户点击刷新或直接编辑。

Editor CLI：

```text
--mcp | --no-mcp
--control-port <1..65535>
--automation-profile l1|l2|l3|custom
```

CLI override 只影响本次运行，优先于持久设置。选项菜单中的 Automation 入口具有现有菜单体系一致的图标，面板文字完整本地化。设置页同时展示持久值、生效值、覆盖来源、运行状态、endpoint 与错误，并提供 Profile、Custom、读写根和端口管理。Custom 工具按公共契约中的领域分别成组，且不显示固有的 L0 工具；组默认收起，可独立展开，并在标题中显示启用数/总数。组级开关可一次开启或关闭整组，单工具开关仍保持独立持久化，且折叠状态不改变权限。端口下拉模式不存在，刷新按钮与端口输入框始终可用并位于同一行。

设置页提供可随时复制的 Connector stdio 配置和 Editor Streamable HTTP 配置。复制内容是单个 server entry，不包含外层 `mcpServers`；即使 MCP 尚未 ready 也能根据持久配置生成稳定内容。读写根的帮助文字明确说明它们只是自动化文件工具的路径 allowlist，不表示本机进程权限；面板不展示无动态事实的“本机进程访问”栏目。

运行时配置变化由 `EditorMcpController` 串行应用：先关闭 admission，再有序停止旧 listener，校验新配置，启动新 listener，最后发布 ready。禁用、换端口、端口冲突、根目录错误和退出都形成可观察状态；已接受的后台 Task 继续由 TaskManager 管理。

## 13. 实施顺序与阶段提交

1. 校正一期 Task 名称与受影响测试。
2. 冻结 175 + 6 工具矩阵、公共 enum、Schema 与版本不变量。
3. 完成 Wire Contract、版本兼容与透明分页游标。
4. 按 24 个域完成 175 个 Registry binding 和 host adapter。
5. 完成 Profile/Custom、File Guard 与 Admission。
6. 完成 Editor 2025-11-25 与 2026-07-28 两套主协议，以及 2025-06-18 兼容握手生命周期。
7. 完成 QLocal discover/watch 与状态机。
8. 完成 Connector 上游、下游、桥接工具、exposure 与兼容缓存。
9. 完成设置页、CLI、运行时启停和多 Connector 收口。
10. 完成静态、单元、组件、进程、GUI 与一次完整 CTest 回归。
11. 更新实现报告和测试报告。

复杂或独立职责使用阶段提交，示例：

```text
refactor(automation): align task operation names
feat(automation): define domain-first public wire contracts
feat(automation): bind editor public tool domains
feat(mcp): support editor protocol negotiation
feat(single-instance): publish automation discovery state
feat(connector): bridge editor tools over stdio
feat(settings): manage automation runtime configuration
test(automation): cover phase two contracts and integration
docs(automation): report phase two delivery
```

## 14. 验收门禁与正式产物

- 175 个 Editor ID、6 个 Connector ID、181 个总 ID 唯一且集合相等。
- 24 个 Editor 域及总线、历史记录、GUI 子区域归属与权威矩阵一致。
- `toolset_version = 1`，且每工具 `minimum_toolset_version = 1`。
- 175 个 Editor 工具均具备严格 Schema、descriptor、binding 与适用测试。
- Editor MCP 2025-11-25 与 2026-07-28 两套主协议、2025-06-18 兼容握手、QLocal watch、Connector stdio/exposure/compatibility、Profile/Custom、File Guard、Admission、设置与 CLI 完成验证。
- Editor 直连与 Connector 转接保持业务结果、稳定错误、历史记录、revision 和 Task 语义等价。
- 多 Connector、运行时换端口/启停、全局准入、退出和资源清理满足有界生命周期。
- Debug 全目标构建与一次完整 CTest 在同一候选上完成，GUI 与真实进程联调形成新证据。

正式文档：

- [公共工具矩阵](public-tool-matrix.md)
- [全量测试大纲](test-outline.md)
- [测试执行计划](test-plan.md)
- [实现报告](implementation-report.md)
- [测试报告](test-report.md)
