# Automation Facade 实现报告

## 1. 最终结论

当前协议无关的 Automation Facade 提供单 DocumentSession 运行时、统一提交语义、异步任务
保护及现有 GUI 业务入口迁移。当前共有 208 个集中 Operation ID，全部具有类型化 C++ 契约、
显式 Dispatcher 路由、真实 handler 和直接行为测试。

MCP、Headless 宿主和多文档产品行为不在 Facade 层实现。GUI、内部异步任务和协议适配器复用
同一套验证、提交、History、revision、显式幂等和错误语义。

## 2. 已交付的公共架构

| 能力 | 最终实现结果 |
|---|---|
| 运行时 | 建立 `CoreRuntime`，集中装配 Session、Resolver、Dispatcher、Committer、TaskManager 和各域 Facade |
| 文档身份 | 生产运行时保持单 DocumentSession；所有文档请求显式携带 `document_id`，写操作携带 `expected_revision` |
| 窗口身份 | GUI-only operation 显式携带 `window_id`；当前由 SingleWindowContext 接受唯一有效窗口 |
| 公共类型 | 建立强类型 Document/Window/Task/Operation/业务对象 ID、DTO、快照、MutationResult 和 AutomationError |
| Operation ID | 208 个 operation ID 只在集中符号表定义；能力集合由 `OperationIds::all()` 返回，产品调用不散落字符串 |
| Dispatcher | 统一文档、revision、对象、领域约束和宿主错误顺序，并为所有错误补齐 operation 上下文 |
| 显式路由 | Dispatcher 对集中 ID 使用类型化分支，不维护平行的 Catalog/Descriptor 注册表 |
| 提交器 | 业务命令统一执行完整验证、不可失败 ActionSequence、一次 execute、最多一次 History 和一次 revision |
| 测试运行时 | 提供可注入 resolver、fake host、受控任务执行器和隔离 Runtime，支持确定性契约及竞态测试 |

公共 DTO 不暴露 Model、QObject、QWidget 或协议 JSON。Facade 不读取隐式“当前文档”，GUI
必须先把当前状态转换成显式 ID 后再调用。

## 3. 已固定的业务一致性语义

### 3.1 文档生命周期

- 应用启动时建立 untitled Session，revision 为 0。
- New/Open 在旧文档外准备结果，提交点再次校验旧 document/revision；成功后轮换 DocumentId、
  清空 History 与 generation 状态并把 revision 置 0。
- New/Open 失败或取消不改变旧 Model、History、路径、loop、幂等缓存或任务记录。
- Save/Save As 不轮换 DocumentId 和 revision，只更新路径、工程名和 History savepoint。
- Import、编辑、Undo、Redo 仅在实际改变时增加 revision。
- Session 替换后，旧 DocumentId、旧对象 ID、排队命令和晚到异步写回不能进入新工程。

### 3.2 Command、History 与批处理

- 合法 no-op 返回 `changed=false`，不写 History、不增 revision、不发业务变更通知。
- `validate_only` 执行完整验证并返回预测，不分配对象 ID、不占用幂等键、不产生宿主或文件副作用。
- 创建类命令以 `client_ref` 返回真实创建对象 ID；该映射不进入工程文件。
- 通用 batch 为 all-or-nothing、单 History、单 revision；文件批量导入保留 `keep_successes`，但成功
  集合仍以一次原子提交进入工程。
- Undo/Redo、savepoint、focus/reveal fallback 与分支截断使用统一 History 语义。

### 3.3 幂等

- 幂等是显式 opt-in；只有请求实际携带受支持的 key 时才计算指纹并进入存储，不带 key 的调用
  不哈希、不创建记录。
- 文档级键空间为 `(document_id, operation_id, idempotency_key)`，生命周期等于当前 document
  generation。
- 同键同规范化输入返回首次结果；同键异参数、异 operation 或异初始 revision 返回
  `idempotency_conflict`。
- 相同并发请求只执行一次；异步重放返回同一 TaskId。
- `validate_only`、验证失败、提交失败和提交前取消不占键；失败或取消的已接受异步任务会释放键。
- Save、焦点和 GUI 状态不清缓存；New/Open 成功后旧 generation 键空间整体失效。

### 3.4 异步任务与派生写回

- 任务状态统一为 Queued、Running、CancelRequested、Committing 和稳定终态。
- Committing 是不可取消点；重复完成、取消竞争和晚到回调最多产生一次最终提交。
- 任务只捕获 DocumentId、base revision、OperationId、对象 ID 和不可变输入快照，不依赖跨线程
  Model/Track/Clip/Note 裸指针提交。
- 推理以 clip revision、piece 输入签名、音符归属和声线快照复检目标；合法兄弟分段可在同一
  DocumentId 内依次重基提交，旧 generation 或输入变化仍被拒绝。
- 音频解析、解码、哈希和状态写回使用 source generation 与各操作实际依赖的指纹；可重建缓存
  不增 revision/History，持久化结果仍遵守统一提交规则。
- Save、Save As、Import 的 busy 窗口会延期已完成任务的提交；New/Open 换代后旧任务只能结束，
  不能写入新文档。

## 4. 208 个 operation 的域覆盖

| 域 | 数量 | 已实现能力 |
|---|---:|---|
| application | 3 | 应用信息、退出、重启及窗口/宿主边界 |
| documents | 10 | New、Open、Import、Save、文档状态与 generation |
| project/tracks/clips/audio/imports | 44 | 工程快照、轨道和片段编辑、音频资产状态、声音上下文与批量导入 |
| notes | 21 | 查询、插入、移动、删除、缩放、量化、拆分、歌词、语言、发音与音素 |
| parameters/speaker mix | 29 | 参数查询/绘制/锚点、固定/动态混合、关键帧、继承和兼容入口 |
| timeline/master | 11 | Tempo、拍号、时间线快照和 Master 查询/细粒度控制 |
| history | 3 | 状态、Undo、Redo |
| inference | 15 | 能力、状态、任务，以及发音、音素、时长、音高、方差、声学、分段和参数写回 |
| extraction | 3 | 能力查询、RMVPE 音高提取、GAME MIDI 提取 |
| exports/formats | 9 | 格式检查、MIDI/音频能力、preview/start/cleanup |
| tasks | 3 | Task get/list/cancel 与稳定终态 |
| playback | 10 | 播放、暂停、停止、seek/位置、last position、loop 设置/启用/清除 |
| editor | 25 | 能力/状态、selection、reveal、视图恢复、面板/子区域、视口、焦点、量化和自动翻页 |
| settings/recent/search paths | 15 | 九个设置域、Recent CRUD、包搜索路径 |
| packages | 4 | 包列表、刷新、验证、工程声音解析 |
| speaker mix presets | 3 | 预设 list/save/delete |
| **合计** | **208** | **集中 ID、显式路由、真实 handler 和领域行为覆盖** |

## 5. GUI 与既有业务入口迁移结果

- 轨道、片段、音符、歌词、音素、参数、Speaker Mix、Tempo、拍号、Master 和 History 的最终提交
  已统一进入 Facade。
- 文档 New/Open/Import/Save、格式发现、MIDI 导出、音频导出、Pitch/MIDI 提取、音频解析/解码/
  哈希和全部推理写回已进入统一文档与任务契约。
- 播放、loop、selection、reveal、视图、量化、自动翻页、Recent、设置、包路径、包验证/解析、
  Speaker Mix 预设和应用退出/重启已具有类型化入口。
- GUI 保留绘制、hover、拖动预览、对话框、文件选择和剪贴板；最终语义提交不再依赖焦点或选区
  推断目标。
- 架构守卫禁止重新引入分散 operation 字符串、直接 History/revision 写入、公开语义 mutator、
  推理结果绕过 Facade、音符 raw-pointer 所有权接口及其他已迁移域的旁路。

## 6. 实现及全量测试中一并修复的产品问题

### Automation 与编辑契约

- 统一补齐 Query、Command 和 Facade 前置校验错误的 OperationId、字段与对象上下文。
- 修复异步任务失败/取消后幂等键不释放，以及同键重试无法重新启动的问题。
- 修复 Note 列表在移动捕获后生成空指纹的问题；移动、删除、左右缩放、量化和词属性的同键异参
  现在均稳定冲突。
- 修复轨道移动 insertion-index 的相邻 no-op 和拖到列表末尾语义。
- 补齐片段材料范围、音素偏移数量/单调性、参数 Draw step、量化网格等领域验证，并消除量化时
  修改遍历容器导致的越界/挂起。
- 补齐 Editor、Recent 和 Speaker Mix Preset 的前置验证、未知 ID、权重归一化及持久化边界。

### 推理、任务与音频资产

- 推理快照改为在业务提交完成后捕获，避免同步 Model signal 读取旧 revision。
- 支持经过目标级复检的兄弟分段依次写回，消除大型多轨工程的 revision 风暴、持续闪烁和后续
  Clip 无法推理；输入变化和撤销仍能阻止晚到结果。
- 撤销进行中的短音符推理后，不再留下空红色失败段、晚到波形或声音。
- 修复相对音频路径打开、fallback 重定位、无 hash 同名确认、解码/哈希竞态和 Save/Import busy
  期间丢失完成结果的问题。
- 引入音频 source generation，阻止同路径换源或 metadata 变化后的旧 cache/hash/status 覆盖新源。
- 统一 Missing、Unconfirmed、Normal 状态顺序；失败源保持静音，成功解码不会隐式确认
  Unconfirmed，重定位确认后可保存并重开。
- 插入或 Undo 恢复带 AudioClip 的轨道时会重新连接解析/解码；删除、换代和退出会终止对应任务。

### GUI、设置与文件工作流

- 音符绘制/拆分改用 Facade 返回的真实 created ID，消除临时 Note 被删除后的悬空访问和错误选择；
  提交失败时编辑会话按 Discard 结束。
- 轨道颜色公开 setter 改为通过 Facade 进行单 History/revision 提交并支持 Undo/Redo；界面临时
  hover 预览仍保持非持久化。
- 可选场景项不存在时不再调用无效 removeItem，消除绘制后的场景警告。
- 自动翻页设置会同步传播到 Track 与 Piano Roll 视图。
- Fill Lyric 保留语言与 Tagger 身份，规则重排不再丢失当前编辑，保存/重开顺序一致。
- Inference Playback Lookahead Window 完整持久化并可恢复。
- MIDI 导入保留有效片段几何，混合批次会报告失败项且不留下半提交。
- Undo 回到保存基线时能恢复 clean 状态和正确 savepoint。
- 修复本地化 MSVC 输出下 Ninja 依赖信息丢失，构建依赖统一保持 UTF-8。

## 7. 测试与长期保护产物

- 208 个 operation 使用集中 ID、显式 Dispatcher 路由和按领域组织的行为测试；测试按实际语义
  覆盖正常、拒绝、no-op、回滚和竞态路径，不维护 Descriptor 镜像或源码扫描门禁。
- 已建立文档生命周期、显式幂等、任务竞态、编辑域、运行时域、异步文件域、音频
  资产、Piano Roll 提交、Fill Lyric、MIDI 导入、设置持久化等直接回归目标。
- 幂等只覆盖显式 opt-in 的代表性操作；任务竞态覆盖 cancel/commit barrier、重复完成、对象删除
  和 generation 换代。
- 测试专用 Modifier 输入桥与受控推理延迟均由编译/运行开关隔离，正式产品默认不启用。
- 本轮最终 Debug 全目标构建与一次完整 CTest 结果见 `test-report.md`。

## 8. 明确边界

以下内容不属于 Automation Facade 层：多个真实 DocumentSession、DocumentRegistry、
WindowRegistry、跨文档操作、多个真实窗口的生命周期、MCP/HTTP/JSON-RPC transport、权限
profile、Headless bootstrap、独立 Core target，以及尚无真实后端的路线图/TODO 能力。

这些边界不作为 skipped operation 混入 208 项能力面。增加真实能力时，应同时增加类型化
handler、集中 OperationId、显式 Dispatcher 路径、Facade 路径和测试矩阵。
