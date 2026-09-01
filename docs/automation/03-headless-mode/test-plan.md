# 三期 Headless 与原生 JSON-RPC 测试执行计划

## 1. 执行目标与顺序

本计划用于三期最终候选的正式重测、缺陷修复回归和私有证据归档。所有通过结论必须来自同一候选
重新产生的证据；一期、二期报告和旧构建缓存只可用于定位，不替代本轮结果。

执行顺序：

1. 冻结源码、分支、公共 Contract、Host 差集、测试清单和环境摘要。
2. 使用项目标准脚本完成 Debug 配置与全目标构建。
3. 验证 QCore composition、无窗口身份、Host gate 与统一错误。
4. 验证 Native JSON-RPC、共享 HTTP 限制和 Native/MCP 共存。
5. 验证 QLocal Bootstrap、单实例、生命周期和真实 Headless 进程。
6. 完成 151 项 QCore 资格关系与代表性业务域测试。
7. 完成 Headless MCP、Connector 与 GUI 回归。
8. 在同一最终候选上串行执行一次完整 CTest。
9. 完成失败修复闭环、最终资源审计、实现报告和测试报告。

当前 `176 - 25 = 151` 只是候选快照。测试门禁使用权威 Contract、Registry binding、
Host metadata、实际发现面和 Connector 目标之间的集合关系，不硬编码 CTest 数量或为 151 项
复制同构协议用例。

## 2. 环境与基线记录

目标环境：

- Windows 11 x64；
- Visual Studio x64 DevShell、MSVC 与 Windows SDK；
- Qt 6 MSVC x64，包含 Widgets、HTTP Server 及项目所需模块；
- CMake preset、Ninja、vcpkg `x64-windows`；
- Debug 配置，`LITE_BUILD_TESTS=ON`；
- 可用的本地 HTTP/QLocal/窗口枚举与进程控制能力；
- 可用时接入真实 DSPX、USTX、代表音频、声库、推理和播放资格环境。

基线至少记录：

- commit、branch、工作树和 submodule 状态；
- 编译器/SDK、Qt、CMake、Ninja、vcpkg triplet 与关键依赖版本；
- 时区、测试 seed、Qt platform/plugin 路径；
- Editor/Connector 进程、Primary、QLocal、listener 和端口占用；
- Public Contract、Registry、GUI-only/both 集合与 CTest 清单快照。

发现来源未知的 Editor/Connector 或已占用全局 Primary 时停止进程阶段，不主动终止或接管用户
进程。静态、纯单元和文档检查可继续执行。

## 3. Phase3 私有证据归档

执行时在用户指定的仓库外 Phase3 私有归档根创建以下结构。仓库报告只使用
`P3-EV-*` Evidence ID 与归档内相对路径，不写入真实根路径。

```text
00-baseline/
01-configure-build/
02-headless-composition/
03-native-json-rpc/
04-headless-mcp-bootstrap/
05-domain-qualification/
06-process-concurrency/
07-gui-regression/
08-ctest/
09-failures-and-fixes/
10-final/
work/
```

每个阶段保存：

- 实际命令、工作目录、开始/结束时间、退出码；
- stdout/stderr 与必要的结构化日志；
- JSON-RPC、MCP、QLocal 请求响应和状态时序；
- PID、process handle、端口、listener、窗口和资源快照；
- CTest `-N`、JSON 清单、逐 case 结果和耗时；
- failure ledger、首次失败、修复 commit 与完整回归链；
- 测试产物所有权、cleanup manifest 和最终清理结果。

仓库文档不得公开用户路径、素材名、实际端口、PID、Document/Object/Task/instance ID 或原始配置。
请求响应进入报告前替换为稳定匿名 ID；原始证据只保存在私有归档。

## 4. 授权素材与写入隔离

- 用户提供的素材根只读使用，不原地保存、导入、导出、推理或生成缓存。
- 只复制实际场景需要的 DSPX、USTX 和代表音频到归档 `work/`。
- 所有保存、转换、导入、导出、推理、日志和缓存指向测试拥有的隔离工作区。
- 对实际使用的原文件在测试前后计算 hash；未使用素材不做全量枚举或无意义 hash。
- File Guard 的访问根只指向隔离工作区；需要验证只读输入时使用明确的会话读授权。
- 每个 Agent/测试新增产物记录精确路径、所有者、创建时间与可清理性。
- 清理只操作 manifest 中的精确目标；来源未知文件、用户原件和用户进程保持不动。
- 正式测试结束后核对所用原件 hash 未变化，并将结果写入匿名 Evidence 索引。

## 5. 串行、进程所有权与 watchdog

所有启动 Editor 或使用全局 QLocal/Primary 的测试：

- 设置 `RUN_SERIAL TRUE`；
- 使用统一 `RESOURCE_LOCK editor_primary`；
- 完整 CTest 使用 `-j 1`；
- 隔离 APPDATA、LOCALAPPDATA、Automation 访问根和端口；
- 只保存并控制测试直接创建的 PID/process handle；
- 每个场景开始前检查 Primary、QLocal、预期端口和 Editor/Connector；
- 每个场景结束后等待进程退出、HTTP/QLocal 释放、pipe EOF 和 Task 终结；
- GUI/offscreen 轮显式解析正确的 Qt platform plugin 路径；
- 使用 watchdog 监测 timeout、crash、Debug Error、模态窗口、顶层窗口和无响应；
- 发生异常时先固化日志、dump、请求、窗口和资源快照，再精确结束测试拥有的进程。

不得按宽泛进程名结束用户进程，也不得以等待超时后遗留资源的方式继续下一场景。

## 6. 构建与命令基线

正式配置和构建使用项目标准脚本，不使用 CLion run configuration：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .agents/skills/scripts/run-cmake-preset.ps1 `
  -Mode ConfigureAndBuild `
  -Preset debug `
  -Target all
```

完整候选显式构建 `all`，覆盖 Editor、Connector 和全部测试目标，同时保持项目 Debug build preset
默认目标不变。构建完成后执行：

```powershell
ctest --test-dir build/Debug -N
ctest --test-dir build/Debug --show-only=json-v1
ctest --test-dir build/Debug --output-on-failure -j 1
```

若 preset 实际输出目录不同，以本轮 configure 产物为准并记录匿名相对路径。禁止使用
`--repeat until-pass` 形成通过结论；压力测试使用固定 seed，偶发失败保存 seed 并转化为可重复用例。

## 7. 阶段 00：基线与静态契约

### 执行

1. 记录 git/submodule、工具链、进程、Primary、QLocal、端口和窗口基线。
2. 执行 `git diff --check`，确认正式文档无真实私有路径或运行期 ID。
3. 从编译后的权威 Public Contract 取得集合 `P`，验证 ID 唯一。
4. 计算 GUI-only 集合 `G` 与 both 集合 `B`，验证互斥、并集及当前差集域。
5. 验证 Registry binding `R=P`，binding Host metadata 与 Contract 一致。
6. 验证全部 `value_sources` provider 存在、可达且 Host metadata 合法。
7. 搜索代码与对外 golden，确认不再产生 `host_unavailable`；必要的历史文档引用不作为运行时
   失败，但必须与当前契约区分。
8. 运行 `ctest -N` 与 `--show-only=json-v1`，记录本轮实际测试清单。

### 门禁

Contract 重复、Host 集合不闭合、Registry 缺失、value source 不可达或对外错误值未迁移时先修复，
不进入真实进程资格。候选数量漂移必须输出集合差异并核对是否为已批准的公共契约变化。

## 8. 阶段 01：配置、完整构建与纯单元测试

### 执行

1. 通过标准脚本执行 Debug configure/build，确认 tests enabled。
2. 验证 HostMode 预解析与完整 `StartupArguments` 的职责分离。
3. 覆盖 `--headless`、`--`、位置工程参数、端口边界、MCP 和 control level。
4. 运行 optional WindowContext、ApplicationCommandContext、Restarter 与 QCore translator 测试。
5. 运行 Host gate、Policy/Custom、Schema、File Guard、Admission 固定顺序测试。
6. 运行 JSON-RPC envelope、ID/params、错误 mapping 与稳定 details 单元测试。
7. 运行 Automation Core、History、revision、Task、文档 generation 和现有领域回归。

### 门禁

配置和全目标构建零错误；纯单元测试完成。GUI-only Headless 请求必须在 Schema 与 handler 前拒绝，
未知 method 与 Host/permission 错误必须互不混淆。

## 9. 阶段 02：Headless composition 与无窗口资格

### 执行

1. 以故意无效的 `QT_QPA_PLATFORM` 和隔离配置启动 Headless。
2. 记录 Application 动态类型，确认根对象为 `QCoreApplication`。
3. 通过对象审计确认 Core composition 完整，GuiContext 对象均未创建。
4. 在 Native ready 前查询或记录首个默认文档建立时序。
5. 使用 Windows API 按 PID 枚举顶层窗口，覆盖启动、请求处理、文件/Task 和退出阶段。
6. 执行主题设置、音频设备失败、AudioDecoding、Playback、translator 和 Restarter 专项。
7. 检查 stderr、Qt plugin 加载、QObject 树和窗口快照，不得存在 GUI fallback。

### 门禁

任何 `QGuiApplication/QApplication`、QWidget、MainWindow、ThemeManager、QWindowKit、对话框或
平台窗口事实均为阻断；修复唯一 composition/注入路径后重跑本阶段。

## 10. 阶段 03：Native JSON-RPC

### 首次状态与业务调用

1. 连接 `POST /automation/v1`，首次调用 `application.get_status`。
2. 验证 `host_mode=headless`、工具版本、control level、真实 DocumentId/revision 和空 windows。
3. 执行代表 Query、同步编辑、Undo/Redo 与异步 Task。
4. 比较 Native result 与公共 output Schema，确认无 MCP wrapper。
5. 通过 `tasks.list/get/cancel` 完成异步状态闭环。

### Envelope 与错误

1. malformed JSON、非 object、Array/Batch、notification。
2. string/safe-integer ID，缺失/null/布尔/小数/越界 ID。
3. params 省略、空 object、非 object、未知顶层字段和非法 `jsonrpc`。
4. unknown method 与明确的 `automation.discover` Method not found。
5. invalid params 的 `-32602`、字段路径和稳定 details。
6. GUI-only method 使用缺失/非法 WindowId，验证 Host gate 优先。
7. permission、revision、File Guard、业务错误和 internal failure 的 `error.data`。

### HTTP 限制

1. Host、Origin、method、MIME、Accept 与 loopback 绑定。
2. body、JSON depth/node、response size 与 deadline 边界。
3. transport in-flight、Registry admission、成功/失败/取消后的计数释放。
4. shutdown 时在途请求、响应完成、listener 和资源释放。

### 门禁

Native 不接受 batch/notification，不提供 discover/catalog，不进入 MCP session；协议错误与业务
AutomationError 分层，成功结果和业务状态与直接 Facade 一致。

## 11. 阶段 04：Headless MCP、Bootstrap 与共享 Server

### `--no-mcp`

1. 启动 Headless `--no-mcp`，确认 `/automation/v1` 可用、`/mcp` 不可用。
2. 读取 QLocal Bootstrap，确认 Headless Host 与 `server_disabled/empty endpoint`。
3. 启动 Connector，确认其准确报告 MCP disabled，且不把 Native listener 当成 MCP endpoint。

### `--mcp`

1. 启动 Headless `--mcp`，确认 Native 与 MCP 在同一 listener/端口同时工作。
2. 完成两套 MCP 主协议与兼容握手的二期保护回归。
3. MCP `tools/list` 只返回当前 control/custom 下的 Headless both 集合。
4. `editor.tools.list/search/describe` 只反映实际 Headless 工具。
5. 固定 GUI wrapper 调用返回 `host_capability_unavailable`。
6. 混合 Native/MCP 并发请求，验证共享 Registry、File Guard、Admission 和停止顺序。
7. 比较代表语料的结果、错误、History、revision 和 Task。

### 启动失败

1. 构造端口冲突、非法访问根和 Registry 初始化失败。
2. 验证 stderr 包含稳定诊断、退出码非零。
3. 验证 listener、QLocal、Primary、Task 和进程全部清理，不发生无服务空转。

### 门禁

Native route 生命周期与 MCP enable 状态分离；Bootstrap 的 `server_*` 只描述 MCP；共享 Server
不改变 MCP session/header/cancel 行为。

## 12. 阶段 05：QCore 业务域资格

### 集合与确定性资格

1. 由 Contract/Registry 测试证明每个 `both` operation 具有 binding 与 QCore Host 资格。
2. 按业务域运行既有 Facade/Dispatcher 测试，验证独特正常、no-op、错误和提交语义。
3. 验证 settings、audio、playback、packages、documents、file tasks 和 inference 不反向创建 GUI。
4. 验证 application-scoped 与 document-scoped Task 的身份、generation 和终态保留。

### 真实代表场景

1. status/documents/轨道/剪辑/音符/能力/设置的代表 Query。
2. 创建轨道/剪辑/音符或参数/时间线修改的同步 Command。
3. History state、Undo、Redo 与 revision 精确变化。
4. 瞬时 playback 与持久 loop 的不同 History/revision 语义。
5. 从只读授权副本 open/import，在隔离工作区 save/save-as。
6. 音频导入、导出、包刷新、文档任务或推理的至少一个异步代表路径。
7. 缺少声库、模型、codec 或音频设备时记录稳定 available/unavailable/module error 事实。

### 等价

同一代表语料按适用性经过：

- 直接 Facade；
- Native JSON-RPC；
- Headless Editor MCP；
- DS Connector Lite MCP。

比较最终模型、成功结果、AutomationError、revision、History、Task 和文件事实。环境资格不可用不
计为成功，但 deterministic fake/fixture 应覆盖对应可用分支。

## 13. 阶段 06：进程、并发与生命周期

使用一个串行 process fixture 统一管理下列场景：

1. 无效 QPA + 零窗口启动。
2. Headless `--no-mcp` Native/Connector 状态。
3. Headless `--mcp` 双 route。
4. status 一文档零窗口。
5. Native 编辑与 Undo/Redo。
6. 只读素材副本 open/save/Task。
7. 音频/导出/推理或不可用事实。
8. GUI-only Host capability 错误。
9. GUI 先运行再启动 Headless，以及 Headless 先运行再启动 GUI。
10. 端口冲突与致命启动失败。
11. clean exit。
12. dirty 默认 busy、无模态窗口，随后 discard exit。
13. restart 的新 instance ID、原参数/工作目录与最终无孤儿进程。

并发专项覆盖：

- Native 与 MCP 混合全局在途上限；
- 并发 revision conflict 和 Task 容量；
- shutdown/disable/断开与计数释放；
- 单实例转发的位置工程请求与队列顺序；
- restart/instance epoch 变化后的旧请求、旧目录和 Task 隔离。

每个场景结束都执行局部 cleanup；阶段结束执行完整 listener、QLocal、Task、Primary、锁、临时根和
进程残留检查。

## 14. 阶段 07：GUI 与 Connector 回归

### GUI 产品回归

1. 使用有效 Qt platform plugin 启动 GUI，确认 `QApplication + CoreRuntime + GuiContext`。
2. `application.get_status` 返回真实 document/window。
3. L3 MCP `tools/list` 等于完整权威 GUI 公共集合。
4. 代表性文档 mutation、Undo/Redo、播放和保存闭环。
5. workspace、track_panel、clip_editor 各执行代表 operation 并 query 回读 GUI 状态。
6. MCP enable/disable、换端口、退出、重启和单实例回归。
7. MCP 修改后的模型与 GUI 状态一致；GUI 修改后的 MCP query/revision 一致。

### Connector 回归

1. 固定六个桥接工具和二期 downstream wrapper 契约不变。
2. GUI 与 Headless 分别验证 Connector status、实际目录、调用与错误。
3. Headless GUI wrapper 返回 `host_capability_unavailable`。
4. `editor.tools.list/search/describe/invoke` 与实际 Host/Policy/exposure 一致。
5. Editor restart 后 Connector 识别新 instance，不重放结果未知 Command。

### 交互控制

优先使用 Connector/Editor MCP、日志、窗口枚举和模型状态完成断言。持续监控 Debug Error、模态
窗口和进程心跳。仅当存在无法通过这些路径验证的必要 GUI 断言时，记录缺口与理由后使用最小
Computer Use；不重复 MCP 已完成的业务动作。

### 门禁

GUI 创建、公共目录、三个 GUI-only 域、MCP 生命周期、单实例和 Connector 均不回退；自动化公共
生命周期无弹窗，GUI 用户生命周期仍保留交互式 Prompt。

## 15. 阶段 08：最终串行 CTest

GUI/进程阶段结束并确认全局资源释放后，在同一 commit、同一构建产物上执行：

```powershell
ctest --test-dir build/Debug -N
ctest --test-dir build/Debug --show-only=json-v1
ctest --test-dir build/Debug --output-on-failure -j 1
```

保存命令、工作目录、开始/结束时间、退出码、实际 case 总数、逐 case 结果、耗时、失败/timeout
和 JSON/XML。任一失败都进入缺陷闭环；修复后完整 CTest 结果作废，必须在新最终候选上重新执行
一次完整串行 CTest。

## 16. 阶段 09：失败与修复闭环

每个失败分配匿名 failure ID，并保留首次失败证据：

1. 在改动前缩小到最小可重复路径；偶发问题保存 seed、时序、进程和资源快照。
2. 判断根因位于 composition、Host gate、协议、业务、线程、文件、配置、Connector 或测试。
3. 修复唯一实现路径，不增加协议别名、平行 Headless DTO 或测试专用产品分支。
4. 运行最小复现。
5. 运行所属测试域。
6. 涉及公共协议时运行 Native + MCP + Connector 等价回归。
7. 涉及 GUI/Host composition 时运行 Headless 无窗口资格与 GUI 回归。
8. 涉及生命周期时重做 clean/dirty/discard/restart 和资源清理。
9. 使用独立 `fix(scope): summary` 提交修复。
10. 在同一新候选上重新完成 Debug build 与一次完整串行 CTest。

GUI 对象泄漏、伪造 WindowId、Host gate 越权、文件越权、数据损坏、错误文档写回、History/revision
破坏、Native 在 Headless 不可用、MCP 回归、crash、stdout/protocol 污染和资源残留均为阻断级。

## 17. 阶段 10：最终审计、报告与清理

最终执行：

- `git diff --check`；
- 工作树、分支与阶段提交历史检查；
- Public Contract、Registry、Host 集合、MCP 实际集合和 Connector 目标快照；
- Editor/Connector 进程、Primary、QLocal、listener、端口、Task 和锁检查；
- 私有 Evidence 索引、failure ledger 与 cleanup manifest 完整性检查；
- 实际使用授权素材的测试前后 hash 比对；
- 精确清理测试拥有的临时配置、工作副本、日志、端口记录和可重建缓存；
- 再次确认用户原件、未知文件和未知进程未修改。

`implementation-report.md` 回填当前代码事实、composition、Native/共享 Server、Host/错误迁移、
隐藏 GUI 依赖消除和长期保护测试。`test-report.md` 回填候选身份、环境摘要、匿名 Evidence ID、
实际命令/退出码、各阶段结果、failure/fix 轨迹、最终 CTest、资格限制和 cleanup 判定。

最终完成门禁：

- 分支阶段提交完整且工作树干净；
- Headless 为真实 QCore、零 GUI 对象/窗口、一个真实文档；
- 151 项候选 both 集合具有 Contract/binding/QCore 资格，25 项 GUI-only 差集准确；
- Native 固定可用，MCP 可独立启停并共用 Registry/Facade 生命周期；
- 身份、错误、History、revision、Task、单实例和 Connector 语义不回退；
- GUI 回归、Debug 全目标构建和一次最终串行 CTest 形成同一候选证据；
- 六份正式文档和私有归档完整、匿名、可追溯。
