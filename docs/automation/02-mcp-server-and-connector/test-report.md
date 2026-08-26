# 二期 MCP Server 与 DS Connector Lite 最终测试报告

## 1. 结论

本轮按当前公共工具设计重新构建并验证 Editor、DS Connector Lite 与全部测试目标。最终候选在
Windows x64 Debug 环境完成全目标构建，随后在同一源码和构建产物上串行执行三轮完整 CTest，
结果为 **198/198 通过、0 失败、0 超时**。

当前公共工具分母为 Editor **134** 项、Connector **6** 项、合计 **140** 项；Editor 类型分布为
**37 Q/S + 87 C/S + 10 C/A**，Profile 累积数量为 Meta 4、L1 91、L2 134、L3 134。
toolset version 与全部逐工具版本继续保持为 1。

本轮新增或调整的文档统计、最近项目、Speaker Mix 预设、有界参数查询和显式锚点曲线操作均已
进入严格契约、公共 Registry、Connector 已知描述及确定性测试。最终判定为 **PASS**。

## 2. 候选与执行摘要

| 项目 | 结果 |
|---|---|
| 分支 | `mcp` |
| 平台 | Windows x64、VS x64 DevShell、Qt 6 MSVC x64、Ninja、vcpkg `x64-windows` |
| Debug 全目标构建 | `cmake --build --preset debug --target all`，退出码 0 |
| 本轮核心 MCP 定向回归 | 6/6 通过，0 失败，59.36 秒 |
| Catalog 基线修复定向回归 | 2/2 通过，0 失败 |
| 完整 CTest 第 1 轮 | 66/66 通过，0 失败，79.24 秒 |
| 完整 CTest 第 2 轮 | 66/66 通过，0 失败，79.14 秒 |
| 完整 CTest 第 3 轮 | 66/66 通过，0 失败，78.97 秒 |
| 三轮合计 | **198/198 通过**，0 失败，0 超时，237.35 秒 |

完整轮次固定使用单并发 `-j 1`，并显式设置 Qt offscreen platform 与有效插件目录。三轮之间没有
修改源码、重新配置或重新构建。

## 3. 工具集合与契约结果

公共集合测试确认：

- Editor 134 项工具分属 19 个业务域，Connector 固定提供 6 项桥接工具；
- `application.get_info` 等 Meta 工具 4 项，L1 累积 91 项，L2/L3 累积 134 项；
- 公开集合中不存在 `project.get`，其内部 Facade operation 不会进入 MCP 发现面；
- `documents.list_recent`、四项 `speaker_mix.presets.*`、
  `parameters.create_anchor_curve` 与 `parameters.merge_anchor_curves` 均出现在预期 Profile；
- `formats.list` 保持 L2；本轮没有新增音符 cent shift 或 line break 编辑工具；
- Contract、Registry、Manifest、Editor `tools/list`、Connector 已知工具表及测试期望集合精确一致；
- input/output JSON Schema、操作类型、同步模式、Profile、工具版本和动态值来源逐项通过门禁。

`TestPublicAutomationContract`、`TestPublicAutomationRegistry`、`TestAutomationWire`、
`TestDsConnectorLite`、`TestMcpHttpServer` 和 `TestMcpProcessIntegration` 构成本轮核心 MCP 定向
回归；相同测试随后全部包含在三轮完整 CTest 中。

## 4. 本轮能力验证

### 4.1 文档与工程

`documents.get` 已验证返回工程长度、轨道总数、空轨/纯歌声/纯音频/混合轨分类，以及片段总数、
歌声片段数和音频片段数。空工程、插入轨道与片段后的统计增量均与模型状态一致。

`documents.list_recent` 已验证只读取应用设置中的最近项目，返回路径、文件名与当前存在状态；调用
不会打开工程、切换文档、修改 revision 或写入最近项目列表。

### 4.2 Speaker Mix 预设

四项预设工具均为 L2，并归入 `speaker_mix` 域：

- `presets.list/save/delete` 管理应用级预设，不产生文档 History 或 revision；
- `presets.apply` 对轨道或片段形成一条可撤销文档编辑；
- singer 动态值会从预设输入的嵌套位置解析并校验；
- 应用预设后保留来源 ID/名称，后续直接编辑相同来源时正确标记 dirty；
- 新建、更新、名称冲突、删除、应用、Undo/Redo 与来源状态均通过。

### 4.3 有界参数查询

`parameters.get` 已验证可选半开时间范围与 `max_points`：

- 默认点数上限有效，显式上限不能被绕过；
- 锚点曲线不丢弃或降采样稳定锚点；上限不足时返回明确参数错误；
- 采样曲线按确定性步长降采样，并返回原始点数、返回点数和 `downsampled`；
- 多曲线预算不足以保留每条匹配采样曲线时明确失败；
- 相同输入重复查询产生相同输出，不修改模型或历史记录。

### 4.4 锚点曲线编辑

锚点操作已调整为显式拓扑边界：

- `create_anchor_curve` 以至少两个初始锚点创建完整曲线并返回稳定 `curve_id`；
- `insert_anchors` 必须指定既有 `curve_id`，不再隐式创建或选择曲线；
- `move_anchors` 拒绝造成曲线重叠或隐式跨曲线合并的移动；
- `merge_anchor_curves` 只合并同一参数层内相邻、完整且不重叠的两条曲线；
- 创建、插入、移动、合并以及 Undo/Redo 后，曲线和锚点 ID 均保持稳定；
- 错误路径完整预检，不产生半提交或额外历史记录。

## 5. Editor、Connector 与真实进程联调

真实进程集成测试启动实际 Debug Editor 与 Connector，而不是只调用进程内替身。测试为 Editor
探测并指定独立空闲 loopback 端口，使用隔离临时配置和临时工程根，避免与本机现有持久端口冲突。

联调路径完成：

1. 启动 Editor，等待 MCP 与 Bootstrap ready；
2. 启动 Connector，经 stdio 建立 downstream 会话并完成 upstream 发现；
3. 通过 Editor direct HTTP 与 Connector 转接分别发现并调用工具；
4. 先读取 `documents.get` 基线，再插入空轨并比较轨道增量与片段统计不变量；
5. 比较 direct 与 Connector 的工具描述、调用结果和错误语义；
6. 正常结束测试拥有的进程并释放端口与临时目录。

`TestMcpHttpServer` 同时覆盖 2025-11-25、2026-07-28 与 2025-06-18 兼容握手、loopback-only
HTTP、安全 Header、资源上限、32 路客户端在途边界、deadline 与有序停止。Connector 测试覆盖六项
桥接工具、Profile/Custom exposure、版本门槛、动态 Manifest 基准、stdio 大帧、并发、取消、超时、
EOF 与重连。

## 6. 测试中发现并修复的问题

| 问题 | 根因与修复 | 回归结果 |
|---|---|---|
| 预设保存的 singer 被误报无效 | 动态值校验未包含嵌套 `/preset/singer` 路径；补入统一 singer reference 来源 | 保存、列出、应用与删除通过 |
| 新建锚点曲线返回后 ID 立即失效 | `AnchorCurve` 历史快照复制时重新生成曲线和节点 ID；复制语义改为保留稳定身份 | 创建、后续编辑、Undo/Redo 通过 |
| 预设应用后的直接编辑未标 dirty | 同来源预设 ID 被误当作新的干净来源 | 来源保留且 dirty 状态通过 |
| 真实 Editor 偶发端口占用 | 测试复用了 Windows 动态端口候选 | 改为启动前探测独立空闲 loopback 端口，连续三轮通过 |
| 一期 Catalog 数量门禁失败 | 新增两项内部锚点 operation 后显式基线仍保留旧数量 | Catalog 明细与总数同步，定向 2/2 及三轮全量通过 |

首次失败日志被保留，修复后先运行最小回归，再从第一轮重新开始三轮完整 CTest；没有使用
`--repeat until-pass` 隐藏失败。

## 7. GUI 与数据安全口径

本轮没有重新执行人工 Computer Use/视觉验收，也不复用旧工具集的 GUI 截图作为当前工具分母的
通过证据。完整 CTest 已覆盖 Editor controller、钢琴窗交互、锚点编辑、Undo/Redo、主题与语言等
确定性 GUI 组件测试；实际 Editor + Connector 进程联调证明业务状态经真实产品链路提交。

本轮测试没有读取或修改用户提供的素材源目录，也没有在其中创建文件。全部运行时写入发生在测试
拥有的临时目录；测试退出后由 fixture 清理。原始构建日志、三轮 CTest、首次失败与修复后回归记录
保存在仓库外私有归档，仓库文档不写用户名、绝对路径、端口、进程 ID 或素材名称。

## 8. 最终通过清单

- [x] Editor 134、Connector 6、合计 140 项的集合与 Profile 分母成立。
- [x] `project.get` 不在公共集合，`documents.get` 统计与 `documents.list_recent` 通过。
- [x] Speaker Mix 预设归入 L2 `speaker_mix` 并通过应用级与文档级原子性验证。
- [x] `parameters.get` 有界出口、锚点精确保留和采样曲线确定性降采样通过。
- [x] 显式创建、插入、移动和合并锚点曲线及稳定 ID/Undo/Redo 通过。
- [x] Editor、Connector、Wire、HTTP、Bootstrap 与真实进程联调通过。
- [x] Debug 全目标构建通过。
- [x] 三轮完整 CTest **198/198 通过**，0 失败，0 超时。
- [x] 私有证据、正式文档脱敏与测试临时资源清理符合约束。

综上，当前候选可交付继续体验和后续评审。
