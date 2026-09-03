# 三期 Headless 契约矩阵

## 1. 口径

本矩阵只记录三期相对[二期公共 MCP 工具矩阵](../02-mcp-server-and-connector/public-tool-matrix.md)
的 Host capability 差集，不复制第二份 176 项公共工具目录。公共 operation 的 ID、域、Schema、
控制层级、同步模式、annotations 和版本仍由二期权威 Public Tool Contract 定义。

当前候选快照为：

```text
Headless Editor 合格集合
= 公共 176 项
- workspace 2 项
- track_panel 7 项
- clip_editor 16 项
= 151 项
```

数字用于候选说明和异常诊断。正式门禁使用编译后的权威 Contract/Registry 集合关系，不把本文
数量、表格顺序或源码文本扫描当作独立能力来源。

## 2. Host 集合关系

设：

- `P`：权威 Editor Public Tool Contract 集合；
- `G`：Contract 中 `host_availability=gui` 的集合；
- `B`：Contract 中 `host_availability=both` 的集合；
- `R`：PublicAutomationRegistry 的 binding 集合；
- `H(mode, policy)`：经过 Host gate 和 Access Policy 后的实际可发现集合。

必须满足：

```text
G ∩ B = ∅
G ∪ B = P
R = P

H(gui, policy)     = policy(P)
H(headless, policy)= policy(B)
```

当前候选的诊断关系为 `|P|=176`、`|G|=25`、`|B|=151`。控制层级或 Custom 可以进一步缩减
`H`，但不能改变工具的 Host 分类；Connector exposure 只能在 Editor 实际集合之上继续缩减。

## 3. 三期 GUI-only 差集

### 3.1 域汇总

| 差集域 | 数量 | Headless 缺少的宿主状态 | Host metadata |
|---|---:|---|---|
| `workspace` | 2 | MainWindow 主编辑区布局、面板可见性与焦点归属 | `gui` |
| `track_panel` | 7 | 轨道面板 QWidget、视口、选择、激活与自动翻页 | `gui` |
| `clip_editor` | 16 | 剪辑编辑器、钢琴/参数子区域、共享视口、选择与工具状态 | `gui` |
| **差集合计** | **25** | **需要真实窗口/视图状态** | **`gui`** |

### 3.2 差集 operation

| 域 | GUI-only operation |
|---|---|
| `workspace` | `workspace.get_state`、`workspace.set_panel_visibility` |
| `track_panel` | `track_panel.get_state`、`track_panel.set_viewport`、`track_panel.reveal_clips`、`track_panel.set_auto_page_turn`、`track_panel.select_track`、`track_panel.select_clips`、`track_panel.clear_selection` |
| `clip_editor` | `clip_editor.get_state`、`clip_editor.set_active_clip`、`clip_editor.set_time_viewport`、`clip_editor.set_auto_page_turn`、`clip_editor.show_region`、`clip_editor.piano.set_pitch_viewport`、`clip_editor.piano.reveal_notes`、`clip_editor.piano.set_edit_mode`、`clip_editor.piano.set_quantize`、`clip_editor.piano.select_notes`、`clip_editor.piano.clear_selection`、`clip_editor.parameters.set_foreground`、`clip_editor.parameters.set_background`、`clip_editor.parameters.swap`、`clip_editor.parameters.set_tool`、`clip_editor.parameters.set_value_viewport` |

除上述差集外，`P` 中所有 operation 的 Host metadata 均为 `both`。某项 `both` operation
当前实现若偶然依赖 QWidget、MainWindow 或 GUI singleton，应修复 composition 或注入边界，不得
把它静默改成 GUI-only。

## 4. 关键 both 能力

以下表格不是 151 项清单，只记录容易因当前实现依赖而被误分类的三期契约：

| 能力 | Headless 契约 |
|---|---|
| `application.get_status` | 返回真实 `document_id/revision`、`host_mode=headless`、`windows: []` |
| `application.request_exit/restart` | 无窗口生命周期；clean 接受，dirty 默认 `busy`，显式 discard 后事件循环退出/重启 |
| `documents.*` | 复用单 DocumentSession、默认 draft/commit、异步 open/import 与无人值守文件策略 |
| `tracks.*`、`clips.*`、`notes.*`、`parameters.*`、`timeline.*`、`master.*` | 使用显式文档/对象身份、History 和 revision，不依赖 selection/focus |
| `history.*` | 与 GUI 相同的 Undo/Redo 栈和 revision 语义，不执行 GUI focus/reveal |
| `playback.*` | 无 screen 时使用稳定 60 Hz fallback；设备/模块不可用返回真实能力事实 |
| `audio_clips.*`、`exports.*`、`extract.*`、`inference.*` | 复用既有 File Guard、Task、不可变快照与最终提交门，不创建对话框或 GUI workflow |
| `tasks.*` | 继续使用 TaskId、scope、终态保留和取消语义，不恢复 `operations.*` |
| `settings.*` | 配置读写在 QCore 可用；主题 query/update 不创建 ThemeManager，并表达当前界面 availability |
| `packages.*`、`voices.*`、`lyric_rules.*` | 复用应用级状态、包索引与配置存储，不要求窗口身份 |

`available=false`、模块未安装、无设备或无模型是运行时能力事实，不改变 operation 的
`host_availability=both`，也不能被误报为 Host capability 缺失。

Headless 控制台终止信号不属于公共集合 `P`，也不产生新的 Contract/Registry binding。它是宿主级
本机进程控制入口，在 Qt 主线程以 `InternalAutomation` 调用现有 application Facade，并固定采用
discard exit。因而它绕过公共 Policy/File Guard，但与 `application.request_exit` 共用 busy 判断、
退出调度和资源清理；任何状态或结果仍不生成 WindowId。GUI Host 不安装此入口。

单实例 `OpenProjects` 同样不增加公共 operation。Headless 在首次 Automation ready 前必须将位置参数
及最终屏障前已确认的 QLocal 转发请求送入同一串行 open queue，并等待其 Task 终态。屏障原子暂停
open/activate dispatch；屏障后的请求暂存且不 ACK，ready 后再作为运行期请求恢复。该时序不改变
DocumentId、revision 或单文档替换契约。

## 5. Window 与 Document 身份

| 场景 | `document_id` | `window_id` |
|---|---|---|
| GUI status | 一个真实活动文档 | 一个真实窗口 |
| Headless status | 一个真实活动文档 | 不存在；`windows: []` |
| both 文档 operation | 按契约显式提供 | 不接受、不推导 |
| GUI-only operation on GUI | 涉及工程状态时显式提供 | 显式真实 ID |
| GUI-only operation on Headless | Host gate 直接拒绝 | 不读取、不校验、不要求伪造 |
| application-scoped Task | 按既有契约可为 null | 不生成 |

`SingleWindowContext` 在 Headless 为 `std::nullopt`。实现和 codec 不允许使用 null UUID、全零
UUID、固定 UUID、DocumentId 转换或任何 sentinel 代替窗口。

## 6. 门禁顺序与错误

PublicAutomationRegistry 的执行顺序固定为：

```text
known operation
→ host capability
→ control level / custom permission
→ input schema
→ file guard
→ admission
→ typed handler
```

| 情形 | 核心/Connector 错误 | Native JSON-RPC |
|---|---|---|
| operation/method 不存在或未注册 | `tool_unavailable` | `-32601 Method not found` |
| operation 存在但当前 Host 不支持 | `host_capability_unavailable` | JSON-RPC 业务 error，`error.data.code` 同值 |
| Host 支持但控制策略禁止 | `permission_denied` | JSON-RPC 业务 error，`error.data.code` 同值 |
| Editor 实例不存在 | `editor_not_running` | 不适用；连接目标不存在 |
| Editor 已知但 Connector 未连接 | `editor_not_connected` | 不适用；Native 直连 |

已知 GUI-only method 在 Headless 上必须先返回 `host_capability_unavailable`，即使请求缺少或提供
非法 `window_id`。`host_unavailable` 不再是对外值，也不保留兼容别名。

## 7. 协议观察面

| 入口 | GUI Host | Headless Host |
|---|---|---|
| Native `/automation/v1` | route 不存在 | 固定存在；已知 GUI-only method 可被 Host gate 拒绝 |
| Editor MCP `/mcp` | 按配置启用；实际工具集合为 `policy(P)` | 按配置启用；实际工具集合为 `policy(B)` |
| MCP `tools/list` | 不超过 GUI 当前 Policy 集合 | 不超过 151 项候选集合及当前 Policy |
| Connector wrapper | 只调用 Editor 实际可用目标 | 固定 wrapper 描述可保留；实际 GUI-only 调用返回 `host_capability_unavailable` |
| `editor.tools.list/search/describe` | 反映 GUI 实际集合 | 只反映 Headless 实际集合 |

Native 不提供工具目录。`automation.discover` 不属于 `P`，在 Native 上按未知 method 处理；MCP
的标准 `tools/list` 和 Connector 的既有工具查询继续承担各自协议的发现职责。

## 8. `value_sources` 约束

- 每个 `value_source` provider 必须属于权威 Public Contract，并具备与目标字段相容的 Host
  availability。
- Headless 可执行的 `both` operation 不得依赖 GUI-only provider 才能获得必需输入。
- 固定 enum 继续由封闭 Contract/Schema 提供，不为 Native 新建 provider 或 Catalog。
- provider 的模块/设备 availability 是返回数据，不修改其 Host metadata。
- Contract、生成 Schema、Registry binding、Editor MCP 与 Connector 已知描述中的 Host metadata
  必须来自同一声明。

## 9. 契约测试门禁

测试必须证明：

1. `P` 中 operation ID 唯一，`G/B` 互斥且并集等于 `P`。
2. `G` 精确等于 workspace、track_panel、clip_editor 三个差集域；候选数量异常时输出集合差异。
3. `R=P`，每项 binding 的 Host metadata 与 Contract 相等。
4. GUI 与 Headless 的 `H(mode, policy)` 分别满足 Host gate 和最新 Access Policy。
5. 所有 `value_sources` 的 provider 存在、可达且 Host metadata 合法。
6. Headless GUI-only 调用在 Schema、File Guard、Admission 和 handler 前被拒绝。
7. Headless status 为一个真实 document、零 windows，返回结构不含伪造 WindowId。
8. `tool_unavailable`、`host_capability_unavailable`、`permission_denied` 与 Editor 连接错误不混淆。
9. 代码、Wire 响应、Connector 状态和对外 descriptor 不再产生 `host_unavailable`。
10. Headless 控制台终止复用 application Facade，GUI 不安装 handler，且不改变 `P/G/B` 集合。
11. Native、MCP、Connector 与直接 Facade 的代表业务错误保留同一 AutomationError code/details。
