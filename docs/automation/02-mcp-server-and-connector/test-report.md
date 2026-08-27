# 二期 MCP Server 与 DS Connector Lite 最终测试报告

## 1. 结论

二期最终候选在 Windows x64 Debug 环境完成标准 preset 配置与构建、完整 CTest 执行、真实 Editor/Connector 联调和 Computer Use GUI 验收。公共工具面为 Editor **179** 项、Connector **6** 项，共 **185** 项；Editor 工具分属 **25** 个业务域，类型统计为 **45 Q/S + 123 C/S + 11 C/A**。

最低 Profile 分布为 Meta 4、L1 87、L2 43、L3 45，累积可见数量为 4、91、134、179。公共工具集版本及 185 项工具的 current、introduced、minimum-compatible 版本均为 **1**。

最终验证按四个不同分母记录：

- 179 项 Editor 工具全部具有 Contract、Schema、Registry、权限、Editor MCP 与 Connector 确定性覆盖；
- 25 个业务域全部完成至少一条真实 Connector 代表路径，并按状态所有者通过 GUI、查询结果、Task 或进程事实闭环；
- L3 的 45 项工具全部通过 Connector 泛化调用逐项执行；
- Connector 的 6 项桥接工具全部逐项执行。

同一候选完成完整 CTest 与 GUI/Computer Use 验收，最终成功路径未出现模态弹窗、界面假死或无人值守阻塞。二期范围内发现的缺陷均已修复并重跑受影响域；第 8 节列出的既有依赖与推理稳定性问题不在本期授权修改范围内，本候选不包含相关修复，也不把对应场景计入通过结论。

## 2. 候选与执行摘要

| 项目 | 最终结果 |
|---|---|
| 分支 | `mcp` |
| 代码候选 | `f4abb8e0` 及后续文档提交 |
| 平台 | Windows 11 x64、VS x64 DevShell、Qt 6.11.2 MSVC x64、Ninja、vcpkg `x64-windows` |
| Debug 配置与构建 | 项目标准 CMake preset wrapper，退出码 0 |
| 最终测试清单 | 67 项 CTest |
| 完整 CTest 第 1 轮 | 67/67 通过，0 失败，92.76 秒 |
| 完整 CTest 第 2 轮 | 67/67 通过，0 失败，92.73 秒 |
| 完整 CTest 第 3 轮 | 67/67 通过，0 失败，93.11 秒 |
| 三轮合计 | **201/201 通过**，0 失败，0 超时，278.60 秒 |
| L3 泛化调用 | 45/45 通过 |
| Connector 桥接调用 | 6/6 通过 |
| GUI/Computer Use | 25 域真实代表路径；界面证据保存在私有归档 |

最终轮开始前使用标准 preset wrapper 的 `--target all` 按当前源码重建全部 67 个注册测试目标，避免 CTest 复用陈旧二进制。三轮均固定使用 `-j 1`，并显式设置 Qt offscreen platform 及有效的 platform plugin 目录；三轮之间没有修改源码或重新构建候选。

## 3. 工具集合与确定性覆盖

公共集合门禁确认：

- Editor Contract、Registry binding、Manifest、Editor `tools/list`、Connector 已知类型化描述和公共矩阵的 179 个 ID 精确相等；
- Connector 固定桥接定义、downstream 固定面和公共矩阵的 6 个 ID 精确相等；
- 25 个域的工具数量、Query/Command 类型、同步模式、最低 Profile、逐工具版本和动态值来源逐项一致；
- 179 项 input JSON Schema 均以 object 为 MCP 根，input/output Schema 均通过严格验证；未知字段、错误类型、非法枚举和不满足联合分支的输入在 handler 前失败；
- Meta、L1、L2、L3 与 Custom 的发现面、Manifest 和 Registry 执行期授权使用同一 Access Policy；
- L2 downstream 为 134 个 Editor wrapper 加 6 个桥接工具，共 140 项；L3 downstream 为 179 个 Editor wrapper 加 6 个桥接工具，共 185 项；
- Connector 的 Manifest 预期摘要按 Editor 实际 Profile、host mode 和 Custom 已知 ID 集合构造，完整 L3 不再因固定 L2 基准误报 `compatible_subset`；
- `project.get` 不在公开集合，工程长度和轨道/片段分类统计由 `documents.get` 提供；
- `tasks.list/get/cancel`、`documents.list_recent`、Speaker Mix 预设、有界参数查询、显式锚点曲线操作、音素位置重置和 L3 六域均进入当前契约。

这里的“179 项确定性覆盖”指逐工具契约、binding、授权、MCP 编解码、适用成功/拒绝/回滚路径和输出校验；它不等同于 179 项全部经过人工 GUI 逐项操作。真实资格按 25 个域建立代表路径，L3 45 项和 Connector 6 项另有逐项实际调用分母。

## 4. L3 进阶控制逐项结果

| 域 | 工具数 | 实际结果 | 闭环方式 |
|---|---:|---|---|
| `workspace` | 2 | 2/2 通过 | 工作区查询、主面板可见性变更、界面观察与恢复 |
| `track_panel` | 7 | 7/7 通过 | 视口、reveal、自动翻页、轨道/片段选择、焦点回读与恢复 |
| `clip_editor` | 16 | 16/16 通过 | 活动片段、共享时间视口、钢琴与参数子区域状态、选择和工具回读 |
| `settings` | 10 | 10/10 通过 | 集中查询、九个稀疏 update、三项复杂更新的 validate-only、GUI/应用状态与恢复 |
| `packages` | 3 | 3/3 通过 | list/describe 及 application-scoped refresh Task 成功终态 |
| `lyric_rules` | 7 | 7/7 通过 | 临时自定义规则的创建、更新、启停、移动、测试、删除与基线恢复 |
| **合计** | **45** | **45/45 通过** | Connector 泛化调用逐项执行 |

前三个 GUI 域共 25 项工具均显式定位窗口；涉及工程对象时同时定位文档和稳定对象 ID。命令只改变 QWidget 表示状态、选择、焦点或视口，不推进工程 revision，也不写入历史记录。钢琴与参数子区域共享的时间视口、各自独立的纵向视口，以及轨道面板自己的视口均通过调用后 query 和界面事实交叉核对。

设置更新只接受公开 allowlist 上的字段，只有音频设备、计算设备和包搜索路径更新开放 validate-only。音频设备稀疏更新回归确认未提交字段保持原值；包搜索路径更新在真实环境使用 validate-only，避免替换用户配置。所有实际配置变更和临时歌词规则均在场景结束后恢复。

## 5. 25 个业务域的真实代表路径

| 业务域 | 真实资格结果 |
|---|---|
| 应用 | `application.get_info` 返回产品、版本、build 与平台事实，无副作用 |
| 自动化与安全边界 | status、Manifest、动态 options 和文件根授权通过；候选值可在不知道预选值时查询 |
| 文档与工程 | 工程统计、最近项目以及支持 validate-only 的批量/另存文件命令授权路径通过 |
| 格式 | 格式列举及对工程副本的 inspect 通过 |
| 轨道 | 重命名、轨道 voice 设置、查询与 Undo/Redo 通过 |
| 总线 | 主总线增益变更、GUI 观察与 Undo 通过 |
| 片段 | 重命名、片段 voice 设置、查询与 Undo 通过 |
| 音频素材 | get、import、import_batch、confirm_path 完成真实调用；relocate 的契约与拒绝路径通过，非零播放位置重载场景不计入通过范围 |
| 声库 | list/describe 及 singer/speaker 动态候选通过 |
| Speaker Mix | fixed 混合和 preset save/list/apply/get/delete 通过，来源与 dirty 状态正确，结果恢复 |
| 音符、歌词、语言、发音与音素 | 歌词编辑和向右级联音素位置重置通过；MCP 路径不触发 GUI 确认弹窗，整体 Undo 通过 |
| 参数曲线与锚点 | capability、有界 query、真实 draw、GUI 观察和 Undo 通过；锚点拓扑由确定性测试覆盖 |
| 时间轴 | Tempo 与拍号变更立即可见并可整体 Undo |
| 历史记录 | get_state、Undo、Redo 与 savepoint 恢复通过 |
| 播放 | seek、play、pause、stop 与持久 loop 设置/查询/恢复通过 |
| 导出 | MIDI 与音频 capability、preview、异步 start 均成功，输出文件存在 |
| 提取 | capability、短音频 Pitch 与 MIDI 成功终态通过；长音频 DirectML RMVPE 不计入本期通过范围 |
| 推理 | capability/status、阶段重置、实际 start Task 与对象结果通过 |
| 异步任务 | list/get 的真实任务进度与成功终态可观察；cancel 语义由确定性测试覆盖 |
| 工作区布局 | 2/2 实际调用通过，状态恢复 |
| 轨道面板 | 7/7 实际调用通过，状态恢复 |
| 片段编辑器 | 16/16 实际调用通过，状态恢复 |
| 设置 | 10/10 实际调用通过，实际变更恢复 |
| 包信息 | 3/3 实际调用通过，刷新 Task 成功且索引原子切换 |
| 歌词规则 | 7/7 实际调用通过，临时规则删除后恢复基线 |

文档编辑域的代表 mutation 均通过 Connector 提交，并由 GUI 或 query 立即观察；需要历史记录的路径随后执行 Undo/Redo 或等价恢复。不直接可见的 Query、安全拒绝、应用设置和后台 Task 使用结构化结果、应用状态或进程事实闭环。

## 6. Editor、Connector、协议与并发

真实 Connector 会话逐项执行：

| 桥接工具 | 结果 |
|---|---|
| `connector.get_status` | 返回 ready、协议、Profile、Manifest 与 exposure 缓存事实 |
| `connector.reconnect` | 进入重连状态后恢复 ready |
| `editor.tools.list` | 分页返回实际 Editor 工具、游标和 toolset version |
| `editor.tools.search` | 按域搜索得到预期工具集合 |
| `editor.tools.describe` | 返回实际 Schema、L3、泛化调用和可用性事实 |
| `editor.tools.invoke` | 成功调用 Editor 目标并返回结构化结果 |

确定性 HTTP、stdio 和真实进程测试覆盖：

- 2025-11-25 的 `initialize → notifications/initialized` 生命周期；
- 2026-07-28 的 `server/discover` 生命周期，且不要求或伪造 `initialize`；
- 客户端请求 2025-06-18 时的兼容握手和 legacy 会话；
- loopback-only、Host/Origin/Header、body/depth/node/response 上限、deadline 与有序停止；
- Connector 上游优先 2026-07-28、回退 2025-11-25，并接受协商至 2025-06-18；
- stdio 大帧、部分写、并发乱序、取消、timeout、EOF、broken pipe、重连和旧 epoch 隔离；
- 同一 logical client 的 32 路在途请求上限与第 33 路稳定拒绝，Connector 不增加串行排队；
- 正常握手只重建一次兼容缓存，`connector.get_status` 不重新计算 179 项 Schema。

动态值全域检查未发现“必须先知道一个可用值才能查询可用值”的循环依赖。singer-only 上下文可以继续查询 speaker，参数、格式、导出、提取、推理和文件访问的候选或范围均有可达入口；Custom 扩展不作为本轮无限递归推导对象。

## 7. GUI 与无人值守结果

Computer Use 核对了 Automation 设置页、编辑区域和 MCP 变更后的 GUI 事实，界面证据保存在仓库外私有归档。主要结果为：

- 选项菜单包含带图标的“自动化”项，面板和 25 个域均有中文显示；`history` 显示为“历史记录”；
- Custom 工具按领域分为可折叠卡片，支持整组开关、启用计数和单项回读；
- 端口刷新按钮与输入框同一行且始终可用；首次持久化后端口保持稳定；
- STDIO 与 Streamable HTTP 配置始终可展示和复制，内容是单个 server entry，不含外层 `mcpServers`；
- Allowed Read/Write Folders 的说明明确为自动化文件路径 allowlist，页面没有“本机进程访问”栏目；
- L1/L2 编辑代表路径和 L3 GUI 控制在调用后立即反映到现有界面，并可查询及恢复；
- 音素位置级联重置、设置更新、文件操作、提取、推理和 Task 调用的最终成功路径均未出现模态窗口；
- 最终窗口监控只观察到一个 Editor 主窗口，无隐藏确认框、错误框或 UI 假死。

二期范围内故障的现场仍保留在仓库外私有证据中；相应修复后的成功路径已重新执行。长音频 RMVPE 的实验性成功证据已撤销，不进入最终结论。

## 8. 测试中发现并修复的问题

| 问题 | 根因与修复 | 回归结果 |
|---|---|---|
| 联合 Schema 在部分 MCP Host 上不可用 | 公共联合类型直接暴露了非 object 根；改为 object-rooted 严格联合 Schema | 179 项 input 根和 tools/list/describe/invoke 通过 |
| 音频设备稀疏更新覆盖未提交字段 | 设置适配器把缺省字段当作新值应用；改为只验证和提交显式字段，并保持回滚 | validate-only、实际更新、GUI 回读和恢复通过 |
| reveal 目标动态值来源不完整 | 互斥目标分支只发布一个候选来源；同时发布轨道与片段/音符适用来源 | 首次查询与两类 reveal 通过 |
| 音频与推理操作的状态不变量不完整 | 文件准备、Task 写回和领域状态检查之间缺少统一约束；复用公共授权、generation/revision 与提交检查 | import、extract、inference、授权与取消路径通过 |
| 推理结果重复报告 scope 对象 | scope 根对象与分片结果合并时未去重；按稳定对象身份合并 | reset/start 返回唯一对象集合 |

talcs v0.1.0 在关闭的 `AudioFormatInputSource` 收到非零读位置时，会在采样率比例建立前换算输入位置；该时序由 talcs 自身的 Mixer、Buffering 和 DSPX 调用链产生，Debug 构建会导致 Editor 进程退出。上游 `main` 已有修复但尚无新发布标签。该依赖问题不属于本期 MCP/L3 的授权修改范围；本报告保留故障事实，不宣称非零位置重载场景通过或已修复。

长音频 DirectML RMVPE 资格测试观察到 Editor 进程退出。该问题属于既有推理实现，不属于本期 MCP/L3 的授权修改范围；为它制作的实验性 synthrt overlay 已完整回滚，当前分支没有相关产品代码或依赖补丁。本报告仅保留故障事实，不宣称该场景通过或已修复。

## 9. 数据安全、恢复与证据

- 用户提供的素材源始终只读；所有写操作使用测试拥有的副本或输出目录。
- 最终按相对路径和 SHA-256 复核源素材树，共 19 个文件，与基线清单零差异。
- Automation、设置、临时规则、工程 History 和测试场景状态均已恢复。
- 应用配置从测试前备份精确恢复，恢复后 SHA-256 与备份一致。
- 测试拥有的 Editor 已正常退出，无 Editor 残留进程；当前 Agent Host 使用的既有 Connector 连接保持运行，未被测试清理误伤。
- 原始构建、CTest、结构化调用、窗口监控、故障现场和界面证据保存在仓库外私有归档；超出范围的依赖修改证据与当前候选明确区分。
- 正式文档不记录用户名、绝对路径、真实端口、PID、实例/对象/Task ID、素材名称、声库名称或用户规则内容。

## 10. 最终通过清单

- [x] Editor 179、Connector 6、合计 185 项的集合、域、类型、Profile 和版本分母成立。
- [x] 179 项 Editor 工具具有 Contract、Schema、Registry、权限和 MCP 确定性覆盖。
- [x] 25 个业务域均有真实 Connector 代表路径和适用的 GUI、query、Task 或进程闭环。
- [x] L3 45/45 项经 Connector 泛化调用逐项执行。
- [x] Connector 6/6 项桥接工具逐项执行。
- [x] 两套 MCP 主协议及 2025-06-18 兼容握手、Editor HTTP、QLocal 与 Connector stdio 通过。
- [x] Profile/Custom、File Guard、32 路并发、动态值、Manifest、exposure 和兼容缓存通过。
- [x] Automation 设置页、中文、图标、配置复制、端口与领域分组通过 GUI 验收。
- [x] Debug 配置与构建通过；三轮完整 CTest **201/201** 通过。
- [x] 用户素材零改动、应用配置精确恢复、Editor 进程与测试状态清理通过。

综上，当前二期候选满足工具契约、协议、Connector、真实业务域、GUI 无人值守和数据安全验收要求，可以提交后续合并评审。
