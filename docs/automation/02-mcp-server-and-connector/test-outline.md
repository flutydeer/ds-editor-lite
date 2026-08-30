# 二期 MCP Server 与 DS Connector Lite 全量测试大纲

## 1. 目标与分母

本大纲验证二期从公共契约到真实 GUI Editor/Connector 联调的完整链路。正式验收以同一候选重新生成的 CTest 与 GUI 双轨证据为准。当前产品清单快照为：

```text
Editor 公共工具：177
Connector 桥接工具：6
总工具面：183
```

上述数量用于描述当前产品面，不作为硬编码测试门禁。公共契约是 Editor 工具清单的唯一来源；测试验证 ID 唯一、Schema 合法，以及 Registry、发现授权和 Connector downstream 与权威集合之间的关系。每个业务域保留独特语义的确定性 CTest 与真实产品会话代表路径：可见状态由 GUI 观察闭环，不直接可见的查询、安全拒绝和后台任务由 Connector 回读、应用状态及进程事实闭环。

测试层次：

| 层次 | 验证对象 |
|---|---|
| 静态契约 | ID 唯一性、元数据合法性、Schema 及 MCP 描述与 binding 的集合一致性 |
| 单元 | codec、validator、Policy、Guard、Admission、exposure、版本兼容 |
| 组件 | MCP parser/router、HTTP、QLocal、stdio framing、上下游 client/server |
| 进程 | Editor、Connector、单实例、重连、运行时启停、多 Connector |
| 业务域 | 类型化 Facade、GUI 状态、应用设置、历史记录、revision、Task 与文件的独特语义 |
| GUI | Automation 设置页、可见编辑结果、Undo/Redo、运行状态 |
| 资格 | 真实格式、声音、推理、播放与 Agent Host 环境 |

## 2. 集合、版本与基础契约

### 2.1 ID 与域

- Editor 公共契约的 tool name 唯一；`project.get` 不在公共集合，内部 Facade 名称不泄露为 MCP 工具。
- Connector 桥接 tool name 唯一，并与固定定义一致。
- Registry binding 与 Editor `tools/list` 分别等于相应授权后的公共契约集合；Connector 已知类型化描述来自同一契约源。
- Connector downstream 等于当前 exposure 下可用的 Editor 工具与桥接工具之并集。
- `master.*` 的 category 为 `bus`，钢琴与参数工具归入 `clip_editor`；其他域、Query/Command、同步模式与最低 Profile 元数据满足声明约束。
- 已移除的复合属性入口不出现在公共 ID、Schema、发现结果或 Dispatcher 路由中。

### 2.2 版本

- 发布前全局 `toolset_version` 为 1；每个工具的 `minimum_toolset_version` 必须是有效且不高于当前工具集的版本。
- `application.get_status` 的全局版本以及 `tools/list` 的逐工具最低版本与契约一致。
- Connector 只按双方全局工具集版本与逐工具最低版本判断同名类型化工具是否兼容。
- 同一工具集版本下的 Schema 差异按测试失败处理，不产生 Schema 子集、digest 或
  `compatible_subset` 状态。

### 2.3 Task 术语校正

- 集中 ID、显式 Dispatcher 路由、Facade、Wire 与测试使用 `tasks.list/get/cancel`。
- `operation_id` 指向能力，`task_id` 指向执行实例。
- 任务状态、进度、终态、取消、generation、幂等和创建者归因保持一致。
- Core 能力集合由 `OperationIds::all()` 返回，Dispatcher 显式路由；CTest case 数只在本轮执行
  报告中回填，不替代冻结工具分母。

## 3. Wire Contract、Schema 与工具发现

### 3.1 Schema 与 codec

- 权威公共契约声明的全部 input/output Schema 均可通过 meta-schema 检查。
- 业务对象为封闭结构；未知字段、错误类型、非法枚举、NaN/Inf、整数溢出、非法 UUID 和超限集合在 handler 前失败。
- 无参工具只接受空 object；必填字段、oneOf 分支、nullable 和分页字段严格执行。
- 轨道/剪辑 voice Schema 要求 singer、允许 speaker 省略或为 `null`；零、单、多 speaker 三种解析分支分别覆盖。
- `tracks.get` 返回轨道属性、统计和 voice/default-language 上下文；`clips.get` 对歌声剪辑返回
  own/effective voice、继承来源和有效默认语言。公共集合不含独立的 voice-context 查询工具。
- 公共 enum/值域在 C++ codec、Schema、动态候选和测试语料中来自同一声明。
- 轨道、剪辑、总线的细粒度标量命令，以及已有音符的歌词、语言和长度命令，各自具有单条历史记录与 revision-check descriptor。
- 空轨道/空歌声剪辑的浅层创建 draft 与完整 note leaf draft 的合法/非法形状分别覆盖；轨道不得嵌套剪辑，剪辑不得嵌套音符、参数、voice 或 mix。
- duplicate、move、resize、split、keyframe 与 anchor 使用稳定 ID，并覆盖重复、失效和跨 owner ID。
- `documents.get` 的统计字段为非负整数且与领域查询一致；`documents.list_recent` 为 L2 应用级只读查询。
- `parameters.get` 的半开范围、默认/显式点数上限、采样降采样元数据和“锚点不得丢失”失败路径均满足严格 Schema。
- `create_anchor_curve/insert_anchors/merge_anchor_curves` 分别承担创建、既有曲线插入和显式合并，不允许隐式创建、跨曲线移动或重叠。
- Speaker Mix 预设的应用级 list/save/delete 与文档级 apply 具有不同 revision、History 和并发域契约；`speaker_mix.get` 返回来源预设及 dirty 状态。
- 25 个 L3 GUI 工具均显式定位 `window_id`，涉及工程对象的工具还显式定位 `document_id`；GUI command 不改变 revision/history。
- 设置更新 Schema 只包含公开 allowlist，全部为稀疏 update；音频设备、计算设备和包搜索路径更新另覆盖 validate-only，所有域覆盖候选值、生效值、重启要求和失败回滚。
- `packages.refresh` 使用 application-scoped Task；歌词规则使用稳定 rule ID，内置规则内容不可修改或删除。

### 3.2 动态值与首次调用

- `application.get_status` 零参数返回 Editor、工具集版本、Profile、host 和当前 document/window 摘要。
- `application.request_exit/restart` 在所有 Profile 与 Custom 中均可发现且不可禁用；输入只允许可选布尔字段 `discard_changes`，输出明确区分 exit/restart。
- 脏工程默认返回 `busy + field_path=discard_changes` 并保持进程与 GUI 可操作；显式丢弃后优雅退出或重启。公共路径不得出现保存确认、忙碌提示或其他决策弹窗，GUI 路径仍保留原行为。
- `value_sources` 指向可达查询，最低 Profile 与 host availability 合法。
- voices、parameters、formats、exports、extract、inference、settings 与 file access 等领域查询覆盖所有公开动态候选，且在目标最低 Profile 下可达。
- 目标隐藏、Custom 关闭、exposure 过滤和 host 能力变化时，领域查询与对应调用遵守各自授权结果。
- 查询返回的候选值可直接用于对应命令；完整 SingerRef 能区分同 ID 的并存版本，speaker 候选可由 `voices.describe` 取得。
- 动态数值范围同时验证最小值、最大值、step 与 unavailable reason。
- 正常 invocation 不自动回查动态 provider；已通过 input Schema 的值直接进入领域 handler，
  provider 只服务显式发现调用。

### 3.3 工具目录与游标

- 标准 MCP `tools/list` 返回获准工具的完整 descriptor、Schema、annotations 与逐工具最低版本。
- 泛化 `editor.tools.list/search` 返回摘要，`editor.tools.describe` 才返回目标完整 descriptor；三者的名称、域、版本和可用性一致。
- 排序确定；首页、中间页、末页、零/最大 limit、非法编码、过期/跨上下文 cursor 行为稳定。
- Cursor 的 base64url payload 只绑定 context、snapshot 和 offset，不使用 HMAC 或认证密钥。
- Profile/Custom 或实际工具目录更新后，新 snapshot 反映新可见集合；旧 cursor 不能跨快照使用。

## 4. Editor 工具的域覆盖

| 域 | 数量 | 重点验证 |
|---|---:|---|
| 应用 | 3 | 产品/版本/平台/build、运行状态、工具集版本、文件授权与权限继承 |
| 文档与工程 | 8 | 文档统计、最近项目、new/open/save/save-as/import/batch、未保存策略、换代与 savepoint |
| 格式 | 2 | 用途过滤、可用性、inspect 诊断与 plan digest |
| 轨道 | 14 | 列表/详情（含声音上下文）、浅层创建、删除/移动、标量命令、语言与声音设置 |
| 总线 | 5 | `bus` 归属、四个标量命令、历史记录/revision |
| 剪辑 | 15 | 筛选/详情（含声音上下文）、浅层创建、duplicate、几何、标量命令和声音继承 |
| 音频剪辑 | 5 | 元数据、导入/batch 任务，以及 relocate/confirm 同步 Mutation 与授权重检 |
| 声库 | 2 | 可用 singer/speaker/language/G2P/mix 能力，以及含 package version 的稳定引用和同 ID 并存版本精确解析；L1/L2 不依赖包信息域 |
| Speaker Mix | 13 | fixed/dynamic/bypass、权重归一化、关键帧稳定 ID、应用级预设与文档级应用 |
| 音符、歌词、语言、发音与音素 | 19 | 查询/搜索/叶节点创建/duplicate/几何、歌词、语言、发音、音素与填充 |
| 参数曲线与锚点 | 12 | capability、有界查询、draw/anchor、replace/draw/erase/bake 与显式曲线拓扑操作 |
| 时间线 | 5 | Tempo/拍号排序、零点锚、单条历史记录 |
| 历史记录 | 3 | 状态、Undo/Redo、分支与 savepoint |
| 播放 | 8 | 瞬时目标状态的幂等/no-op；持久 loop 的历史记录、revision、Undo/Redo 与并发检查 |
| 导出 | 6 | MIDI/audio capability、preview、写授权、任务与清理 |
| 提取 | 3 | 来源/模型/语言能力、pitch/MIDI 任务与写回 |
| 推理 | 4 | scope/stage/provider/device/model、状态、任务和 reset |
| 异步任务 | 3 | 分页/筛选/详情、取消点、终态与创建者归因 |
| 工作区布局 | 2 | 主编辑面板布局、可见性、至少保留一个主面板与焦点归属 |
| 轨道面板 | 7 | 稀疏视口、reveal、自动翻页、有序选择、primary、活动区域与尽力键盘焦点 |
| 剪辑编辑器 | 16 | 活动剪辑、共享时间视口、钢琴/参数显示、选择、工具和值域视口 |
| 设置 | 10 | domain query、公开字段稀疏更新、立即/重启生效、无弹窗与回滚 |
| 包信息 | 3 | 读取根内路径披露、详情、后台刷新、取消与索引原子切换 |
| 歌词规则 | 7 | 稳定 ID、CRUD、启停、分类内移动、非法规则与只读流水线测试 |

共享的 Schema 拒绝、授权、错误封装、revision、Query 无副作用、Command 提交与 Task 状态机在其所有者层验证一次，不按工具或域复制相同矩阵。各域测试聚焦其独有的数据边界、历史记录粒度、失败原子性和异步写回条件；真实 Editor、Connector 与 GUI 则按调用类型、风险和可见行为选择代表路径。

## 5. Query、同步 Command 与异步 Command

### 5.1 Query

- 确定性契约测试验证代表结果和错误满足 output Schema；运行时不逐次重复校验输出。
- `structuredContent` 与 TextContent 表达等价。
- 排序、分页、Unicode、空集合、长文本和边界数字确定。
- Query 不推进 revision、历史记录或 Task 状态。
- 能力查询同时表达 supported、available 和 unavailable reason。
- `documents.get` 的工程长度和轨道/剪辑分类统计在空工程、空轨、有歌声/音频/混合轨状态下准确；`documents.list_recent` 不改变文档身份或 revision。
- `parameters.get` 对采样数据执行确定性有界返回，对锚点数据完整返回或明确拒绝过小上限，不静默裁剪稳定对象。

### 5.2 同步 Command

- 工程编辑执行前校验 document、expected revision、对象 owner/type、动态值、File Guard 和 Admission；瞬时 GUI 与播放目标状态命令不要求客户端版本令牌。
- 成功结果含 previous/current、changed、affected/created objects、resolved values、presentation effects 与 warnings。
- 合法 no-op 返回 `changed=false`，历史记录与 revision 保持原值。
- `documents.save_as`、`documents.import_batch`、`audio_clips.import_batch`、三项复杂设置更新以及歌词规则 create/update 的 validate-only 执行完整校验，且不产生 ID、Task、文件写入、历史记录、revision 或业务通知；其他命令拒绝该额外字段。
- 批量命令先完整预检，再以一条历史记录和一次 revision 提交。
- handler、I/O、Schema 编码和 host adapter 失败不产生半提交。
- `audio_clips.import*`、`relocate` 与 `confirm_path` 的 SHA-512 和解码结果来自同一临时字节快照；同步工具完成校验和最终写回后返回 Mutation，不创建 Task。
- `playback.set_loop`、`set_loop_enabled` 与 `clear_loop` 修改工程持久状态，各自产生一条可撤销、可重做的历史记录；瞬时播放命令不进入历史记录，播放头在调用间变化不造成冲突，重复目标调用返回 no-op。
- Speaker Mix 预设 save/delete 不改变文档 revision 或 History；apply 只形成一条文档历史记录，后续直接编辑将来源标记为 dirty。
- 创建、插入和合并锚点曲线分别形成单条历史记录；非法重叠、非相邻合并和跨曲线移动在提交前失败。

### 5.3 异步 Command 与 Task

- 接受响应只创建一个 TaskId，并记录 operation、scope、创建者和初始进度；document scope 记录 base document，application scope 的 document 为 null。
- Queued、Running、CancelRequested、Committing 和各终态遵守允许迁移。
- 排队/运行取消、重复取消、终态取消和提交点取消分别覆盖。
- Editor 重启、文档 generation 更换、revision 前进、目标消失和晚到结果不能写入错误文档；文档 generation 清理不得误删 application task。
- application task 的活动记录全部保留；终态历史超过 128 项时只淘汰最旧终态，不影响活动任务。
- 最终成功在业务写回、历史记录/revision 和信号完成后发布。
- MCP/Connector 断线后已接受任务仍可通过 `tasks.list/get/cancel` 观察和管理。
- 结果未知场景通过 idempotency、revision 或 Task 查询确认，Connector 不重放有副作用请求。

## 6. Profile、Custom 与 exposure

- L0 为语义上固有且不可禁用的集合；L1、L2、L3 逐级包含，L3 等于完整公共契约集合。
- `selectedProfile` 与 `customPermissions` 独立持久化，preset 切换保持 Custom 内容。
- Custom 的单项、领域分组、空集合、新 operation 安全默认和坏配置恢复覆盖；L0 不进入 Custom 列表且不能被其配置禁用。
- 领域卡片默认收起；展开/收起不改变权限，标题启用数与单项状态一致，组级关闭/开启分别原子更新整组并持久化。
- Editor `tools/list` 和执行期 Registry 使用同一策略。
- 列表后改变 Profile/Custom 时，实际调用按最新策略判定。
- Connector exposure profile `l0/l1/l2/l3`、include、exclude 和 selector 规范化覆盖；L0 profile 固有包含 L0 Editor 工具，exclude 不能移除。
- `id:`、`category:`、`prefix:`、裸 ID、重复 selector、pending selector、非 L0 目标的 exclude 优先级和 L0 不可排除约束覆盖。
- 同一 exposure 同时约束类型化工具与 `editor.tools.list/search/describe/invoke`。
- Connector exposure 只塑造下游说明面，Editor 执行期策略仍是最终授权。

## 7. File Guard 与 Admission

### 7.1 File Guard

- 配置读根、写根、会话读/写 grant 和快照 round-trip。
- 根本身、根内文件、相邻前缀、`..`、相对路径、drive-relative、UNC、大小写和混合分隔符。
- symlink、junction、reparse point 与根边界的 canonical 判定。
- 输出文件尚未创建时按最近存在父目录授权。
- authorize 与实际 I/O 前 reauthorize；根配置变化、目标变化和链接重定向被识别。
- 单文件和 batch 路径全部通过同一 Guard，类型化与泛化调用结果一致。
- overwrite、只读、父目录、权限、磁盘和失败清理覆盖。

### 7.2 Admission

- 业务层 global 32 与 background-task 8 两个硬上限。
- HTTP 层 global 32 硬上限；不设置 client、peer、domain 或 token bucket。
- 正常握手和基线查询后，32 路同时请求全部进入；第 33 个在途请求稳定拒绝且不排队。
- 成功、失败、取消、deadline、断线、disable 和 shutdown 都释放租约与计数。
- 多 Connector 共享同一全局上限，客户端身份只用于归因，不改变 Profile、Task 可见性或配额。

## 8. MCP 双协议、兼容握手与 Editor HTTP

### 8.1 协议生命周期

- `2025-11-25`：`initialize → notifications/initialized → ping/tools/list/tools/call`。
- `2026-07-28`：`server/discover → ping/tools/list/tools/call`，逐请求 `_meta`。
- 兼容握手：客户端请求 `2025-06-18` 时，服务端回显该版本并完成 legacy initialize/initialized、ping、tools/list 与 tools/call；它不改变两套主协议契约的分母。
- 2026-07-28 对 `initialize` 给出协议错误；协商、支持版本列表、client/server info、client capabilities 和 tools capability 精确。
- 协商版本决定结果形状；2025-11-25/2025-06-18 legacy 与 2026-07-28 的 metadata、structuredContent 和兼容文本分别验证。
- request ID、notification、parse/invalid/method/params/internal error 与业务错误分层。

### 8.2 Header 与 transport

- `MCP-Protocol-Version`、`Mcp-Method`、`Mcp-Name` 与 body 镜像一致。
- header Base64 sentinel、Unicode、控制符、前后空格、重复 header 和大小写规则。
- POST 承载 request/notification 且 notification 返回 202；legacy session 可由带 session 与协议 header
  的 DELETE 结束，缺失/重复 header、版本不符、未知或重复结束均返回稳定状态。
- legacy session 最多保留 128 个，超限只淘汰最早建立的会话。
- 单消息、非法 JSON、数组/batch、错误 id、超大 body、深度、节点、响应体和 deadline。
- listener 仅为 `127.0.0.1`；Host、Origin、DNS rebinding、CORS 与远端地址拒绝矩阵。
- shutdown 先停止 admission，再结束在途请求并释放端口。

### 8.3 Tools

- 2025-11-25 与 2026-07-28 两套主协议，以及协商到 2025-06-18 的兼容会话，对当前授权契约集合、分页、Schema、annotations 和适用 server metadata 进行比对。
- `tools/call` 覆盖缺 name/arguments、未知工具、权限变化、Schema 错误、业务错误与成功结果。
- Registry binding 集合与公共契约集合相等；按参数形状和调用类型选取代表工具验证 schema-valid 路由，按工具独特语义覆盖成功、不可用、no-op、失败回滚和异步终态中的适用分支。
- 确定性语料覆盖 output Schema 的有效结果与反例；Editor 运行时不执行逐次 output Schema assert。

## 9. QLocal Bootstrap

- Editor 与 Connector 对产品身份、服务名和 framing 的 golden test。
- 既有单实例命令回归，以及 discover 一次性快照、watch 初始快照和后续广播。
- starting、disabled、starting-listener、ready、stopping、editor-stopping、error 的状态与 endpoint 不变量。
- 分片帧、合并帧、零/超长帧、非法 JSON/UTF-8、未知 command 和 request ID。
- watcher 数量、待写帧数、累计字节、慢读背压、异常断开和 timeout。
- Editor instance ID、PID、endpoint 和版本变化触发 Connector epoch 切换。
- Connector 离线观察只建立客户端连接；全局 Editor Primary 的锁与服务所有权保持唯一。

## 10. DS Connector Lite

### 10.1 stdio

- Editor 离线时 Connector 仍完成 downstream 握手并发布固定桥接面。
- stdout 字节流仅含 MCP 帧，诊断只在 stderr。
- CRLF、部分行、多帧、notification 洪泛、非法 JSON、最大帧、超限、EOF 与 broken pipe。
- writer queue 的部分写、backpressure、停滞 deadline 和确定性关闭；183 工具大响应分别由正常读端与延迟慢读端完整接收。
- downstream request ID 到 upstream ID 的映射支持并发乱序、取消和 timeout。

### 10.2 双协议与兼容握手的上游/下游

- Downstream 分别执行 2025-11-25 initialize 生命周期、2026-07-28 discover 生命周期，以及请求 2025-06-18 的兼容 initialize 生命周期。
- Upstream 优先执行 2026-07-28 discover，并覆盖回退到 2025-11-25 初始化及服务端协商到 2025-06-18。
- 握手成功后协议版本固定，`tools/list` 分页完整读取，随后只调用一次 `application.get_status` 取得全局工具集版本、Profile 与 host 摘要。
- 重复 ready 合并、尾随刷新、有界指数退避、手动 reconnect 和目标变化覆盖。
- instance/endpoint 改变后旧 request、工具目录、状态摘要、cursor 和兼容缓存失效。
- Editor 未运行时持续报告 `editor_not_running`，不得被首包计时器延迟覆盖为
  `bootstrap_timeout`；后者只覆盖 watch 请求已写入而首个状态快照超时。

### 10.3 六个桥接工具与兼容

- `connector.get_status` 各字段与 Bootstrap、HTTP、toolset compatibility 和 exposure 事实相等。
- `connector.reconnect` 的并发与重复调用合并。
- `editor.tools.list/search` 返回摘要，`describe` 返回完整 Schema，`invoke` 透传调用；四者对可用性、过滤和调用授权一致。
- 各 Profile 的 downstream 集合等于相应 exposure 下可用的类型化 Editor wrapper 与固定桥接工具之并集，且随 Profile 逐级包含。
- 双向全局 toolset/逐工具 minimum toolset 门槛覆盖；不执行 input/output Schema 子集证明或
  Schema digest 兼容计算。
- Connector 只验证六个桥接工具 envelope；类型化工具和泛化 invoke 的业务 Schema 由 Editor MCP 验证。
- 同版本 Schema 漂移由契约测试直接失败，不报告 `compatible_subset`；compatible、contract
  incompatible、tool unavailable、profile blocked、host unavailable 分开报告。
- upstream transport error、MCP error、invalid response、timeout、cancel 与 outcome unknown 分开报告。

## 11. 设置、CLI 与运行时生命周期

- Automation 配置模型的 enabled、持久非零端口、Profile、Custom、读写根默认与 round-trip；端口首次生成后不随启动变化。
- `--mcp`、`--no-mcp`、`--control-port`、`--automation-profile` 的合法与冲突组合。
- CLI override 优先级、运行期生效和持久设置保持。
- Runtime enable、端口冲突、ready、disable、换端口、错误恢复和退出顺序。
- Profile/Custom/roots 在运行中变化时，Editor list/dispatch 与 Connector 状态及时更新。
- 选项菜单“自动化”项的图标与中文名称、面板完整中文翻译通过 Computer Use 验证。
- Custom 工具按领域显示为可折叠卡片；逐组验证默认收起、展开/收起、启用计数、整组关闭/开启和单项状态回读。
- 端口刷新按钮与 number box 同行且始终可用；不存在固定/随机下拉框，刷新或直接编辑后的值与持久化结果正确。
- stdio 与 Streamable HTTP 配置在 ready/disabled/error 状态下始终可复制，复制对象不含外层 `mcpServers`。
- 读写根帮助文本解释其为自动化文件路径 allowlist；设置页不出现无动态事实的本机进程访问栏目。
- Automation 调用全过程监测模态对话框、主线程阻塞和 UI 假死。

## 12. 进程联调、多 Connector 与 GUI

### 12.1 进程联调

- Connector 先启动，再经历 Editor 启动、MCP enable 和 ready。
- Editor 先 ready，Connector 首次 watch 后完成协议、tools 目录和状态摘要握手。
- Editor direct HTTP 与 Connector stdio 复用同一 L0/L1/L2/L3 业务语料并比对结果。
- 在隔离配置下分别验证 clean 默认退出、dirty 默认拒绝、dirty 显式丢弃退出，以及重启后的新 instance ID、Connector 自动重连与最终优雅退出。
- open/save/import/export、音频剪辑、推理、Task、播放与历史记录使用隔离工作区。
- 两至八个 Connector 并发查询、编辑、任务与重连；覆盖 revision conflict、全局上限和状态隔离。
- 一个 Connector 退出、崩溃、慢读或触发全局准入上限时，Editor 与其他 Connector 继续服务。
- Editor stop/restart、instance ID 与 endpoint 更换后自动建立新链路。

### 12.2 GUI 与真实资格

- 设置页默认、修改、CLI 覆盖、Custom 领域分组和运行状态展示。
- Contract、Registry、Editor MCP 与 Connector 的集合关系由确定性测试覆盖；真实 Connector 会话按业务域、调用类型与风险覆盖代表路径，并执行各 Connector 桥接工具的独特行为。
- 24 个业务域均建立真实产品会话代表路径；具有可见 UI 的域同时保存 GUI 观察证据。MCP 编辑后 GUI 立即呈现；GUI 编辑后 MCP 查询与 revision 立即更新；不直接可见的查询、应用设置和 Task 通过对应 query、应用状态或进程事实闭环。
- 轨道、总线、剪辑、音符、参数曲线、时间线、Speaker Mix、历史记录与播放均执行至少一条真实 mutation，并使用 GUI 与 MCP Undo/Redo 验证代表路径的历史记录粒度及状态恢复；其余领域特有边界由确定性测试覆盖。
- 工作区布局、轨道面板和剪辑编辑器分别以代表路径保存调用前后界面、对应 get 回读与恢复证据；验证 Editor 处于后台且无法取得键盘焦点时，已完成的显示、选择和定位仍成功，并持续监控实际焦点、顶层窗口和活动模态窗口。
- 设置更新按主要参数形状和生效方式取样，包刷新和歌词规则管理覆盖其关键成功与回滚路径，并在场景后恢复隔离配置。
- 文档、格式、音频剪辑、声库、导出、提取、推理和异步任务均在隔离工作区执行真实资格路径；环境缺少 codec、声音、模型或音频设备时，保存结构化不可用事实，并由确定性 CTest 覆盖可用分支。
- 一个真实 Agent Host 通过 stdio 启动 Connector，执行各桥接工具的独特行为，并以各域代表性 Query/Command/Task 覆盖产品链路；公共契约、权限与 Schema 的通用不变量由组件测试覆盖。

## 13. 通过标准

- 权威公共契约、Registry、Editor 发现面与 Connector downstream 的集合关系成立，名称唯一且元数据合法。
- 各业务域的独特语义具有确定性覆盖和真实代表路径；Connector 桥接工具的独特行为均经验证。
- 两套 MCP 主协议、2025-06-18 兼容握手/会话、Editor HTTP、QLocal、Connector stdio、exposure、Profile/Custom、File Guard 和 Admission 全部完成。
- Editor direct 与 Connector 路径的业务结果、错误、历史记录、revision 和 Task 语义一致。
- 24 个 Editor 业务域均完成确定性覆盖和真实代表路径；可见 UI 域另有 GUI 证据，进程联调、真实资格与清理结果形成同一候选的证据。
- 完整 Debug build 和一次全部 CTest 在同一候选、串行约束下完成；Qt 组件轮显式配置可用的
  offscreen 平台插件路径。任一修复后先运行最小复现与受影响域，再重新执行一次完整 CTest。
