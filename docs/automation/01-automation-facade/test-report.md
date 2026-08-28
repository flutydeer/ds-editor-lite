# 一期 Automation Facade 最终测试报告

## 1. 结论

本报告记录当前最终 Automation Facade 架构的回归结果。内部能力集合为
`OperationIds::all()` 中的 208 个稳定 Operation ID；Dispatcher 使用显式类型化路由，不维护
`OperationCatalog` 或 `OperationDescriptor` 注册表。

本轮最终判定：**通过**。

## 2. 构建与自动化结果

| 门禁 | 最终结果 |
|---|---|
| Debug configure/generate | 项目标准 preset `ConfigureAndBuild` 通过 |
| Debug 全目标构建 | `all` target 通过 |
| 注册 CTest | 62 项 |
| 一次完整 CTest | 62/62 通过，35.58 s |
| Qt/进程异常 | 定向回归 7/7 通过，15.54 s；最终无遗留异常或无人值守弹窗 |

测试使用项目标准 preset wrapper。Qt 组件测试显式配置可用的 offscreen platform plugin 路径。
任一失败都先保留首次证据并修复，随后运行最小复现、所属域回归和一次完整 CTest。

## 3. 内部契约覆盖

- 208 个 Operation ID 集中定义，`OperationIds::all()` 是能力集合的唯一运行时来源；该数量作为当前产品快照记录，不作为测试硬编码门禁；
- Dispatcher 使用显式类型化路由，代表性 Query/Command、未知文档、revision 与 handler 错误具有稳定 operation 上下文；
- 不维护精确 Descriptor 镜像，也不使用源码文本扫描作为实现完整性的替代品；
- Facade、CommandCommitter、History、revision、DocumentSession 和 TaskManager 由行为测试覆盖；
- 幂等为显式 opt-in：只有实际携带受支持的 `idempotency_key` 时才计算请求指纹并进入存储，
  不带 key 的调用不哈希、不创建幂等记录。

内部契约实测结果：**通过**。共享 Dispatcher 边界、领域独特行为、确定性输出校验和显式
opt-in 幂等均通过完整 CTest；测试不再为每个 Operation 复制同构错误矩阵或数量断言。

## 4. 一致性与竞态

回归范围包括：

- New/Open generation、旧 DocumentId、save/save-as、savepoint 和失败回滚；
- document → revision → object/type → domain 的错误优先级；
- 合法 no-op、单 History、单 revision、Undo/Redo 与分支截断；
- 显式幂等重放、冲突、并发去重、失败释放与 generation 隔离；
- Task 的 Queued、Running、CancelRequested、Committing 和稳定终态；
- cancel/commit、对象删除、revision 前进、文档换代与晚到写回竞态；
- 轨道、片段、音符、参数、Speaker Mix、时间轴、播放、设置、文件和推理领域行为。

一致性与竞态实测结果：**通过**。幂等重放按工具显式能力生效；真实联调中的陈旧 revision
与未结束 Task 冲突均返回稳定 `revision_conflict`，客户端通过重新查询后重试完成闭环。

## 5. GUI 与真实环境

Computer Use 与 Connector 联调在独立测试实例和隔离工作区执行。GUI 负责验证用户入口、可见状态、
Undo/Redo、播放头、波形、设置与文件结果；内部 ID、revision、Task 状态和 DTO 由确定性测试及
结构化调用补证。自动化路径持续监控模态窗口与无响应状态，用户提供的素材根始终只读。

GUI 与真实环境实测结果：**通过**。测试实例打开只读素材副本后完成精确声库选择、轨道 voice、
片段和 7 个音符创建；界面可见歌词、发音、音高曲线和合成波形。播放头由 101:01:341 前进至
102:02:437，工程另存成功，并在无弹窗情况下关闭。

## 6. 数据安全与清理

- 所有写操作使用测试拥有的副本或输出目录；
- 测试前后按相对路径与 SHA-256 对照只读素材；
- 临时配置、缓存、工程副本、进程、端口和 QLocal 资源按所有权清单管理；
- 正式报告不记录用户名、绝对路径、真实端口、PID、对象 ID 或素材名称；
- 失败现场保存在仓库外私有归档，成功后的可重建产物按清单清理。

数据安全与清理实测结果：**通过**。素材源 19/19 项前后 SHA-256 一致，真实用户应用配置
前后 SHA-256 一致；写入仅发生在测试拥有的副本和输出位置。证据归档为下载目录中的
`DS-Editor-Lite-MCP-Simplification-Test-Archive-20260828`。

## 7. 最终通过清单

- [x] Operation ID 保持单一来源，显式 Dispatcher 共享边界与领域独特行为覆盖通过。
- [x] 文档生命周期、History、revision、显式幂等和 Task 竞态通过。
- [x] GUI 与内部任务复用同一领域 Facade 和提交路径。
- [x] Debug 配置、全目标构建与一次完整 CTest 通过。
- [x] Computer Use、Connector 实际联调与无人值守窗口监控通过。
- [x] 用户素材零改动、应用配置恢复和测试进程/状态清理通过。

最终签署：**通过，可以交付**。
