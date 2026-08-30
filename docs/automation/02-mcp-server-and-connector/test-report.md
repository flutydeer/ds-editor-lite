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
| 一次完整 CTest | 62/62 通过，42.62 s |
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
32 路硬上限；没有 client/peer/domain 配额、令牌桶或公平队列。第 33 个并发在途请求应立即
得到稳定拒绝，所有完成、失败、取消、deadline、disable 和 shutdown 路径都必须释放计数。

Dispatcher 的幂等处理为显式 opt-in。只有工具支持且请求实际带有 `idempotency_key` 时才计算
请求指纹并进入幂等存储；不带 key 的调用不哈希、不创建幂等记录。公开 key 的 128 字符上限与
每个 document generation 最近 256 个成功键的 FIFO 保留上限均通过边界测试。

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

协议与进程验证覆盖 2025-11-25 `initialize/initialized`、2026-07-28
`server/discover`、2025-06-18 兼容握手、legacy session 有界淘汰与 DELETE 结束、Connector 实例
身份跨 HTTP 连接稳定及排队取消、无显式实例 ID 的现代直连客户端隔离、loopback HTTP、QLocal
watch、stdio 大帧、并发乱序、取消、timeout、EOF、broken pipe、重连和旧 epoch 隔离。Connector 常规握手分页读取
`tools/list` 后只读取 `application.get_status` 的轻量状态，不为 177 项 Schema 做兼容重算。

音频准备回归确认哈希与临时快照由同一次读取产生，导入解码只读取该快照；源文件随后变化不会
使已准备的摘要与解码内容分离，且提交前的后台摘要复核会拒绝同路径换内容。

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
均通过受影响测试与最终 62/62 完整 CTest（42.62 s）。压力测试仍在默认套件中，保留通知洪泛、
并发请求、大帧、慢读、取消与竞态覆盖；没有将其拆分、降次或改为可选执行。

时间线回归还覆盖了删除中间拍号后后续拍号继承超长小节的边界：validate-only 与实际删除均在
派生 tick 超出模型范围时原子拒绝，文档版本和拍号序列保持不变。

MIDI 导出回归在受控渲染阻塞期间分别发起取消和文档 generation 淘汰，确认 Task 在最终发布前
仍可取消且不会留下目标文件；拒绝覆盖路径还模拟检查后出现同名文件，排他发布稳定返回
`overwrite_denied` 并保留外部内容。

音频导出回归使用两个同目录暂存输出，在第二个目标由外部占用后执行拒绝覆盖发布；最终发布稳定
失败并保留外部内容，同时回滚本次已经发布的第一个目标，不留下部分批次。

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
