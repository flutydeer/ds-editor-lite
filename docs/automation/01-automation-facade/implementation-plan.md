# 一期：Automation Facade 与必要重构实施计划

## 1. 目标与边界

一期建立协议无关的自动化业务入口，统一现有 GUI、内部任务和未来协议适配器的
业务语义。实现以当前可达、可工作的产品能力为范围，不为补齐路线图矩阵而新增
尚不存在的产品功能。

本期采用单 DocumentSession：生产运行时始终最多装配一个文档，但所有文档 Query
和 Command 都显式携带 `document_id`，写操作额外携带 `expected_revision`。Facade
不得通过活动窗口、焦点或选区推断业务目标。未来多文档只替换 session resolver 和
状态所有权，不改变 operation DTO。

本期明确不实现：

- 多个真实 DocumentSession、DocumentRegistry 和跨文档操作；
- 多个 Model、History、DocumentWorkflow 或窗口实例；
- MCP、HTTP、JSON-RPC、权限等级、Headless 启动和独立 Core target；
- 仅存在于路线图、TODO、占位入口或没有真实后端的能力；
- 为被动渲染读取进行完整 snapshot/MVVM 改造。

GUI 的绘制、hover、动画、拖动过程预览、对话框、文件选择和剪贴板可保留在 GUI
侧；所有最终业务写入、History、文件/异步提交以及稳定 GUI 状态命令必须经过
Facade。同一业务域完成迁移后不得继续保留第二套业务实现。

## 2. 运行时和公共契约

### 2.1 逻辑结构

```text
AppContext
├─ CoreRuntime
│  ├─ AppModel / HistoryManager
│  ├─ DocumentSession
│  │  ├─ DocumentId / Revision
│  │  ├─ path / project name / lifecycle
│  │  └─ IdempotencyStore
│  ├─ SingleDocumentSessionResolver
│  ├─ EditorAutomationFacade
│  ├─ AutomationDispatcher
│  ├─ CommandCommitter
│  └─ AutomationTaskManager
└─ GuiContext
   ├─ SingleWindowContext
   ├─ GUI Controller / Adapter
   └─ dialog / clipboard / view service
```

AppModel 和 HistoryManager 本期仍为现有唯一实例。DocumentSession 逻辑聚合其访问、
文档身份、路径、revision 和生命周期。Automation 代码只能通过显式
`resolveDocument(documentId)` 获取 session，不提供无参数 `currentDocument()`。

GUI-only operation 携带明确 `window_id`。一期的 SingleWindowContext 只接受唯一
有效窗口 ID；文档业务 operation 不携带窗口 ID。

### 2.2 类型和结果

公共契约使用类型化 C++ DTO，包括：

- `DocumentId`、`DocumentVersion`、`Revision`、`WindowId`、`OperationId`、`TaskId`；
- 强类型业务对象 ID；
- `CommandContext`、`AutomationResult<T>`、`AutomationError`；
- `MutationResult`、查询快照 DTO 和异步任务 DTO。

DTO 不暴露 Model/QObject 指针、QWidget 或协议 JSON。错误模型稳定区分参数错误、
对象不存在/类型错误、文档已替换、revision 冲突、幂等冲突、能力/模块不可用、
busy、不可取消、I/O 和内部错误，并携带 operation、字段、对象及版本上下文。

### 2.3 Operation ID 与显式路由

内部能力由 `OperationIds::all()` 给出集中、稳定的 ID 集合。Dispatcher 对每个 ID 使用显式、
类型化路由；领域 Facade 的 C++ 函数签名和 DTO 是编译期契约，不维护另一份
`OperationCatalog` / `OperationDescriptor` 注册表，也不在运行时重述 document、revision、
History、file、host 或 safety 文档字段。

一期内部入口不实现 Schema AST、schema digest、权限 profile 或 `QString + QVariantMap` 式
泛化业务调用；公共 MCP 契约由 Wire 层单独维护。

所有产品 operation ID 只在 `OperationIds.h` 定义一次，Facade、任务和测试均引用该符号表。
一期不为同一进程内的契约维护 `.v1` 字符串或逐操作 wire version。

## 3. 一致性语义

### 3.1 文档生命周期

- 启动后创建 untitled session；新建/打开成功时生成新 `document_id`，revision 为 0。
- 新建/打开的加载结果在当前文档之外准备，最终提交前重新校验旧 document/revision；
  失败或取消不得改变旧 session。
- import/edit/undo/redo 保留 document ID，真正修改成功后 revision 增加一次。
- save/save-as 保留 document ID 和 revision，只更新路径、名称和 History savepoint。
- 文档替换后，旧 document ID、对象 ID、排队命令和后台写回返回
  `document_changed`。
- 校验顺序固定为 document ID、expected revision、对象 ID/类型、领域约束。

### 3.2 Command 和 History

文档修改统一执行：完整验证、构造提交后不可失败的 ActionSequence、execute、最多
record 一次、revision 最多增加一次。合法 no-op 返回 `changed=false`，不写
History、不增 revision、不发业务变更通知。

通用 batch 为 all-or-nothing、单 History、单 revision，并支持请求内唯一的
`client_ref`；创建成功时由 `MutationResult` 返回有序的 `client_ref → object_id`
绑定，绑定只属于响应、不写入工程。批量文件导入保留 `keep_successes` 行为：全部预处理
后把成功集合一次原子提交并返回逐项结果。

`validate_only` 执行完整验证并返回 `would_change`、warnings 和可预测影响，但不
分配对象 ID、不写幂等记录、不产生副作用。

### 3.3 幂等

幂等是显式 opt-in。只有调用方实际提供受支持的 `idempotency_key` 时，Dispatcher 才计算规范化
输入指纹并访问当前 document generation 的幂等记录；不带 key 的请求不哈希、不创建记录。
键为：

```text
(document_id, operation_id, idempotency_key)
```

记录从 DocumentId 创建保留到成功 new/open 替换 session、显式关闭文档或应用
退出。相同键和规范化输入重放首次结果；同键异参返回 `idempotency_conflict`。
缓存查询先于当前 revision 校验。并发相同请求只执行一次。

只缓存成功提交或已接受的异步操作；验证失败、提交前取消和 `validate_only` 不占用
键。new/open、应用级设置和纯 GUI 状态不支持文档级幂等键。异步重试返回相同
TaskId，并在当前 generation 生命周期内保持结果可查询。

### 3.4 异步任务

任务只捕获 document ID、base revision、operation ID、目标对象 ID 和必要的不可变
输入快照，不以跨线程 raw Model/Object 指针作为最终提交依据。

```text
Queued → Running → CancelRequested → Committing → Succeeded/Failed/Canceled
```

最终写回依次检查取消、重新 resolve document、检查 revision、进入不可取消的
Committing、原子提交一次。session 替换后旧任务不得写入新工程。

推理任务额外使用 clip revision、piece 输入签名、音符归属和声线快照做目标级校验。
目标级门禁通过后，允许把同一 DocumentId 下的全局 expected revision 重基到提交瞬间，
以免互不冲突的并行分段写回彼此判为过期；DocumentId 不匹配时禁止重基。持久化推理
结果仍按实际提交逐次增加 document revision，可重建缓存仍不增加。

## 4. 实施顺序和提交策略

基线为同步后的 `origin/main`。所有改动在 `codex/automation-facade-phase-1` 分支完成。

1. 建立契约、错误、集中 Operation ID、显式 Dispatcher、单槽 resolver、SingleWindowContext 和测试支点。
2. 集中 document revision、History、CommandCommitter 和幂等语义。
3. 迁移轨道/Master、片段、音符/歌词/音素。
4. 迁移参数、声线/Speaker Mix、Tempo/拍号和 History。
5. 迁移文档、导入/保存、解码、导出、提取、包任务和自动推理写回。
6. 迁移播放、Recent、设置、包路径、预设、歌词规则和稳定 GUI 状态。
7. 删除业务绕过路径并增加行为与边界回归。
8. 完成实现级保护测试、完整构建和全量 CTest，随后提交详细测试大纲。

复杂、分层或可独立验证的工作包分别提交，提交格式为
`type(scope): summary`。每个提交保持可构建并运行对应保护测试，不提交构建产物。

实现优先复用现有逻辑；兼容入口只能是调用 Facade 的薄 adapter。代码注释只用于
说明必要的业务不变量、线程约束或兼容原因，设计说明写入文档，不在代码中记录
实施会话或临时讨论。

## 5. 测试门禁

实现阶段运行保护性契约测试、受影响回归、完整构建和当前全部 CTest；只有这些门禁
通过后才输出详细测试大纲并请用户执行 GUI 冒烟。由 Codex 使用 Computer Use 执行的
全量 GUI 回归必须等待用户明确批准，不在本阶段提前启动。

`OperationIds::all()` 是能力来源，Dispatcher 为每个受支持 ID 提供显式类型化路由。测试按领域
语义覆盖适用的正常、拒绝、no-op、回滚和竞态路径，不维护 Descriptor 镜像，也不以固定场景数
作为门禁。

单 session 专项覆盖 new/open generation、旧 ID、revision、savepoint、异步竞态、
幂等清理、SingleWindowContext 和 selection 隔离。Dispatcher 使用可注入 resolver
的双 fake 测试证明它按请求 ID 路由；生产 CoreRuntime 仍只装配一个 session。

确定性测试覆盖全部内部 operation 的适用行为，并在最终候选上运行一次完整 CTest。真实环境资格验证
单列 DSPX/MIDI、音频 codec、声库、短推理和设备初始化。最终报告记录构建环境、
Operation ID 快照、命令、逐域结果、耗时、失败修复轨迹、环境限制和残余
风险。

实施产物：

- [迁移矩阵](migration-matrix.md)
- [全量测试大纲](test-outline.md)
- [实现报告](implementation-report.md)
- [最终测试报告](test-report.md)
