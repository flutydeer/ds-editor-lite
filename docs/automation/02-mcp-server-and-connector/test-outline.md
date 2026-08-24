# 二期 MCP Server 与 DS Connector Lite 全量测试大纲

## 1. 目标与分母

本大纲验证二期从公共契约到真实 GUI Editor/Connector 联调的完整链路。冻结分母为：

```text
Editor 公共工具：127
Connector 桥接工具：6
总工具面：133
```

Editor 的 127 项按 19 个业务域追踪；Connector 的 6 项单独追踪。每个工具至少关联 descriptor/Schema、发现授权、Registry binding、Editor MCP、Connector 路径中适用的测试证据。Query、同步 Command、异步 Command、文件、动态值和 History 维度按工具类型展开。

测试层次：

| 层次 | 验证对象 |
|---|---|
| 静态契约 | ID、域、Profile、版本、Schema、descriptor、集合相等 |
| 单元 | codec、validator、Manifest、Policy、Guard、Admission、exposure、兼容算法 |
| 组件 | MCP parser/router、HTTP、QLocal、stdio framing、上下游 client/server |
| 进程 | Editor、Connector、单实例、重连、运行时启停、多 Connector |
| 业务域 | 127 项工具与类型化 Facade、History、revision、Task、文件语义 |
| GUI | Automation 设置页、可见编辑结果、Undo/Redo、运行状态 |
| 资格 | 真实格式、声音、推理、播放与 Agent Host 环境 |

## 2. 集合、版本与一期校正

### 2.1 ID 与域

- `P2-TOOL-001～127` 连续、唯一，operation ID 唯一。
- `P2-CONN-001～006` 连续、唯一，六个桥接 ID 与固定定义一致。
- Editor Contract、Registry binding、Manifest、Editor `tools/list`、Connector 已知类型化描述和矩阵的 127 个 ID 精确相等。
- Connector bridge definitions、downstream 固定桥接面和矩阵的 6 个 ID 精确相等。
- 19 个域的数量与矩阵一致；`master.*` 的 category 为 `bus`；历史记录域严格包含三项。
- Query/Command、同步模式与最低 Profile 逐项相等。
- 旧复合属性入口由源码守卫与 ID 反例保证无法成为现行公共契约。

### 2.2 版本

- `toolsetVersion` 精确为 1。
- 133 个工具的 current、introduced、minimum compatible version 均精确为 1。
- Manifest 中每项 Editor operation 的版本 descriptor 与契约一致。
- 规范化 Schema/descriptor 内容在键顺序、空白、locale 和构建机器变化时产生稳定 digest。
- 修改公共契约 fixture 会改变相应 Schema digest 与 Manifest digest。

### 2.3 Task 术语校正

- 集中 ID、Catalog、Facade、Wire 与测试使用 `tasks.list/get/cancel`。
- `operation_id` 指向能力，`task_id` 指向执行实例。
- 任务状态、进度、终态、取消、generation、幂等和创建者归因保持一致。
- 一期受影响 Catalog 与行为集合在实现完成后重新快照，最终计数由本轮执行记录回填。

## 3. Wire Contract、Schema 与 Manifest

### 3.1 Schema 与 codec

- 127 个 Editor 工具的 input/output Schema 均可通过 meta-schema 检查。
- 业务对象为封闭结构；未知字段、错误类型、非法枚举、NaN/Inf、整数溢出、非法 UUID 和超限集合在 handler 前失败。
- 无参工具只接受空 object；必填字段、oneOf 分支、nullable 和分页字段严格执行。
- 公共 enum/值域在 C++ codec、Schema、动态候选和测试语料中来自同一声明。
- 轨道、片段、总线的细粒度标量命令各自具有单 History 与 revision-check descriptor。
- shallow track/clip draft 与完整 note leaf draft 的合法/非法形状分别覆盖。
- duplicate、move、resize、split、keyframe 与 anchor 使用稳定 ID，并覆盖重复、失效和跨 owner ID。

### 3.2 动态值与首次调用

- `automation.get_status` 零参数返回 Editor、Manifest 和当前 document/window 摘要。
- `value_sources` 指向可达查询，最低 Profile 与 host availability 合法。
- `automation.get_options` 对目标字段、数组通配路径和依赖上下文正确解析。
- 目标隐藏、Custom 关闭、exposure 过滤和 host 能力变化时，动态候选遵守同一授权结果。
- voices、parameters、formats、exports、extract、inference 与 file access 返回的值可直接用于对应命令。
- 动态数值范围同时验证最小值、最大值、step 与 unavailable reason。

### 3.3 Manifest 与游标

- 根级 `toolset_version`、digest、profile、host、operations、extensions、next cursor 形状完整。
- 每项 descriptor 包含 domain/category、kind、sync mode、Schema、value sources、Profile、History/file/host/concurrency/conflict/safety 与版本。
- 排序确定；首页、中间页、末页、零/最大 limit、过期/伪造/跨上下文 cursor 行为稳定。
- Manifest operation 集合与当前 `tools/list` 相等。
- Profile/Custom 更新后新 Manifest 与 digest 反映新可见集合；旧 cursor 不能跨快照使用。

## 4. 127 项 Editor 工具的域覆盖

| 域 | 数量 | 重点验证 |
|---|---:|---|
| 应用 | 1 | 产品/版本/平台/build 字段、无副作用 |
| 自动化与安全 | 4 | 状态、Manifest、options、文件授权与权限继承 |
| 文档与工程 | 8 | new/open/save/save-as/import/batch、未保存策略、换代与 savepoint |
| 格式 | 2 | 用途过滤、可用性、inspect 诊断与 plan digest |
| 轨道 | 15 | 列表/详情、浅层创建、删除/移动、标量命令、语言与声音上下文 |
| 总线 | 5 | `bus` 归属、四个标量命令、History/revision |
| 片段 | 16 | 筛选、浅层创建、duplicate、几何、标量命令和声音继承 |
| 音频片段 | 5 | 元数据、导入/batch、relocate/confirm、授权重检与任务 |
| 声音 | 2 | singer/speaker/language/G2P/mix 能力与稳定引用 |
| Speaker Mix | 9 | fixed/dynamic/bypass、权重归一化、关键帧稳定 ID |
| 音符 | 18 | 查询/搜索/创建/duplicate/几何、歌词、语言、发音、音素与填充 |
| 参数 | 10 | capability、draw/anchor、replace/draw/erase/bake 与批量锚点 |
| 时间线 | 5 | Tempo/拍号排序、零点锚、单 History |
| 历史记录 | 3 | 状态、Undo/Redo、分支与 savepoint |
| 播放 | 8 | state version、状态转换、seek、loop 与并发域 |
| 导出 | 6 | MIDI/audio capability、preview、写授权、任务与清理 |
| 提取 | 3 | 来源/模型/语言能力、pitch/MIDI 任务与写回 |
| 推理 | 4 | scope/stage/provider/device/model、状态、任务和 reset |
| 任务 | 3 | 分页/筛选/详情、取消点、终态与创建者归因 |

每个域都执行：

1. 空状态、最小状态和代表性有数据状态；
2. 有效输入、每类边界输入和 schema-invalid 输入；
3. 旧 document/revision、未知对象、owner/type 不匹配和 host capability unavailable；
4. Query 的零 Model/History/revision/Task/文件副作用；
5. Command 的成功、no-op、validate-only、失败原子性、单 History 和 revision；
6. Editor direct 与 Connector 路径的归一化结果和稳定错误等价；
7. GUI 可见变更与 Undo/Redo 的领域一致性。

## 5. Query、同步 Command 与异步 Command

### 5.1 Query

- 结果满足 output Schema，`structuredContent` 与 TextContent 表达等价。
- 排序、分页、Unicode、空集合、长文本和边界数字确定。
- Query 不推进 revision、History、state version 或 Task 状态。
- 能力查询同时表达 supported、available 和 unavailable reason。

### 5.2 同步 Command

- 执行前校验 document、expected revision、对象 owner/type、动态值、File Guard 和 Admission。
- 成功结果含 previous/current、changed、affected/created objects、resolved values、presentation effects 与 warnings。
- 合法 no-op 返回 `changed=false`，History 与 revision 保持原值。
- validate-only 执行完整校验，且不产生 ID、Task、文件写入、History、revision 或业务通知。
- 批量命令先完整预检，再以一条 History entry 和一次 revision 提交。
- handler、I/O、Schema 编码和 host adapter 失败不产生半提交。

### 5.3 异步 Command 与 Task

- 接受响应只创建一个 TaskId，并记录 operation、base document、创建者和初始进度。
- Queued、Running、CancelRequested、Committing 和各终态遵守允许迁移。
- 排队/运行取消、重复取消、终态取消和提交点取消分别覆盖。
- Editor 重启、文档 generation 更换、revision 前进、目标消失和晚到结果不能写入错误文档。
- 最终成功在业务写回、History/revision 和信号完成后发布。
- MCP/Connector 断线后已接受任务仍可通过 `tasks.list/get/cancel` 观察和管理。
- 结果未知场景通过 idempotency、revision 或 Task 查询确认，Connector 不重放有副作用请求。

## 6. Profile、Custom 与 exposure

- Meta 精确 4 项；L1 累积精确 89 项；L2 与 L3 累积均精确 127 项。
- `selectedProfile` 与 `customPermissions` 独立持久化，preset 切换保持 Custom 内容。
- Custom 的单项、分类、空集合、新 operation 安全默认和坏配置恢复覆盖。
- Editor `tools/list`、Manifest、options 和执行期 Registry 使用同一策略。
- 列表后改变 Profile/Custom 时，实际调用按最新策略判定。
- Connector exposure profile `l0/l1/l2/l3`、include、exclude 和 selector 规范化覆盖。
- `id:`、`category:`、`prefix:`、裸 ID、重复 selector、pending selector 和 exclude 优先级覆盖。
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

### 7.2 Admission 与公平

- 业务层 global、client、background-task、domain 和 token bucket 上限。
- HTTP 层 global、peer、logical-client 并发与速率。
- 成功、失败、取消、deadline、断线、disable 和 shutdown 都释放租约与计数。
- fake clock 验证 token 恢复、容量边界和时间跳变。
- 多 Connector 压力下单客户端无法长期占满 Dispatcher。
- client identity 只用于归因和配额，不改变 Profile 或 Task 可见性。

## 8. MCP 三版本与 Editor HTTP

### 8.1 协议生命周期

- `2025-06-18`：`initialize → notifications/initialized → ping/tools/list/tools/call`。
- `2025-11-25`：`initialize → notifications/initialized → ping/tools/list/tools/call`。
- `2026-07-28`：`server/discover → ping/tools/list/tools/call`，逐请求 `_meta`。
- 协商、支持版本列表、client/server info、client capabilities 和 tools capability 精确。
- 请求版本决定结果形状；2025 与 2026 的 metadata、structuredContent 与兼容文本分别验证。
- request ID、notification、parse/invalid/method/params/internal error 与业务错误分层。

### 8.2 Header 与 transport

- `MCP-Protocol-Version`、`Mcp-Method`、`Mcp-Name` 与 body 镜像一致。
- header Base64 sentinel、Unicode、控制符、前后空格、重复 header 和大小写规则。
- 仅 `POST /mcp`，notification 返回 202；method、Content-Type 和 Accept 行为符合约定。
- 单消息、非法 JSON、数组/batch、错误 id、超大 body、深度、节点、响应体和 deadline。
- listener 仅为 `127.0.0.1`；Host、Origin、DNS rebinding、CORS 与远端地址拒绝矩阵。
- shutdown 先停止 admission，再结束在途请求并释放端口。

### 8.3 Tools

- 三版本 `tools/list` 对当前 127 集合、顺序、分页、Schema、annotations 和 server metadata 进行比对。
- `tools/call` 覆盖缺 name/arguments、未知工具、权限变化、Schema 错误、业务错误与成功结果。
- 127 项均有一次 schema-valid Registry 可达性测试；各域选择代表工具完成真实成功与回滚语料。
- output Schema 自检失败转换为内部错误，且不产生第二次业务提交。

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
- writer queue 的部分写、backpressure、停滞 deadline 和确定性关闭。
- downstream request ID 到 upstream ID 的映射支持并发乱序、取消和 timeout。

### 10.2 三版本上游/下游

- Downstream 分别执行两个 2025 initialize 生命周期与 2026 discover 生命周期。
- Upstream 优先现代发现，并覆盖 2025-11-25 初始化及协商到 2025-06-18。
- 握手成功后协议版本固定，工具分页与 Manifest 分页均完整读取。
- 重复 ready 合并、尾随刷新、有界指数退避、手动 reconnect 和目标变化覆盖。
- instance/endpoint 改变后旧 request、Manifest、cursor 和兼容缓存失效。

### 10.3 六个桥接工具与兼容

- `connector.get_status` 各字段与 Bootstrap、HTTP、Manifest、compatibility 和 exposure 事实相等。
- `connector.reconnect` 的并发与重复调用合并。
- `editor.tools.list/search/describe/invoke` 对分页、搜索、Schema、可用性、过滤和调用授权一致。
- 127 个类型化 Editor wrapper 与六个桥接工具形成 133 项 L2 downstream 集合。
- 双向 minimum-compatible 门槛、digest fast path、input 子集、output 子集和未知关键字覆盖。
- compatible、compatible subset、contract incompatible、tool unavailable、profile blocked、host unavailable 分开报告。
- upstream transport error、MCP error、invalid response、timeout、cancel 与 outcome unknown 分开报告。

## 11. 设置、CLI 与运行时生命周期

- Automation 配置模型的 enabled、端口、Profile、Custom、读写根默认与 round-trip。
- `--mcp`、`--no-mcp`、`--control-port`、`--automation-profile` 的合法与冲突组合。
- CLI override 优先级、运行期生效和持久设置保持。
- Runtime enable、端口冲突、ready、disable、换端口、错误恢复和退出顺序。
- Profile/Custom/roots 在运行中变化时，Editor list/dispatch 与 Connector 状态及时更新。
- 设置页的持久值、生效值、覆盖来源、endpoint、错误、Custom 和根目录控件通过 Computer Use 验证。
- Automation 调用全过程监测模态对话框、主线程阻塞和 UI 假死。

## 12. 进程联调、多 Connector 与 GUI

### 12.1 进程联调

- Connector 先启动，再经历 Editor 启动、MCP enable 和 ready。
- Editor 先 ready，Connector 首次 watch 后完成协议、tools 和 Manifest 握手。
- Editor direct HTTP 与 Connector stdio 复用同一 Meta/L1/L2 业务语料并比对结果。
- open/save/import/export、音频片段、推理、Task、播放与 History 使用隔离工作区。
- 两至八个 Connector 并发查询、编辑、任务与重连；覆盖 revision conflict、公平性和状态隔离。
- 一个 Connector 退出、崩溃、慢读或触发限流时，Editor 与其他 Connector 继续服务。
- Editor stop/restart、instance ID 与 endpoint 更换后自动建立新链路。

### 12.2 GUI 与真实资格

- 设置页默认、修改、CLI 覆盖和状态展示。
- MCP 工具编辑后 GUI 立即呈现；GUI 编辑后 MCP 查询与 revision 立即更新。
- 轨道、总线、片段、音符、参数、Speaker Mix、时间线和 History 的可见变更。
- Connector mutation 后使用 GUI Undo/Redo，验证单 History 粒度。
- 播放、文档、导入、导出、可用声音与推理环境的可见行为。
- 一个真实 Agent Host 通过 stdio 启动 Connector，执行 status、list、describe、Query、Command 与 Task。
- 环境资格按实际具备的 codec、声音、模型和音频设备逐项记录，结果与确定性测试分母分开。

## 13. 通过标准

- 127 + 6 = 133 的工具集合、追踪号、域和版本不变量全部成立。
- 127 个 Editor 工具均有 Contract、Registry、权限和适用调用证据；六个 Connector 工具逐项验证。
- 三个 MCP 协议、Editor HTTP、QLocal、Connector stdio、exposure、兼容、Profile/Custom、File Guard 和 Admission 全部完成。
- Editor direct 与 Connector 路径的业务结果、错误、History、revision 和 Task 语义一致。
- 进程联调、GUI、真实资格与清理结果形成本轮证据。
- 完整 Debug build 和全部 CTest 在同一候选、串行约束下连续三轮完成；任一修复后从受影响域回归并重新开始三轮计数。
