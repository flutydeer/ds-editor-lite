# 二期 MCP Server 与 DS Connector Lite 实现报告

## 1. 交付结论与口径

二期已在一期 Automation Facade 基线上形成可由 Agent Host 使用的完整链路：GUI Editor 内置 Streamable HTTP MCP Server，DS Connector Lite 以 stdio 向下游提供 MCP，并通过本机实例观察和 HTTP 与运行中的 Editor 建立连接。公共工具面按业务域组织，Editor 134 项、Connector 6 项，共 140 项。

Editor 的 134 项工具分属 19 个业务域，类型统计为 **37 Q/S + 87 C/S + 10 C/A**。公共工具集与 140 项逐工具的 current、introduced、minimum-compatible 版本均保持为 **1**。

本报告记录当前源码的实现事实。权威工具分母和逐项语义见[公共工具矩阵](public-tool-matrix.md)，验证范围与执行方法见[全量测试大纲](test-outline.md)和[测试执行计划](test-plan.md)，实际运行结果由[测试报告](test-report.md)统一记录。

## 2. 工具集设计原则的实现

当前实现把 GUI 与 MCP 视为同一业务模型的两个入口，边界遵循以下规则：

- 公共能力先按状态所有者归入业务域，再映射到 Profile、Manifest 和 Connector exposure；总线、时间轴、历史记录和播放各自独立。
- 在相应开放层级内，GUI 可完成的原子操作具有对应 MCP 工具；MCP handler 复用同一 Model、Action、历史记录、Task 和文件后端，提交后由既有信号链立即反映到 GUI。
- 历史记录是编辑工具的原子边界。能够整体撤销和重做的同类多对象操作可批量提交；不能共同撤销的属性使用独立工具。例如轨道、片段和总线的标量控制分别提交，已有音符的歌词、语言、长度和发音也分别提交。
- 创建输入限制对象树深度。轨道创建只接收空轨道的名称和颜色；歌声片段创建只接收目标轨道、位置、长度和名称；音符是叶节点，可在创建时携带完整初始属性。
- duplicate、move、resize、split、关键帧和参数锚点以稳定对象 ID 工作，不依赖当前选区、焦点或活动面板。
- Query 不提交业务状态；同步 Command 在模型、历史记录和 revision 完成后返回；异步 Command 返回 Task 句柄，并在最终写回完成后进入成功终态。
- 自动化路径使用显式参数、稳定错误和无对话框的文件/任务流程，适合无人值守调用。

## 3. 实现分层

| 层 | 主要代码 | 已实现职责 |
|---|---|---|
| Wire Contract | `src/libs/AutomationWire` | 134 项公共定义、值域、严格 JSON Schema、Manifest、digest、游标、MCP codec、exposure 与 Schema 兼容 |
| Domain Facade | `src/app/Automation` | 类型化 Query/Command、Dispatcher、CommandCommitter、历史记录、revision、Task 与领域适配 |
| Public Registry | `src/app/Automation/Public` | 134 项 binding、Profile/Custom、动态值、File Guard、Admission、输入/输出校验与 handler 路由 |
| Editor MCP | `src/app/Automation/Mcp` | 双协议请求处理、loopback HTTP、运行时配置、启停与状态发布 |
| Instance Bootstrap | `src/app/Bootstrap` | 单实例身份、`automation.discover/watch`、完整状态快照与广播 |
| Connector | `src/connector` | QLocal watcher、HTTP upstream、stdio downstream、六项桥接工具、exposure、兼容缓存与重连 |
| 产品集成 | `AutomationOption`、`StartupArguments`、`AutomationPage` | 持久设置、CLI override、中文设置页、状态和客户端配置复制 |

公共执行入口的顺序固定为：

```text
contract / handler 查找
  → Profile / Custom 执行期授权
  → input Schema
  → dynamic value validation
  → File Guard
  → playback/inference 专项并发校验
  → Admission lease
  → typed handler / host adapter
  → output Schema
```

异步工具的 Admission lease 会保留到 Task 终态。Registry 在启动时把实际 binding ID 与公共契约 ID 排序后做精确相等校验；完整性门禁失败时，Editor 发布错误状态，不启动缺少 binding 的 MCP listener。

## 4. 134 项域优先公共契约

`src/libs/AutomationWire/PublicToolDefinitions.inc` 是 Editor 工具身份、域、最低 Profile、类型、同步模式和逐工具版本的单一来源。

| 业务域 | category | 数量 |
|---|---|---:|
| 应用 | `application` | 1 |
| 自动化与安全边界 | `automation` | 4 |
| 文档与工程 | `documents` | 8 |
| 格式 | `formats` | 2 |
| 轨道 | `tracks` | 15 |
| 总线 | `bus` | 5 |
| 片段 | `clips` | 16 |
| 音频素材 | `audio_clips` | 5 |
| 声库 | `voices` | 2 |
| Speaker Mix | `speaker_mix` | 13 |
| 音符、歌词、语言、发音与音素 | `notes` | 19 |
| 参数曲线与锚点 | `parameters` | 12 |
| 时间轴 | `timeline` | 5 |
| 历史记录 | `history` | 3 |
| 播放 | `playback` | 8 |
| 导出 | `exports` | 6 |
| 提取 | `extract` | 3 |
| 推理 | `inference` | 4 |
| 异步任务 | `tasks` | 3 |
| **合计** |  | **134** |

`master.*` 的公开 operation ID 保持稳定，descriptor category 为 `bus`。历史记录域使用 `history.get_state/undo/redo`，异步执行实例统一使用 `tasks.list/get/cancel`；`operation_id` 表示能力定义，`task_id` 表示一次执行实例。

每个 Editor 工具均生成或持有：

- 封闭 input/output JSON Schema 和公共 enum codec；
- MCP name、title、description、annotations 与逐工具版本元数据；
- document、revision、历史记录、文件、host、并发、冲突和安全 descriptor；
- dynamic `value_sources` 及其上下文参数；
- input/output Schema digest 和 Manifest descriptor；
- 唯一 Registry binding 与输出 Schema 自检。

Manifest 包含工具集版本、规范化 digest、当前 Profile、host mode、分页 operations、版本化 extensions 和不透明 cursor。分页绑定同一快照，Profile 或 Custom 变化会形成新的可见集合与 digest。

## 5. 领域语义收口

### 5.1 音符叶节点与 voice 归属

`notes.insert` 的 note draft 支持位置、长度、音高、cent shift、歌词、语言、发音、发音候选、音素、音素偏移和换行标记。创建完整音符叶节点不要求 voice context；省略语言时保留“跟随歌手”语义，省略歌词时才按片段、轨道和应用设置推导与 GUI 同源的默认歌词，并把该默认值写入 `resolved_values`。

已有音符的后续编辑保持细粒度：歌词、语言、发音、音素名称、音素偏移、左右边界、移动和量化均为独立 Command。`notes.reset_phoneme_offsets` 接受所选词根，由领域 Facade 计算向右级联闭包，将恢复后会与左词重叠的已编辑右邻一并纳入同一条历史记录；GUI Controller 只在闭包超出选区时显示确认，公共 Registry 直接调用无界面的领域路径，因此 MCP 调用不会弹窗。批量命令先完整预检，再以一条历史记录和一次 revision 提交；合法 no-op 不新增历史记录。

轨道和片段各自拥有 voice 操作：

- 轨道域提供 `tracks.get_voice_context/set_voice/clear_voice`；
- 片段域提供 `clips.get_voice_context/use_track_voice/set_voice/clear_voice`；
- `use_track_voice` 恢复片段对轨道 voice 的继承，`set_voice` 设置片段自己的 voice；
- `set_voice` 必须指定 singer，speaker 可省略或为 `null`；零 speaker 声库保持空 speaker，单 speaker 声库解析唯一项，多 speaker 声库要求显式选择，查询统一以 `speaker: null` 表达空 speaker；
- 声库域只负责 `voices.list/describe`，Speaker Mix 的固定、动态、bypass 和关键帧操作保留在独立域。

### 5.2 创建深度、duplicate 与 NoteTransfer

`tracks.insert` 的公开 Schema 不接受片段树；`clips.insert` 不接受音符、参数、voice 或 mix 树。调用方先创建空容器，再通过所属域的工具填充内容。相对地，`clips.duplicate` 和 `notes.duplicate` 以已有稳定 ID 为来源，执行深复制并整体撤销，不把任意对象树暴露为创建参数。

音符深复制和 GUI 剪贴板现在共用 `NoteTransfer`：

- 捕获完整 note draft，并按所选音符范围切取 Edited/Envelope 参数曲线；
- 粘贴或 duplicate 时按目标起点平移，清除来源对象身份；
- 目标区间先移除被覆盖的参数片段，再合并来源曲线；
- GUI 剪贴板 v2 序列化相同 payload，旧剪贴内容仍可按音符数据读取；
- MCP duplicate 与 GUI copy/paste 因而复用同一深复制语义和提交路径。

### 5.3 同步音频路径修复与持久循环

`audio_clips.relocate` 和 `audio_clips.confirm_path` 是同步 Command。Registry 在调用模型前完成读路径重新授权、音频解码与内容摘要准备，在准备后再次授权，再通过 Project Facade 提交最终路径和音频元数据。成功返回 Mutation，不创建后台 Task，模型信号完成后 GUI 可立即看到结果。

播放状态被拆为两类：

- `playback.play/pause/stop/seek` 只修改瞬时播放状态和 playback state version，不进入历史记录；
- `playback.set_loop/set_loop_enabled/clear_loop` 修改工程持久循环状态，通过 `CommandCommitter` 各自形成一条历史记录，并推进文档 revision；Undo/Redo 使用同一 ActionSequence 恢复循环设置。

持久循环同时校验文档 revision 与调用方可选的 playback state version，避免播放状态和工程状态的竞态写入。

### 5.4 格式、MIDI 与 LibreSVIP

文档打开、导入和批量导入复用 Project Format Registry 与 `IProjectLoadSession`。自动化 host adapter 使用 `interactive=false` 创建 headless session，格式选项由严格 Schema 提供，不打开配置对话框；Task 保留文档 generation 和调用者归因，最终通过 Document Facade 完成换代或单条历史记录导入。

MIDI 路径拆为可复用的两段：

1. `MidiFileParser` 读取并解析为可复用中间数据，不修改 Model；
2. `MidiTrackGenerator` 根据编码、轨道选择、Tempo 和拍号选项生成轨道与时间轴结果，再由 GUI session、批量导入或自动化提交者应用。

`formats.inspect` 与 MIDI 文档导入复用 parser，交互式 GUI、批量导入和 headless session 复用 generator。MIDI capability/preview/start 则复用同一 converter 和公开 option Schema；导出支持 Tempo、拍号和歌词选项，使用临时文件语义的安全写入后提交目标文件。

LibreSVIP 转换抽取为共享 `LibreSVIPConverter`。GUI 转换 Task 与自动化文件服务使用同一外部转换入口、临时输出、超时和错误映射；headless 转换显式提供默认 stdin 回答，因此不会等待交互式输入。格式能力会根据转换器可用状态返回 available 与稳定原因。

### 5.5 文档统计、有界参数查询与 Speaker Mix 预设

公开工具不再暴露聚合对象树式的 `project.get`。`documents.get` 在既有文档状态上增加工程长度、轨道总数与空轨/纯歌声/纯音频/混合轨分类，以及片段总数与歌声/音频分类统计；对象详情继续由轨道和片段域分页查询。L2 的 `documents.list_recent` 只读取应用最近项目记录并报告路径、文件名和存在状态，不打开文件、不切换文档。

`parameters.get` 接受可选半开时间范围和 `max_points`。锚点曲线始终完整返回，点数上限不足以容纳稳定锚点身份时明确失败；采样曲线使用确定性步长降采样，并返回原始点数、实际点数和 `downsampled`。锚点拓扑操作也改为显式边界：`create_anchor_curve` 创建至少含两个锚点且不重叠的新曲线，`insert_anchors` 只写指定既有曲线，`merge_anchor_curves` 只显式合并相邻且不重叠的完整曲线；移动锚点不会隐式跨曲线合并。

Speaker Mix 预设并入 `speaker_mix` 域并开放在 L2。`presets.list/save/delete` 管理应用级预设，不改变文档 revision 或 History；`presets.apply` 把预设值作为一条文档历史记录应用到轨道或片段。查询快照携带可空来源预设和 dirty 标记，后续直接编辑混合会保留来源身份并标记已偏离预设。

## 6. 历史记录、revision 与 Task

`CommandCommitter` 为一次编辑构造一个 `ActionSequence`，成功时记录一条历史记录并推进一次 revision；预检失败、handler 失败、文件失败和 no-op 都不会产生半提交。Undo/Redo 仅在真实导航时推进 revision，save/save-as 更新 savepoint，文档换代重置 generation 与历史基线。

异步工具由 `AutomationTaskManager` 管理 Queued、Running、CancelRequested、Committing 和终态。Task 记录 operation、基准文档、创建者、进度、结果或错误；最终写回前复核 generation、revision、对象和文件授权。Connector 在有副作用请求的结果事实不明确时返回 `outcome_unknown`，不自动重放 Command。

## 7. Profile、Custom、File Guard 与 Admission

`AutomationAccessPolicy` 分离 preset Profile 与 Custom operation 集合。Editor 的 `tools/list`、Manifest、`automation.get_options` 和 Registry 执行期 dispatch 都读取同一策略；工具列表缓存不能替代每次调用时的授权检查。Connector exposure 只决定下游可见面，Editor 仍执行最终授权。

`L3` 在产品文案中称为“进阶控制”（Advanced Control），表示在 L2 之上按明确范围增加部分 GUI 自动化操作与设置项更改，而非不受限制的完全控制；自动化/MCP 自身的配置和运行生命周期明确排除在外。

Allowed Read Folders 与 Allowed Write Folders 是自动化文件工具的规范路径 allowlist：前者约束打开、导入、检查和读取素材等操作，后者约束保存、导出等写操作。它们不表示本机进程权限，也不改变非文件工具的能力。`AutomationFileGuard` 还分离持久根与会话 grant，处理路径组件边界、相对路径、相邻前缀、链接/重解析点和未创建输出的最近现存父目录；授权后、实际 I/O 前会再次检查 canonical 目标。

业务 Admission 同时限制全局、逻辑客户端、后台 Task、并发域和令牌桶。业务层与 HTTP Transport 的每逻辑客户端在途上限均为 32，突发令牌容量均为 64，使正常握手和基线查询后仍可提交完整 32 路并发；Connector 直接并发转发，不增加串行排队。超限请求在 handler 前返回稳定错误，不进入业务层；RAII lease 在成功、失败或取消时释放，异步 lease 延续到 Task 终态。HTTP Transport 另有 global 与 peer 限制。

## 8. MCP 双协议与 Editor HTTP

Wire 和 Editor MCP 同时实现两套主协议：

- **2025-11-25**：`initialize → notifications/initialized`，随后调用 `ping/tools/list/tools/call`；
- **2026-07-28**：`server/discover` 与逐请求 `_meta`，随后调用 `ping/tools/list/tools/call`，请求不经过 `initialize`。

同一 legacy 入口接受客户端以 **2025-06-18** 发起握手，回显协商版本并完成 initialize/initialized 生命周期。2026-07-28 请求使用 `MCP-Protocol-Version`、`Mcp-Method`，`tools/call` 还校验 `Mcp-Name`；协议版本决定现代或 legacy 的结果塑形、server metadata 与 structured content 表达。

`McpHttpServer` 仅监听数值 loopback 地址，固定路由为 `POST /mcp`。Transport 校验本机地址、Host、Origin、Content-Type、Accept、请求元数据、JSON 资源上限、请求/响应大小和 deadline；notification 接受后返回 HTTP 202。有序停止先关闭 admission，再等待或终止在途工作并释放 listener。

## 9. QLocal Bootstrap 与 DS Connector Lite

Editor 在既有单实例协议上增加 `automation.discover` 和 `automation.watch`。长度前缀 JSON framing 承载完整快照；watch 建立时立即返回当前状态，Editor 启用/禁用 MCP、监听器 ready、换端口、报错或退出时广播新快照。Watcher 数量、帧尺寸、待写帧和累计字节均有界，慢读或异常断开只清理对应连接。

Connector 可先于 Editor 启动。`BootstrapWatcher` 只作为 QLocal 客户端观察当前实例，使用实例身份与 handshake epoch 丢弃旧结果，并以有界退避重连。上游优先使用 2026-07-28 `server/discover`，失败时进入 2025-11-25 initialize 流程，并接受服务端协商到 2025-06-18。协议建立后，Connector 完整分页读取 `tools/list`，再用一次 `automation.get_status` 取得 toolset version、Manifest digest、Profile 和 host；完整 `automation.get_manifest` 不再进入常规握手，只保留为按需诊断与审计能力。

Connector 固定提供六个桥接工具：

| 工具 | 职责 |
|---|---|
| `connector.get_status` | 返回 Connector、Bootstrap、Editor、协议、Manifest、兼容与 exposure 的缓存事实 |
| `connector.reconnect` | 主动重建观察和上游握手 |
| `editor.tools.list` | 分页列出 exposure 后的实际 Editor 工具 |
| `editor.tools.search` | 按 ID、标题、说明和域搜索实际工具 |
| `editor.tools.describe` | 返回实际 Schema、版本、权限、兼容与可用性 |
| `editor.tools.invoke` | 按 Editor 当前真实 Schema 调用获准目标 |

六项桥接工具的版本三元组均为 1。Connector 还携带构建时已知的 134 项 Editor 类型化描述；downstream 工具面由六项桥接工具与 exposure 选择的类型化工具组成，并在单个 Connector 生命周期内保持描述稳定。

Exposure 默认 L1，支持 `l0/l1/l2/l3`、`id:`、`category:`、`prefix:`、include 和 exclude；同一选择同时约束类型化 wrapper 和泛化 list/search/describe/invoke。兼容计算先检查双方逐工具版本门槛；完全相同的 input/output Schema 直接判定兼容，存在差异时再执行方向性包含检查。`tools/list` 的 namespaced `_meta` 同时携带 `kind` 与 host availability，确保未知泛化工具仍保留 Command outcome 和 host 过滤语义。Editor 的分页 cursor 摘要只规范化工具集版本与有序工具 ID，页面内容仍缓存完整 descriptor；Connector 的 cursor 摘要使用 Manifest digest、状态、exposure 与有序工具名，未知或 Schema 有差异的 descriptor 仍完整纳入。正常握手只重建一次 Connector 缓存；完整 Manifest 按访问策略和 host mode 缓存，Profile 或 Custom 集合变化时自动失效。Manifest digest 使用完整非 Schema 元数据和逐项 Schema digest，避免再次规范化所有 Schema 原文；Connector 的预期摘要按 Editor 实际 Profile、host mode 以及 Custom 已知 ID 集合动态构造，不再固定使用 L2 摘要。`connector.get_status` 只读取已经计算的兼容缓存和当前观察状态，不在每次查询时重建整套结果。

Downstream stdio 支持两套主协议及 2025-06-18 兼容握手，具有有界 reader/writer 队列、并发请求 ID 映射、取消、超时、EOF、broken pipe 和 backpressure 处理。Windows 非阻塞管道按 4 KiB 分块写出大响应，停滞计时只在连续无进度时触发；stdout 只输出 MCP 帧，诊断写入 stderr。

## 10. 设置页、配置复制与 CLI

Automation 设置已进入选项对话框导航，并使用与其他选项页一致的主题图标。页面、运行状态和 19 个域的显示文本已补齐中文翻译，其中 `history` 显示为“历史记录”。Custom 工具不再使用平铺清单，而是按公共契约的领域建立独立卡片；每组默认收起，支持独立展开/收起、启用计数和整组启停。单工具开关仍直接写入同一 `customPermissions` 集合，组级操作只批量更新这些既有权限，不引入第二套配置。

持久设置的安全默认值为 MCP 关闭、L1、空的额外读写根和非零控制端口。配置中缺少有效端口时在动态私有范围生成一个端口，保存后后续启动继续使用该值；只有用户点击“刷新”或直接编辑才改变端口。页面不提供固定/随机下拉框，“刷新”按钮与端口输入框位于同一行并始终可操作。

设置页始终生成两份可复制配置：

- Connector stdio 配置包含 `type`、Connector command 与 exposure 参数；
- Editor Streamable HTTP 配置包含 `type` 与基于当前生效端口或持久端口生成的 URL。

两份内容都是单个 server entry，不带外层 `mcpServers`。即使 MCP 处于 disabled、starting 或 error，文本框和复制按钮仍有稳定内容。页面展示真实的运行状态、当前 endpoint 和最近错误，不设置“本机进程访问”栏目；读写根说明明确其作用是自动化文件路径 allowlist。

Editor CLI 支持：

```text
--mcp | --no-mcp
--control-port <1..65535>
--automation-profile l1|l2|l3|custom
```

CLI override 只影响本次运行，并在设置页显示来源，不改写持久设置。Connector CLI 支持 `--exposure-profile`、`--include-tool`、`--exclude-tool`、`--help` 和 `--version`。

## 11. 长期保护与验证承接

当前测试代码对以下实现不变量建立了自动保护：

- 134 项稳定 tool name、19 个域、类型、Profile、Schema 和逐工具版本；
- 134 项 Contract、Registry、Manifest、Editor `tools/list` 与 Connector 已知描述的集合相等；
- 六个 Connector 桥接工具和 exposure 后的 downstream 集合；
- 历史记录原子边界、创建深度、音符叶节点、轨道/片段 voice 和持久循环；
- NoteTransfer 的音符与参数曲线深复制、GUI 剪贴板 round-trip；
- MIDI headless 解析/生成、LibreSVIP 共享转换、文件授权与异步写回；
- Profile/Custom、File Guard、Admission、Manifest、游标和 Schema 兼容；
- 2025-11-25 与 2026-07-28 两套主协议、2025-06-18 兼容握手、HTTP、QLocal、Connector stdio 和真实进程路径；
- Automation 设置持久化、CLI override、端口、配置 JSON、Custom 领域分组与中文界面。

实现阶段已完成 Debug clean 全目标构建。定向组件、真实进程、GUI 和连续三轮完整 CTest 的最终证据、修复轨迹及验收判断集中写入测试报告，避免实现事实与运行结论混为一处。
