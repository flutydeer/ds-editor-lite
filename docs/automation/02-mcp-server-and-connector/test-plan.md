# 二期 MCP Server 与 DS Connector Lite 测试执行计划

## 1. 执行目标

本计划用于二期候选的正式测试、缺陷修复回归和证据归档。执行顺序为：

1. 冻结源码、工具清单、测试清单和环境摘要。
2. 配置并完整构建 Editor、Connector 与全部测试目标。
3. 验证 Wire、127 个 Editor 工具、6 个 Connector 工具、Profile/Custom、安全和兼容。
4. 验证 Editor 三版本 MCP、QLocal Bootstrap 与 Connector stdio。
5. 完成真实进程联调、多 Connector 与 GUI。
6. 在同一候选上串行执行三轮完整 CTest。
7. 完成失败修复闭环、实现报告和测试报告。

冻结分母为 Editor 127、Connector 6、总计 133。CTest 数量、顶层场景数和断言数由本轮配置与执行产物分别记录，彼此不替代。

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
08-ctest-rounds/
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
- 三轮完整 CTest 使用 `-j 1`。
- 每次启动前检查 Editor/Connector、全局锁、QLocal 服务和预期端口。
- 发现来源未知的 Editor 时停止该阶段并记录，不主动结束用户进程。
- 测试 fixture 只管理自己创建的 PID/process handle、端口、socket 和临时根。
- 一个场景结束后等待进程退出、pipe EOF、QLocal 服务消失和端口释放，再进入下一场景。
- 多 Connector 场景由一个串行 fixture 统一管理所有子进程和 cleanup。
- 自动组件轮可使用 offscreen platform；真实 GUI 轮单独运行，并持续监控模态窗口和进程心跳。
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
3. 解析 `PublicToolDefinitions`、测试期望和公共矩阵，核对 `P2-TOOL-001～127`。
4. 核对六个 Connector 桥接工具及 127 + 6 = 133。
5. 核对 19 个 Editor 域数量、`bus` category 和三项历史记录域。
6. 核对 toolset v1 与每工具 current/introduced/minimum-compatible 均为 1。
7. 核对 `tasks.list/get/cancel` 与一期受影响集合。
8. 获取 `ctest -N` 与 JSON 清单，区分 target 数、CTest case 数和源码顶层场景数。

### 门禁

ID、追踪号、域、Profile、类型、版本或集合出现缺口、重复或漂移时先修复契约源和期望，再进入构建。

## 8. 阶段 B：配置、完整构建与纯单元测试

### 执行

1. Debug configure，确认 `LITE_BUILD_TESTS=ON`、Qt 与 vcpkg 配置。
2. 完整构建所有产品和测试目标。
3. 运行 JSON Schema、codec、Manifest、digest、游标、Profile 与版本单元测试。
4. 运行 Access Policy、Custom、File Guard、Admission、exposure 与 Schema compatibility 单元测试。
5. 运行一期 Catalog、Core、History、revision、idempotency、Task、document 与 architecture 回归。
6. 使用固定 seed 运行路径、兼容和令牌桶边界语料。

### 门禁

Configure/build 零错误；生成物可复现；受影响回归和二期纯单元测试完成。任一失败进入第 16 节流程。

## 9. 阶段 C：127 项域契约与 Registry

### 执行

1. 精确比较 127 项 ID、顺序、域、Profile、Query/Command、sync mode、Schema 和版本。
2. 精确比较 Profile 累积数量 Meta 4、L1 89、L2 127、L3 127。
3. 对全部 127 项生成 schema-valid 输入并验证 Registry binding 可达。
4. 对每项执行 schema-invalid、权限关闭或 host unavailable 的适用拒绝路径。
5. 按 19 个域执行代表性真实成功、no-op、validate-only、revision conflict 和失败原子性。
6. 对 15 个拆分标量写操作验证单 History entry 与单 revision。
7. 验证动态值来源、output Schema 自检、异步任务和文件重新授权。

### 证据

保存逐工具追踪表，列出 Contract、Registry、授权、Editor MCP 与 Connector 的证据 ID；能力型工具记录测试 host 的 available/unavailable 事实。

### 门禁

127 项无漏项；Registry 与 Contract 集合精确；业务失败不推进 History/revision；每个域至少完成一个真实成功与 Undo/Redo 闭环。

## 10. 阶段 D：Editor MCP 三版本与 HTTP

### 执行

1. 在动态端口启动测试 Server，确认 listener 仅为 `127.0.0.1`。
2. 执行 `2025-06-18` initialize/initialized、ping、tools/list、tools/call。
3. 执行 `2025-11-25` initialize/initialized、ping、tools/list、tools/call。
4. 执行 `2026-07-28` server/discover、逐请求 metadata、ping、tools/list、tools/call。
5. 验证版本协商、支持列表、header/body 镜像、请求版本对应的结果形状。
6. 验证 127 项 descriptor、分页、Schema、structured/text 内容和业务错误。
7. 验证 Host、Origin、method、Content-Type、Accept、body/depth/node/response 上限和 deadline。
8. 验证 global/peer/client 并发与速率、timeout、disable、换端口和 shutdown 配额释放。

### 门禁

三个协议生命周期和结果塑形都形成证据；HTTP 安全与限流在 handler 前生效；Server 停止后无残留 listener 或在途计数。

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
2. 验证 stdout 仅含 MCP 帧，stderr 承载诊断。
3. 分别执行三个协议版本的 downstream 生命周期和结果塑形。
4. 验证 upstream 2026 发现、2025-11-25 初始化及协商到 2025-06-18。
5. 验证 ID 重映射、并发乱序、notification、取消、timeout、EOF、broken pipe 与 backpressure。
6. 验证 `l0/l1/l2/l3`、include/exclude、三类 selector、pending 与 exclude 优先级。
7. 验证同一 exposure 约束类型化工具与 list/search/describe/invoke。
8. 验证版本门槛、digest fast path、输入/输出方向性 Schema 兼容和状态分类。
9. 验证 L2 downstream 集合为 127 个 Editor 工具加 6 个桥接工具，共 133 项。
10. 验证 ready burst 合并、尾随刷新、退避、manual reconnect、instance/endpoint 变化。

### 门禁

stdio 零污染；工具面、exposure 与兼容结果确定；旧握手结果不会污染新 epoch；Connector 不自动重放 Command。

## 13. 阶段 G：真实进程与多 Connector 联调

### 执行

1. Connector 先启动，再启动 Editor 并启用 MCP，观察自动接入。
2. Editor 先 ready，再启动 Connector，验证首次 watch 到完整握手。
3. 在 direct HTTP 与 Connector stdio 上复用 Meta/L1/L2 代表语料。
4. 在隔离工作区完成 project、tracks、bus、clips、notes、parameters、history、文件、Task 与 playback 链路。
5. 运行两至八个 Connector，并发 Query、Command、Task 和 reconnect。
6. 验证 revision conflict、公平配额、独立缓存与请求映射。
7. 运行时切换 Profile/Custom/roots/port/enabled，验证两侧状态与调用结果。
8. Editor stop/restart 与 Connector crash/exit/slow-reader 场景后检查自动恢复与资源清理。

### 门禁

Editor direct 与 Connector 转接在结果、错误、History、revision 和 Task 上等价；多 Connector 无串线或饿死；所有测试拥有的资源清理完成。

## 14. 阶段 H：GUI、Computer Use 与真实资格

### Computer Use

1. 打开 Automation 设置页，核对 enabled、端口、Profile、Custom、roots、status 和 endpoint。
2. Connector 先启动，在 GUI 启用 MCP，观察自动连接与状态更新。
3. 切换 Profile/Custom，比较 GUI、Editor list、Connector status 和实际调用。
4. 修改端口、制造冲突、恢复端口、disable/enable，核对完整状态序列。
5. 使用隔离工作区验证 File Guard 允许与拒绝。
6. 通过 Connector 编辑轨道、总线、片段、音符、参数、Speaker Mix 和时间线，观察 GUI。
7. 使用 GUI Undo/Redo，验证每个细粒度 Command 的 History 粒度。
8. 验证文档、保存、导入、导出、任务、播放和可用推理资格。
9. 同时运行多个 Connector，结束其中一个后验证其余链路。
10. 使用 CLI override 启动测试拥有的 Editor，核对来源显示与持久配置保持。

### Agent Host 与环境资格

使用临时测试配置启动一个真实 Agent Host，经 stdio 调用 status、list、describe、Query、Command 和 Task。配置只在隔离工作区存活，结束后进入 cleanup manifest。

格式、声音、模型、codec 和音频设备资格逐项探测；环境条件与确定性测试分母分开记录。每项结论只依据本轮实际证据。

### 门禁

无模态阻塞、UI 假死或用户文件写入；GUI 可见状态与 MCP 事实一致；所有测试新增产物进入清理清单。

## 15. 阶段 I：完整 CTest 三轮

GUI 阶段结束并确认全局资源释放后，在同一 commit、同一构建产物上连续执行三轮：

```powershell
ctest --test-dir build/Debug --output-on-failure -j 1
ctest --test-dir build/Debug --output-on-failure -j 1
ctest --test-dir build/Debug --output-on-failure -j 1
```

每轮保存：实际命令、CTest 清单、开始/结束时间、退出码、总数、逐 case 结果、耗时、失败/timeout、XML/JSON 与环境差异。轮间保持源码和构建候选不变。

任一轮失败、timeout、crash 或残留均使三轮门禁重新开始。修复后先执行第 16 节回归链，再从第一轮重新计数。

三轮后执行：

- `git diff --check` 与工作树审计；
- Editor/Connector、Primary、QLocal、端口、pipe 和临时目录残留检查；
- 127/6/133 工具与 CTest 清单最终快照；
- Evidence 索引、failure ledger 和 cleanup manifest 完整性检查。

## 16. 缺陷修复与回归门禁

每个失败建立匿名 failure ID，保留首次失败证据。修复流程：

1. 在改动前复现并缩小；偶发问题保留时序、seed、进程和资源快照。
2. 定位协议、业务、线程、文件、配置或测试契约根因。
3. 修改唯一实现路径，保持 Schema、安全与断言强度。
4. 执行最小复现。
5. 执行所属组件完整测试域。
6. 执行一期受影响 Facade/History/GUI 回归。
7. 执行 Editor direct + Connector stdio 等价联调。
8. 涉及 UI 或生命周期时重做对应 Computer Use 场景。
9. 重新构建受影响目标并从头执行完整 CTest 三轮。

协议、安全、越权文件访问、数据损坏、crash、stdout 污染、错误文档写回和 History/revision 破坏为阻断级。阶段性修复使用独立 `fix(scope): summary` 提交，私有证据保持在归档中。

## 17. 报告与最终清理

### 实现报告

`implementation-report.md` 记录：

- 127 + 6 工具与 19 个域；
- Wire/Registry/Manifest/Profile/Custom；
- 三版本 Editor MCP 与 QLocal；
- Connector stdio/exposure/compatibility；
- File Guard、Admission、设置、CLI 与生命周期；
- 当前代码事实、实现差异与长期保护测试。

### 测试报告

`test-report.md` 由本轮执行回填：

- 候选身份与环境摘要；
- 实际命令、退出码和 Evidence ID；
- 工具/域/协议/安全/兼容/Profile/Custom/Bootstrap/Connector/Editor/联调/GUI 结果；
- 失败与修复轨迹；
- 三轮完整 CTest；
- 资格项、残余风险和 cleanup；
- 最终判定。

### 最终清理

先验证证据索引可读，再按所有权清单清理 Agent 新增 scratch、临时配置、工作副本、端口记录和可重建缓存。仓库报告仅保留匿名 Evidence ID、公开结果和可复现命令模板。
