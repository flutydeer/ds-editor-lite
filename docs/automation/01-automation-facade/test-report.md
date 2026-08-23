# 一期 Automation Facade 最终测试报告

## 1. 结论

一期最终源码通过 Debug 全目标构建、三轮完整 CTest、122-operation 确定性矩阵、竞态/幂等压力
测试、真实文件与模型资格验证以及 GUI 全域 Computer Use 回归。

当前结论为：**一期范围内的自动化、Computer Use 和人工补测均已通过；没有隐藏的失败或待测项。**
原由 Computer Use 能力、原生确认规则、任务时长和听音端点限制而保留的 M01～M07，已由用户
完成人工补测并确认通过，结果见第 6 节。

最终测试树基于提交 `2dc60c51`；本报告整理只改变文档，不改变已测试的生产代码或测试代码。

## 2. 最终构建与自动化结果

| 门禁 | 最终结果 |
|---|---|
| Debug configure/generate | 通过 |
| Debug 全目标构建 | 通过，`DsEditorLite` 完成链接和 Qt 部署 |
| 注册 CTest | 53 |
| 完整 CTest 第 1 轮 | 53/53 通过，0 失败，0 超时，8.54 秒 |
| 完整 CTest 第 2 轮 | 53/53 通过，0 失败，0 超时，8.48 秒 |
| 完整 CTest 第 3 轮 | 53/53 通过，0 失败，0 超时，8.39 秒 |
| 三轮合计 | 159/159 通过，无 flaky |
| Qt/进程异常 | 无 platform/offscreen plugin 错误、Debug Error、ASSERT、fatal、超时或弹窗阻塞 |

Vulkan headers 缺失是既有可选能力提示，不影响本期目标、应用链接或测试执行。

### 2.1 高价值确定性套件

下表为每一轮完整 CTest 中的稳定结果：

| 套件 | Operation/场景 | 断言 | 结果 |
|---|---:|---:|---|
| Editing Dimensions | 40 operation / 273 场景 | 825 | 全部通过 |
| Runtime Dimensions | 48 operation / 357 场景 | 543 | 全部通过 |
| Async Dimensions | 34 operation / 233 场景 | 254 | 全部通过 |
| Task Races | 15 场景 | 1,729 | 全部通过 |
| Audio Asset Resolution | 17 场景 | 102 | 全部通过 |
| MIDI Import Automation | 6 场景 | 24 | 全部通过 |

此外，Catalog、架构、Core、Document Lifecycle、Idempotency、Editing Domains、Runtime Domains、
Async File Domains、Piano Roll Note Commit、Fill Lyric、设置、控制器和既有应用回归目标均包含在
53 个 CTest 中并全部通过。

### 2.2 契约覆盖结论

- OperationIds、Catalog 和三套逐 operation 矩阵的集合均为 122/122，重复、缺失和额外项均为 0。
- 122 个 operation 全部具有直接 handler 行为断言；不存在只测 Catalog、未进入 handler 的条目。
- 产品 operation ID 均引用集中常量，无 `.v1` 后缀和分散字符串字面量。
- 文档命令覆盖 DocumentId → revision → object/type → domain 的错误优先级。
- 适用命令覆盖正常提交、合法 no-op、validate-only、History/revision、Undo/Redo、宿主/I/O 失败
  回滚和幂等重放/冲突。
- 幂等协议通过 16/64 路并发一次执行、失败释放、TaskId 重放和 generation 隔离。
- 异步协议覆盖 Queued、Running、CancelRequested、Committing、terminal、取消/提交点竞争、重复
  完成、对象删除、revision 前进和 New/Open generation 换代。
- 单 Session Resolver 通过双 fake 证明按请求 DocumentId 路由；生产运行时仍只装配一个 Session。

## 3. GUI 与真实环境最终结果

| 范围 | 最终结论 |
|---|---|
| 隔离与 Modifier 桥 | 隔离配置/缓存、Control/Shift/Alt/组合鼠标与滚轮桥通过；正式构建默认关闭 |
| 文档与 Recent | New/Open/Import/Save/Save As、取消、坏文件回滚、Recent 与 savepoint 通过 |
| 轨道与 Mixer | 插入、移动、删除、属性、颜色、默认语言、Master、Undo/Redo 通过 |
| Clip、音符、歌词与音素 | 创建、移动、缩放、拆分、跨轨、剪贴板、量化、单字歌词、音素偏移通过 |
| 参数与 Speaker Mix | 参数曲线、锚点、Track/Clip 固定与动态混合、继承、预设 CRUD 通过 |
| Tempo、拍号与 History | 锚点、排序、删除保护、Master、focus/reveal、分支截断通过 |
| Editor 与 Playback | selection、reveal、面板、缩放、编辑模式、量化、自动翻页、loop 和播放控制通过 |
| 设置与应用 | General、Appearance、Developer、Audio、Inference、Window、重启、退出和持久化恢复通过 |
| 包与声音解析 | 搜索路径、包列表/验证、voice 解析、动态 Speaker Mix 菜单和配置恢复通过 |
| MIDI/DSPX/LibreSVIP | MIDI 取消/选择/导入、DSPX Open/Import、LibreSVIP 转换、跨窗口拖放、原子回滚和保存通过 |
| 音频资产 | 普通导入、相对路径、Missing Relink、无 hash 同名 Confirm、错误候选拒绝、长解码取消和保存重开通过 |
| 推理与提取 | 零缓存 4 轨/8 Clip 推理及逐项听音、受控延迟 Undo、RMVPE Pitch、GAME MIDI、完成/取消和单步 Undo 通过 |
| MIDI 导出 | Cancel/No、Confirm/Yes、新文件导出、独立解析和 round-trip 全部通过 |
| 应用生命周期 | 两次真实 Restart、单进程/单窗口、窗口模式恢复、Exit Cancel、运行任务退出、干净退出和最终清理通过 |

## 4. 真实后端与文件资格结论

- DSPX 基线工程包含 4 条歌声轨和 1 条音频轨，Tempo、拍号、loop、参数、音频引用和保存重开
  均已实际验证。
- 零缓存推理工程的 4 条歌声轨各含 2 个可区分 Clip；8 个 Clip 均从 cache miss 到 Success，
  显示稳定音符、音素、音高和非平直合成波形，无持续闪烁、红色失败段或跨 Clip 串写。
- 受控 30 秒声学延迟中 Undo 后，旧任务进入 canceled；超过原完成窗口后音符、分段、波形和
  失败标记均未恢复。
- RMVPE 模型通过 DirectML 完成真实 Pitch 提取；一次 Undo 完整移除目标 edited pitch。
- GAME 四个模型组件通过 DirectML 完成真实 MIDI 提取，新增 1 条歌声轨和 12 个音符；一次 Undo
  后工程字节恢复为提取前结果。
- MIDI 导出结果可独立识别为 SMF format 1；回读恢复 4 条歌声轨、883 个音符、歌词、两个 Tempo
  和两个拍号。
- LibreSVIP CLI 已实际完成目标格式转换，并通过 Open 与 Import 两条产品路径验证。
- 普通音频、缺失音频 Relink、无 hash 同名确认、SHA-512 保护和正确候选自动恢复均通过。

## 5. 最终通过清单

- [x] 122 个 Catalog operation 均有唯一集中 ID、descriptor、handler 和直接行为覆盖。
- [x] 文档业务不依赖隐式当前文档；GUI selection 不成为命令的隐式目标。
- [x] 生产运行时最多一个 DocumentSession，New/Open generation 语义稳定。
- [x] 所有文档提交遵守统一验证、History、revision、no-op 和原子性契约。
- [x] 幂等重放、冲突、并发去重、失败释放和 generation 生命周期通过。
- [x] 异步任务状态机、提交点、重复完成、删除、revision 和 generation 竞态通过。
- [x] 推理、提取、音频资产派生写回不向新文档或变化后的旧目标写入。
- [x] GUI 业务提交已迁移到 Facade；架构守卫阻止已知旁路回退。
- [x] 自动化元数据和运行期 Speaker Mix ID 不进入工程序列化结果。
- [x] 应用、全部测试目标和连续三轮 CTest 通过。
- [x] 用户基础 GUI 冒烟通过，Computer Use 对全部可执行域完成双保险回归。
- [x] 测试配置已恢复、缓存已清理；输出和生成文件始终位于隔离环境并已归档。
- [x] M01～M07 人工补测全部完成并由用户确认通过。

## 6. 人工补测结果

以下场景原未被自动化结果冒充为通过；现已由用户在工作副本和可丢弃输出上完成人工补测，
M01～M07 全部为 **PASS**。

### M01：工程与音频混合跨窗口拖放

- [x] 在文件管理器中同时选择一个可导入工程和一个有效短音频，拖到编辑器工作区。
- 预期：混合工程批次被整体拒绝并显示明确消息；当前工程的轨道、Tempo、拍号、loop、History
  和未保存状态均不产生半提交。
- 清理：关闭提示；若任何对象被错误创建，保留副本并记录拖放前后截图，不在原工程继续操作。
- 人工结果：**PASS**。

### M02：不支持文件跨窗口拖放

- [x] 把一个明确不支持的普通文本文件拖入编辑器。
- 预期：显示不支持/无法导入提示；Model、History、revision、Recent 和文件内容均不变。
- 人工结果：**PASS**。

### M03：MIDI 已存在目标确认覆盖

- [x] 准备一个可丢弃、内容与当前工程不同的既有 MIDI，先记录大小和 SHA-256。
- [x] 执行 File → Export → MIDI file，选择该文件，并在原生确认框选择 Yes/Confirm。
- 预期：文件被一次性替换为非空、可解析的 MIDI，大小或 SHA-256 改变；没有临时文件或半写入。
- [x] 新建工程，导入覆盖后的 MIDI 并勾选全部轨、Tempo 和拍号。
- 预期：轨数、音符位置、可表示歌词、Tempo 和拍号与源工程一致。
- 人工结果：**PASS**。

### M04：零缓存 8 Clip 逐项听音

- [x] 在安全音频端点下清空测试缓存，打开 4 条歌声轨、每轨 2 个 Clip 的推理工程。
- [x] 等待 8 个 Clip 均为 Success；逐轨 Solo 或静音其他轨，把播放头放入每个 Clip 后逐一播放。
- 预期：每个 Clip 均有与自身内容对应的非静音声音，电平和播放头正常；无静音、串轨、旧结果、
  持续闪烁或红色失败段。
- 人工结果：**PASS**。

### M05：长音频解析/解码取消

- [x] 导入足够慢的有效长音频并在 Running 阶段 Cancel。
- 预期：任务终态为 Canceled；不留下空轨、空 Clip、旧音频、波形 cache、Missing/Normal 错乱或
  后续晚到写回。
- 人工结果：**PASS**。

### M06：Pitch/MIDI 提取取消

- [x] 分别对足够长的音频启动 Extract Pitch 和 Extract MIDI，在 Running 阶段各 Cancel 一次。
- 预期：Pitch 不留下半条曲线；MIDI 不留下半条歌声轨或部分音符；任务稳定为 Canceled，随后可
  重新执行并成功完成。
- 人工结果：**PASS**。

### M07：运行任务期间退出

- [x] 在可稳定保持 Running 的专用任务中请求 File → Exit。
- 预期：应用先处理任务取消/结束，再退出；没有残留任务框、后台进程、半提交工程或下次启动
  自动写回。
- 人工结果：**PASS**。

### 人工确认

- 确认日期：2026-08-23。
- 确认范围：M01～M07。
- 最终结果：**7/7 PASS**。

## 7. 不属于一期测试分母的能力

- 多个真实 DocumentSession、DocumentRegistry、跨文档复制/拖放或原子 batch。
- 多个真实窗口、WindowRegistry、窗口创建/关闭和 Window→Document 绑定。
- MCP、HTTP、JSON-RPC、权限 profile、Headless bootstrap 和独立 Core target。
- 当前没有真实后端的路线图/TODO 能力。
- 音频导出 GUI；该入口受既定 GUI smoke 边界排除，未以 MIDI 导出结果替代。

历史逐轮记录、证据索引、脱敏证据和完整隔离回归产物已归档到仓库外；本报告只保留最终结论、
可复核计数、明确边界和人工待补项。
