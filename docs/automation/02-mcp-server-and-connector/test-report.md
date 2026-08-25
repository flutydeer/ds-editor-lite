# 二期 MCP Server 与 DS Connector Lite 最终测试报告

## 1. 结论

二期最终源码通过 Debug 配置与全目标构建、127 项 Editor 公共工具和 6 项 Connector
桥接工具的确定性契约/行为验证、两套 MCP 主协议及兼容握手的真实进程联调、真实 Agent
Host 驱动的 GUI 可见编辑、修复后的连续三轮完整 CTest，以及握手性能优化后的追加完整
CTest。

当前结论为：**二期 MCP Server、DS Connector Lite、设置与连接器配置、公共工具集及其回归门禁
全部通过，可以交付测试。**

最终测试的生产代码与测试代码截至提交 `55d5a8dc`；其中大 stdout 修复位于 `35884a9c`，
握手与摘要性能优化位于 `55d5a8dc`。本报告整理只修改文档，不改变上述已测试代码。

## 2. 候选身份与最终门禁

| 项目 | 最终结果 | 证据 |
|---|---|---|
| 分支 | `codex/automation-phase2-mcp-server-connector` | 候选基线记录 |
| 环境 | Windows x64、VS 2026 x64 DevShell、Qt 6 MSVC x64、Ninja、vcpkg `x64-windows` | 私有环境快照 |
| Debug configure/generate | 通过 | `E-P2-CONFIGURE-R63` |
| Debug 全目标构建 | 通过；受影响产品和测试目标完成编译、链接 | `E-P2-BUILD-R64` |
| 修复后定向回归 | 2/2 通过，0 失败 | `E-P2-TARGETED-R65` |
| 注册 CTest | 66 项，枚举退出码 0 | `E-P2-CTEST-CATALOG-R66` |
| 完整 CTest 第 1 轮 | 66/66 通过，0 失败，0 超时，113.78 秒 | `R67` |
| 完整 CTest 第 2 轮 | 66/66 通过，0 失败，0 超时，113.62 秒 | `R68` |
| 完整 CTest 第 3 轮 | 66/66 通过，0 失败，0 超时，113.93 秒 | `R69` |
| 三轮合计 | **198/198 通过**，0 失败，0 超时，341.33 秒，无 flaky | `R67`～`R69` |
| 性能优化后定向回归 | 4/4 通过，0 失败，62.03 秒 | `E-P2-PERF-TARGETED` |
| 性能优化后完整 CTest | 66/66 通过，0 失败，0 超时，78.44 秒 | `R70` |

三轮完整 CTest 均在相同候选和构建产物上串行执行；握手性能优化完成后又执行了一轮完整
CTest 重新取得候选资格。Qt 组件测试显式使用有效的 offscreen 平台插件环境；未出现 platform
plugin 错误、崩溃、断言、超时或弹窗阻塞。

配置阶段报告 Vulkan headers 未安装，这是既有可选能力提示，不影响本期产品目标、链接或测试。

## 3. 固定工具分母与确定性覆盖

最终静态和运行时发现结果一致：

- Editor 公共工具 **127** 项，分属 **19** 个业务域；
- Connector 桥接工具 **6** 项，下游完整工具面合计 **133** 项；
- Editor 类型分布为 **36 Q/S + 81 C/S + 10 C/A**；
- 公共工具集版本为 1，133 项工具各自的 current、introduced、minimum-compatible 均为 1；
- Profile 累积发现数为 Meta 4、L1 89、L2 127、L3 127。

19 个 Editor 域及数量为：应用 1、自动化与安全边界 4、文档与工程 8、格式 2、轨道 15、
总线 5、片段 16、音频素材 5、声库 2、Speaker Mix 9、音符/歌词/语言/发音/音素 18、
参数曲线与锚点 10、时间轴 5、历史记录 3、播放 8、导出 6、提取 3、推理 4、异步任务 3。

`TestAutomationCatalogContract`、`TestPublicAutomationContract` 和
`TestPublicAutomationRegistry` 对全部 127 项 Editor 工具逐项验证唯一 ID、域、类型、Profile、
版本、严格输入/输出 Schema、Manifest、Registry binding、发现授权与可达 handler。六个 Connector
桥接工具及其 exposure 后的类型化工具集合由 `TestDsConnectorLite` 完整验证。

其余确定性套件覆盖：

- 编辑、运行时和异步维度中的成功、no-op、validate-only、错误优先级、原子回滚与幂等；
- 同类批量操作的一次提交，以及标量编辑、音符叶节点和浅层容器创建的历史记录边界；
- 轨道与片段声库归属、Speaker Mix、参数曲线、时间轴、持久 loop、撤销/重做与 revision；
- 文档换代、Task 状态机、取消/提交竞态、文件写回、MIDI、LibreSVIP 与音频路径确认；
- Profile/Custom、File Guard、Admission、动态值、Manifest 游标、Schema 方向性兼容和缓存失效；
- Editor HTTP、QLocal discover/watch、Connector stdio、进程启动/退出和既有应用回归。

因此，127 + 6 的“全工具覆盖”结论来自可重复的确定性 CTest 与集合门禁；第 6 节的真实 GUI
测试用于验证代表性业务变化是否立即可见，不以少量 GUI 操作替代全工具分母。

## 4. MCP、HTTP 与 Connector 真实进程结果

协议口径固定为两套主协议：`2025-11-25` 与 `2026-07-28`；同一 legacy 入口另接受
`2025-06-18` 兼容握手。

| 路径 | 生命周期与结果 | 工具发现 |
|---|---|---:|
| 主协议 `2025-11-25` | `initialize` 后协商并完成 initialized 生命周期，stderr 为空、正常退出 | 133 |
| 主协议 `2026-07-28` | 直接 `server/discover`，逐请求 metadata，**没有发送 `initialize`**，stderr 为空、正常退出 | 133 |
| `2025-06-18` 兼容握手 | `initialize` 回显协商版本并完成 legacy 生命周期，stderr 为空、正常退出 | 133 |

确定性协议测试进一步确认：2026-07-28 不具有 initialize 生命周期，对该请求返回协议错误；
两套主协议及兼容会话均覆盖 ping、tools/list、tools/call、分页、结果塑形和稳定错误映射。

Editor Streamable HTTP 验证了 loopback-only `POST /mcp`、notification 202、Header/Body
一致性、Host/Origin 防护、JSON 与响应资源上限、deadline、准入配额和有序停止。QLocal
Bootstrap 验证 discover/watch、完整状态快照、部分帧、慢读背压、异常断开和 Editor 状态变化。

Connector 六项桥接工具均通过：`connector.get_status`、`connector.reconnect`、
`editor.tools.list`、`editor.tools.search`、`editor.tools.describe` 和 `editor.tools.invoke`。
L0/L1/L2/L3、include/exclude、三类 selector、执行期二次授权、逐工具版本门槛、Schema 完全相同
快速路径、输入/输出 Schema 方向性兼容及 Manifest 摘要缓存均由确定性测试覆盖。Connector 的
预期摘要按 Editor 实际 Profile、host mode 和 Custom 已知工具集合构造；L3 不再因固定 L2 基准
误报 `compatible_subset`。

## 5. 大 stdout 缺陷、握手优化与性能复测

真实进程复测发现过一个阻断缺陷：兼容计算已得到 127/127 后，约 524 KiB 的 133 工具
`tools/list` 响应触发 `stdout_write_timeout`，客户端收不到完整结果。失败原始证据保留为
`E-P2-STDIO-FAIL-R3`，未被覆盖或改写为通过记录。

根因是 Windows `PIPE_NOWAIT` 下把整段剩余 JSON 交给 `WriteFile`；当帧大于管道即时容量时，
即使读取端持续排空也可能反复得到零字节进展，最终触发五秒无进展监视器。修复把每次写入限制为
**4 KiB**，每个成功分块重置既有进展监视器，同时保留有界队列、背压和真正慢读端的失败保护。

修复后的定向测试覆盖完整大帧和故意慢读端；真实 R4/R5 复测均返回全部 133 项。R5 中最大
JSON-RPC 行长为 523,791 字节，完整 `tools/list` 用时 536.12 ms，无 stderr、无遗留请求，进程正常退出。

随后对约五秒的兼容收敛时间做了 CPU/墙钟与非侵入式栈采样。样本热点位于 Editor 对完整
`automation.get_manifest` 结果执行递归资源限制和输出 Schema 校验；Connector 主线程大部分时间
处于 Qt 事件等待，并不存在持续数秒的 127 项逐工具 Schema 兼容计算。

常规握手因此改为：完整分页读取 `tools/list`，再调用一次轻量 `automation.get_status` 取得
toolset version、Manifest digest、Profile 和 host；完整 Manifest 保留为按需诊断与审计工具。
已知且 Schema 完全相同的工具走精确快速路径，差异 Schema 才进入结构校验和方向性包含证明；
Editor/Connector cursor 使用紧凑摘要，正常握手只重建一次缓存。Manifest 总摘要复用逐工具
Schema digest，不再把全部 Schema 原文重复规范化。

同一稳定 Editor 上的真实复测结果如下：

| 路径 | 优化前 refreshing → compatible | 优化后 | 缩短 |
|---|---:|---:|---:|
| `2026-07-28` | 4,881.07 ms | 455.82 ms | 90.7% |
| `2025-11-25` | 5,041.10 ms | 509.84 ms | 89.9% |
| `2025-06-18` | 5,052.75 ms | 509.13 ms | 89.9% |

三种路径从 Connector 进程启动到 compatible 均约 1.07～1.08 秒，比优化前缩短约 82%。稳定态
连续 40 次 `connector.get_status` 中位数 5.46 ms、p95 5.89 ms；133 项 `tools/list` 为
535.74 ms。未通过提高工具调用超时掩盖耗时。冷 Debug Editor 首次建立工具目录与摘要仍存在一次性
初始化波动，本轮为 8.16～13.39 秒；完成一次初始化后连续两轮均回到上述稳定结果，因此冷路径与
常规重连结果分开报告，不把它归因于逐工具兼容计算。优化证据为 `E-P2-CONNECTOR-PERF-R71`。

缺陷账本保留本轮所有构建、契约、行为和测试夹具发现；全部产品缺陷均完成最小复现、定向回归
和最终全量回归。R59～R61 发生在 stdout/缓存修复之前，只作为历史诊断证据；最终放行使用
修复后的 R67～R69 以及性能优化后的 R70。

## 6. GUI 与真实 Agent Host 结果

Automation 设置页及连接配置通过 Computer Use：

- 选项菜单存在带图标的中文“自动化”入口，页面和 19 个域显示文本已本地化；
- 页面没有“本机进程访问”栏目，允许读取/写入根明确解释为自动化文件工具的路径 allowlist；
- 刷新按钮和端口输入框位于同一行、始终可用，不存在随机模式下拉框；
- 刷新、恢复、停用和重新启用时，状态与 endpoint 动态更新，持久端口保持稳定；
- STDIO 与 Streamable HTTP 配置在各运行状态下均可复制，且不含外层 `mcpServers`。

真实 Agent Host 经已配置 Connector 连接 Debug GUI Editor，使用 2026-07-28 路径完成状态、列表、
描述、查询和命令调用。真实 GUI 的代表性覆盖如下：

| 业务域 | 真实操作与可见结果 |
|---|---|
| 轨道 | rename 立即可见；撤销、重做、保存和重开后结果一致 |
| 总线 | master gain 标量修改立即可见；一次撤销恢复保存点 |
| 片段 | rename 同时更新编排区与编辑器名称；一次撤销恢复 |
| 音符 | 严格 Schema 拒绝未知字段；歌词修改、读回、重新合成与撤销通过 |
| 参数曲线 | capability/get、draw 和撤销通过；曲线车道立即变化并恢复 |
| 历史记录 | 多域 get_state、undo、redo、revision 与 savepoint 往返通过 |
| 播放 | 真实播放位置推进、停止；Debug Connector 的 play/get/stop 闭环通过 |
| 持久 loop | set_loop 产生一条历史记录和可见循环区间；新 Connector 调用 undo 后恢复保存点 |

测试工程从隔离副本打开，五条轨道、音符、音素、音高曲线和非平直合成波形均正常显示；保存后
重开保留真实 MCP 轨道编辑。歌词重新合成期间的派生更新会推进 revision，使用旧 revision 的撤销
被正确拒绝；查询实际 revision 后撤销成功，这一时序未被误记为工具失败。

Speaker Mix 在真实进程中完成目录与 Schema 发现；所选片段没有已分配的 voice context，因此未把
真实 mutation 冒充为通过，其 mutation 由确定性 CTest 覆盖。时间轴真实路径完成读取，mutation
同样以确定性 CTest 结果为准。以上资格边界不改变 127 项工具的确定性测试结论。

## 7. 数据安全、隐私与清理

- 用户提供的源 fixture 仅用于读取；所有写操作均在测试拥有的隔离副本中完成。
- 最终完整性审计比较 19 个文件、224,169,615 字节及逐文件 SHA-256，全部与基线逐字节一致。
- 测试拥有的 Editor 正常关闭，Debug Connector 全部退出；用户既有 Connector 进程未被修改。
- 成功路径未观察到崩溃对话框、模态阻塞、Qt platform/offscreen 错误或意外 stderr。
- 原始日志、截图、fixture 清单、运行时身份和诊断产物保存在仓库外私有归档；正式文档只引用匿名证据。
- 本报告未记录用户名、本机绝对路径、素材名称、进程号、实例标识或控制端口。

关键匿名证据为：`E-P2-CONTRACT-BASELINE`、`E-P2-CONFIGURE-R63`、`E-P2-BUILD-R64`、
`E-P2-TARGETED-R65`、`E-P2-CTEST-CATALOG-R66`、`R67`～`R70`、
`E-P2-CONNECTOR-PERF-R5`、`E-P2-CONNECTOR-PERF-R71`、`E-P2-REAL-HOST-R62`、`E-P2-GUI-SETTINGS`、
`E-P2-GUI-HOST-ROUNDTRIP`、`E-P2-GUI-CLEAN-CLOSE` 和 `E-P2-SOURCE-INTEGRITY`。

## 8. 最终通过清单

- [x] 127 个 Editor 工具、6 个 Connector 工具和 19 个业务域的集合、类型与版本不变量成立。
- [x] 全部公共工具具有严格契约、Registry/Connector 可达性和确定性测试覆盖。
- [x] 2025-11-25 与 2026-07-28 两套主协议及 2025-06-18 兼容握手通过。
- [x] 2026-07-28 无 initialize 生命周期，真实 Connector 未发送 initialize。
- [x] Profile/Custom、File Guard、Admission、Manifest、兼容、QLocal、HTTP 和 stdio 通过。
- [x] 大 stdout 修复经慢读定向测试、真实 133 工具大帧和三轮完整回归验证。
- [x] 常规握手不再拉取完整 Manifest，三协议稳定刷新耗时约 0.46～0.51 秒。
- [x] L3/Custom/host 动态摘要基准通过，不再固定使用 L2 导致兼容状态误报。
- [x] 设置页、配置复制、真实 Agent Host、GUI 即时呈现、历史记录及保存重开通过。
- [x] 修复后三轮完整 CTest 198/198 通过，无失败、超时或 flaky。
- [x] 性能优化后的追加完整 CTest 66/66 通过，无失败或超时。
- [x] 源 fixture 完整性、正式文档脱敏和测试进程清理通过。

综上，二期候选满足当前公共工具集、协议、Connector、Editor、GUI 与安全门禁，最终判定为
**PASS**。
