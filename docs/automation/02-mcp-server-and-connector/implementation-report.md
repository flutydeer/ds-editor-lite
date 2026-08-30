# 二期 MCP Server 与 DS Connector Lite 实现报告

## 1. 交付结论与口径

二期形成了可由 Agent Host 使用的完整自动化链路：GUI Editor 内置 Streamable HTTP MCP Server，DS Connector Lite 以 stdio 向下游提供 MCP，并通过本机实例观察和 HTTP 与运行中的 Editor 建立连接。公共工具面按业务域组织，Editor 176 项、Connector 6 项，共 182 项。

Editor 的 176 项工具分属 24 个业务域，类型统计为 **41 Q/S + 122 C/S + 13 C/A**。
最低 Profile 分布为 L0 4 项、L1 85 项、L2 43 项、L3 44 项；累积可见数量分别为
4、89、132、176。公共工具集 `toolset_version` 与 182 项工具的
`minimum_toolset_version` 均为 **1**。

本报告记录当前源码的实现事实。权威工具分母和逐项语义见[公共工具矩阵](public-tool-matrix.md)，验证范围与执行方法见[全量测试大纲](test-outline.md)和[测试执行计划](test-plan.md)，实际运行结果由[测试报告](test-report.md)统一记录。

## 2. 工具集设计原则的实现

当前实现把 GUI 与 MCP 视为同一业务模型的两个入口，边界遵循以下规则：

- 公共能力先按状态所有者归入业务域，再映射到 Profile 和 Connector exposure；总线、时间线、历史记录和播放各自独立。
- 在相应开放层级内，GUI 可完成的原子操作具有对应 MCP 工具；MCP handler 复用同一 Model、Action、历史记录、Task 和文件后端，提交后由既有信号链立即反映到 GUI。
- 历史记录是编辑工具的原子边界。能够整体撤销和重做的同类多对象操作可批量提交；不能共同撤销的属性使用独立工具。例如轨道、剪辑和总线的标量控制分别提交，已有音符的歌词、语言、长度和发音也分别提交。
- 创建输入限制对象树深度。轨道创建只接收空轨道的名称和颜色；歌声剪辑创建只接收目标轨道、位置、长度和名称；音符是叶节点，可在创建时携带完整初始属性。
- duplicate、move、resize、split、关键帧和参数锚点以稳定对象 ID 工作，不依赖当前选区、焦点或活动面板。
- Query 不提交业务状态；同步 Command 在模型、历史记录和 revision 完成后返回；异步 Command 返回 Task 句柄，并在最终写回完成后进入成功终态。
- 自动化路径使用显式参数、稳定错误和无对话框的文件/任务流程，适合无人值守调用。

## 3. 实现分层

| 层 | 主要代码 | 已实现职责 |
|---|---|---|
| Wire Contract | `src/libs/AutomationWire` | 176 项公共定义、值域、严格 JSON Schema、游标、MCP codec、exposure 与工具集版本 |
| Domain Facade | `src/app/Automation` | 类型化 Query/Command、显式 Dispatcher 路由、CommandCommitter、历史记录、revision、Task 与领域适配 |
| Public Registry | `src/app/Automation/Public` | 176 项 binding、Profile/Custom、动态值发现、File Guard、Admission、输入校验与 handler 路由；输出契约由确定性测试验证 |
| Editor MCP | `src/app/Automation/Mcp` | 双协议请求处理、loopback HTTP、运行时配置、启停与状态发布 |
| Instance Bootstrap | `src/app/Bootstrap` | 单实例身份、`automation.discover/watch`、完整状态快照与广播 |
| Connector | `src/connector` | QLocal watcher、HTTP upstream、stdio downstream、六项桥接工具、exposure、兼容缓存与重连 |
| 产品集成 | `AutomationOption`、`StartupArguments`、`AutomationPage` | 持久设置、CLI override、中文设置页、状态和客户端配置复制 |

内部 Automation 不设 `OperationCatalog` 或 `OperationDescriptor` 注册表。Dispatcher 直接按
集中 `OperationId` 显式路由，能力列表由 `OperationIds::all()` 返回；编译期类型和行为测试负责
保护路由完整性。幂等是显式 opt-in：只有支持该能力且请求实际携带 `idempotency_key` 时才计算
请求指纹并进入幂等存储，不带 key 的调用不哈希、不创建幂等记录。

公共执行入口的顺序固定为：

```text
contract / handler 查找
  → Profile / Custom 执行期授权
  → input Schema
  → File Guard
  → playback/inference 专项并发校验
  → Admission lease
  → typed handler / host adapter
  → output 编码
```

异步工具的 Admission lease 会保留到 Task 终态。动态 `value_sources` 指向同层级可达的领域查询，
正常 invocation 不自动回查 provider。Registry 在启动时把实际
binding ID 与公共契约 ID 排序后做精确相等校验；完整性门禁失败时，Editor 发布错误状态，不启动
缺少 binding 的 MCP listener。

## 4. 176 项域优先公共契约

`src/libs/AutomationWire/PublicToolDefinitions.inc` 是 Editor 工具身份、域、最低 Profile、类型、
同步模式和逐工具 `minimum_toolset_version` 的单一来源。

| 业务域 | category | 数量 |
|---|---|---:|
| 应用 | `application` | 5 |
| 文档与工程 | `documents` | 8 |
| 格式 | `formats` | 2 |
| 轨道 | `tracks` | 14 |
| 总线 | `bus` | 5 |
| 剪辑 | `clips` | 15 |
| 音频剪辑 | `audio_clips` | 5 |
| 声库 | `voices` | 2 |
| Speaker Mix | `speaker_mix` | 13 |
| 音符、歌词、语言、发音与音素 | `notes` | 19 |
| 参数曲线与锚点 | `parameters` | 12 |
| 时间线 | `timeline` | 5 |
| 历史记录 | `history` | 3 |
| 播放 | `playback` | 8 |
| 导出 | `exports` | 6 |
| 提取 | `extract` | 3 |
| 推理 | `inference` | 4 |
| 异步任务 | `tasks` | 3 |
| 工作区布局 | `workspace` | 2 |
| 轨道面板 | `track_panel` | 7 |
| 剪辑编辑器 | `clip_editor` | 16 |
| 设置 | `settings` | 9 |
| 包信息 | `packages` | 3 |
| 歌词规则 | `lyric_rules` | 7 |
| **合计** |  | **176** |

`master.*` 的公开 operation ID 保持稳定，descriptor category 为 `bus`。历史记录域使用 `history.get_state/undo/redo`，异步执行实例统一使用 `tasks.list/get/cancel`；`operation_id` 表示能力定义，`task_id` 表示一次执行实例。

每个 Editor 工具均生成或持有：

- 封闭 input/output JSON Schema 和公共 enum codec；
- MCP name、title、description、标准 annotations 与 `minimum_toolset_version`；
- 执行所需的 Profile、host 和 file access 元数据；
- dynamic `value_sources` 及其上下文参数；
- 唯一 Registry binding；输出 Schema 由确定性契约测试验证，运行时不逐次 assert。

标准 MCP `tools/list` 提供运行时工具目录和完整 Schema，`application.get_status` 提供工具集版本、
当前 Profile、host mode 及文档/窗口摘要。标准 MCP annotations 保持在工具 descriptor 中。
Cursor 对调用方透明地延续分页，内部只将 `context + snapshot + offset` 编码为 base64url 并校验
上下文/快照，不使用密钥或 HMAC。

## 5. 领域语义收口

### 5.1 L0 生命周期闭环

`application.request_exit` 与 `application.request_restart` 复用一期的类型化生命周期 Facade、
文档工作流和产品 Restarter，并作为 L0 固有工具同时出现在 Editor 与 Connector 工具面。输入只含
可选 `discard_changes`：默认值 `false` 在脏工程上返回字段路径为 `discard_changes` 的 `busy`，
显式传 `true` 才允许丢弃改动；繁忙工作流同样返回 `busy`。公共路径在进入状态机前完成这些判断，
不取消正在执行的会话，也不触发保存确认或忙碌提示。GUI 发起的关闭仍沿用交互式保存询问。

成功响应包含 `accepted`、`action` 和实际请求的 `discard_changes`，随后由既有关闭链路等待后台
任务结束；重启仅以当前可执行文件和原参数重新启动 Editor。两项为 Destructive、Non-idempotent、
Closed World Command，不提供 `force`、`validate_only`、幂等键或任意进程启动入口。

### 5.2 音符叶节点与 voice 归属

`notes.insert` 的 note draft 支持位置、长度、音高、cent shift、歌词、语言、发音、发音候选、音素、音素偏移和换行标记。创建完整音符叶节点不要求 voice context；省略语言时保留“跟随歌手”语义，省略歌词时才按剪辑、轨道和应用设置推导与 GUI 同源的默认歌词，并把该默认值写入 `resolved_values`。

已有音符的后续编辑保持细粒度：歌词、语言、发音、音素名称、音素偏移、左右边界、移动和量化均为独立 Command。`notes.reset_phoneme_offsets` 接受所选词根，由领域 Facade 计算向右级联闭包，将恢复后会与左词重叠的已编辑右邻一并纳入同一条历史记录；GUI Controller 只在闭包超出选区时显示确认，公共 Registry 直接调用无界面的领域路径，因此 MCP 调用不会弹窗。批量命令先完整预检，再以一条历史记录和一次 revision 提交；合法 no-op 不新增历史记录。

轨道和剪辑各自拥有 voice 操作与回读：

- `tracks.get` 在轨道属性和统计之外返回自有/有效 voice 与默认语言上下文；轨道域另提供
  `tracks.set_voice/clear_voice`；
- `clips.get` 对歌声剪辑同时返回 own/effective voice、继承来源和有效默认语言；剪辑域另提供
  `clips.use_track_voice/set_voice/clear_voice`；
- `use_track_voice` 恢复剪辑对轨道 voice 的继承，`set_voice` 设置剪辑自己的 voice；
- `set_voice` 必须指定由 `package_id`、`package_version` 和 `singer_id` 共同组成的 singer 引用；同 ID 并存版本按完整引用精确解析。L1/L2 调用方通过 `voices.list/describe` 即可获得该引用及对应 speaker，不需要访问 L3 `packages.*`；speaker 可省略或为 `null`，零 speaker 声库保持空 speaker，单 speaker 声库解析唯一项，多 speaker 声库要求显式选择，查询统一以 `speaker: null` 表达空 speaker；
- 声库域只负责 `voices.list/describe`，Speaker Mix 的固定、动态、bypass 和关键帧操作保留在独立域。

### 5.3 创建深度、duplicate 与 NoteTransfer

`tracks.insert` 的公开 Schema 不接受剪辑树；`clips.insert` 不接受音符、参数、voice 或 mix 树。调用方先创建空容器，再通过所属域的工具填充内容。省略剪辑长度时按所在小节起算四个完整小节，使用宽整数累计并在提交前拒绝超出模型 tick 范围的终点。相对地，`clips.duplicate` 和 `notes.duplicate` 以已有稳定 ID 为来源，执行深复制并整体撤销，不把任意对象树暴露为创建参数。

音符深复制和 GUI 剪贴板现在共用 `NoteTransfer`：

- 捕获完整 note draft，并按所选音符范围切取 Edited/Envelope 参数曲线；
- 锚点曲线只在所选音符与曲线的交集内按固定步长采样，以 64 位整数计算点数并受公共曲线点数上限约束，不会为局部复制展开远超所选范围的整条锚点曲线；
- 粘贴或 duplicate 时按目标起点平移，清除来源对象身份；
- 目标区间先移除被覆盖的参数片段，再合并来源曲线；
- GUI 剪贴板 v2 序列化相同 payload，旧剪贴内容仍可按音符数据读取，越界或无效的外部音符几何会整包丢弃；
- MCP duplicate 与 GUI copy/paste 因而复用同一深复制语义和提交路径。

### 5.4 音频路径修复与持久循环

`audio_clips.relocate` 和 `audio_clips.confirm_path` 是异步 Command。Registry 先规范化并授权读路径，再返回 Task；Host Adapter 在线程池中以一次顺序读取同时创建摘要和临时快照，并只解码该快照。任务不再为防止接单后路径变化而重复授权或哈希原文件；进入提交点前仍校验文档 revision 与目标剪辑。最终路径和音频元数据通过 Project Facade 形成一条历史记录，Task 仅在模型信号完成后进入成功终态，GUI 随即反映结果。

播放状态被拆为两类：

- `playback.play/pause/stop/seek` 以目标状态或目标位置表达瞬时控制，不进入历史记录，也不要求调用方维护状态版本；重复调用同一目标是合法 no-op；
- `playback.set_loop/set_loop_enabled/clear_loop` 修改工程持久循环状态，通过 `CommandCommitter` 各自形成一条历史记录，并推进文档 revision；Undo/Redo 使用同一 ActionSequence 恢复循环设置。

持久循环只按工程文档 revision 校验并发编辑。播放头在播放期间持续变化不会使 pause、stop、seek 或循环编辑产生瞬时版本冲突。

### 5.5 格式、MIDI 与 LibreSVIP

文档打开、导入和批量导入复用 Project Format Registry 与 `IProjectLoadSession`。自动化 host adapter 使用 `interactive=false` 创建 headless session，格式选项由严格 Schema 提供，不打开配置对话框；任务开始时重新检查读权限，带 plan digest 的输入由检查器返回同一份已验证字节，session 只解析保留原扩展名的临时快照，提交阶段不再重复读取原路径。DSPX 输入在构造 Timeline 前使用宽整数检查每段拍号投影，无法表示在模型 tick 范围内的工程会直接拒绝，单文件与批量加载共享该入口。Task 保留文档 generation 和调用者归因，最终通过 Document Facade 完成换代或单条历史记录导入。

音频导入、重定位和路径确认在一次顺序读取中同时生成 SHA-512 与临时文件快照，随后只解码该快照；
三类操作不在解码后重新读取原始路径。提交记录规范化授权路径，摘要与解码结果来自同一份已接受
快照；临时快照随准备任务结束清理，不进入工程。

MIDI 路径拆为可复用的两段：

1. `MidiFileParser` 读取并解析为可复用中间数据，不修改 Model；
2. `MidiTrackGenerator` 根据编码、轨道选择、Tempo 和拍号选项生成轨道与时间线结果，再由 GUI session、批量导入或自动化提交者应用。

`formats.inspect` 与 MIDI 文档导入复用 parser，交互式 GUI、批量导入和 headless session 复用 generator。同步检查最多读取 64 MiB 的单份文件快照，超限在解析前拒绝；MIDI 解析、LibreSVIP 转换和摘要都消费该快照，不重复打开原路径。异步文档打开、导入和批量导入在任务开始时检查当前读权限；带 plan digest 时加载已验证快照，提交阶段不再复算原路径摘要。MIDI capability/preview/start 则复用同一 converter 和公开 option Schema；导出支持 Tempo、拍号和歌词选项，先写同目录临时文件并在最终发布前复核文件授权、Task 取消和文档 generation。拒绝覆盖时，文档保存、MIDI 和音频均把完成的同目录暂存文件以拒绝已存在目标的重命名一次性发布；目标在检查后被其他进程创建也不会被替换，也不会向并发读取者暴露部分 MIDI 或 DSPX。批量音频逐项原子发布，后续目标失败不回滚已经完成的输出；允许覆盖时每个输出使用 `QSaveFile` 原子替换提交。

`documents.save` 在公共 Registry 中只补齐当前文档路径；显式 `overwrite_policy` 保持调用值，省略
时才采用当前路径保存的默认覆盖策略，避免把调用方要求的拒绝覆盖改写为覆盖。

音频导出始终把渲染结果保留在暂存文件中，并在最终发布前检查取消状态与输出授权；等待推理或渲染期间发生同一工程的普通编辑时，已完成的暂存结果仍可发布，文档 generation 换代则取消任务。observer 覆盖渲染与 deferred publication 两个阶段；无法清理的暂存路径作为 warning 进入 Task mutation，并同步转发给调用方 observer。

LibreSVIP 转换抽取为共享 `LibreSVIPConverter`。GUI 转换 Task 与自动化文件服务使用同一外部转换入口、临时输出、超时和错误映射；headless 转换显式提供默认 stdin 回答，因此不会等待交互式输入。格式能力会根据转换器可用状态返回 available 与稳定原因。

### 5.6 文档统计、有界参数查询与 Speaker Mix 预设

公开工具不再暴露聚合对象树式的 `project.get`。`documents.get` 在既有文档状态上增加工程长度、轨道总数与空轨/纯歌声/纯音频/混合轨分类，以及剪辑总数与歌声/音频分类统计；对象详情继续由轨道和剪辑域分页查询。L2 的 `documents.list_recent` 只读取应用最近项目记录并报告路径、文件名和存在状态，不打开文件、不切换文档。

`parameters.get` 接受可选半开时间范围和 `max_points`。锚点曲线始终完整返回，点数上限不足以容纳稳定锚点身份时明确失败；采样曲线使用确定性步长降采样，并返回原始点数、实际点数和 `downsampled`。局部 `parameters.bake` 在把锚点转换为采样曲线前以 64 位整数计算总点数和可表示终点，超过公共曲线点数上限或无法落入模型时间轴时在分配与提交前拒绝。锚点拓扑操作也改为显式边界：`create_anchor_curve` 创建至少含两个锚点且不重叠的新曲线，`insert_anchors` 只写指定既有曲线，`merge_anchor_curves` 只显式合并相邻且不重叠的完整曲线；移动锚点不会隐式跨曲线合并。

Speaker Mix 预设并入 `speaker_mix` 域并开放在 L2。`presets.list/save/delete` 管理应用级预设，不改变文档 revision 或 History；`presets.apply` 把预设值作为一条文档历史记录应用到轨道或剪辑。查询快照携带可空来源预设和 dirty 标记，后续直接编辑混合会保留来源身份并标记已偏离预设。

### 5.7 工作区、轨道面板与剪辑编辑器

GUI 进阶控制按真实面板层级归入 `workspace`、`track_panel` 和 `clip_editor` 三个域。所有工具都显式定位 `window_id`；涉及工程对象的命令同时定位 `document_id`，对象选择继续使用稳定的 track、clip 和 note ID。它们只修改 QWidget 表示状态、选择、活动区域或视口，不推进工程 revision，也不写入历史记录，因此公共输入不要求 `expected_revision`。

`workspace` 管理轨道面板与剪辑编辑器的可见性，并保证至少保留一个主编辑面板。`track_panel` 管理自己的共享轨道视口、自动翻页、当前轨道、有序剪辑选择与 primary item；reveal 会根据目标轨道或剪辑调整可见范围，但不改变工程内容。选择类命令会显示并激活所属区域，键盘焦点只作尽力获取；Editor 位于后台、操作系统拒绝抢焦点时，已完成的选择或定位仍按成功返回。查询继续报告实际焦点事实。

`clip_editor` 把钢琴窗和参数面板建模为同一编辑面板的子区域：两者共享时间位置和横向缩放，钢琴另有音高纵向视口，参数另有值域纵向视口。活动剪辑、区域显示、自动翻页、钢琴编辑模式与量化、音符选择，以及参数前景、背景、交换和工具状态均走现有 Controller 与 ViewModel 路径，因此 MCP 结果会通过同一信号链立即反映在 GUI。

### 5.8 设置、包信息与歌词规则

设置工具采用 `settings.<domain>.update` 三段式命名，并以 `settings.query` 集中返回公开域的配置值、生效值、候选/范围、重启要求和不可用原因。八个 update 都是严格 allowlist 上的稀疏更新，只验证和提交调用方明确提供的字段；其中音频设备和计算设备更新开放 validate-only。所有设置更新失败时都不留下部分持久化或运行时状态，也不触发交互式错误对话框。UI 语言使用独立 `ui_language` 域；包搜索路径可查询但不能由 MCP 修改，自动化/MCP 自配置、开发者选项和其他未列入契约的设置也不向 MCP 暴露。

`packages.list/describe` 从当前包索引提供受读取根约束的规范路径和公开元数据。File Guard 约束 MCP 显式提供及向调用方披露的路径；`packages.refresh` 不接收路径，只触发 Editor 按既有应用配置执行与 GUI 相同的扫描，因此不把当前 effective 搜索路径重新解释为 MCP 路径授权。公共接口不提供包搜索路径修改能力。刷新使用 application-scoped Task，在后台完成扫描后原子切换索引；并发调用可复用已提交的扫描结果，但如果领先扫描在提交门被拒绝，等待调用会重新取得刷新权并执行自己的扫描，不会把保留的旧索引误报为刷新成功。它不伪造文档身份，也不参与工程 revision 或历史记录。

`lyric_rules` 使用稳定 rule ID 管理 splitter 与 tagger 两类规则。自定义规则支持创建、稀疏更新、删除、启停和分类内移动；内置规则内容不可修改或删除，但可以独立启停。`lyric_rules.test` 只读运行 splitter→tagger 流水线，返回逐阶段结果，不改变规则、文档或应用状态。

## 6. 历史记录、revision 与 Task

`CommandCommitter` 为一次编辑构造一个 `ActionSequence`，成功时记录一条历史记录并推进一次 revision；预检失败、handler 失败、文件失败和 no-op 都不会产生半提交。Undo/Redo 仅在真实导航时推进 revision，save/save-as 更新 savepoint，文档换代重置 generation 与历史基线。

拍号新增、替换和删除会先以宽整数计算候选序列中每个拍号的派生 tick；任何会超出底层模型
整数范围的投影都在 validate-only 或提交前原子拒绝，不进入 Timeline 缓存重建。
音符量化复用同一时间轴规则的宽整数实现，先计算全局起点、长度和局部终点，再缩窄为模型 tick；
任何越界候选都在 Action 构造前拒绝，预检和提交不会产生部分几何变化。

公开 `validate_only` 仅保留在 `documents.save_as`、`documents.import_batch`、`audio_clips.import_batch`、`settings.audio_device.update`、`settings.compute_device.update`、`lyric_rules.create` 和 `lyric_rules.update`。其他命令仍执行同样的提交前校验，但不向 Agent 暴露无实际收益的预演开关。

公开 `idempotency_key` 收敛到 `tracks.insert`、`clips.insert/duplicate`、`audio_clips.import*`、`speaker_mix.keyframes.insert`、`notes.insert/duplicate/split_at`、`parameters.create_anchor_curve/insert_anchors` 和 `extract.*.start`。这些操作能够以稳定结果安全去重；key 最长 128 个字符，每个 document generation 以 FIFO 保留最近 256 个成功键。`documents.import*`、`inference.start` 及其余普通文档写操作只使用文档 revision 或 Task 状态处理冲突。瞬时 GUI 与播放目标状态命令不依赖客户端版本令牌，并通过 MCP `idempotentHint` 表达可安全重复调用。

音频单项和批量导入在创建 Task 及启动哈希/解码前，以公开操作 ID、文档版本和完整导入请求
占用幂等键；同指纹重试返回原 `TaskAcceptedResult`，不同指纹立即返回冲突。失败或取消会释放
占用，最终剪辑提交不再使用同一个 key 二次去重。

异步工具由 `AutomationTaskManager` 管理 Queued、Running、CancelRequested、Committing 和终态。Task 记录 operation、基准文档、创建者、进度、结果或错误；最终写回前复核 generation、revision、对象和文件授权。`tasks.list` 的游标摘要只包含 scope、筛选条件、document generation 和有序 Task ID，因此运行中任务的进度、消息或状态更新不会阻断后续分页；筛选后成员变化仍会使游标失效。Pitch/MIDI 提取由后台任务创建并哈希源音频快照，后端只读取快照；完成后再次后台哈希原路径，并在进入 Committing 前复核音频剪辑身份和冻结路径权限，源内容或权限变化都不会写回参数或工程对象。MIDI 导出在准备和渲染期间保持 Running，只有紧邻最终文件发布时才进入不可取消的 Committing。每个文档 generation 与应用级作用域的活动任务都不被清理，各自终态历史按完成顺序有界保留最近 128 项。Connector 在有副作用请求的结果事实不明确时返回 `outcome_unknown`，不自动重放 Command。

## 7. Profile、Custom、File Guard 与 Admission

`AutomationAccessPolicy` 分离 preset Profile 与 Custom operation 集合。Editor 的 `tools/list` 和 Registry 执行期 dispatch 读取同一策略；工具列表缓存不能替代每次调用时的授权检查。Connector exposure 只决定下游可见面，Editor 仍执行最终授权。

`L3` 在产品文案中称为“进阶控制”（Advanced Control），表示在 L2 之上按明确范围增加部分 GUI 自动化操作与设置项更改，而非不受限制的完全控制；自动化/MCP 自身的配置和服务启停明确排除在外。应用优雅退出与重启属于独立的 L0 固有能力。

L0 是不可禁用的固有工具层。Editor 的所有 preset 和 Custom 都始终包含 L0，Custom 设置页不显示这些工具，也不为其持久化开关。Connector 的 `l0` exposure 包含同一组 L0 Editor 工具；include 可以增加其他工具，但任何 `--exclude-tool` selector 都不能移除 L0。

Allowed Read Folders 与 Allowed Write Folders 是自动化文件工具的规范路径 allowlist：前者约束打开、导入、检查和读取素材等操作，后者约束保存、导出等写操作。它们不表示本机进程权限，也不改变非文件工具的能力。`AutomationFileGuard` 还分离持久根与会话 grant，处理路径组件边界、相对路径、相邻前缀、链接/重解析点和未创建输出的最近现存父目录；授权后、实际 I/O 前会再次检查 canonical 目标。

业务 Admission 只限制全局 32 个在途请求和 8 个后台 Task；HTTP Transport 同样只执行全局
32 路硬上限。不设置 logical client、peer、domain 配额、令牌桶或公平排队。每个 Connector
downstream 最多保留 32 个在途工具调用，直接并发转发且不增加串行队列；第 33 个请求在创建
上游 `QNetworkReply` 前返回稳定 `busy`。RAII lease 和 Connector 请求槽均在成功、失败或取消时
释放，异步 lease 延续到 Task 终态。

## 8. MCP 双协议与 Editor HTTP

Wire 和 Editor MCP 同时实现两套主协议：

- **2025-11-25**：`initialize → notifications/initialized`，随后调用 `ping/tools/list/tools/call`；
- **2026-07-28**：`server/discover` 与逐请求 `_meta`，随后调用 `ping/tools/list/tools/call`，请求不经过 `initialize`。

同一 legacy 入口接受客户端以 **2025-06-18** 发起握手，回显协商版本并完成 initialize/initialized 生命周期。每个 legacy session 独立记录初始化状态；`initialize` 响应与 `notifications/initialized` 之间仅接受 `ping` 和初始化通知，其他 request 返回 `ServerNotInitialized` 且不进入业务处理。2026-07-28 请求使用 `MCP-Protocol-Version`、`Mcp-Method`，`tools/call` 还校验 `Mcp-Name`；协议版本决定现代或 legacy 的结果塑形、server metadata 与 structured content 表达。

`McpHttpServer` 仅监听数值 loopback 地址，固定路由为 `/mcp`。Legacy POST 承载请求与 notification，除 `initialize` 外的每个 legacy 请求都必须携带并命中存活 session；legacy 客户端可用带 session 与协议 header 的 DELETE 主动结束会话，服务端最多保留 128 个 legacy session，超限淘汰最早建立者。Legacy session 和 Connector 显式 instance metadata 形成连接无关的客户端身份；Connector 在所有现代上游请求中携带同一个实例 ID，因此项目扩展的 `notifications/cancelled` 可以从另一条 HTTP 连接取消尚未分派的请求。2026-07-28 核心 Streamable HTTP 不发送该 notification；未携带 Connector 扩展身份的现代直连客户端按 TCP 连接隔离，相同 client info 不会把不同客户端合并。若同一身份与 request ID 同时匹配多个请求，则拒绝猜测。Transport 校验本机地址、Host、Origin、method、Content-Type、Accept、请求元数据、JSON 资源上限、请求/响应大小和 deadline；接受 notification 后返回 HTTP 202。有序停止先关闭 admission，再等待或终止在途工作并释放 listener 与会话。

## 9. QLocal Bootstrap 与 DS Connector Lite

Editor 的单实例协议提供 `automation.discover` 和 `automation.watch`。长度前缀 JSON framing
承载完整快照；watch 建立时立即返回当前状态，Editor 启用/禁用 MCP、监听器 ready、换端口、
报错或退出时广播新快照。Watcher 数量、帧尺寸、待写帧和累计字节均有界，慢读或异常断开只
清理对应连接。

Connector 可先于 Editor 启动。`BootstrapWatcher` 只作为 QLocal 客户端观察当前实例，使用实例
身份与 handshake epoch 丢弃旧结果，并以有界退避重连。上游优先使用 2026-07-28
`server/discover`，失败时进入 2025-11-25 initialize 流程，并接受服务端协商到 2025-06-18。
协议建立后，Connector 完整分页读取 `tools/list`，再用一次 `application.get_status` 取得
toolset version、Profile 和 host。

Connector 固定提供六个桥接工具：

| 工具 | 职责 |
|---|---|
| `connector.get_status` | 返回 Connector、Bootstrap、Editor、协议、工具集兼容与 exposure 的缓存事实 |
| `connector.reconnect` | 主动重建观察和上游握手 |
| `editor.tools.list` | 分页列出 exposure 后的实际 Editor 工具摘要 |
| `editor.tools.search` | 按 ID、标题、说明和域搜索实际工具摘要 |
| `editor.tools.describe` | 返回实际 Schema、版本、权限、兼容与可用性 |
| `editor.tools.invoke` | 按 Editor 当前真实 Schema 调用获准目标 |

六项桥接工具的 `minimum_toolset_version` 均为 1。Connector 还携带构建时已知的 176 项
Editor 类型化描述；downstream 工具面由六项桥接工具与 exposure 选择的类型化工具组成，并在
单个 Connector 生命周期内保持描述稳定。标准 downstream `tools/list` 直接发布类型化描述；
泛化 `editor.tools.list/search` 只返回摘要，需要完整 Schema 时调用 `editor.tools.describe`。

Exposure 默认 L1，支持 `l0/l1/l2/l3`、`id:`、`category:`、`prefix:`、include 和
exclude；L0 固有工具始终保留，其余工具才应用 exclude。同一选择同时约束类型化 wrapper 和泛化 list/search/describe/invoke。兼容计算只检查
全局 `toolset_version` 与每工具 `minimum_toolset_version`，不计算 Schema 方向、子集、digest
或 `compatible_subset`。同版本 Schema 不一致按产品缺陷处理，并由 MCP 输入校验与测试发现。
`tools/list` 的 namespaced `_meta` 仍携带 `kind` 与 host availability，确保泛化工具保留
Command outcome 和 host 过滤语义。Connector 只验证自己六个桥接工具的输入输出 envelope；
类型化工具和泛化 invoke 的业务 Schema 由 Editor MCP 验证，避免在转接层重复执行同一套 Schema
引擎。Editor 与 Connector 的分页 cursor 均以 base64url 承载上下文、快照和 offset，不使用
HMAC；正常握手只重建一次工具与兼容缓存，`connector.get_status` 只读取现有缓存和当前观察状态。
Bootstrap 的首包计时从 watch 请求成功写入后开始；连接失败会停止该计时并稳定报告
`editor_not_running`，只有已建立引导连接但 2 秒内没有首个状态快照时才报告
`bootstrap_timeout`。

Downstream stdio 支持两套主协议及 2025-06-18 兼容握手，具有有界 reader/writer 队列、32 路
在途工具调用硬上限、并发请求 ID 映射、取消、超时、EOF、broken pipe 和 backpressure 处理。
Windows 非阻塞管道按 4 KiB 分块写出大响应，停滞计时只在连续无进度时触发；stdout 只输出
MCP 帧，诊断写入 stderr。

## 10. 设置页、配置复制与 CLI

Automation 设置已进入选项对话框导航，并使用与其他选项页一致的主题图标。页面、运行状态和 24 个域的显示文本已补齐中文翻译，其中 `history` 显示为“历史记录”。Custom 工具不再使用平铺清单，而是按公共契约的领域建立独立卡片；固有的 L0 工具不进入 Custom 列表。每组默认收起，支持独立展开/收起、启用计数和整组启停。自定义工具集顶部可以将当前所选 L1、L2 或 L3 的完整工具范围同步到 Custom；选择 Custom 时同步按钮禁用。单工具开关仍直接写入同一 `customPermissions` 集合，组级操作与预设同步只更新这套既有权限，不引入第二套配置。

持久设置的安全默认值为 MCP 关闭、L1、当前用户文档目录的读写根和非零控制端口。配置中缺少读写根字段时使用 Qt 提供的跨平台文档目录；已显式保存的空列表保持为空。配置中缺少有效端口时在动态私有范围生成一个端口，保存后后续启动继续使用该值；只有用户点击“刷新”或直接编辑才改变端口。页面不提供固定/随机下拉框，“刷新”按钮与端口输入框位于同一行并始终可操作。

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

- 公共 tool name 唯一，域、类型、Profile、Schema 和逐工具最低工具集版本合法；
- Contract、Registry、Editor `tools/list` 与 Connector 已知描述来自同一权威集合；
- Connector 桥接工具唯一，exposure 后的 downstream 等于可用 Editor 工具与桥接工具之并集；
- 历史记录原子边界、创建深度、音符叶节点、轨道/剪辑 voice 和持久循环；
- NoteTransfer 的音符与参数曲线深复制、局部锚点有界采样、GUI 剪贴板 round-trip 与无效几何拒绝；
- MIDI headless 解析/生成、LibreSVIP 共享转换、文件授权与异步写回；
- Profile/Custom、File Guard、全局 Admission、游标和版本兼容；
- 2025-11-25 与 2026-07-28 两套主协议、2025-06-18 兼容握手、HTTP、QLocal、Connector stdio 和真实进程路径；
- L0 退出/重启的 Schema、不可禁用策略、dirty 拒绝、显式丢弃、响应后关闭、重启换代与 Connector 自动重连；
- 工作区、轨道面板和剪辑编辑器的 GUI 状态、选择、焦点、共享视口及无历史记录副作用；
- 设置稀疏更新、包索引后台刷新与提交门拒绝后的并发重试、歌词规则管理与失败回滚；
- Automation 设置持久化、CLI override、端口、配置 JSON、Custom 领域分组与中文界面。

最终候选在 Visual Studio 2026 v18.9.0、Qt 6.11.2 环境中通过标准 preset
`ConfigureAndBuild` 和 `all` target，完整 CTest 为 62/62（41.77 s）。Editor 176 项、Connector
6 项、24 个业务域、L3 44 项和内部 207 个 Operation ID 为当前产品快照；契约集合关系、共享不变量和领域独特语义的确定性覆盖通过；2025-11-25 下游
握手、2026-07-28 上游连接、真实编辑联调、Computer Use GUI、配置恢复和只读素材完整性均通过。
生命周期增量联调还确认 dirty 默认拒绝无弹窗、显式丢弃重启后 instance ID 变化且 Connector
自动恢复 compatible，以及 clean 默认退出后无 Editor 进程残留。
