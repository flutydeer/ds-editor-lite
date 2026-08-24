# 二期 MCP Server 与 DS Connector Lite 全量测试大纲

## 1. 目的与边界

本大纲覆盖二期从公共 Wire Contract 到真实 GUI editor/connector 联调的完整验证。业务
Facade 的一期 122-operation 回归继续保留；二期新增的唯一公共工具分母是
[公共工具矩阵](public-tool-matrix.md) 的 87 项，另有 connector 六个固定桥接工具。

测试必须区分四条路径：

1. 类型化领域 Facade；
2. editor Streamable HTTP MCP；
3. connector 上游 MCP Client；
4. connector 下游 stdio MCP Server。

同一业务语料在适用路径上验证成功结果、稳定错误、History、revision、Task 和文件语义
等价。Headless、原生 JSON-RPC、L3 业务工具和未来多窗口条件工具不进入本期正向分母；
它们只参加“不注册、不泄漏、不误路由”的负向边界测试。

## 2. 覆盖分母与追踪

每个 `P2-TOOL-001～087` 必须至少关联：

- 一条 descriptor/Schema/Manifest 契约场景；
- 一条当前 profile/Custom 发现与执行权限场景；
- 一条 editor MCP 有效调用或明确的能力不可用场景；
- 一条 connector 类型化调用场景；
- 该 Query/Command/async/file/`value_sources` 类型适用的全部基础维度。

六个 connector 固定工具使用 `P2-CONN-001～006`。QLocal、transport、安全、兼容、运行时
启停和 GUI 使用独立场景号。预计确定性场景量为：

```text
87 × 6～10 + 120～200 个协议、安全、兼容、连接与跨域场景
= 642～1,070 个场景
```

这是规划量，不是通过率目标。多个断言验证同一输入路径时只计一个场景；不得用重复断言
或参数化展开夸大数量。最终报告必须能从工具、场景、测试目标和证据双向追踪。

## 3. 测试层次

| 层 | 验证对象 | 主要方法 |
|---|---|---|
| 静态/生成 | Wire Contract、枚举、Schema、ID、公共/内部集合 | 生成快照、集合相等、编译期和源码守卫 |
| 单元 | codec、validator、Manifest、Policy、Guard、Admission、exposure、兼容算法 | Qt Test + fake clock/filesystem/host |
| 组件 | MCP parser/router、QHttpServer、QLocal bootstrap、stdio framing | loopback、内存/临时 pipe、受控 socket |
| 进程 | editor、connector、全局单实例、重连、多 connector | 真实子进程、动态端口、串行执行 |
| 业务契约 | 87 项工具与一期 Facade 等价性 | 共用语料、fixture 工程、输出归一化 |
| GUI | Automation 设置、运行时启停、CLI 覆盖、可见错误 | Computer Use + 截图/日志 |
| 资格 | 真实 DSPX/MIDI/audio/voice/inference/Agent Host | 隔离文件根、真实模块、端到端联调 |

所有会取得全局 editor Primary 的测试必须 `RUN_SERIAL` 并使用同一个 CTest resource lock。
测试前确认没有用户 editor，测试后等待全局锁和 QLocal 服务释放；不得并行启动 GUI 与测试
editor，也不得为了测试自动结束来源不明的现有 editor。

## 4. 一期契约校正与回归

### 4.1 `operations.* → tasks.*`

- 集中 ID、Catalog descriptor、Facade 路由和测试名只存在 `tasks.list/get/cancel`；
- Catalog 仍为 122 个唯一 operation，除三项改名外 descriptor 语义不变；
- 产品源码、Wire Contract、Manifest、connector 及一期文档不存在旧名；
- 不接受旧名称调用，不注册兼容别名；
- `operation_id` 与 `task_id` 字段不混用，Task 快照保留实际创建 operation；
- 一期 Task 状态、取消、generation、并发和幂等回归全部通过。

### 4.2 公共暴露审计

- 122 项内部 Catalog 精确分为 public binding、公共编排内部步骤、纯内部三类；
- Public Binding、Manifest、editor `tools/list`、connector 内置业务描述均为 87 个唯一 ID；
- 内部 commit/apply/cache/cleanup、退出/重启和 `editor.get_capabilities` 不可通过任一路径调用；
- 新增参数细粒度 Facade 只能通过统一 CommandCommitter 修改，不建立平行业务实现；
- GUI 一期 Controller/Facade、History、revision 与真实功能回归不退化。

## 5. Wire Contract、Schema 与 Manifest

### 5.1 单一来源与封闭值

- 每个公共封闭枚举由同一声明生成 C++ wire type、codec、JSON Schema 和有效值语料；
- 在测试 fixture 中增加内部公共枚举值后，生成物自动包含该值，Schema/digest/版本门禁按规则
  变化；缺一侧生成步骤时测试失败；
- UI 语言、主题、量化、编辑模式、插值、任务状态、包/格式、推理阶段、merge/failure policy
  等不由第二份字符串表维护；
- 所有业务 object 默认 `additionalProperties: false`，无参工具只接受空 object；
- 未知属性、错误大小写枚举、NaN/Inf、整数溢出、非法 UUID、数组/字符串上限和 JSON 深度
  被 schema/codec 层拒绝且不进入 Facade；
- 公共业务 Schema 不含无约束 `options/patch/scope` 或裸受控字符串；只有 Manifest
  `extensions` 和泛化 invoke arguments 符合开放对象例外。

### 5.2 `value_sources` 与稳定 ID

- 每个受控字段具有生成 enum/range、可达 `value_sources` 或此前查询返回的稳定 ID；
- source operation 的最低 profile 不高于消费者、相同 host 可用，依赖上下文被声明并校验；
- `automation.get_options` 仅接受目标输入 Schema 的合法子集；缺依赖、非法字段和类型错误稳定
  失败；固定 enum 不重复返回；
- L2/L3/Custom/connector exposure 未开放的目标不会从 options 泄漏名称、字段或候选值；
- `automation.get_status` 在零参数下返回首次调用需要的 editor/document/window 摘要；
- `parameters.get` 的 curve/anchor ID 在查询、局部编辑、undo/redo 和文档 generation 中遵守
  稳定与失效规则；
- voices/parameters/formats/export/extract/inference/file-access 的能力值能原样用于相应命令；
- 字体族不提供 list/options，显式字体字符串只在 validate/执行时报告可用性。

### 5.3 Manifest 与版本

- `toolset_version` 从 1 开始、无符号、严格递增且不复用；同一 Manifest 的全部工具
  `version` 等于全局版本；
- `minimum_compatible_version` 默认 1、单调不降、不大于当前版本；破坏性变更 fixture 正确提升；
- `automation.get_manifest` 最小包络在新旧 fixture 间均可解析，新增兼容元数据只进入
  `extensions`；根级未知字段被拒绝；
- Schema 和 descriptor 规范化不受对象键顺序、空白、locale、绝对构建路径或运行机器影响；
  digest 相同内容稳定、任一公共语义变化产生新 digest；
- pagination 的 cursor、limit、排序、末页、过期/伪造 cursor 和重复读取稳定；
- profile/Custom/host 过滤后的 operations 与 `tools/list` 一致；兼容状态和可用状态分开；
- `automation.get_manifest.minimum_compatible_version == 1` 的不变量被静态保护。

## 6. 87 项公共工具的通用维度

### 6.1 Query

对每个适用 Query 覆盖：

1. 空/最小状态与有数据状态的完整类型化结果；
2. 输入/输出 Schema、Wire codec、structuredContent 和 JSON TextContent 等价；
3. 旧/未知 DocumentId、对象 ID、模块/host/service 不可用；
4. Unicode、长文本、空集合、边界数值、稳定排序和分页；
5. Query 不修改 Model、History、revision、文件、Task、幂等或通知计数；
6. L1/L2/Custom 发现与执行结果一致，未授权时 handler 调用计数为零；
7. 直接 Facade、editor MCP 和 connector 类型化结果归一化后等价。

### 6.2 同步 Command

对每个适用同步 Command 覆盖：

1. 正常变更、affected/created object、warning 与响应 document version；
2. 合法 no-op：`changed=false`，History/revision/业务通知不变；
3. `validate_only` 完整验证、无 ID/Task/文件/幂等/持久化副作用；
4. document → revision → object/type → domain → file/host 的稳定错误优先级；
5. batch/client_ref 去重、all-or-nothing、单 History、单 revision；
6. 支持幂等的操作覆盖重放、同键异参、并发去重；不支持者稳定拒绝；
7. handler/host/I/O 失败无半提交，输出编码失败不能造成第二次提交；
8. profile/Custom 在 `tools/list` 后、dispatch 前改变时，以执行期策略拒绝；
9. editor 直连和 connector 路径得到相同业务 code，不解析日志文本。

### 6.3 异步 Command 与 Task

- validate-only 不分配 TaskId；接受请求只启动一个后端任务并返回完整 `TaskAccepted`；
- Queued、Running、CancelRequested、Committing、Succeeded/Failed/Canceled 允许迁移；
- 排队/运行取消、重复取消、终态取消、提交点不可取消和断线后继续；
- document generation 替换、base revision 前进、目标删除和结果晚到不写入错误文档；
- `tasks.list/get/cancel` 改名后的分页、筛选、进度、结果、错误和稳定终态；
- `tasks.list` 默认可见当前 generation 全部任务，不按 connector 隔离；创建者仅诊断；
- connector 断开/MCP 禁用不终止已接受任务；重连可查询，不自动重放 start；
- response 前断线且结果不可知返回 `outcome_unknown`，可通过幂等键/revision/Task 查询确认。

### 6.4 文件工具

- 读/写根目录与 session grant 分离，`automation.get_file_access` 返回 canonical 事实；
- 允许根内部、根本身、兄弟前缀、`..`、大小写、混合分隔符、相对路径、UNC、drive-relative、
  空路径和超长路径；
- symlink/junction/reparse point 从允许根逃逸被拒绝；指向根内的合法链接按最终 canonical 路径
  判定；
- 不存在输出文件以最近存在父目录判定，创建前后重新验证；
- read-only、已存在/overwrite、父目录缺失、磁盘满、权限拒绝、临时文件和失败清理；
- domain batch 数量上限、批量音频 `atomic/best_effort` 与单次提交；
- 泛化 invoke、类型化 connector 和 editor 直连使用同一个 File Guard，不能传原始路径绕过。

## 7. Profile、Custom 与执行期授权

- 安全默认值为 MCP disabled、L1、无额外读写根；Meta 始终存在；
- L1 精确为 55 项，L2 精确为 87 项，`l3` 本期也精确为 87 项且无额外 L3 注册；
- L1 调用 L2、Custom 关闭项和未知 operation 均在进入 Facade 前稳定拒绝；
- 切换 L1/L2/L3 只改变 `selectedProfile`，不复制、覆盖或重置 Custom；
- Custom 单项/分类选择持久化，切出后忽略、切回恢复；新增工具无记录时默认关闭；
- InternalOnly 与延期工具不出现在 Custom UI 或持久化公开集合；
- `tools/list`、Manifest、get_options、connector 实际目标和执行期 Policy 使用同一判定；
- connector exposure 只缩减说明面，include L2 目标不能越过 editor L1/Custom；
- Automation enabled/profile/Custom/file roots 不存在 MCP、泛化或 connector 写入口。

## 8. MCP 2025-06-18 / 2025-11-25 / 2026-07-28 三版本与 HTTP

### 8.1 Transport 合规

- 仅 `POST /mcp`；GET/DELETE 返回 405，不存在 standalone SSE 或 session DELETE；
- 每 POST 只接受单个 JSON-RPC request/notification，Batch Array 和客户端 response 拒绝；
- notification 接受时返回 202 空 body；请求返回单 JSON 或请求级 SSE；
- 2026-07-28 的 `MCP-Protocol-Version` 与 body
  `_meta.io.modelcontextprotocol/protocolVersion` 必须相等；`_meta` 的
  protocolVersion/clientCapabilities 和 `Mcp-Method`、适用的 `Mcp-Name` 必填，clientInfo
  省略或合法提供均可；
- 2025-06-18 与 2025-11-25 分别完整执行 `initialize → notifications/initialized`；
  initialize 无协议头并精确回显请求或协商版本，后续带协商版本头，普通 request 不含 2026
  per-request 元数据或路由头；initialized notification 返回 202 空 body；
- 2026 initialize、2025 `server/discover`、缺失/不支持/不匹配版本均按对应协议稳定拒绝；
  header name 大小写不敏感，镜像值与 body 大小写敏感一致；
- header Base64 sentinel 编解码、Unicode/控制符/前后空格、伪造 header/body 差异和
  `HeaderMismatch -32020`；
- 三版本都不签发、不依赖、不回显 `Mcp-Session-Id`；session/Last-Event-ID 不产生隐藏状态；
- Content-Type/Accept、JSON 编码、重复 header、错误 method、未知 RPC、非法 id 和超大消息；
- 请求级 SSE 关闭会取消尚未提交的短请求且不再写响应；TaskAccepted 后由 `tasks.cancel` 管理。

### 8.2 Tools 语义

- 2026 `server/discover` 无需 initialize 即可返回三个支持版本、tools capability、serverInfo、
  `ttlMs` 与 `cacheScope`；未知版本按 `UnsupportedProtocolVersion -32022` 返回精确支持列表；
- 两个 2025 initialize 均返回协商版本、tools capability、serverInfo 与 instructions；2025 list/call
  不出现 `resultType`、`ttlMs`、`cacheScope` 或 2026 server metadata；非对象
  `structuredContent` 降级为 TextContent；
- tools capability、`tools/list` 稳定顺序/分页/cache 元数据和不声明首版 `listChanged`；
- 87 项工具的 name、title、description、inputSchema、outputSchema、annotations 与 Manifest
  descriptor 一致；
- `tools/call` 缺 arguments、未知 name、参数 schema 错误、业务错误与内部错误分层正确；
- 完成结果的 `resultType`、`content`、`structuredContent`、`isError` 与 outputSchema；
- MCP transport/JSON-RPC error 不混同业务 AutomationError；message 不泄露栈、路径或秘密值；
- 不声明 Resources、Prompts、Sampling、Elicitation 或 subscription 能力。

## 9. HTTP 安全与 Admission Control

### 9.1 本机绑定和 DNS rebinding

- 实际 listener 只有 `127.0.0.1`；枚举 socket/address 证明未绑定 wildcard、IPv6 或 LAN；
- 正确 Host、缺失 Host、错误端口、`localhost`/DNS alias、尾点、userinfo、多个 Host、CRLF
  和伪造 Host 的决策符合单一 allowlist；
- Origin 缺失可用于非浏览器客户端；允许的明确本机 Origin 通过，恶意域、`null`、file、
  混淆主机和 DNS rebinding Origin 返回 403；
- OPTIONS/CORS 不开放宽松跨域；远程接口和任意 URL proxy 不存在；
- 无 Bearer Token 的剩余风险在设置页/文档可见，但错误中不生成虚假认证语义。

### 9.2 资源限制与公平性

- 请求/响应体、JSON 深度、字符串/数组、tools/Manifest 分页、domain batch 和 watcher 帧上限；
- 每客户端在途数、全局在途数、每客户端速率、全局后台 Task 容量和各 concurrency scope；
- 超限立即返回 `too_many_requests` 或 `busy`，不无限排队、不进入 handler；
- fake clock 验证令牌恢复、窗口边界、超时释放配额和取消释放；
- 多 connector 压力下轮转公平，单一客户端不能长期占满 Dispatcher；
- 客户端 ID 仅归因/配额，不成为认证、profile 或 Task ACL；伪造 body 字段不能改写归因；
- shutdown/disable 与在途短命令、Committing、已接受 Task 的有界行为。

## 10. QLocal Instance Bootstrap

- editor 与 connector 共享同一产品身份/应用数据目录/服务名计算 golden test；
- connector 在 editor 不运行时只连接失败，不取得锁、不创建 server、不成为 Primary；
- 旧 `activate`、`openProjects` 的精确 v1 请求/响应和一次连接行为不回退；
- `automation.discover` 返回完整快照后关闭；旧 editor 返回能力缺失而非破坏旧协议；
- `automation.watch` 立即快照、长连接、多次完整 stateChanged、request ID 与 protocolVersion；
- 分片帧、合并帧、零/超大长度、非法 JSON/UTF-8、未知 command、读写 timeout；
- 多 watcher 广播、慢读者写队列上限、背压断开、数量上限和异常断开即时清理；
- starting/disabled/starting/ready/stopping/editor_stopping/error 每个状态与 endpoint 不变量；
- 监听失败不广播伪 ready；runtime 端口变化广播新 endpoint；
- editorInstanceId 每进程变化，PID 复用不会保留旧身份；路径/build ID/host mode 事实准确；
- 指数退避、抖动、discover fallback 和 editor 重启后重新 watch。

## 11. DS Connector Lite

### 11.1 stdio 与转接

- editor 离线时 connector 仍完成 downstream MCP 请求并发布固定工具集；
- stdout 字节流只包含合法 MCP stdio 帧；banner、Qt 日志、warning、崩溃诊断全部在 stderr/
  日志文件；高并发日志不污染 framing；
- 部分行、多消息、非法 JSON、超大帧、EOF、破管和 stdout backpressure；
- downstream/upstream 分别覆盖 2026 per-request 元数据与两个 2025 initialize 生命周期；上游
  优先 2026 并在明确不兼容时以 2025-11-25 发起 initialize，接受服务端协商为 2025-06-18，
  保留 session 并固定后续版本；两侧重新分配 request ID，并发乱序响应映射回正确 downstream ID；
- downstream stdio 取消映射为中止对应上游 HTTP 请求，不能取消其他请求；
- 连接/响应 timeout、editor stop、endpoint change 和 instance change 的稳定错误；
- connector 不逐字节透传、不修改业务结果、不重写 revision、不自动重放 Command；
- discover 出来的 endpoint 必须是 127.0.0.1 且与快照身份一致，任意 URL 输入路径不存在。

### 11.2 固定工具与离线状态

- 六个桥接工具始终存在，Schema 固定且 `connector.get_status` 真正无参数；
- l1 默认 downstream 业务工具为 55，l2/l3 本期为 87，l0 无 include 时为 0；
- downstream 类型化工具集合在进程生命周期内固定，editor profile/上线/下线/版本变化不触发
  增删或 Schema 改写；
- `editor_not_running`、`editor_starting`、`mcp_disabled`、`mcp_starting`、
  `editor_not_connected` 状态与 bootstrap/MCP 事实对应；
- `connector.reconnect` 并发/重复调用合并，不泄漏旧 socket/client/request；
- `connector.get_status` 的 instance/version、editor、bootstrap、MCP、Manifest、exposure 字段
  完整、规范化且不保存“预期 editor”事实。

### 11.3 Exposure Policy

- l0/l1/l2/l3 preset、默认值、重复 include/exclude 和配置归一化；
- `id:`、`category:`、`prefix:`、裸名语法；空值、未知前缀、正则/glob 字符和非法 UTF-8；
- 语法错误启动失败；合法无匹配 selector 保持 pending，并在较新 editor Manifest 出现后匹配；
- `(preset ∪ include) - exclude` 与 exclude 优先级；同一目标多次命中不重复；
- 相同 exposure 同时过滤类型化 wrapper 以及泛化 list/search/describe/invoke；
- 过滤目标返回 `connector_tool_filtered`，不能通过搜索模糊匹配、describe 或 invoke 绕过；
- include 高级目标只能扩 connector 说明面，editor profile/Custom/host/file policy 仍最终拒绝；
- 未知新工具不动态生成类型化 wrapper，只在 exposure 匹配且 editor 允许时走泛化路径。

### 11.4 版本与 Schema 兼容

- connector/editor 全局版本相同与不同、双方新旧方向、同名/单侧工具；
- 双向 `minimum_compatible_version` 门槛：版本通过/Schema 失败、Schema 通过/版本失败、两者
  通过、两者失败；
- digest 相同 fast path；digest 不同的输入协变/输出逆向包含关系 fixture；
- 不支持关键字、无法证明、递归/组合歧义按不兼容，不猜测；
- profile 变化只改变 availability，不伪装为 contract incompatibility；
- 工具新增/删除/兼容扩展/破坏性更改/最低 profile 更改后的 Manifest 与状态；
- `compatible_subset`、`contract_incompatible`、`tool_unavailable`、`profile_blocked`、
  `host_unavailable` 分开报告；
- 类型化不兼容不阻止 MCP 链路和实际工具发现，泛化调用仍使用 editor 当前 Schema 与权限。

## 12. 设置页、CLI 与运行时生命周期

- 首次运行/迁移旧设置的 disabled、L1、随机生成并立即持久化的非零端口、空 roots 和空
  Custom 安全默认；
- MCP enabled、稳定具体端口、profile、Custom、读写 roots 的持久化 round-trip、坏配置
  回退和原子写入；
- `--mcp`/`--no-mcp`、`--control-port 有效端口`、
  `--automation-profile l1|l2|l3|custom` 的合法组合；
- 同时给出互斥开关、缺值、越界端口、未知 profile、重复冲突值时启动前失败；
- CLI > persistent settings > safety default，覆盖不回写；设置页显示持久值、生效值和覆盖来源，
  被覆盖控件不能绕过；
- 启用：starting 先广播，真实 bind/register 可用后才 ready；端口冲突/路由失败进入 error；
- 禁用：stopping、拒绝新请求、短命令宽限、关闭 transport、disabled；后台 Task 继续；
- 启用中换端口的有序 stop/start、旧 endpoint 失效、新 endpoint 自动重连和失败恢复；
- profile/Custom 变化对 editor list/执行立即生效，connector 固定 downstream 工具集不变；
- 端口输入与刷新按钮始终可用；刷新会生成并保存新的具体端口，普通重启不得自行换端口；
  始终可复制不带 `mcpServers` 外壳的 stdio/Streamable HTTP 服务对象；
- 关闭窗口/退出时 watcher、HTTP、TaskManager 的确定关闭顺序，无 use-after-free 或悬挂端口。

## 13. 端到端、多 connector 与 GUI

### 13.1 进程级联调

- connector 先启动 → editor 不存在 → editor 启动 disabled → GUI 开启 MCP → 自动 ready；
- editor 先启动 ready → connector 启动 → 首个 watch 快照后连接和 Manifest 握手；
- editor 退出/重启、PID/port 复用、instance ID 改变、Manifest cache/旧句柄失效；
- 两至八个 connector 独立 watch、MCP Client、类型化调用、revision 冲突、Task 查询和断开；
- 一个 connector 崩溃/退出/慢读/限流不影响 editor 或其他 connector；
- 两客户端同 revision 写入只有一个成功，另一个 `revision_conflict`，无部分提交；
- editor MCP 直连与 connector stdio 对代表 L1/L2 查询、编辑、文件、Task、播放语料等价；
- 全局 Primary 竞争只有一个 editor 获胜，connector 不参与竞争；所有进程测试串行清理。

### 13.2 Computer Use GUI 回归

在自动化门禁通过后，用真实 Windows GUI 验证：

- Automation 设置页布局、说明、默认值、持久/生效/CLI 覆盖状态；
- MCP 开启、ready endpoint、端口冲突错误、换端口、关闭和再次开启；
- L1/L2/L3/Custom 切换，Custom 独立保存与新增默认关闭的可见行为；
- read/write roots 增删、canonical 显示、非法路径和不可写目录提示；
- connector 先启动时设置页开启后自动连接，多个 connector 状态同时更新；
- MCP 工具编辑后 GUI 立即反映，GUI 编辑后 MCP 查询/revision 立即反映；
- undo/redo、播放、打开/保存/导入/导出、Task 进度/取消的可见结果；
- MCP 禁用或 connector 退出不破坏 GUI，已接受长任务按约定继续；
- 一期完整 GUI 冒烟：文档、轨道、片段、音符、歌词/音素、参数、声线、时间线、History、
  播放、保存和退出路径无回退。

每个场景记录前置工程、进程启动次序、可见步骤、MCP/QLocal/connector 日志、结构化响应、
截图和清理结果。覆盖文件、坏路径和冲突端口只使用隔离测试目录与专用端口。

### 13.3 真实环境资格

- 至少一个真实 Agent Host 通过 stdio 启动 connector，调用 status/list/describe/类型化工具；
- DSPX open/save round-trip、MIDI import/export、当前支持的音频 decode/export；
- 可用声库下 voices/parameters/inference capability、短句推理、波形和 Task 查询/取消；
- 有/无音频设备时播放 capability 和稳定错误；
- 真实 Windows junction、Unicode 路径、空格路径和受限目录 File Guard。

缺少 codec、声库、模型、设备或 Agent Host 时标记“环境未具备”，不记为通过，也不降低
确定性测试分母。

## 14. 延期范围负向守卫

- Public Manifest、editor/connector `tools/list` 和泛化搜索中没有任何 L3 工具；
- `l3` profile/descriptor/CLI 可解析和持久化，但只产生本期 87 项集合；
- 不存在 `/automation/v1` 路由、JSON-RPC adapter、Ready File 或 Headless Composition；
- `documents.list`、`windows.list`、`documents.close` 不注册占位工具；
- MCP capabilities 不声明 Resources/Prompts/Sampling/Elicitation/Subscriptions；
- 内部 application lifecycle、commit/apply/cache/cleanup operation 无任何泛化 escape hatch。

## 15. 通过标准

- 适用确定性场景 100% 通过，87 项与六个 connector 工具追踪完整；
- 一期 Catalog/行为回归、公共集合、生成契约和源码边界守卫通过；
- Editor、connector、QLocal、MCP、安全、File Guard、兼容与多客户端测试通过；
- 完整 Debug build 和全部 CTest 串行连续三轮通过，无 flaky、残留进程、端口或临时文件；
- GUI/真实 Agent 联调通过；环境未具备和残余风险逐项说明；
- 任一修复后执行“最小复现 → 所属域 → editor/connector 联调 → 三轮全量”的回归门禁。
