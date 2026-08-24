# 二期 MCP Server 与 DS Connector Lite 实现报告

## 1. 实现结论

二期已在一期 Automation Facade 基线上完成公共 Wire Contract、87 项 Meta/L1/L2 公共工具、
GUI editor 内置 MCP Server、单实例 Automation Bootstrap、DS Connector Lite、Profile/Custom
权限、File Guard、Admission Control、Automation 设置页与 CLI 的代码交付。

Editor MCP、connector 和 GUI 继续复用一期的类型化 Facade、Dispatcher、History、revision、
幂等与 TaskManager。协议层没有直接访问 Model、Controller 或 History Action，也没有建立第二套
业务提交实现。公共工具分母以[公共工具矩阵](public-tool-matrix.md)为准；一期 122 个内部
Catalog operation 仍是进程内能力全集，不等同于 MCP 工具数。

本报告记录实现事实；最终冻结树已经完成完整构建、进程联调、安装 staging、GUI/真实环境
资格和连续全量回归，并在本期边界内达到可交付状态。逐轮结果、失败保留和放行结论见
[最终测试报告](test-report.md)。

## 2. 已交付的公共架构

| 能力 | 最终实现结果 |
|---|---|
| 公共 Wire 库 | 建立 `AutomationWire`，集中维护 MCP 2026-07-28、Profile、公共工具契约、公共枚举和值域、JSON Schema、规范化摘要、游标、exposure 与 Schema 兼容算法 |
| Public Registry | 建立 `PublicAutomationRegistry`，为 87 项工具登记严格 Schema、类型化 codec、动态值来源、Facade/host binding、输出校验和执行期安全门禁 |
| Editor Adapter | 建立 `PublicAutomationHostAdapter`，把文档打开/导入、音频片段、导出、提取和推理等公共编排接回一期真实运行时 |
| MCP Runtime | 建立 `EditorMcpController`、`McpRequestDispatcher` 和 `McpHttpServer`，负责设置生效、运行时启停、协议分发和 loopback HTTP 生命周期 |
| Instance Bootstrap | 从既有单实例机制提取共享身份与协议，增加 `automation.discover/watch`、完整状态快照和有界 watcher 广播 |
| Connector | 新增独立 `DsConnectorLite`，下游提供 MCP stdio Server，上游连接 editor HTTP MCP，并通过 QLocal watch 自动发现和重连 |
| 安全与授权 | 建立 `AutomationAccessPolicy`、`AutomationFileGuard` 和 `AdmissionController`，统一 profile、路径、调用来源、速率、并发和后台任务限制 |
| 产品配置 | 新增 Automation 持久设置、设置页、editor CLI override、connector exposure CLI 和运行时状态展示 |

公共调用路径固定为：

```text
MCP transport
  → protocol / metadata / JSON Schema
  → Public Automation Registry
  → Access Policy → File Guard → Admission Control
  → typed Facade / host adapter
  → Automation Dispatcher / TaskManager
  → History / revision / Model / file backend
```

Connector 对上、下游分别执行 MCP 语义校验和请求 ID 映射，不做逐字节透传，也不复制编辑
业务规则。

## 3. 公共工具、Schema 与 Manifest

### 3.1 单一契约来源

- 公共工具、枚举和值域集中定义在 `src/libs/AutomationWire`，由同一声明生成 C++ 契约、
  MCP tool descriptor、JSON Schema 和 Public Automation Manifest。
- Wire 字段统一使用 `snake_case`；业务对象默认拒绝未知属性，封闭枚举、数组上限、数值范围、
  标识符和分页规则在进入 Facade 前验证。
- 每项工具具有稳定 operation ID、追踪号、类别、Query/Command、同步模式、最低 profile、输入/
  输出 Schema、annotations、`value_sources` 和版本信息。
- Manifest 提供工具集版本、规范化契约摘要、host/profile、逐工具 descriptor 与分页；游标使用
  不透明编码并绑定查询上下文。
- Connector 使用版本门槛、输入兼容方向、输出兼容方向和 Schema 摘要共同判断类型化工具兼容；
  无法证明的变化不会被猜测为兼容。
- `automation.get_options` 只解析目标 Schema 声明的动态值来源，并继承目标的 profile、Custom、
  host 与 exposure，不形成隐藏工具探测入口。

### 3.2 87 项公共工具

| 集合 | 数量 | 实现结果 |
|---|---:|---|
| Meta/发现 | 4 | 应用信息、运行时状态、Manifest 和动态选项 |
| L1 只读 | 9 | 文档、工程、音符、参数、时间线、History 与声音能力查询 |
| L1 编辑 | 42 | 轨道、片段、音符、参数、声线、时间线、Master 与 History 编辑 |
| L2 | 32 | 文件、文档生命周期、音频片段、导出、提取、推理、Task、播放与循环 |
| **合计** | **87** | **具有公共契约、Registry binding、Manifest 描述和 editor/connector 工具描述** |

L1 preset 累积公开 55 项，L2 累积公开全部 87 项。Custom 始终保留四项 Meta 工具，其余
公共工具按稳定 operation ID 独立控制。内部 commit/apply/cache/cleanup operation 不进入
Manifest、`tools/list` 或 connector 泛化入口。

一期的任务查询与取消契约已从 `operations.list/get/cancel` 集中更名为
`tasks.list/get/cancel`。`OperationId` 继续表示能力定义，`TaskId` 表示一次异步执行实例；
旧名称不保留兼容别名。

### 3.3 Connector 六个固定桥接工具

| 工具 | 实现职责 |
|---|---|
| `connector.get_status` | 返回 connector、Bootstrap、editor、MCP、Manifest、兼容与 exposure 状态 |
| `connector.reconnect` | 主动重新执行 discover/watch、HTTP 连接和 Manifest 握手 |
| `editor.tools.list` | 分页列出通过 exposure 的 editor 实际目标 |
| `editor.tools.search` | 按名称、说明和类别搜索实际目标 |
| `editor.tools.describe` | 返回目标 Schema、版本、权限、兼容与可用性 |
| `editor.tools.invoke` | 按 editor 当前真实 Schema 泛化调用获准目标 |

六个桥接工具不计入 87 项公共业务工具，且在 connector 生命周期内保持固定。Connector 的
类型化工具面也在进程启动时由 exposure 固定；editor 上线、离线、profile 或 Manifest 变化
只更新实际可用性和兼容缓存，不动态改写下游 Schema。

## 4. MCP、Bootstrap 与 Connector

### 4.1 Editor MCP 2026-07-28

- Editor 只在数值地址 `127.0.0.1` 上提供 `POST /mcp`；端口可固定或由系统分配。
- 实现 `server/discover`、`tools/list` 和 `tools/call`，每次 POST 只接收单个 JSON-RPC request
  或 notification；notification 接受后不生成业务响应。
- 协议版本、客户端能力、客户端信息及 `Mcp-Method`/`Mcp-Name` 元数据按 MCP 2026-07-28
  校验；header 与 body 不一致时在进入业务 handler 前拒绝。
- 工具结果同时提供结构化内容和文本兼容表示，返回前按 output Schema 自检；协议错误、业务
  AutomationError 和 HTTP transport 错误保持分层。
- 不建立旧式 initialize/session 兼容层，不签发 `Mcp-Session-Id`，也不提供 GET stream 或
  DELETE session。
- HTTP 层校验本机地址、Host、Origin、method、Content-Type、Accept、请求体、JSON 深度/
  节点、响应体与请求期限，并分别限制全局、peer 和逻辑客户端的并发及令牌桶速率。
- HTTP limits 的安全默认值由 server 实现单元内的无参数 overload 构造；显式 limits overload
  不携带跨编译单元的聚合默认实参，避免结构布局演进时旧调用方对象误传 deadline 或配额。
- 运行时停止会先停止接收新请求，再有界结束在途请求；只有 listener 实际可用后才发布
  `mcp_ready`。

### 4.2 Instance Bootstrap

- 既有 `activate`、`openProjects` 单实例协议保持兼容；新增 `automation.discover` 一次性快照和
  `automation.watch` 长连接完整快照。
- Editor 与 connector 共用稳定产品身份、服务名和长度前缀 JSON framing。Connector 只创建
  QLocal 客户端，不竞争 editor 的全局锁，也不会成为 Primary。
- Bootstrap 发布 `starting`、`mcp_disabled`、`mcp_starting`、`mcp_ready`、`mcp_stopping`、
  `editor_stopping` 和 `error` 状态，并携带当前 editor 实例、host 与 endpoint 事实。
- Watcher 实现连接数、帧大小、待写帧数和累计字节上限；慢读或异常客户端不会形成无界写
  队列，断开后会从广播集合清理。

### 4.3 DS Connector Lite

- Connector 可在 editor 未运行或 MCP 未启用时先启动，下游仍能发现固定工具面，并以稳定状态
  错误报告 editor 不可用；editor 后续 ready 时通过 watch 自动握手。
- 下游 stdio 与上游 Streamable HTTP 分别维护 MCP 请求语义。上游请求重新分配 ID，并将并发
  乱序响应、取消、timeout、断线和 editor 实例更换映射回原下游请求。
- `l0/l1/l2/l3` preset 与 `id:`、`category:`、`prefix:` selector 形成固定 exposure；最终集合
  遵循 include 后 exclude，且同一结果同时约束类型化工具和泛化 list/search/describe/invoke。
- Manifest 支持分页握手、工具逐项兼容判断和实际可用性缓存。Editor 实例变化时清除旧 Manifest
  与句柄状态，不自动重放有副作用 Command。
- 相同 target 的重复 ready 在握手进行中会合并，完成后至多执行一次尾随刷新；target 变化立即
  取消旧周期。暂态握手失败使用有界指数退避，成功、target 变化和手动重连都会重置预算。
- Upstream client 区分可信 HTTP transport 错误、MCP 协议响应和不可确认结果；只有无法判断
  Command 是否已执行的情形才转换为 `outcome_unknown`。
- stdio 输入使用帧数与累计字节双上限及合并唤醒；stdout 由专用 writer thread 处理，并使用
  有界输出队列和确定性背压错误，避免主事件循环因下游停止读取而阻塞。
- stdout 只承载 MCP 帧；运行诊断与错误输出不进入协议字节流。

## 5. Profile、文件、Admission 与设置

### 5.1 Profile、Custom 与调用归因

- `selectedProfile` 与 `customPermissions` 独立持久化；切换 L1/L2/L3 不覆盖 Custom，切回
  Custom 时恢复原选择。
- 同一 `AutomationAccessPolicy` 同时控制 Manifest、editor `tools/list`、动态选项和执行期
  Registry。隐藏只影响发现，实际调用仍会在 dispatch 前再次授权。
- HTTP 请求携带的 MCP clientInfo 被规范化为逻辑 client ID，用于日志归因、配额与 Task 创建者
  诊断，不作为认证凭据或 Task ACL。
- Automation 启用、profile、Custom 和文件根目录没有 MCP 写入口。

### 5.2 File Guard

- 读根、写根、会话读授权和会话写授权分别维护；`automation.get_file_access` 只读返回当前
  canonical 事实。
- 路径在进入文件后端前转换为 canonical absolute path，并按读写目的检查根边界、大小写、
  分隔符、父目录、链接与不存在输出目标的最近存在父级。
- Guard 返回 `AuthorizedPath`；执行前可以重新授权，避免检查后路径变化绕过。Facade 和 host
  adapter 使用已经授权的 canonical path，不重新采用原始字符串。

### 5.3 Admission Control

- 业务 Admission Controller 维护全局在途、每客户端在途、后台任务容量、并发域和每客户端
  令牌桶；租约生命周期负责确定释放配额。
- HTTP transport 另有请求体、响应体、deadline、全局/peer/client 在途和速率门禁；超过容量
  时立即返回稳定的 `busy` 或 `too_many_requests`，不建立无界等待队列。
- 停止 MCP 时统一关闭新 admission，已进入一期 TaskManager 的后台任务继续遵守既有 Task
  生命周期。

### 5.4 Automation 设置与 CLI

- `AutomationOption` 独立保存 MCP enabled、固定/随机控制端口模式、具体非零端口、profile、
  Custom 权限和 canonical 读写根；安全默认值为 MCP 关闭、L1、随机端口和空的额外文件根。
- Automation 设置页提供 MCP 开关、单行端口模式/刷新/端口控件、L1/L2/L3/Custom、分类/单项
  Custom、读写根、运行时状态、当前 endpoint 与错误。固定模式允许编辑端口，随机模式允许刷新
  具体端口；CLI 覆盖时整组控件不可编辑。
- 设置页始终展示并允许复制 stdio 与 Streamable HTTP 配置；复制内容是单个服务对象，不包含
  `mcpServers` 外层容器。
- Editor 支持 `--mcp`、`--no-mcp`、`--control-port` 和 `--automation-profile`；CLI override
  优先于持久设置且不回写，设置页显示来源并禁用被覆盖项。
- 设置变化由 `EditorMcpController` 执行有序 stop/start；端口或根目录无效时进入可解释 error，
  不发布伪 endpoint。

## 6. 实施中完成的关键修复

### 契约与一期回归

- 完成 Task 控制名称集中校正，并同步 Catalog、Facade、Wire、测试和一期文档，避免
  OperationId 与 TaskId 术语混用。
- 将公共 Wire 字段、封闭枚举和值域与冻结矩阵对齐，补齐受控集合、参数采样、批量导入和分页
  上限，避免合法但无界的公共输入进入业务层。
- 修正稀疏属性对象的至少一项约束、提取参数 Schema 和动态值来源，使 Schema、codec 和真实
  binding 使用同一语义。
- 异步公共入口保留一期不可变执行快照、单 TaskId、取消点和 generation/revision 门禁；同步
  更新与二期语义冲突的旧回归期望，没有恢复平行的两阶段执行实现。

### HTTP、Connector 与生命周期

- HTTP request admission 从仅有全局限制扩展为全局、peer 与逻辑客户端三层限制，并确保成功、
  拒绝、timeout 和 shutdown 都释放在途计数。
- 补齐请求集合上限、JSON 复杂度、响应体上限和超限错误，防止 Schema 合法请求或过大结果绕过
  transport 资源边界。
- Connector 上游错误映射只接受状态码、媒体类型和严格错误包络一致的可信 transport 错误；
  异常响应保持 `invalid_upstream_response`，不会把所有 Command transport 错误泛化为
  `outcome_unknown`。
- stdio reader 改为有界共享队列和单一合并通知；stdout 写入移至专用线程，处理部分写、管道
  暂不可写、停滞超时和队列超限，消除主线程阻塞及无限 queued signal 风险。
- Editor runtime、Bootstrap watch 和 connector handshake 都以 editor instance/epoch 隔离晚到
  结果；换端口、停启或 editor 重启不会继续使用旧 endpoint 与 Manifest。
- 最终进程联调曾暴露一个增量构建 ABI 问题：调用方对象仍按旧版 `McpHttpLimits` 聚合布局传递
  默认值，使 HTTP request deadline 被误读并压到 10 ms，继而触发 504、握手重试和 429。
  强制重编调用方验证根因后，提交 `68162c7e` 通过实现单元内构造默认 limits 的 overload 消除
  跨编译单元默认聚合实参，保留显式 limits 测试入口与原有协议契约。

## 7. 阶段提交分组

实际提交按依赖和可独立审查职责分组，没有把 editor、connector、测试与文档压入单一提交：

| 分组 | 代表性提交主题 |
|---|---|
| 计划与一期契约校正 | `docs(automation): plan phase two mcp delivery`、`refactor(automation): rename task control operations` |
| 公共安全底座 | `feat(automation): add public invocation attribution`、`feat(automation): enforce canonical file access roots`、`feat(automation): add fair request admission control` |
| Bootstrap 与产品配置 | `feat(single-instance): add automation discovery and watch`、`feat(settings): add automation controls and cli overrides` |
| Wire 与字段冻结 | `feat(automation): add phase two wire contracts`、`docs(automation): align public wire field names` |
| Connector | `feat(connector): add stdio MCP bridge`、`fix(connector): harden transport and reconnection` |
| Editor 公共绑定与 MCP | `feat(automation): bind phase two public editor tools`、`feat(editor): host loopback MCP server` |
| 测试与收口修复 | `test(automation): cover phase two editor and connector`、`fix(automation): preserve phase two async contracts`、`test(automation): align phase one regression expectations`、`fix(automation): bound public requests and client quotas`、`fix(editor): stabilize default MCP transport limits`、`test(automation): exercise real MCP process workflows`、`test(automation): align audio snapshot regression` |
| 无人值守与测试文档 | `docs(test): harden unattended phase two runs` |

后续正式测试中发现的独立缺陷继续使用 `fix(scope): summary` 分组，并在修复后重新执行所属域与
全量门禁；私有证据和环境信息不进入提交。

## 8. 测试与长期保护产物

- 已建立 Wire/Schema/Manifest、Public Contract、Public Registry、File Guard、Admission、Automation
  设置、启动参数、MCP HTTP、Single Instance、Connector 和进程联调测试目标。
- 公共集合测试以 87 项矩阵、六个 connector 桥接工具、122 项内部 Catalog 审计和延期集合负向
  守卫为机器不变量。
- 协议测试覆盖 MCP metadata、header/body 一致性、HTTP 安全、分页、Schema、结构化结果、限流、
  timeout 和 runtime stop；connector 测试覆盖离线工具面、exposure、兼容、重连、错误映射、
  stdio framing 与背压。
- 一期 Catalog、文档生命周期、幂等、异步 Task、编辑域和运行时域保护继续作为二期回归基线。
- 最终冻结树完成 Debug 全目标构建；真实 Editor/Connector 进程联调 R17、R18 连续通过。
- `package-dml-release` 完成 editor 与 connector 构建和 staging 安装；安装目录同时具有两款
  可执行文件以及 Connector 所需 Qt Core/Network 与运行库，安装版 CLI smoke 通过。
- GUI/真实资格在隔离副本上完成打开、播放、编辑、保存，以及 Connector mutation 后 GUI Undo；
  只读源 19/19 保持完整，无无人值守弹窗或测试进程残留。
- 修正一个与 `RevisionPolicy::None` 新契约冲突的旧测试断言后，最终冻结树连续三轮完整 CTest
  均为 65/65。正式执行细节仍以[全量测试大纲](test-outline.md)、
  [测试执行计划](test-plan.md)和[最终测试报告](test-report.md)为准。

## 9. 明确的三期边界

二期只交付运行中 GUI editor 的 MCP Server 与独立 connector，不实现三期 Headless 产品形态。
以下能力仍属于后续阶段：

- `--headless` 启动模式、`QCoreApplication` Composition、独立无窗口 Core target 和 Headless
  生命周期；
- Ready File、`POST /automation/v1`、原生 JSON-RPC Adapter 及其请求/响应兼容层；
- 多个真实 DocumentSession、DocumentRegistry、WindowRegistry 和未来条件工具；
- MCP Resources、Prompts、Sampling、Elicitation、长期订阅和 standalone SSE；
- L3 业务工具。二期仅保留 L3 profile、descriptor、持久化和 CLI 解析基础，选择 L3 时实际
  公共业务集合与 L2 相同，没有注册 L3 占位工具。

三期可以复用本期已形成的 Wire Contract、Public Registry、Access Policy、File Guard、
Admission Control 和一期 Facade，但这些复用点不表示 Headless/JSON-RPC 已在二期实现。
