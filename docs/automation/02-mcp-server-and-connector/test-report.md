# 二期 MCP Server 与 DS Connector Lite 最终测试报告

## 1. 结论

本报告对应当前最终架构的同一候选验证。公共工具面为 Editor **177** 项、Connector **6** 项，
共 **183** 项；Editor 工具分属 **24** 个业务域，类型统计为
**41 Q/S + 125 C/S + 11 C/A**。

最低 Profile 分布为 L0 4、L1 85、L2 43、L3 45，累积可见数量为
4、89、132、177。L0 downstream 为 4 个固有 Editor wrapper 加 6 个桥接工具，共 10 项；L2 downstream 为 132 个 Editor wrapper 加 6 个桥接工具，共 138 项；
L3 downstream 为 177 个 Editor wrapper 加 6 个桥接工具，共 183 项。全局
`toolset_version` 与每工具 `minimum_toolset_version` 均为 **1**。

本轮最终判定：**通过**。

## 2. 候选与执行摘要

| 项目 | 最终结果 |
|---|---|
| 分支 | `mcp` |
| 代码候选 | `mcp` 分支当前最终候选 |
| 平台与工具链 | Visual Studio 2026 v18.9.0；Qt 6.11.2 |
| Debug 配置与构建 | 标准 preset `ConfigureAndBuild` 通过；随后 `all` target 通过 |
| 最终 CTest 清单 | 62 项 |
| 一次完整 CTest | 62/62 通过，45.80 s |
| Connector 真实联调 | 2025-11-25 下游握手和 2026-07-28 上游连接通过；L0 重启后自动重连且 toolset compatible |
| GUI/Computer Use | 真实编辑、合成、播放、另存，以及 dirty 拒绝、丢弃重启和 clean 退出全程无决策弹窗 |
| 测试素材完整性 | 素材源 19/19 项 SHA-256 不变；真实用户应用配置 SHA-256 不变 |

构建与 CTest 使用项目标准 preset wrapper。Qt 组件测试显式配置可用的 offscreen platform
plugin 路径；GUI 与 Connector 进程测试使用独立测试实例和隔离工作区。若本轮修复任何缺陷，
先重跑最小复现与受影响域，再对最终候选执行一次完整 CTest。

## 3. 工具集合与契约覆盖

本轮需要确认以下不变量：

- Editor 公共 Contract 的 ID 唯一；Registry binding、授权后的 Editor `tools/list` 与相应契约集合相等，Connector 已知类型化描述来自同一权威源；
- Connector 固定桥接定义唯一，downstream 等于当前 exposure 下可用的 Editor 工具与桥接工具之并集；
- 域、Query/Command 类型、同步模式、最低 Profile、`minimum_toolset_version` 和动态值来源满足声明约束；工具与域数量只作为候选快照记录，不作为固定测试门禁；
- `tracks.get` 返回轨道属性、统计、自有/有效 voice 与默认语言上下文，`clips.get` 对歌声剪辑
  返回 own/effective voice、继承来源和有效默认语言；公共集合不含
  `tracks.get_voice_context` 或 `clips.get_voice_context`；
- 177 项 input JSON Schema 均为 object 根，未知字段、错误类型、非法枚举和不满足联合分支的输入
  在 handler 前失败；output Schema 由确定性契约测试覆盖，运行时不逐次 assert；
- L0、L1、L2、L3 与 Custom 的发现面和 Registry 执行期授权使用同一 Access Policy；L0 固有工具始终可用、不进入 Custom 列表，Connector exclude 也不能移除；
- L0 的 `application.request_exit/restart` 只接受可选 `discard_changes`；dirty 默认拒绝并指向该字段，显式丢弃或 clean 工程返回动作明确的 accepted 结果；
- 动态 `value_sources` 只服务显式发现；正常 invocation 不自动回查 provider；
- 标准 MCP `tools/list` 提供工具目录和完整 Schema，`application.get_status` 提供全局工具集版本、
  Profile、host 与当前文档/窗口摘要；
- Cursor 的 base64url payload 只绑定 context、snapshot 和 offset，不使用密钥或 HMAC。

集合与契约实测结果：**通过**。当前候选快照为 Editor 177 项、Connector 6 项、总计 183 项；
最低 Profile 累计数量为 4/89/132/177，内部能力集合为 208 个 Operation ID。集合正确性由单一
契约源及其与 Registry、发现面和 downstream 的关系验证。Connector 工具集状态报告
`compatible`，177 项 compatible、0 项 unavailable、0 项 incompatible。

## 4. 版本兼容、准入与幂等

Connector 的类型化兼容只检查：

```text
connector toolset_version >= editor minimum_toolset_version
AND editor toolset_version >= connector minimum_toolset_version
```

系统不计算 Schema 方向性、Schema 子集、Schema digest 或 `compatible_subset`。同一工具集版本下
Editor 与 Connector 的 Schema 不一致属于缺陷，由 MCP 输入校验和契约测试发现并修复；Profile、
Custom、host availability 与契约版本分别报告。

业务 Admission 只保留全局 32 个在途请求和 8 个后台 Task 容量，HTTP Transport 只执行全局
32 路硬上限；没有 client/peer/domain 配额、令牌桶或公平队列。每个 Connector downstream
同样最多保留 32 个在途工具调用，第 33 个请求在上游转发前立即返回 `busy`，不创建
`QNetworkReply`、不排队；完成、失败、取消、deadline、disable 和 shutdown 路径均释放计数。

Dispatcher 的幂等处理为显式 opt-in。只有工具支持且请求实际带有 `idempotency_key` 时才计算
请求指纹并进入幂等存储；不带 key 的调用不哈希、不创建幂等记录。公开 key 的 128 字符上限与
每个 document generation 最近 256 个成功键的 FIFO 保留上限均通过边界测试。
真实音频导入进程回归确认，首次调用创建 Task 后，同键同请求立即重放同一个 Task ID；同键但
起点不同的请求在新 Task、哈希或解码创建前返回 `idempotency_conflict`，最终只插入一个剪辑。

每个文档 generation 与应用级 Task 作用域的活动记录都不受历史清理影响，终态分别只保留最近
128 项；边界测试确认第 129 项完成后，最旧终态不可再查询，最新终态、既有活动任务和其他
作用域的记录仍可查询。

瞬时播放命令不再公开或维护 playback state version，`play/pause/stop/seek` 以目标状态或位置
执行并保留 `idempotentHint`；重复 pause 返回 `changed=false`。三个持久循环工具只校验文档
revision。L3 的选择、定位、面板和视口命令不修改工程，也不要求 `expected_revision`；除
`clip_editor.parameters.swap` 外，目标状态类 GUI 命令均保留 `idempotentHint`。

版本、准入和幂等实测结果：**通过**。版本与工具目录契约通过完整 CTest；真实 Editor 会话在
改变播放位置后不带版本令牌执行 pause 成功，再次 pause 得到 no-op；
`connector.get_status` 只读取缓存状态，标准 `tools/list` 不执行 Schema 兼容计算。真实 `notes.insert`
以相同幂等键精确重放成功，未开放幂等的写操作不进入指纹与存储路径。

## 5. 24 个业务域与 L3 进阶控制

| 范围 | 工具数 | 本轮结果 |
|---|---:|---|
| Editor 全部业务域 | 177 | 契约与完整 CTest 通过 |
| `workspace` | 2 | 契约与确定性测试通过 |
| `track_panel` | 7 | 契约、确定性测试与后台窗口 GUI 代表路径通过 |
| `clip_editor` | 16 | 契约、确定性测试与后台窗口 GUI 代表路径通过 |
| `settings` | 10 | 契约与确定性测试通过 |
| `packages` | 3 | 契约与确定性测试通过 |
| `lyric_rules` | 7 | 契约与确定性测试通过 |
| L3 合计 | 45 | 契约覆盖、区域激活与尽力焦点代表路径通过 |

24 个业务域的 Query、同步 Command 和异步 Task 由确定性测试覆盖。真实 Connector 代表路径
覆盖精确声库选择、轨道 voice、剪辑、音符、合并后的 voice context 查询、泛化 invoke、L3 UI
区域激活与另存；编辑结果以结构化查询和 GUI 可见状态交叉确认。后台窗口增量场景确认
`track_panel.select_track` 在 `focused=false` 时仍成功并可回读选中轨道，`reveal_clips` 与
`clip_editor.show_region` 同样不因操作系统未授予键盘焦点而报错。
`clips.insert` 省略长度时的四小节推导使用宽整数；tick 上界输入稳定返回参数错误且不推进 revision。

## 6. Editor、Connector 与协议

六个 Connector 桥接工具均保留：

| 桥接工具 | 本轮结果 |
|---|---|
| `connector.get_status` | 真实调用通过，约 5–6 ms |
| `connector.reconnect` | 确定性协议与进程测试通过 |
| `editor.tools.list` | 摘要分页与确定性契约测试通过 |
| `editor.tools.search` | 摘要搜索与确定性契约测试通过 |
| `editor.tools.describe` | 完整 descriptor 与 Schema 契约测试通过 |
| `editor.tools.invoke` | 真实调用 `clips.get` 通过；生命周期目标的 exposure 与转发由确定性测试覆盖 |

Bootstrap 错误分类回归通过：不存在 Editor 引导端点时，Connector 在超过旧首包计时窗口后仍
稳定报告 `editor_not_running`；`bootstrap_timeout` 只覆盖 watch 请求成功写入后 2 秒内未收到
首个状态快照。公共契约、Registry 和进程联调均使用 `playback.get_state`、
`workspace.get_state`、`track_panel.get_state` 与 `clip_editor.get_state`。

同机 A/B 使用 125 个工具、每页 17 项的 8 页握手夹具，对比本轮修改前提交与最终候选；预热后
各执行 5 轮并取中位数。list/search/describe/status 各在一次就绪连接内连续调用 20 次：

| 指标 | 修改前中位数 | 最终候选中位数 | 改善 |
|---|---:|---:|---:|
| 完成分页 `tools/list` 与状态握手 | 862.24 ms | 775.24 ms | 10.1% |
| `editor.tools.list` × 20 | 1239.33 ms | 516.26 ms | 58.3% |
| `editor.tools.search` × 20 | 635.17 ms | 174.56 ms | 72.5% |
| `editor.tools.describe` × 20 | 34.21 ms | 23.91 ms | 30.1% |
| `connector.get_status` 状态读取 × 20 | 3.58 ms | 3.18 ms | 11.2% |

结果表明清理后的连接与查询路径均有可测提升，其中摘要 list/search 的收益最明显；完整 describe
仍按需返回 Schema，因此收益较小但未回退。基准插桩只用于仓库外测量，未进入产品或测试代码。

协议与进程验证覆盖 2025-11-25 `initialize/initialized`、初始化通知前工具调用的
`ServerNotInitialized` 门禁、2026-07-28 `server/discover`、2025-06-18 兼容握手、legacy 非
initialize 请求的存活 session 强制校验、有界
淘汰与 DELETE 结束、Connector 实例
身份跨 HTTP 连接稳定及排队取消、无显式实例 ID 的现代直连客户端隔离、loopback HTTP、QLocal
watch、stdio 大帧、32 路 downstream 饱和与第 33 路拒绝、并发乱序、取消、timeout、EOF、
broken pipe、重连和旧 epoch 隔离。Connector 常规握手分页读取 `tools/list` 后只读取
`application.get_status` 的轻量状态，不为 177 项 Schema 做兼容重算。

音频准备回归确认哈希与临时快照由同一次读取产生，导入解码只读取该快照；源文件随后变化不会
使已准备的摘要与解码内容分离，且提交前的后台摘要复核会拒绝同路径换内容。

文档计划回归确认带 digest 的 open/import/import_batch 解析检查器返回的同一份原始字节，并在
提交前拒绝原路径换内容；Pitch/MIDI 提取回归确认后端只读取哈希快照，原文件摘要、音频剪辑
身份或读权限变化时均不进入 Committing。

协议与真实联调结果：**通过**。Connector 使用 2025-11-25 完成下游握手，并以
2026-07-28 连接 Editor 上游；自动化协议兼容路径通过完整 CTest。真实会话通过
`package_id + package_version + singer_id` 精确选择同 ID 多版本声库中的目标版本，随后完成
`tracks.set_voice`、`clips.insert`、`notes.insert`、`tracks.get`、`clips.get` 和
`editor.tools.invoke(clips.get)`；合并后的两个 get 工具均返回有效 voice context。

生命周期真实联调另以隔离配置完成：Connector 工具面为 183 项；dirty 工程对默认 exit 与
restart 均返回 `busy + field_path=discard_changes`，Editor 继续运行；显式丢弃的 restart 在
8.93 ms 内返回 accepted，随后 instance ID 变化，Connector 观察到断开并自动恢复为
connected/compatible；重启后的 clean 工程默认 exit 在 9.91 ms 内返回 accepted 并优雅退出。

## 7. GUI 与无人值守

Computer Use 验收 MCP 调用后的 GUI 即时状态、L3 区域激活与焦点事实、合成波形、播放和另存结果；
全过程监控顶层窗口、活动模态窗口、进程心跳与无响应状态，自动化调用未触发交互式确认框。

GUI 与无人值守实测结果：**通过**。Computer Use 可见创建后的剪辑、7 个音符、歌词、发音、
音高曲线和合成波形；L3 UI 区域激活成功。增量回归把 Editor 保持在后台，真实调用轨道选择、
剪辑定位和参数区域显示均成功；query 回读同时证明目标状态已应用而键盘焦点仍为 false。
播放时间由 101:01:341 前进至 102:02:437，
`documents.save_as` 成功。生命周期增量场景中，dirty 默认拒绝后测试轨道仍即时可见且窗口保持
可操作；显式丢弃后旧窗口关闭、只出现一个 clean 新实例，未保存轨道不再存在；clean 默认退出后
Editor 窗口与进程均消失。三个阶段均未出现保存确认或其他模态窗口。

## 8. 缺陷与回归

最终候选的共享 Dispatcher、公共契约、Registry、Wire、Connector、文档生命周期和真实进程路径
均通过受影响测试与最终 62/62 完整 CTest（45.80 s）。压力测试仍在默认套件中，保留通知洪泛、
并发请求、大帧、慢读、取消与竞态覆盖；没有将其拆分、降次或改为可选执行。

时间线回归还覆盖了删除中间拍号后后续拍号继承超长小节的边界：validate-only 与实际删除均在
派生 tick 超出模型范围时原子拒绝，文档版本和拍号序列保持不变。
DSPX 加载回归使用正数但超过模型范围的拍号，在 Timeline 缓存构造前稳定拒绝；自动化单文件与
批量加载复用同一转换器，不会先执行有符号整数乘法再由 Facade 补救。
音符量化回归使用接近 tick 上界的长度和“剪辑起点 + 局部起点”组合，确认候选几何由共享宽整数
时间轴算法计算，并在无法缩窄为模型 tick 时拒绝且不推进文档版本。

参数烘焙回归使用从零延伸到模型 tick 上界的编辑锚点，并只请求最小局部区间；实现会在任何采样
分配前以 64 位整数识别超出公共曲线点数上限或无法表示的终点，稳定返回 `invalid_argument`，
不推进文档版本，也不会进入长循环或大内存分配。

音符转移回归在所选音符之外放置延伸至模型 tick 上界的编辑锚点，确认 duplicate 只物化与所选
范围相交的固定步长采样点，并使用 64 位计数与公共点数上限；目标音符和局部参数正确平移，不会
因整条锚点曲线展开而长循环、溢出或大量分配。
目标剪辑已有曲线的保留前缀或尾段若无法在公共点数上限内无损物化，duplicate 会在创建音符和
提交历史记录前返回 `unsupported`；专项回归确认文档 revision、目标音符集合和原曲线均不改变。

包刷新并发路径区分“扫描已提交”和“提交门拒绝”两类完成结果：只有前者可由等待调用复用；后者
会使等待调用重新取得刷新权并执行自己的扫描，避免把刷新前保留的旧索引误报为成功结果。

MIDI 导出回归在受控渲染阻塞期间分别发起取消和文档 generation 淘汰，确认 Task 在最终发布前
仍可取消且不会留下目标文件；拒绝覆盖路径还模拟检查后出现同名文件，同目录暂存文件的排他
重命名稳定返回 `overwrite_denied` 并保留外部内容，不暴露部分 MIDI。

文档保存回归在序列化期间创建同名目标，确认 `documents.save_as` 的拒绝覆盖路径保留外部内容、
返回 `overwrite_denied`，且文档路径、savepoint、revision 与工程内容均保持不变。
当前路径保存回归另确认 `documents.save` 不会改写显式 `overwrite_policy: reject`：外部修改保持
不变且保存后端未被调用；省略策略时仍沿用默认覆盖行为并完成保存。

音频导出回归使用两个同目录暂存输出，在第二个目标由外部占用后执行拒绝覆盖发布；最终发布稳定
失败并保留外部内容，同时通过发布前记录的文件身份回滚本次已经发布的第一个目标，不留下部分
批次。回滚实现不会直接删除最终目标：它先原子移入随机隔离名，再核对设备、文件 ID、尺寸和
修改时间；身份不符时恢复隔离文件并保留原备份，避免误删后到的外部替换内容。
deferred publication 的成功回归另模拟发布阶段 warning，确认它保留在成功 Task 的 mutation 中，
不会因渲染阶段 observer 连接结束而丢失。

音频导出 revision 回归在任务完成渲染、进入发布前修改轨道并推进文档版本；实现确认所有导出均
使用暂存输出，最终任务稳定失败为 `revision_conflict`、执行清理且从未调用发布，避免接单后对
live model 的编辑静默改变最终文件。

Task 分页回归在 application scope 的多页结果间推进运行中任务的进度和状态，后续页游标仍可用；
游标摘要只绑定筛选条件与有序 Task ID，成员集合变化时仍保持失效保护。

文档输入回归通过 open 代表路径确认共用的 plan revalidator 不会把接单时授权当作长期凭据：即使
未提供 plan digest，任务开始前撤销读根也会稳定返回 `permission_denied`；带 digest 时校验器返回
与摘要一致的原始字节，原路径随后换内容会在加载或提交边界拒绝。open、import 和 import_batch
均接入该共用入口。格式检查回归使用超过 64 MiB 的稀疏工程文件，确认同步调用在解析和摘要前以
`invalid_argument` 拒绝；MIDI 检查和 LibreSVIP 转换复用已经受限的单份字节快照。

提取回归确认 Pitch 与 MIDI 成功结果必须携带已验证的源快照身份；未验证结果以
`invalid_argument` 失败。另在后端启动后、完成回调前撤销读权限，两项任务均在进入 Committing
前稳定失败为 `permission_denied`，文档 revision、参数和轨道集合保持不变。实际 adapter 在后台
复制并哈希源音频、只把快照交给 RMVPE/GAME，完成后再后台哈希原路径并比对。

音频重定位和候选路径确认已改为异步 Task。Registry 回归确认两项工具把规范化路径、目标剪辑和
命令上下文路由到 Host Adapter 并返回符合契约的 Task 接受结果；实际 adapter 在线程池中完成
快照、hash 与解码，再对原路径执行最终摘要比对，并在提交前复核路径授权和文档目标。完整构建、
公共契约、Registry、异步文件域、真实进程集成及最终 CTest 均通过。

测试实现不再维护第二份手工工具清单，也不以固定工具数量、CTest 数量、场景配额或逐工具复制的
通用错误矩阵证明正确性。共享规则在其所有者层验证一次，各领域只保留独特业务语义与高风险边界。

超出本分支授权范围的第三方依赖或既有推理问题只记录事实，不在本报告中冒充已修复或通过。

## 9. 数据安全、恢复与证据

- 用户提供的素材源保持只读；所有写操作使用测试拥有的副本或输出目录；
- 测试前后按相对路径和 SHA-256 对照素材树；
- Automation 配置、临时规则、工程 History 与测试状态在场景后恢复；
- 测试拥有的 Editor、Connector、端口、QLocal 服务、临时配置和缓存按所有权清单清理；
- 原始构建、CTest、结构化调用、窗口监控与界面证据保存在仓库外私有归档；正式文档不记录
  用户名、绝对路径、真实端口、PID、实例/对象/Task ID、素材名或声库名。

数据安全和清理实测结果：**通过**。素材源 19/19 项前后 SHA-256 一致，真实用户应用配置
前后 SHA-256 一致。证据归档为下载目录中的
`DS-Editor-Lite-MCP-Simplification-Test-Archive-20260828`。

## 10. 最终通过清单

- [x] Editor 契约 ID 唯一，Registry、发现面与 Connector downstream 的集合关系成立；177/6/183 为当前候选快照。
- [x] 全局工具集版本和每工具最低工具集版本契约成立。
- [x] 全部公共 Contract Schema 可校验，Registry 与授权集合一致；共享权限/MCP 不变量和各域独特语义覆盖通过。
- [x] 24 个业务域具有 Connector 代表路径和适用的 GUI、query、Task 或进程闭环。
- [x] L3 契约集合、GUI 代表路径与 Connector 桥接工具的独特行为覆盖通过。
- [x] 两套 MCP 主协议、2025-06-18 兼容路径、Editor HTTP、QLocal 与 Connector stdio 通过；
  2025-11-25 下游和 2026-07-28 上游真实握手成功。
- [x] Profile/Custom、File Guard、global/background Admission、动态值、工具目录、exposure
  和版本兼容通过。
- [x] 两个 L0 生命周期工具不可禁用或排除；dirty 默认拒绝、显式丢弃重启、Connector 自动重连与 clean 优雅退出通过，且不触发 GUI 决策弹窗。
- [x] GUI 可见编辑结果、L3 区域激活、后台尽力焦点语义、播放、另存与无人值守关闭验收通过。
- [x] Debug 配置、全目标构建与一次完整 CTest 通过。
- [x] 用户素材零改动、应用配置恢复和测试进程/状态清理通过。

最终签署：**通过，可以交付**。
