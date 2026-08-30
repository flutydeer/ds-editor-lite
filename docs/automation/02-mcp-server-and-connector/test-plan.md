# 二期 MCP Server 与 DS Connector Lite 测试执行计划

## 1. 执行目标

本计划用于二期候选的正式全量重测、缺陷修复回归和证据归档。全部结论必须来自同一候选重新产生的证据；既往记录只可用于定位线索，不替代本轮执行。执行顺序为：

1. 冻结源码、工具清单、测试清单和环境摘要。
2. 配置并完整构建 Editor、Connector 与全部测试目标。
3. 验证 Wire、Editor 公共契约、Connector 桥接契约、Profile/Custom、安全和兼容。
4. 验证 Editor MCP 2025-11-25 与 2026-07-28 两套主协议、2025-06-18 兼容握手、QLocal Bootstrap 与 Connector stdio。
5. 完成真实进程联调、多 Connector 与 GUI。
6. 在同一候选上串行执行一次完整 CTest。
7. 完成失败修复闭环、实现报告和测试报告。

当前产品清单为 Editor 177、Connector 6、总计 183，仅作为候选快照记录。测试以权威契约源、Registry、发现面和 Connector 下游面之间的集合关系为门禁，不把工具数量、顺序、顶层场景数或断言数硬编码为正确性证明。CTest 数量由本轮配置与执行产物记录。

## 2. 环境与记录项

目标环境：

- Windows 11 x64；
- Visual Studio x64 DevShell、MSVC 与 Windows SDK；
- Qt 6 MSVC x64，包含项目要求的模块；
- CMake preset、Ninja、vcpkg `x64-windows`；
- Debug 配置，`LITE_BUILD_TESTS=ON`；
- GUI 桌面会话与 Computer Use；
- 可用时接入真实格式、声音、推理、播放和 Agent Host 资格环境。

本轮环境摘要至少记录：commit、branch、工作树状态、submodule、编译器/SDK、Qt、CMake、Ninja、vcpkg triplet、关键依赖、时区、测试 seed 和 GUI 会话类型。仓库报告只保存公开摘要；完整工具链输出进入私有归档。

## 3. Phase2 私有证据归档

原始证据保存在仓库外的 Phase2 私有归档根。真实位置只写入归档自己的环境快照，仓库文档使用相对 Evidence ID。

建议结构：

```text
00-baseline/
01-configure-build/
02-contract-and-domains/
03-editor-mcp/
04-bootstrap/
05-connector/
06-process-integration/
07-gui-and-qualification/
08-ctest/
09-failures-and-fixes/
10-final/
work/
```

每阶段保存：

- 实际命令、开始/结束时间、退出码、stdout/stderr；
- CTest JSON/XML、测试目标与 case 清单；
- 结构化 MCP/QLocal 请求响应、协议版本和状态时序；
- 进程、端口、listener、QLocal 与资源快照；
- GUI 截图/录屏与可见结果；
- failure ledger、修复 commit、回归链；
- 临时产物所有权和 cleanup manifest。

Evidence 索引使用归档内相对路径。敏感字段、用户文本、绝对路径和大块内容在进入可公开报告前匿名化。

## 4. Fixture 与写入隔离

仓库正式文档只使用“用户提供只读 fixture 根”这一称谓。执行约束：

- fixture 根保持只读；Editor、Connector、测试和辅助脚本均不原地写入。
- 每项素材分配匿名 fixture ID；绝对路径、原始文件名、目录清单、大小和内容 hash 仅进入私有归档 manifest。
- open/save/import/export/relocate/extract/inference/cache 等可能写入的场景，先复制必要输入到测试拥有的隔离工作区。
- File Guard 的写 allowlist 指向隔离工作区；只读检查使用独立 read grant。
- Agent 新增工程、导出物、日志、配置、端口记录和可重建缓存均标注所有者与创建时间。
- 证据固化后可清理 Agent 新增产物与可重建缓存；原始 fixture、用户既有文件和来源未知进程保持原状。
- 清理使用精确路径和所有权清单，避免宽目录递归操作。

仓库报告只引用匿名 fixture ID 与资格类型，不记录素材名称或路径。

## 5. 单实例与串行进程约束

GUI、进程测试与开发构建共享一个全局 Editor Primary。所有启动 Editor 或使用全局 QLocal 服务的阶段串行执行：

- 相关 CTest 设置 `RUN_SERIAL TRUE` 和统一 resource lock。
- 完整 CTest 使用 `-j 1`。
- 每次启动前检查 Editor/Connector、全局锁、QLocal 服务和预期端口。
- 发现来源未知的 Editor 时停止该阶段并记录，不主动结束用户进程。
- 测试 fixture 只管理自己创建的 PID/process handle、端口、socket 和临时根。
- 一个场景结束后等待进程退出、pipe EOF、QLocal 服务消失和端口释放，再进入下一场景。
- 多 Connector 场景由一个串行 fixture 统一管理所有子进程和 cleanup。
- 自动组件轮显式设置 `QT_QPA_PLATFORM=offscreen`，并从当前 Qt 安装解析有效 platform plugin 路径；若插件加载失败，先修正环境再重跑，不把环境闪退记为业务结论。
- 真实 GUI 轮单独运行，持续监控模态窗口、顶层窗口数量、进程心跳和无响应状态；出现弹窗或卡住时先固化证据，再由测试控制器处理测试拥有的窗口/进程。
- crash/timeout 先保存 dump、日志和运行时快照，再精确结束测试拥有的进程。

进入 GUI 阶段前结束全部自动测试进程；GUI 阶段结束后再次确认 Primary、QLocal 和端口释放。

## 6. 命令基线

配置与构建通过项目标准 DevShell/preset 入口执行。核心命令模板：

```powershell
cmake --preset debug
cmake --build build/Debug --target all
ctest --test-dir build/Debug -N
ctest --test-dir build/Debug --show-only=json-v1
ctest --test-dir build/Debug --output-on-failure -j 1 -R "Automation|Mcp|Connector|SingleInstance"
ctest --test-dir build/Debug --output-on-failure -j 1
```

正式归档保存 wrapper 展开后的实际命令、工作目录和退出码。`debug` build preset 的默认目标是两个产品可执行文件，因此正式测试构建在 preset 配置目录上显式构建 `all`，覆盖 `DsEditorLite`、`DsConnectorLite`、AutomationWire 及全部 tests。若 preset 的实际 build 目录不同，以 configure 产物为准，并在报告中记录匿名化后的相对目录。

禁止使用 `--repeat until-pass` 形成结论。压力测试使用固定 seed；随机失败保存 seed 并收敛为确定性回归。

## 7. 阶段 A：基线与静态审计

### 执行

1. 记录 git/submodule/工具链和现有进程状态。
2. 执行 `git diff --check` 与敏感信息扫描。
3. 以编译后的公共契约枚举为唯一工具清单，验证 ID 唯一，并确认公共集合不含 `project.get`；Registry binding 与 Editor `tools/list` 必须与相应授权后的契约集合相等，不使用源码文本扫描或手工镜像替代行为验证。
4. 验证 Connector 桥接定义唯一，downstream 集合等于当前 exposure 下可用的 Editor 工具与桥接工具之并集。
5. 核对域、category、Query/Command、同步模式、Profile 和版本元数据的合法性及关键语义归属，不以各域固定数量作为门禁。
6. 核对全局 `toolset_version` 与每工具 `minimum_toolset_version` 的版本关系。
7. 核对 `tasks.list/get/cancel` 的语义及 Dispatcher 代表性显式路由。
8. 获取 `ctest -N` 与 JSON 清单，区分 target 数、CTest case 数和源码顶层场景数。

### 门禁

权威契约存在重复或非法元数据，或 Registry、发现面、Connector 下游面与其应有集合关系不一致时，先修复再进入构建。清单新增或删除本身不因固定数量断言而失败。

## 8. 阶段 B：配置、完整构建与纯单元测试

### 执行

1. Debug configure，确认 `LITE_BUILD_TESTS=ON`、Qt 与 vcpkg 配置。
2. 完整构建所有产品和测试目标。
3. 运行 JSON Schema、codec、base64url cursor、Profile 与版本单元测试。
4. 运行 Access Policy、Custom、File Guard、global/background Admission、exposure 与版本兼容单元测试。
5. 运行 Automation Core、显式 Dispatcher 路由、历史记录、revision、显式幂等、Task 与 document 回归。
6. 使用固定 seed 运行路径、分页上下文与版本门槛边界语料。

### 门禁

Configure/build 零错误；生成物可复现；受影响回归和二期纯单元测试完成。任一失败进入第 16 节流程。

## 9. 阶段 C：域契约与 Registry

### 执行

1. 验证权威契约 ID 唯一、Schema 可通过 meta-schema 检查，Registry binding 与授权后的 Editor `tools/list` 集合分别等于契约源。
2. 验证 Profile 集合逐级包含、L3 等于完整公共契约集合，并验证语义上固有的 L0 工具不进入 Custom 设置且不能被任何权限配置禁用。
3. 按 Query、同步 Command、异步 Command 和特殊参数形状选取代表工具，验证 Registry JSON 映射与结果封装；不为每个工具生成一份同构 smoke 调用。
4. Schema-invalid、权限关闭、host unavailable、错误优先级、revision 与通用失败封装在各自共享层验证一次；领域测试仅补充工具特有的拒绝条件。
5. 由确定性语料覆盖各域独有的成功、no-op、validate-only、冲突、不可用、失败原子性和异步终态；真实产品会话按风险与可见行为选择代表路径，不设置逐工具场景配额。
6. 验证历史记录原子边界：同类批量操作整体撤销；轨道/总线/剪辑标量、已有音符歌词/语言/长度互不捆绑。
7. 验证创建深度：轨道只能创建为空轨道，歌声剪辑只能创建为空剪辑，音符叶节点可携带完整初始数据且不要求 voice context；剪辑省略长度时验证跨拍号的四小节默认值和 tick 上界拒绝；音符量化以宽整数计算候选几何并拒绝越界结果。
8. 验证 `tracks.get` 直接返回轨道属性、统计和 voice/default-language 上下文，`clips.get`
   对歌声剪辑直接返回 own/effective voice、继承来源和有效默认语言；公共集合没有独立的
   voice-context 查询工具。
9. 验证 `audio_clips.relocate/confirm_path` 同步返回 Mutation、不创建 Task，并在 GUI 中立即反映。
10. 验证 `playback.set_loop/set_loop_enabled/clear_loop` 形成工程持久历史记录，逐项 Undo/Redo；play/pause/stop/seek 保持瞬时、目标状态幂等，播放头在查询与命令间变化不导致版本冲突。
11. 验证动态值发现、异步任务和文件重新授权；open/import/import_batch 在开始和提交边界检查
    当前读权限，带 plan digest 时加载检查器返回的同一字节快照并拒绝随后换内容；`formats.inspect`
    在解析前拒绝超过 64 MiB 的输入，MIDI 解析、LibreSVIP 转换和摘要复用同一有界快照；
    Pitch/MIDI 提取只解码哈希绑定的音频快照，完成后复核原文件摘要、剪辑身份和源路径权限。
    正常 invocation 不自动回查 provider，output Schema 由确定性契约测试
    覆盖，运行时不逐次 assert。
12. 验证零 speaker 声库可用完整 SingerRef 设置到轨道/剪辑；单 speaker 自动解析，多 speaker 候选可由 `voices.describe` 查询，缺少选择时稳定拒绝；构造 package ID 与 singer ID 相同、package version 不同的两个并存版本，在 L2 明确拒绝 `packages.*` 的前提下验证 list、describe、轨道/剪辑设置、Speaker Mix/预设和回读均按版本精确闭环。
13. 验证 `documents.get` 的工程长度、轨道/剪辑总数和分类统计；验证 `documents.list_recent` 只读取应用设置且不修改当前文档。
14. 验证 `parameters.get` 的半开范围、默认/显式点数上限、采样曲线确定性降采样，以及锚点曲线在上限不足时明确失败而不丢失稳定 ID。
15. 验证 `parameters.create_anchor_curve`、显式 `insert_anchors`、跨曲线移动拒绝和 `merge_anchor_curves` 的相邻/重叠规则及逐步 Undo/Redo。
16. 验证 Speaker Mix 预设 list/save/delete 的应用级无文档副作用，apply 的单条 History，以及来源预设 dirty 状态。
17. 对 workspace、track_panel、clip_editor 的布局、焦点事实、共享/独立视口、选择顺序与 primary 各选代表路径验证真实 QWidget 状态；确认 revision/history 不变，且 Editor 在后台无法取得键盘焦点时已完成的显示、选择和定位不被误报为失败，也不出现模态窗口。
18. 验证拍号新增、替换和删除后的完整时间线投影使用宽整数校验；删除中间拍号会使后续
    拍号 tick 超出模型范围时，validate-only 与实际提交均原子拒绝。
19. 对 settings.query 与设置更新的公共 allowlist、domain 过滤、候选/生效/重启信息、稀疏更新、持久化和失败回滚进行分形状代表覆盖；validate-only 只覆盖音频设备、计算设备和包搜索路径更新，其他设置 update 必须拒绝该字段。
20. 对 packages.list/describe/refresh 验证读取根内路径、有效搜索路径、application task 的成功/取消/部分失败，以及索引原子切换。
21. 对 lyric_rules 验证稳定 ID 迁移、内置/自定义边界、CRUD、启停、分类内移动、非法规则回滚和 splitter→tagger 只读测试。
22. 验证音频导入、重定位和路径确认的 SHA-512 与解码结果来自同一临时快照；在快照解码完成后
    替换原始路径内容，确认提交前的后台摘要复核拒绝陈旧快照。分别验证每个文档 generation 与
    应用级作用域的活动任务不被终态历史上限淘汰、只保留最近 128 项终态，且互不影响。
23. 分别模拟文档保存、MIDI 与音频完成覆盖预检后由外部创建同名目标，确认同目录暂存文件的
    排他最终发布不会替换外部文件，也不会暴露部分 DSPX/MIDI；多文件音频发布在后续目标冲突时
    回滚本次已发布的前序文件。

### 证据

保存测试目标职责、关键关系断言、代表性业务场景和失败回归的证据索引；能力型工具记录测试 host 的 available/unavailable 事实。不维护与权威契约重复的逐工具测试名册。

Registry 测试只拥有公共适配层风险：契约与 binding 集合一致、代表性 JSON 参数/结果映射、权限与
严格 Schema 前置拒绝、同 ID 不同版本声音引用，以及异步回调跨 Registry 生命周期的隔离。编辑、
播放、设置、文件、Task 和协议行为分别由对应领域、Task/生命周期、Wire/HTTP/Connector/进程测试
拥有；Registry 不再重复这些领域的完整成功/失败矩阵。

### 门禁

Registry、Editor 发现面与 Contract 集合关系精确；公共 Schema 全部可校验；业务失败不推进不属于它的历史记录或 revision；文档编辑域完成代表性 Undo/Redo，GUI 与应用域完成 query 回读和状态恢复闭环。

## 10. 阶段 D：Editor MCP 双协议、兼容握手与 HTTP

### 执行

1. 在隔离端口启动测试 Server，确认 listener 仅为 `127.0.0.1`。
2. 执行 `2025-11-25` initialize/initialized、ping、tools/list、tools/call。
3. 执行 `2026-07-28` server/discover、逐请求 metadata、ping、tools/list、tools/call，并验证 `initialize` 被拒绝。
4. 请求 `2025-06-18` initialize，确认服务端接受并回显兼容版本，再完成 initialized、ping、tools/list 与 tools/call。
5. 验证版本协商、支持列表、header/body 镜像、协商版本对应的结果形状。
6. 验证当前授权契约集合的 MCP 描述、分页、Schema、structured/text 内容和代表性业务错误。
7. 验证 Host、Origin、method、Content-Type、Accept、body/depth/node/response 上限和 deadline。
8. 验证 global 32 路硬上限、background 8 路容量、timeout、disable、换端口和 shutdown
   计数释放；同时发出 32 个请求应全部进入，第 33 个在途请求应被拒绝且不排队。
9. 验证除 `initialize` 外的 legacy 请求必须携带存活 session；覆盖缺失、未知、已淘汰和已结束
   session，以及 128 项上限、最旧会话淘汰、DELETE 正常结束、重复结束和协议版本不符。
10. 验证 legacy session 与 Connector metadata 的连接无关身份；使用两条 HTTP 连接发送带同一
    Connector instance ID 的现代排队请求与取消通知，确认请求不进入 handler；另验证未携带实例
    ID 且 client info 相同的独立直连客户端仍按连接隔离。

### 门禁

两套主协议和 2025-06-18 兼容握手/会话都形成证据；HTTP 安全、跨连接取消、全局准入和 legacy session 保留上限生效；Server 停止后无残留 listener、session 或在途计数。

## 11. 阶段 E：QLocal Bootstrap

### 执行

1. 回归既有单实例命令和身份/服务名 golden test。
2. 验证 discover 一次性快照与 watch 初始/后续完整快照。
3. 验证状态机、endpoint ready 时机、Editor instance ID 和错误传播。
4. 验证分片/合并/非法/超限帧、timeout、写缓冲、慢 watcher 背压和数量上限。
5. 验证多 watcher、异常断开、Editor restart、PID/endpoint 变化和资源清理。
6. 验证 Connector 始终作为 Bootstrap 客户端，Editor Primary 所有权保持唯一。

### 门禁

状态与 listener 事实一致；watcher 队列有界；慢读与异常连接不会影响其他 watcher；场景后全局服务释放。

## 12. 阶段 F：Connector stdio、exposure 与 compatibility

### 执行

1. Editor 离线启动 Connector，验证 downstream 握手与六个固定桥接工具。
2. 验证 stdout 仅含 MCP 帧、stderr 承载诊断；完整 downstream 工具大响应在正常读端和延迟慢读端均无截断、无零进度误超时。
3. 分别执行 2025-11-25 与 2026-07-28 两套主协议的 downstream 生命周期，并执行请求 2025-06-18 的兼容握手和结果塑形；2026-07-28 不执行 `initialize`。
4. 验证 upstream 优先执行 2026-07-28 发现，回退到 2025-11-25 初始化，并接受协商到 2025-06-18。
5. 验证每轮握手完整读取所有 `tools/list` 页后只调用一次 `application.get_status`，不调用额外的并行工具目录接口。
6. 验证 ID 重映射、并发乱序、notification、取消、timeout、EOF、broken pipe 与 backpressure。
7. 验证 `l0/l1/l2/l3`、include/exclude、三类 selector、pending、非 L0 工具的 exclude 优先级，以及 L0 不可排除约束。
8. 验证同一 exposure 约束类型化工具与 list/search/describe/invoke；泛化 list/search 只返回摘要，describe 返回完整 Schema。
9. 验证双方全局 `toolset_version` 与逐工具 `minimum_toolset_version` 门槛；不运行 Schema
   方向性/子集/digest 兼容计算，同版本 Schema 差异直接使契约测试失败。
   Connector 只校验自身桥接工具 envelope，类型化工具和泛化 invoke 的业务 Schema 由 Editor MCP 校验。
10. 验证各 Profile 的 downstream 集合等于相应 exposure 下可用的 Editor 工具与固定桥接工具之并集，且 Profile 逐级包含。
11. 验证 `application.request_exit/restart` 无论 preset、Custom 或 Connector exclude 都保留；Schema 只允许可选 `discard_changes`，且两项均可经类型化 wrapper 与泛化 invoke 调用。
12. 在脏工程上不传 `discard_changes`，确认返回 `busy`、字段路径正确、Editor 保持运行且没有活动模态窗口；传 `true` 后确认工具结果先返回，再完成优雅退出或重启。
13. 重启场景确认只产生一个新 Editor instance、复用原启动参数、Connector 自动重连；随后用默认策略关闭 clean 工程，确保无孤儿进程。
14. 验证 ready burst 合并、尾随刷新、退避、manual reconnect、instance/endpoint 变化。

### 门禁

stdio 零污染；工具面、exposure 与兼容结果确定；旧握手结果不会污染新 epoch；Connector 不自动重放 Command。

## 13. 阶段 G：真实进程与多 Connector 联调

### 执行

1. Connector 先启动，再启动 Editor 并启用 MCP，观察自动接入。
2. Editor 先 ready，再启动 Connector，验证首次 watch 到完整握手。
3. 在 direct HTTP 与 Connector stdio 上复用 L0/L1/L2/L3 代表语料；契约集合关系由确定性组件测试覆盖，真实 Connector 会话按业务域、调用类型和风险选择代表路径，并覆盖所有桥接工具的独特行为。
4. 在隔离工作区完成文档、格式、轨道、总线、剪辑、音频剪辑、声库、Speaker Mix、音符、参数曲线、时间线、历史记录、播放、导出、提取、推理与 Task 链路；GUI、设置、包信息和歌词规则域在同一真实进程候选中闭环。
5. 运行两至八个 Connector，并发 Query、Command、Task 和 reconnect。
6. 验证 revision conflict、全局 32 路上限、独立缓存与请求映射。
7. 运行时切换 Profile/Custom/roots/port/enabled，验证两侧状态与调用结果。
8. Editor stop/restart 与 Connector crash/exit/slow-reader 场景后检查自动恢复与资源清理。

### 门禁

Editor direct 与 Connector 转接在结果、错误、历史记录、revision 和 Task 上等价；多 Connector 无串线或饿死；所有测试拥有的资源清理完成。

## 14. 阶段 H：GUI、Computer Use 与真实资格

### Computer Use

1. 从带图标的选项菜单“自动化”项进入设置页，核对面板中文翻译、enabled、端口、Profile、Custom、roots、status 和 endpoint。
2. Connector 先启动，在 GUI 启用 MCP，观察自动连接与状态更新。
3. 切换 Profile/Custom，比较 GUI、Editor list、Connector status 和实际调用；在 Custom 下逐组核对 24 个领域卡片默认收起、展开/收起不改变权限、整组关闭/开启、启用计数和单项状态回读。
4. 核对端口刷新按钮与输入框同一行且始终可用；首次配置生成非零端口后重启不变化，刷新、直接编辑、冲突恢复、disable/enable 的状态序列正确。
5. 在 ready、disabled 和 error 状态分别复制 stdio 与 Streamable HTTP 配置；解析为单个 server entry，并确认不含外层 `mcpServers`。
6. 核对读写根说明为自动化文件工具 allowlist，且页面不存在无动态内容的本机进程访问栏目；使用隔离工作区验证允许与拒绝。
7. 通过 Connector 对轨道、总线、剪辑、音符、参数曲线、时间线、Speaker Mix、历史记录与播放逐域执行真实 mutation，观察 GUI 立即变化，并以对应 Query 核对同一状态。
8. 对第 7 步各编辑域的 mutation 使用 GUI 与 MCP Undo/Redo；细粒度 Command、批量命令和三个持久循环命令的历史记录粒度均由确定性 CTest 覆盖。
9. 对文档、格式、音频剪辑、声库、保存、导入、导出、提取、推理和异步任务逐域执行真实资格路径；环境缺少 codec、声音、模型或音频设备时保存结构化不可用事实，同时由确定性 CTest 覆盖可用分支。
10. 同时运行多个 Connector，结束其中一个后验证其余链路。
11. 使用 CLI override 启动测试拥有的 Editor，核对来源显示与持久配置保持。
12. 对 workspace、track_panel、clip_editor（含 piano/parameters）的布局、焦点事实、选择与共享视口分别建立代表性 GUI 场景；增加 Editor 处于后台的调用，确认键盘焦点仅尽力获取且不会推翻已完成操作，并保存调用前后截图、query 回读和恢复证据。
13. 对设置更新的主要形状、包刷新与歌词规则管理选择代表路径观察 GUI/应用即时状态、重启后持久状态和失败回滚；全过程监测顶层窗口与活动模态窗口。

### Agent Host 与环境资格

使用临时测试配置启动一个真实 Agent Host，经 stdio 调用 status、list、describe；通过泛化调用覆盖 L3 的代表性 GUI、设置、包和歌词规则路径，并以业务域代表性 Query/Command/Task 覆盖其余产品链路，同时执行各 Connector 桥接工具的独特行为。公共集合、权限与 Schema 的通用不变量由同一候选的组件测试完成。异步工具同时保存 Task 创建、查询与终态证据。配置只在隔离工作区存活，结束后进入 cleanup manifest。

格式、声音、模型、codec 和音频设备资格逐项探测；环境条件与确定性测试分母分开记录。环境依赖不满足时必须同时具备真实结构化不可用证据与确定性可用分支证据，不能以“未执行”代替域覆盖。

### 门禁

24 个 Editor 业务域均具有确定性覆盖与真实代表路径，可见 UI 域另有 GUI 证据；无模态阻塞、UI 假死、offscreen 插件错误或用户文件写入；GUI 可见状态与 MCP 事实一致；所有测试新增产物进入清理清单。

## 15. 阶段 I：完整 CTest

GUI 阶段结束并确认全局资源释放后，在同一 commit、同一构建产物上执行一次：

```powershell
ctest --test-dir build/Debug --output-on-failure -j 1
```

本轮测试数量以 `ctest -N` 实测为准。保存实际命令、CTest 清单、开始/结束时间、退出码、总数、
逐 case 结果、耗时、失败/timeout、XML/JSON 与环境摘要。

任一失败、timeout、crash 或残留都必须进入第 16 节回归链；修复后先执行最小复现和所属域，再
重新执行一次完整 CTest。

完整 CTest 后执行：

- `git diff --check` 与工作树审计；
- Editor/Connector、Primary、QLocal、端口、pipe 和临时目录残留检查；
- 权威契约、Registry、发现面、Connector downstream 与 CTest 清单最终快照；
- Evidence 索引、failure ledger 和 cleanup manifest 完整性检查。

## 16. 缺陷修复与回归门禁

每个失败建立匿名 failure ID，保留首次失败证据。修复流程：

1. 在改动前复现并缩小；偶发问题保留时序、seed、进程和资源快照。
2. 定位协议、业务、线程、文件、配置或测试契约根因。
3. 修改唯一实现路径，保持 Schema、安全与断言强度。
4. 执行最小复现。
5. 执行所属组件完整测试域。
6. 执行受影响的 Facade、历史记录与 GUI 回归。
7. 执行 Editor direct + Connector stdio 等价联调。
8. 涉及 UI 或生命周期时重做对应 Computer Use 场景。
9. 重新构建受影响目标并执行一次完整 CTest。

协议、安全、越权文件访问、数据损坏、crash、stdout 污染、错误文档写回和历史记录/revision 破坏为阻断级。阶段性修复使用独立 `fix(scope): summary` 提交，私有证据保持在归档中。

## 17. 报告与最终清理

### 实现报告

`implementation-report.md` 记录：

- 177 + 6 工具与 24 个域；
- Wire/Registry/Profile/Custom；
- Editor MCP 2025-11-25 与 2026-07-28 两套主协议、2025-06-18 兼容握手与 QLocal；
- Connector stdio/exposure/compatibility；
- File Guard、Admission、设置、CLI 与生命周期；
- 当前代码事实、实现边界与长期保护测试。

### 测试报告

`test-report.md` 由本轮执行回填：

- 候选身份与环境摘要；
- 实际命令、退出码和 Evidence ID；
- 工具/域/协议/安全/兼容/Profile/Custom/Bootstrap/Connector/Editor/联调/GUI 结果；
- 失败与修复轨迹；
- 一次完整 CTest；
- 资格项、残余风险和 cleanup；
- 最终判定。

### 最终清理

先验证证据索引可读，再按所有权清单清理 Agent 新增 scratch、临时配置、工作副本、端口记录和可重建缓存。仓库报告仅保留匿名 Evidence ID、公开结果和可复现命令模板。
