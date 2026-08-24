# 二期 MCP Server 与 DS Connector Lite 最终测试报告

## 1. 结论

本报告按[测试执行计划](test-plan.md)和[全量测试大纲](test-outline.md)记录二期测试事实。二期
基础冻结树已完成公共 Wire、Editor MCP、Bootstrap、Connector、一期受影响回归、真实进程
联调、Release package staging、GUI/真实环境资格和连续三轮 65/65 完整 CTest。

基础冻结树之后又增加 MCP 2025-06-18、2025-11-25 与 2026-07-28 三版本兼容增量。按本轮
快速交付要求，该增量只执行定向构建、四目标 CTest、真实进程连续复跑、Release Connector
构建和 Codex Agent Host 实测，没有重跑全部 65 项。

当前结论为：**基础冻结树 PASS / RELEASE-READY；三版本兼容增量 FOCUSED PASS /
USABLE-CANDIDATE。增量已证明当前 Connector 可由 Codex 以 2025-06-18 握手并调用，但在完整
65 项回归补跑前不把历史放行结论自动外推到当前工作树。** 两项结论均只覆盖 MCP Server、
DS Connector Lite 和 L1/L2 公共能力，不把延期范围计入分母。

本报告按 append-only 原则保留 R01～R16 以及冻结 R01 的失败事实；后续成功轮只用于证明修复
后的最终候选，不删除、降格或改写既有失败。

## 2. 环境与覆盖分母

### 2.1 当前执行环境

| 项目 | 当前记录 | 最终状态 |
|---|---|---|
| 操作系统 | Windows 11 x64 | 已确认 |
| 编译环境 | Visual Studio 2026 x64 DevShell、MSVC v145、Windows SDK | 已确认 |
| Qt | Qt 6.11.x，MSVC 2022 x64 | 已确认 |
| 构建系统 | CMake preset、Ninja、vcpkg `x64-windows` | 已确认 |
| 构建类型 | Debug，`LITE_BUILD_TESTS=ON` | 已确认 |
| 非 GUI 运行 | `QT_QPA_PLATFORM=offscreen`，显式配置 platform plugin | 已确认 |
| GUI 运行 | 真实 Windows GUI、无人值守弹窗监控 | PASS |
| 安装 staging | `package-dml-release` 配置、双目标构建与安装 | PASS |
| 最终测试树 | 同一冻结源码和 Debug 构建产物 | 已确认 |

早期完整 Debug 构建和后续定向目标构建均已成功；最终冻结树又重新完成完整 Debug build，
并由同一产物执行三轮完整 CTest。Release staging 是独立安装资格，不替代 Debug 测试分母。

### 2.2 覆盖分母

| 集合 | 最终分母 | 复核说明 |
|---|---:|---|
| 一期内部 Catalog operation | 122 | 最终集合与受影响回归已复核 |
| 二期公共业务工具 | 87 | Public Contract、Registry、Manifest 与 Connector 描述面已复核 |
| Connector 固定桥接工具 | 6 | 固定工具面、stdio 与 exposure 已复核 |
| 注册 CTest case | 65 | 最终冻结树的 `ctest -N`/JSON 清单已复核 |

122、87、6 是互不混用的三个集合：122 表示一期进程内能力全集，87 表示二期公开业务工具，
6 表示 Connector 自身固定桥接工具。L3 业务工具和 Headless 能力不进入本期正向分母。

## 3. 已完成的组件验证

下表记录各组件在对应轮次的通过事实；组件结果本身不替代进程、GUI、安装和连续全量门禁，
最终放行结论以第 4～7 节的合并结果为准。

| 测试域 | 已完成内容 | 当前结果 | Evidence ID |
|---|---|---|---|
| Wire 与游标 | MCP 2025-06-18 / 2025-11-25 / 2026-07-28 codec、metadata、Schema、分页游标与边界语料 | PASS | `P2-E-0008`～`P2-E-0009`、`P2-E-0180` |
| Editor HTTP MCP | 两个 2025 initialize 生命周期、2026 discover、tools/list、tools/call、协议错误、HTTP 安全、限流、timeout 与 stop | PASS | `P2-E-0011`、`P2-E-0127`、`P2-E-0134`、`P2-E-0180` |
| 设置与 CLI | Automation 安全默认、持久值、生效值和 CLI override 单元路径 | PASS | `P2-E-0012` |
| File Guard 与公共绑定 | canonical 授权、读写目的、公共 Registry、音频路径重新授权 | PASS | `P2-E-0025`～`P2-E-0027` |
| 一期受影响回归 | Catalog、Idempotency、Editing Dimensions、Async Dimensions、Architecture | PASS（修复后定向轮） | `P2-E-0046` |
| Connector transport | 可信 HTTP transport envelope、稳定错误码、`outcome_unknown` 边界 | PASS | `P2-E-0051`～`P2-E-0052` |
| Connector stdio | framing、乱序映射、取消、EOF、背压、stdout 零污染 | PASS | `P2-E-0034`、`P2-E-0042` |
| Connector 握手 | 三版本下游、2026 优先探测、2025-11-25 回退、2025-06-18 协商、重复 ready 合并和有界退避 | PASS | `P2-E-0094`、`P2-E-0097`、`P2-E-0180` |
| Connector 完整组件套件 | exposure、兼容、分页、状态、transport、stdio 与握手合并回归 | PASS | `P2-E-0096` |
| 旧版真实进程联调 | Editor、Bootstrap、HTTP MCP 与 Connector stdio 闭环 | R11～R14 PASS | `P2-E-0080`～`P2-E-0083` |
| Release package staging | `package-dml-release` 双目标构建、安装、依赖与 CLI smoke | PASS | `P2-E-0101`～`P2-E-0103`、`P2-E-0109`、`P2-E-0112`、`P2-E-0114` |
| Codex Agent Host | 默认 2025-06-18 握手、工具发现和离线 `connector.get_status` 实际调用 | PASS | `P2-E-0182` |

组件轮还验证了以下关键不变量：

- Task 控制名称统一为 `tasks.list/get/cancel`，Catalog 总数保持 122；
- Editor 公共集合为 87 项，Connector 六个桥接工具独立计数；
- HTTP 429/503 等可信 transport 错误保持稳定 code，只有请求结果确实未知时才返回
  `outcome_unknown`；
- 同一 ready burst 在 fake 组件测试中被合并为一次在途握手和至多一次尾随刷新，请求量低于
  默认客户端配额；
- Connector stdio 的输入、输出队列有界，业务事件循环不承担阻塞式 stdout 写入；
- Audio export 使用不可变执行快照，失败或取消会自动清理，成功后释放后端记录；
- 架构守卫继续禁止分散 operation ID 和 Automation 对全局 current runtime 的依赖。

## 4. 失败与修复轨迹

### 4.1 Focused R01～R04

| 轮次 | 事实结果 | 处置与复核 | Evidence ID |
|---|---|---|---|
| R01 | INVALID：测试命令所在环境找不到 CTest | 不计通过或失败分母；改用项目标准 DevShell/preset 执行入口 | `P2-E-0044` |
| R02 | FAIL：25 个定向 CTest 中 20 个通过、5 个失败 | 保留 Catalog、Idempotency、Editing、Async、Architecture 的原始失败 | `P2-E-0045` |
| R03 | PASS：上述 5 个受影响目标全部通过 | 修正真正过时的契约期望，并修复 TaskId、音频导出生命周期、编辑行为和生产架构缺陷 | `P2-E-0046` |
| R04 | PASS：Connector 完整组件与 stdio/backpressure 回归通过 | 合并 transport、stdio 和握手修复后重建并执行完整 Connector 目标 | `P2-E-0096` |

R02 的失败未通过放宽 Schema、白名单或恢复旧错误语义处理。`tasks.cancel` 不重新引入 revision
依赖；Audio export 的快照、清理和资源释放按新契约修复；operation ID 与 runtime 依赖通过生产
架构收敛处理。

### 4.2 真实 Editor/Connector 进程联调 R01～R14

| 轮次 | 结果 | 主要观察或修复方向 | Evidence ID |
|---|---|---|---|
| R01 | FAIL | 进程测试失败但缺少可行动正文，推动补强子进程诊断 | `P2-E-0071` |
| R02 | FAIL | Bootstrap 等待超时 | `P2-E-0072` |
| R03 | FAIL | Editor 在发布 ready 前发生访问冲突 | `P2-E-0073` |
| R04 | FAIL | Editor 访问冲突稳定复现，继续保留启动日志与进程状态 | `P2-E-0074` |
| R05 | FAIL/诊断轮 | 调试器定位到 Audio export generation 清理中的无效记录访问 | `P2-E-0069`、`P2-E-0086` |
| R06 | FAIL | Editor status 未满足 Bootstrap identity 与 document discovery 断言 | `P2-E-0075` |
| R07 | FAIL | Connector 上游握手报告 Manifest unavailable | `P2-E-0076` |
| R08 | FAIL | Connector 拒绝无效上游 tool descriptor | `P2-E-0077` |
| R09 | FAIL | `automation.get_options` 输入根 Schema 不是 object schema | `P2-E-0078` |
| R10 | FAIL | 提取能力的 option schema 含无法解析的本地引用 | `P2-E-0079` |
| R11 | PASS | 真实 Editor + Connector MCP 进程闭环通过 | `P2-E-0080` |
| R12 | PASS | 同一联调目标再次通过 | `P2-E-0081` |
| R13 | PASS | 同一联调目标再次通过，进程和临时根清理检查完成 | `P2-E-0082` |
| R14 | PASS | 同一联调目标第四次通过，清理检查完成 | `P2-E-0083` |

R03～R05 的访问冲突通过真实调试栈定位并修复，没有把崩溃改写为 timeout 或 skip。R07～R10
逐步暴露并修复 Manifest、tool descriptor、根 Schema 和局部引用问题。R11～R14 证明当时版本
能够稳定闭环，但它们早于后续 Connector 收口改动，不能作为最终候选的替代证据。

### 4.3 最终候选重新阻断与修复

| 轮次 | 结果 | 结论 | Evidence ID |
|---|---|---|---|
| R15 build | PASS / BUILD ONLY | 只完成目标构建，没有执行真实进程联调，不计 runtime 通过轮 | `P2-E-0068` |
| R16 build | PASS | 最新候选能够完成进程联调目标构建 | `P2-E-0098` |
| R16 integration | FAIL | Bootstrap snapshot 已前进，但 Connector 最终返回 `too_many_requests`，Manifest 保持 `not_loaded` | `P2-E-0099` |
| D02 | FAIL / DIAGNOSTIC | Manifest HTTP 请求在约 10 ms deadline 下超时；旧调用方对象仍按早期 `McpHttpLimits` 聚合布局传参，错误 deadline 诱发 504、重试和 429 | `P2-E-0129`、`P2-E-0131`、`P2-E-0136` |
| D03 | PASS / DIAGNOSTIC | 强制重编调用方后完整进程链路通过，确认根因是陈旧 caller ABI，而非 Manifest 构造性能 | `P2-E-0128`、`P2-E-0130` |
| `68162c7e` | FIX | 增加在 server 实现单元内构造默认 limits 的 overload；显式 limits overload 不再携带跨编译单元默认聚合实参 | `P2-E-0126`～`P2-E-0127`、`P2-E-0134` |
| R17 | PASS | 修复后第一轮真实进程联调通过，耗时 18.50 s | `P2-E-0117` |
| R18 | PASS | 同一候选第二轮真实进程联调通过，耗时 18.61 s | `P2-E-0118` |

R16 的 build PASS 与 runtime FAIL 分开保留。D02/D03 先以失败时序和强制重编对照排除了
Manifest 计算性能，再用 `68162c7e` 消除跨编译单元默认聚合 ABI 风险；R17、R18 的连续通过
以及后续 GUI、安装和全量门禁共同关闭该阻断。

### 4.4 MCP 三版本兼容增量

| 轮次 | 结果 | 结论 | Evidence ID |
|---|---|---|---|
| C00 | FAIL，3/4 | 首个 2025-06-18 `connector.get_status` 等待超过原 5 s；未扩大 timeout | `P2-E-0175` |
| D04 | PASS / DIAGNOSTIC | Debug 下 87 项双向 Schema 校验约 1.02～1.06 s；稳态 status 本体约 0.8～6.0 ms，stdout 写帧约 11～119 us | `P2-E-0170` |
| S01 | PASS，5/5 | 保持 5 s status timeout，真实进程联调连续五轮通过，共 121.89 s | `P2-E-0171` |
| C03 | PASS，4/4 | Wire、Connector、Editor HTTP 和真实进程聚焦 CTest 全部通过，共 68.06 s | `P2-E-0169` |
| C04/C05 | FAIL，3/4 | Connector 13 场景全套在负载下触及 60 s CTest 总 watchdog；单场景 watchdog 未失败 | `P2-E-0176`～`P2-E-0177` |
| O01/C06/C07 | PASS | 同 Schema 快速路径后 Connector 单套 40.18 s；总 wrapper 调为 90 s 后最终聚焦 CTest 4/4、71.64 s | `P2-E-0178`～`P2-E-0180` |
| B01～B03 | FAIL → PASS | Codex 自动重启同路径 Connector 一度锁住 Release 输出；临时禁用该项、精确清理同路径进程后最终构建通过并恢复启用 | `P2-E-0172`～`P2-E-0173`、`P2-E-0181` |
| H01/H02 | PASS | Codex 未设置协议覆盖，实际以 2025-06-18 初始化并成功调用 `connector.get_status`；最终 Release 再次通过 | `P2-E-0174`、`P2-E-0182` |

D04 证明 87 项完整兼容计算不是 10 s 量级，也不能单独解释 C00 的偶发 5 s 无响应。正式
实现首先在握手完成时计算并缓存兼容计数，使状态查询不再重复执行整组分析；随后为输入和
输出 Schema 完全相同的常见路径增加精确相等快速通过，只有发生漂移时才进入完整双向子集
证明。legacy status 的单次 timeout 始终保持 5 s，缓存后的五轮真实进程压力复跑未复现 C00。

C04/C05 是整个 13 场景测试可执行文件触及 60 s CTest wrapper watchdog，不是单次 status
超时。各场景已有 5～15 s 独立 watchdog，且 O01 单套实际在 40.18 s 通过，因此只把总 wrapper
调整为 90 s；产品请求 deadline、status 5 s 门限和单场景 watchdog 均未放宽。原始失败继续
保留，本报告不把未捕获到的调度或管道抖动臆断为已定位根因。

## 5. GUI、Computer Use 与真实环境资格

### 5.1 GUI 与真实副本资格

| 场景 | 实际执行范围 | 结果 | Evidence ID |
|---|---|---|---|
| G01 | 从用户提供的只读 fixture 创建隔离工作副本，并由真实 GUI 打开 | PASS | `P2-E-0120`～`P2-E-0121` |
| G02 | 在工作副本上完成播放、GUI 编辑和保存，保存结果保持可用 | PASS | `P2-E-0123`～`P2-E-0125` |
| G03 | 通过 Connector 提交真实 mutation，确认 GUI 可见，再由 GUI Undo 恢复 | PASS | `P2-E-0122`、`P2-E-0125` |
| G04 | 对只读源执行前后完整性复核，19/19 项保持不变 | PASS | `P2-E-0119` |
| G05 | 全程监控模态弹窗；结束后检查测试进程、Primary、QLocal 与端口残留 | PASS，无弹窗或残留 | `P2-E-0125` |

所有写操作只发生在测试拥有的隔离副本；仓库报告不记录原始 fixture 的路径、名称或内容指纹。
Automation 设置、profile、端口生命周期、多 Connector 和协议状态由组件与进程联调覆盖；本节
只把实际完成的真实 GUI/fixture 资格写成 PASS，不把未观察的可选环境能力推断为通过。
基础冻结轮当时没有形成独立第三方 Agent Host 资格；三版本兼容增量已补充 Codex 实测：
Codex 默认以 2025-06-18 初始化 Release Connector，发现工具并在 editor 未运行时成功调用
离线可用的 `connector.get_status`。该资格不进入 65 项确定性 CTest 分母；连接 editor 后的
业务工具与 direct-editor 等价链路仍由真实进程联调覆盖。

### 5.2 Release 安装资格

| 资格项 | 结果 | Evidence ID |
|---|---|---|
| `package-dml-release` configure | PASS | `P2-E-0102` |
| `DsEditorLite` 与 `DsConnectorLite` Release build | PASS | `P2-E-0101` |
| CMake install staging | PASS，两款可执行文件同时存在 | `P2-E-0103`、`P2-E-0114` |
| Connector PE/Qt 依赖 | PASS，Qt Core/Network 与非系统直接依赖齐全 | `P2-E-0109` |
| 安装版 CLI smoke | PASS；帮助正常退出，非法参数 stdout 为空、诊断仅写 stderr | `P2-E-0106`～`P2-E-0107`、`P2-E-0111`～`P2-E-0112` |

本资格只验证安装 staging，不构建或宣称 Windows 安装器结果。测试前目标 staging 不存在，因此
没有删除或接管来源不明的既有产物；smoke 结束后无 Connector 进程残留。

## 6. 连续三轮完整 CTest

冻结 R01 首次执行为 64/65：`TestAudioAssetResolution` 仍要求一个
`RevisionPolicy::None` 的派生音频回写优先返回 revision conflict，与已冻结的新契约冲突。
失败原样保留；生产 Catalog、Dispatcher 和 Facade 未改。仅测试修复提交 `88347bb1` 将旧断言
改为验证 stale asset guard，随后定向复核 17 个场景、102 个断言全部通过。

三轮正式计数从该修复后的 R02 重新开始，并在同一冻结源码与 Debug 构建上串行完成：

| 门禁 | 分母 | 结果 | Evidence ID |
|---|---:|---|---|
| 最终 Debug 全目标 build | 全部产品与测试目标 | PASS | `P2-E-0138` |
| 最终 `ctest -N` 与 JSON 清单 | 65 | PASS，清单一致 | `P2-E-0159`、`P2-E-0163` |
| 冻结 R01 | 65 | FAIL，64/65；旧测试契约 | `P2-E-0139` |
| 修复后定向复核 | 17 场景 / 102 断言 | PASS | `P2-E-0166` |
| 完整 CTest R02 | 65 | PASS，65/65，82.65 s | `P2-E-0140` |
| 完整 CTest R03 | 65 | PASS，65/65，82.84 s | `P2-E-0141` |
| 完整 CTest R04 | 65 | PASS，65/65，82.85 s | `P2-E-0142` |
| 连续三轮合计 | 195 次 case 执行 | PASS，195/195 | `P2-E-0140`～`P2-E-0142` |
| flaky、timeout、弹窗和 Qt plugin 审计 | R02～R04 | PASS | `P2-E-0143`、`P2-E-0147`～`P2-E-0157` |

R02～R04 轮间没有源码修改或选择性重跑，也没有失败、timeout、无人值守弹窗或 Qt platform
plugin 错误。最终残留检查未发现测试拥有的 Editor/Connector 进程或相关运行时资源。

上述 65/65 三轮是二期基础冻结树的历史放行证据，不包含其后的 MCP 三版本兼容增量。当前
增量遵照快速交付要求只执行第 4.4 节的聚焦验证；不得用 4/4 聚焦 CTest 替代当前工作树的
65 项完整分母，也不得把历史 195/195 改写成当前增量的测试结果。

## 7. 最终放行清单

- [x] 二期测试计划、大纲、公共工具矩阵和实现报告已建立。
- [x] 组件层 Wire、HTTP MCP、File Guard、Public Registry、Connector 与一期受影响回归已有通过证据。
- [x] R01～R16 及冻结 R01 的失败和旧版通过轨迹已保留，不以最新成功子集覆盖历史失败。
- [x] R16 阻断已修复并由 R17、R18 连续真实进程联调确认。
- [x] 最终冻结树完成 Debug 全目标构建。
- [x] 122 内部 operation、87 公共工具、六个 Connector 工具及 65 个注册 CTest case 完成最终快照复核。
- [x] Release package staging、依赖与安装版 Connector CLI smoke 通过。
- [x] GUI/真实副本资格完成，包含播放、编辑、保存、Connector mutation 与 GUI Undo。
- [x] 只读源 19/19 保持完整，无无人值守弹窗、Primary、QLocal、端口或产品进程残留。
- [x] 同一冻结树的完整 CTest 串行连续三轮 65/65，无失败、timeout 或 flaky。
- [x] 私有证据索引、清理检查和仓库敏感信息扫描通过。
- [x] 三版本兼容增量完成四目标 CTest 4/4、真实进程连续复跑 5/5 和 Release Connector 构建。
- [x] Codex 默认 2025-06-18 握手及 `connector.get_status` 实际调用通过。
- [ ] 三版本兼容增量尚未重跑完整 65 项 CTest；这是本轮明确的快速交付边界。

**最终判定：基础冻结树 PASS / RELEASE-READY；三版本兼容增量 FOCUSED PASS /
USABLE-CANDIDATE。完整 65 项回归补跑前，不把当前增量标记为 RELEASE-READY。**

## 8. 证据与隐私说明

原始命令、完整 stdout/stderr、结构化请求响应、调试记录、进程与端口快照、截图、fixture 私有
manifest 和清理清单保存在仓库外私有归档。本报告只使用匿名 Evidence ID；不记录本机绝对
路径、用户目录、原始素材名称、内容指纹、真实可执行位置或未清理的原始日志。

最终证据索引使用归档内相对路径映射本报告中的 Evidence ID，并已纳入完整性检查。私有归档
不提交 Git；仓库报告只保留可公开的结论、分母、失败轨迹、明确边界与残余风险。
