# 二期 MCP Server 与 DS Connector Lite 测试执行计划

## 1. 执行目标

本计划用于实现完成后的正式测试执行、缺陷修复回归和证据归档。测试范围以
[实施计划](implementation-plan.md)、[公共工具矩阵](public-tool-matrix.md) 和
[全量测试大纲](test-outline.md) 为准，依次完成：

1. 一期契约改名与 122-operation 回归；
2. Wire Contract、87 项公共绑定、profile/Custom、安全与 editor MCP；
3. QLocal Instance Bootstrap；
4. DS Connector Lite stdio、exposure 与兼容性；
5. editor/connector 进程级联调、多 connector 和 GUI；
6. 全部 CTest 连续三轮；
7. 失败修复、完整回归、实现报告和最终测试报告。

任何阶段的失败都不得用 skipped、降低分母、放宽 schema/policy 或只重跑成功子集掩盖。

## 2. 环境与前置条件

正式执行环境：

- Windows 11 x64；
- Visual Studio 2026 x64 DevShell、MSVC v145、Windows 11 SDK；
- Qt 6.11.1+ MSVC 2022 64-bit、Qt State Machines 与 Qt HTTP Server；
- CMake/Ninja、项目 vcpkg manifest 与 `x64-windows` triplet；
- Debug preset，`LITE_BUILD_TESTS=ON`；
- 可选的 DSPX/MIDI/audio fixture、声库、推理模型、音频设备和 Agent Host。

configure、build 按项目 CMake Configure/Build 规范，在同一个 VS x64 DevShell 和 Qt
环境中执行。正式记录至少包含：commit、branch、工作树、submodule、Windows/MSVC/SDK、
Qt、CMake、Ninja、vcpkg triplet、关键依赖版本、测试 seed 和系统时区。

不得把完整环境变量表、访问令牌、用户目录清单或无关隐私写入日志。只记录需要复现实验的
白名单变量，路径进入仓库报告前匿名化。

## 3. 私有证据归档

正式证据保存在仓库外、由测试运行者拥有的 Phase2 私有归档根。仓库文档只使用匿名占位符，
不固化用户目录、下载目录或带日期的真实归档名称：

```text
<private-evidence-root>/
```

真实绝对位置和运行批次标识只写入私有环境快照；报告与证据索引从该根开始使用相对路径。

建议结构：

```text
00-baseline/
01-configure-build/
02-contract-policy/
03-editor-mcp/
04-bootstrap/
05-connector/
06-integration/
07-gui-and-qualification/
08-ctest-rounds/
09-failures-and-fixes/
10-final/
work/
```

每个阶段保存：原始命令、stdout/stderr、退出码、开始/结束时间、测试 XML/JSON、服务日志、
结构化请求/响应（隐私清理后）、截图/录屏、进程与端口快照、临时文件清单和清理结果。
`10-final/evidence-index.*` 把场景号映射到相对证据路径；证据一律使用 archive 内相对路径，
避免归档移动后失效。

归档不提交 git。仓库中的 `implementation-report.md` 和 `test-report.md` 只写可公开、可复现的
摘要和匿名证据 ID；本机绝对路径、原始素材清单、内容 hash、可执行路径细节和未清理日志
只保留在私有归档。

## 4. Fixture 与写入隔离

仓库正式文档只称 **“用户提供只读 fixture 根”**，不得记录其真实路径、目录结构或文件名。
执行时遵守：

- fixture 根只读；测试、editor、connector 和辅助脚本不得原地修改、重命名或生成旁车文件；
- 为每个测试素材分配匿名 ID，例如 `FX-DSPX-001`、`FX-MIDI-001`、`FX-AUDIO-001`、
  `FX-VOICE-001`；仓库报告只引用匿名 ID 和资格类型；
- 任何可能写入的 open/save/import/export/relocate/inference/cache 场景，先把必要输入复制到
  archive `work/<fixture-id>/<run-id>/input/`，输出写入同级 `output/`；
- 实际 fixture 路径、原始文件名、清单、大小和 SHA-256 只写入 archive 的私有 manifest；
- File Guard 测试的 allowlist 指向隔离工作区，不指向原始 fixture 根；只读检查确需访问原始
  fixture 时使用单独 read grant；
- Agent 新建的工程、导出物、日志、临时端口文件和可重建缓存均标记所有者与创建时间；测试
  完成且证据固化后可清理，绝不删除原始 fixture 或用户既有文件；
- 可重建缓存允许在测试前后清空；清理前后记录范围、结果和残留，不使用宽目录递归删除。

## 5. 全局单实例与进程安全

GUI、开发构建和进程级测试共享一个全局 editor Primary。所有会启动 editor 的测试必须
串行：

- CTest 为相关测试设置 `RUN_SERIAL TRUE` 和统一 `RESOURCE_LOCK editor_primary`；
- 完整 CTest 三轮使用 `-j 1`，避免第三方/旧测试间接争用全局实例；
- 每次启动前检查 DsEditorLite、全局锁、QLocal 服务和预期端口；发现来源不明的 editor 时
  停止本阶段并记录，不自动结束用户进程；
- 测试夹具保存自己创建的精确 PID/process handle，退出时只清理这些进程；
- 优先使用控制端口 0；端口冲突场景显式保留测试端口并在场景后释放；
- 等待进程退出、socket EOF、QLocal 服务消失和端口释放后才启动下一场景；
- connector 可多实例并行，但涉及同一 editor 的多 connector 场景由一个串行 fixture 统一管理；
- 非 GUI 资格测试显式设置 `QT_QPA_PLATFORM=offscreen` 并固定 Qt plugin path；真实 GUI 轮次
  单独执行，持续监控意外模态弹窗和进程心跳，禁止无人值守等待用户输入；
- 崩溃/timeout 时先保存 dump、日志、PID/端口/QLocal 状态，再精确结束测试拥有的进程。

进入 Computer Use GUI 阶段前结束全部自动测试 editor；GUI 阶段完成后也必须确认 Primary
完全释放，再执行最终 CTest 三轮。

## 6. 命令基线

以下为计划中的核心命令；正式归档保存项目 skill/wrapper 展开后的实际完整命令和退出码。

```powershell
cmake --preset debug
cmake --build build/Debug
ctest --test-dir build/Debug -N
ctest --test-dir build/Debug --show-only=json-v1
```

本文所称“注册 CTest 分母”是最终冻结树由 `ctest -N` 与 JSON 清单共同枚举的 test case 数；
它不等于测试可执行 target 数，也不等于单个 target 内的场景数或断言数。最终报告分别记录这
几类计数，禁止用较小的 target 分母替代注册 case 分母。

完整 build 不只构建 `DsEditorLite`，还必须包含 `DsConnectorLite`、Wire Contract 生成目标和
所有 tests。若保留 preset 的指定 target 形式，则显式增加 connector；正式报告记录最终目标
列表。

实现后预期使用以下测试域或等价命名；最终以 `ctest -N` 清单为准：

```powershell
ctest --test-dir build/Debug --output-on-failure -j 1 `
  -R "Automation|Mcp|Connector|SingleInstance"

ctest --test-dir build/Debug --output-on-failure -j 1
```

不得使用 `--repeat until-pass` 得出通过结论。压力测试使用固定 seed 列表；随机失败必须保留
seed 并转换成最小确定性回归。

## 7. 阶段 A：基线冻结与静态审计

### 执行内容

1. 记录 git/submodule/工具链和现有进程状态；运行 `git diff --check`。
2. 保存一期 122-operation Catalog 和测试清单快照。
3. 验证 `operations.list/get/cancel → tasks.list/get/cancel` 集中改名、总数不变、旧名无残留。
4. 机器解析 `public-tool-matrix.md`、Wire Binding、Manifest fixture 和 connector 内置业务表，
   核对 87 个唯一 operation 及 `P2-TOOL-001～087` 连续无缺口。
5. 核对六个 connector 固定工具、InternalOnly 审计和延期范围负向集合。
6. 运行 Schema/codegen reproducibility，两次干净生成的内容和 digest 必须相同。

### 退出门禁

122 内部 ID、87 公共 ID、六个 connector 工具各自集合精确；旧名、重复 ID、手写平行枚举、
未注册 handler、L3/Headless/未来条件工具占位注册任一出现即停止后续测试。

## 8. 阶段 B：配置、完整构建与单元契约

### 执行内容

1. Debug configure，保存 CMake cache 中 Qt/vcpkg/test/connector 关键项。
2. 完整 build editor、connector、生成目标和全部 tests。
3. 运行 Wire codec/Schema、Manifest/version/digest、`value_sources`、Public Binding 集合测试。
4. 运行 profile/Custom、Access Policy、File Guard、Admission Control 与兼容算法单元测试。
5. 运行一期 Automation Catalog、Core、Dimensions、Idempotency、Task Races、Document、
   Architecture 等保护测试。
6. 对定向 schema compatibility、Windows path 和 rate-limit fake clock 语料运行固定 seed 压力。

### 退出门禁

configure/build 零失败；生成工作树无意外脏文件；一期保护和二期纯单元测试全部通过。失败先
进入第 15 节修复流程，不带已知失败进入真实 server 测试。

## 9. 阶段 C：Editor MCP 组件与协议测试

### 执行内容

1. 使用 loopback 动态端口启动测试 server，验证只绑定 `127.0.0.1`。
2. 完成 MCP 2026-07-28 POST、per-request metadata、header/body、tools/list/call、分页、
   structuredContent/outputSchema、错误和请求级取消全矩阵。
3. 验证 GET/DELETE/session/Last-Event-ID/Batch/Resources/Prompts 等延期路径拒绝。
4. 完成 Host/Origin/DNS rebinding、Content-Type、body/depth/response 上限与 timeout 测试。
5. 对 87 项执行 descriptor/profile/schema 路径；从每个域选代表工具执行真实 Facade 等价语料，
   全部 87 项至少执行适用的有效/拒绝路径。
6. 运行多 HTTP 客户端、revision 冲突、公平限流和 runtime disable/shutdown 在途测试。

### 证据

保存监听地址、HTTP 请求/响应语料、MCP 协议 case 表、87 项追踪结果、配额时序和内存/句柄
基线。请求日志先清除用户文本、绝对路径和大块二进制。

### 退出门禁

87 项无漏项；协议/业务错误分层、profile 发现与执行、Guard/Admission 均一致；无非本机
listener、无未授权 handler 调用、无残留端口或 session 状态。

## 10. 阶段 D：QLocal Bootstrap 测试

### 执行内容

1. 旧 `activate/openProjects` 回归和旧 editor 不支持自动化时的兼容诊断。
2. discover 一次性快照、watch 初始快照和所有状态广播。
3. 分片/合并/非法/超大帧、timeout、写缓冲、慢 watcher 背压和数量上限。
4. 多 watcher、异常断开清理、editor restart/instance ID/endpoint 变化。
5. 证明 editor/connector 使用相同全局身份；connector 离线时不取得锁或创建 server。
6. Primary 竞争、PID 复用和全局实例释放测试串行执行。

### 退出门禁

旧协议不回退；watcher 无泄漏；ready 只在真实 endpoint 可用后发布；connector 从未成为
Primary；每个测试后全局锁和 QLocal 名称释放。

## 11. 阶段 E：Connector stdio、Exposure 与兼容测试

### 执行内容

1. editor 离线启动 connector，验证 stdio MCP 正常、六个桥接工具和固定业务工具面。
2. 字节级检查 stdout 只含 MCP 帧；stderr/log 承载启动、重连和错误诊断。
3. 请求 ID 重映射、并发乱序、downstream cancel→upstream abort、timeout/EOF/破管。
4. l0/l1/l2/l3、include/exclude、selector 语法、pending 和 exclude 优先级。
5. 类型化与泛化 list/search/describe/invoke 使用同一 exposure；尝试所有绕过路径。
6. 使用版本/Schema fixture 验证双方新旧、compatible subset、单侧工具和不确定 schema。
7. editor offline/starting/disabled/ready/stopping/error、reconnect、instance change 与
   `outcome_unknown`。
8. `connector.get_status` 逐字段与实际进程、QLocal、HTTP、Manifest、exposure 事实对照。

### 退出门禁

stdio 零污染；downstream 固定工具面不随上游动态变化；exposure 无绕过；版本与 Schema 门槛
必须同时通过；connector 不启动/退出 editor、不接受任意 URL、不自动重放 Command。

## 12. 阶段 F：Editor/Connector 进程级联调

### 执行内容

1. connector 先启动，依次观察 not-running、disabled、starting、ready 并自动连接。
2. editor 先启动 ready，connector 首次 watch 后完成 MCP/Manifest 握手。
3. 在 direct editor MCP 与 connector stdio 上复用代表 Meta/L1/L2 语料并比较归一化结果。
4. 用隔离 fixture 完成 project/notes/parameters、open/save/import/export、Task、playback 链路。
5. 两至八个 connector 并发查询/编辑/Task；验证 revision 冲突、公平配额和互不影响。
6. runtime profile/Custom/file roots/端口/MCP enable 变化；connector fixed list 不变、实际状态
   与泛化目标更新。
7. editor stop/restart、instance ID 与 endpoint 变化、旧句柄失效、未知结果不重放。
8. 任一 connector 正常退出、崩溃、慢读或限流时其他 connector/editor 继续工作。

### 退出门禁

四层语义等价、自动接入闭环完成、多 connector 无串线/饿死/共享状态泄漏；全部测试进程、
watch socket、端口和工作输出按所有权清理。

## 13. 阶段 G：GUI、Computer Use 与真实资格

### 执行前门禁

阶段 A～F 全部通过；没有自动测试进程；确认真实 GUI editor 能取得唯一 Primary；GUI 使用的
输入已从用户提供只读 fixture 根复制到隔离工作区。

### Computer Use 执行

1. 首次打开 Automation 设置页，记录默认值、说明、Custom 和 roots UI。
2. 先启动 connector，再在 GUI 开启 MCP；验证 state/endpoint 和自动上游连接。
3. 切换 L1/L2/L3/Custom，比较 GUI、editor tools/list、connector status 和实际调用。
4. 修改端口、制造端口冲突、恢复动态端口、禁用/再次启用；验证完整状态序列。
5. 设置合法/非法读写根，在隔离副本上执行文件允许/拒绝场景。
6. 通过 connector 创建/编辑轨道、片段、音符、参数、声线和时间线；检查 GUI 可见结果、
   History 和保存/重开。
7. 验证播放、导入/导出、可用声库下短推理、Task 轮询/取消和 MCP 禁用后任务行为。
8. 同时运行多个 connector，结束一个后确认其他连接和 GUI 正常。
9. 带 CLI override 重启测试拥有的 editor，验证设置页只读提示、优先级与不回写。
10. 执行一期 GUI 冒烟全表，确认 MCP/Bootstrap/设置改造未造成产品功能回退。

### 真实 Agent Host

使用临时、测试专用 stdio MCP 配置启动一个可用 Agent Host；不写入用户长期配置。执行
`connector.get_status`、实际工具 list/describe、一个 L1 查询/编辑和一个获准 L2 任务，并
保存 Agent 侧与 connector/editor 两侧证据。完成后删除测试配置和 Agent 新增产物。

### 环境未具备处理

缺少声库、模型、codec、设备或 Agent Host 时，私有归档记录探测证据，仓库报告只按匿名
资格项写“环境未具备”。不得以 mock 通过替代真实资格，也不得影响确定性分母结论。

## 14. 阶段 H：完整 CTest 三轮

GUI/资格阶段完成并确认全局实例释放后，从同一构建、同一 commit 连续执行三轮：

```powershell
ctest --test-dir build/Debug --output-on-failure -j 1
ctest --test-dir build/Debug --output-on-failure -j 1
ctest --test-dir build/Debug --output-on-failure -j 1
```

每轮分别保存完整 log、CTest XML/JSON、开始/结束时间、总测试数、总耗时和失败清单。轮间不
修改源码、不选择性清理能掩盖生命周期缺陷的状态；只执行测试契约要求的确定性隔离与缓存
初始化。任一轮失败或 timeout 均判三轮门禁失败，进入修复流程后从第一轮重新计数。

三轮结束后运行：

- `git diff --check`、工作树/未跟踪文件审计；
- DsEditorLite/DsConnectorLite 进程、全局锁、QLocal 服务、测试端口与临时目录残留检查；
- 87 工具/六桥接工具/CTest 清单最终快照；
- 私有 archive 完整性和证据索引检查。

## 15. 缺陷修复与回归门禁

每个失败建立私有 failure ID，保存首次证据、最小复现、seed/fixture ID、根因、修复 commit 和
回归结果。修复顺序固定为：

1. 在未修改前复现并缩小；无法稳定复现时保留完整时序和压力 seed；
2. 修复唯一业务/协议实现，不在测试中绕过或降低断言；
3. 运行最小回归用例；
4. 运行所属组件全域；
5. 运行一期受影响 Facade/GUI 回归；
6. 运行 editor direct + connector stdio 等价联调；
7. 如影响 UI/运行时状态，重做对应 Computer Use 场景；
8. 从零重新执行完整 CTest 三轮。

协议、安全、File Guard、数据损坏、崩溃、stdout 污染、错误 document 写回和越权均为阻断级，
发现后暂停同域扩展测试，先保存证据并修复。阶段性修复按用户要求独立提交，提交信息使用
`fix(scope): summary`，但私有测试记录和本机路径不进入提交。

## 16. 报告与最终清理

### 仓库报告

实现完成后补充：

- `implementation-report.md`：最终架构、87 项完成状态、主要 commit、与计划差异、已知限制；
- `test-report.md`：环境摘要、命令、场景/断言/轮次、逐域结果、三轮 CTest、GUI/资格、失败
  修复轨迹、环境未具备和残余风险。

报告不复制原始日志，不写本机绝对路径、真实 fixture 名、hash、用户信息或私有目录结构。
证据以 `P2-TOOL-*`、`P2-CONN-*`、test case/failure ID 和 archive 内匿名 evidence ID 引用。

### 私有归档

`10-final/` 保存环境全文、实际命令、测试清单、公开工具快照、三轮摘要、failure ledger、
fixture 私有 manifest、证据索引和 cleanup manifest。先校验归档可读、索引无断链，再清理
agent 生成的 scratch、测试配置、临时端口文件和可重建缓存。

最终不得清理：原始只读 fixture、用户既有工程/设置、未确认所有权的进程或文件。发现无法
归属的残留时只报告，不删除。

## 17. 最终放行标准

- 计划四份前置文档与最终实现/测试报告齐全；
- Debug 完整构建成功，editor、connector 和所有 tests 均在目标列表；
- 122 内部 Catalog、87 公共工具、六 connector 工具的集合与追踪不变量全部成立；
- 适用确定性测试、editor/connector 联调和 GUI 冒烟全部通过；
- 全部 CTest 同一 commit 串行连续三轮通过，无 flaky；
- 阻断缺陷全部修复并完成完整回归；环境未具备与残余风险逐项可解释；
- 无残留测试进程、全局锁、QLocal 服务、端口、协议污染或越权文件；
- 私有 Phase2 archive 完整，仓库报告不包含本机素材信息或私有绝对路径。
