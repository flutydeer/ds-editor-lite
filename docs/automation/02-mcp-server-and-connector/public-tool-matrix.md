# 二期公共 MCP 工具矩阵

## 1. 口径与机器不变量

本矩阵冻结二期 editor Public Automation Manifest 的公共业务工具分母。总计 **87** 项：

```text
Meta/发现 4 + L1 只读 9 + L1 编辑 42 + L2 32 = 87
```

`automation.get_options` 本身属于稳定元工具，但它的目标、参数和返回内容继承目标工具的
profile、Custom、host 和 connector exposure。L1 preset 实际可见 55 项；L2 preset 累积可见
全部 87 项。Custom 始终保留四个元工具，其他工具按稳定 operation ID 独立开关。

本表不包含 DS Connector Lite 自己的六个固定桥接工具，也不包含一期 InternalOnly、L3、
Headless/JSON-RPC 或未来多窗口条件工具。每一行的 `P2-TOOL-NNN` 是稳定测试追踪号；实现后
Binding Registry、Manifest、editor `tools/list`、connector 内置业务描述和本文 ID 集合必须
完全相等且无重复。

标记说明：

- `Q/S`：同步 Query；`C/S`：同步 Command；`C/A`：返回 `TaskAccepted` 的异步 Command。
- `A`：一期同名类型化 handler 可直接绑定；`A*`：先做确定的契约改名；`A/B`：一期底座
  已有，仍需公共 DTO、能力/路径或动态值绑定；`B`：组合一期底座的新公共入口；`C`：新增
  细粒度领域 Facade；`N`：纯二期协议/安全能力。
- 所有本期 public host 都是运行中的 GUI editor；descriptor 保留未来非 GUI host 表达能力，
  但本期不做 Headless 资格声明。

## 2. Meta 与 L1 只读发现（13）

| 追踪号 | 公共工具 | Profile | 类型 | 底座 | 绑定与本期要点 |
|---|---|---|---|---|---|
| P2-TOOL-001 | `application.get_info` | Meta | Q/S | A | 裁剪为产品名、版本、平台；不得公开退出/重启 |
| P2-TOOL-002 | `automation.get_status` | Meta | Q/S | B | 组合实例、profile、Manifest、当前 document/window ID 与 revision |
| P2-TOOL-003 | `automation.get_manifest` | Meta | Q/S | N | 全局版本/digest、逐工具 descriptor、Schema、可用性与分页 |
| P2-TOOL-004 | `automation.get_options` | Meta（继承目标） | Q/S | N | 严格 target-input 子集、`valueSources`、不得探测隐藏目标 |
| P2-TOOL-005 | `documents.get` | L1 | Q/S | A | 显式 `document_id`，返回路径、名称、busy、dirty/savepoint |
| P2-TOOL-006 | `project.get` | L1 | Q/S | A | 有序轨道/片段稳定快照和强类型对象 ID |
| P2-TOOL-007 | `notes.get` | L1 | Q/S | A | 指定 singing clip，返回有序音符与稳定 ID |
| P2-TOOL-008 | `parameters.get` | L1 | Q/S | A/B | 公共快照补稳定 `curve_id`、`anchor_id` 与封闭参数类型 |
| P2-TOOL-009 | `parameters.get_capabilities` | L1 | Q/S | B | 参数名、层、范围、步长、曲线/插值及编辑性能力视图 |
| P2-TOOL-010 | `timeline.get` | L1 | Q/S | A | tempo、拍号锚点和 document version |
| P2-TOOL-011 | `history.get_state` | L1 | Q/S | A | undo/redo 名称、savepoint 和栈状态，无副作用 |
| P2-TOOL-012 | `voices.list` | L1 | Q/S | B | 歌手/说话人稳定 `VoiceRef`；不暴露安装路径 |
| P2-TOOL-013 | `voices.describe` | L1 | Q/S | B | singer→speaker→语言、默认值、G2P 与混合能力 |

## 3. L1 当前文档基础编辑（42）

### 3.1 轨道与片段（10）

| 追踪号 | 公共工具 | 类型 | 底座 | 绑定与本期要点 |
|---|---|---|---|---|
| P2-TOOL-014 | `tracks.insert` | C/S | A | 显式索引、`TrackDraft`、`client_ref`，单 History/revision |
| P2-TOOL-015 | `tracks.remove` | C/S | A | 批量 ID、完整预检、all-or-nothing |
| P2-TOOL-016 | `tracks.move` | C/S | A | 单轨目标索引，边界与 no-op |
| P2-TOOL-017 | `tracks.set_properties` | C/S | A | 名称/增益/声像/静音/独奏的封闭稀疏属性 DTO |
| P2-TOOL-018 | `tracks.set_color` | C/S | A | 颜色范围由共享调色板常量生成 |
| P2-TOOL-019 | `tracks.set_default_language` | C/S | A/B | `valueSources` 指向当前 VoiceCapabilities |
| P2-TOOL-020 | `clips.insert` | C/S | A | 公共 Schema 仅允许 singing clip；音频走 L2 import |
| P2-TOOL-021 | `clips.remove` | C/S | A | 批量显式 ID、级联对象与撤销原子性 |
| P2-TOOL-022 | `clips.set_properties` | C/S | A | 名称/几何/增益/静音，可显式移至目标轨道 |
| P2-TOOL-023 | `clips.set_default_language` | C/S | A/B | 仅 singing clip，语言来自片段实际声音能力 |

### 3.2 音符、歌词与音素（9）

| 追踪号 | 公共工具 | 类型 | 底座 | 绑定与本期要点 |
|---|---|---|---|---|
| P2-TOOL-024 | `notes.insert` | C/S | A | 批量 `NoteDraft`、重叠规则、`client_ref` 真实 ID 映射 |
| P2-TOOL-025 | `notes.remove` | C/S | A | clip 归属、重复 ID 与原子删除 |
| P2-TOOL-026 | `notes.move` | C/S | A | 批量时间/音高 delta、边界、no-op 与重叠 |
| P2-TOOL-027 | `notes.resize_left` | C/S | A | 批量左边界与最短长度 |
| P2-TOOL-028 | `notes.resize_right` | C/S | A | 批量右边界与最短长度 |
| P2-TOOL-029 | `notes.split` | C/S | A | 单命令拆分、子音符 draft/ID、单次撤销 |
| P2-TOOL-030 | `notes.quantize` | C/S | A | 量化值由共享枚举生成，起点/长度开关显式 |
| P2-TOOL-031 | `notes.set_word_properties` | C/S | A/B | 批量歌词/语言/发音/音素；语言绑定动态来源 |
| P2-TOOL-032 | `notes.set_phoneme_offsets` | C/S | A | 单音符边界数量、单调性与清空语义 |

### 3.3 参数曲线（8）

| 追踪号 | 公共工具 | 类型 | 底座 | 绑定与本期要点 |
|---|---|---|---|---|
| P2-TOOL-033 | `parameters.replace` | C/S | A/B | 显式替换一个参数层；范围和曲线类型由 capability 约束 |
| P2-TOOL-034 | `parameters.draw` | C/S | C | 局部采样绘制/覆盖，封闭 `CurveMergeMode` |
| P2-TOOL-035 | `parameters.erase` | C/S | C | 局部区间擦除并按领域规则归一化边界 |
| P2-TOOL-036 | `parameters.insert_anchor` | C/S | C | 可指定 `curve_id`，生成稳定 `anchor_id`，插值枚举 |
| P2-TOOL-037 | `parameters.move_anchor` | C/S | C | 按稳定 `anchor_id` 原子移动位置和值 |
| P2-TOOL-038 | `parameters.remove_anchor` | C/S | C | 按稳定 ID 删除并保持曲线有效 |
| P2-TOOL-039 | `parameters.set_anchor_interpolation` | C/S | C | 仅修改一个锚点插值，不全量替换曲线 |
| P2-TOOL-040 | `parameters.bake` | C/S | C | 将可重建原始曲线/选定区间烘焙为可编辑曲线 |

### 3.4 声线与 Speaker Mix（8）

| 追踪号 | 公共工具 | 类型 | 底座 | 绑定与本期要点 |
|---|---|---|---|---|
| P2-TOOL-041 | `speaker_mix.track.select_single` | C/S | A | singer/speaker 稳定引用与单说话人归一化 |
| P2-TOOL-042 | `speaker_mix.track.apply` | C/S | A | 应用完整轨道声音上下文和混合 |
| P2-TOOL-043 | `speaker_mix.track.replace` | C/S | A | 仅替换混合数据并保留既有 voice 语义 |
| P2-TOOL-044 | `speaker_mix.clip.use_track` | C/S | A | 恢复轨道继承，合法 no-op |
| P2-TOOL-045 | `speaker_mix.clip.select_single` | C/S | A | 片段自有 singer/speaker 上下文 |
| P2-TOOL-046 | `speaker_mix.clip.enable_dynamic` | C/S | A | 动态混合模式、keyframe 排序与归一化 |
| P2-TOOL-047 | `speaker_mix.clip.apply` | C/S | A | 应用完整片段声音上下文和混合 |
| P2-TOOL-048 | `speaker_mix.clip.replace` | C/S | A | 仅替换片段混合数据，保留自有 voice |

### 3.5 时间线、Master 与 History（7）

| 追踪号 | 公共工具 | 类型 | 底座 | 绑定与本期要点 |
|---|---|---|---|---|
| P2-TOOL-049 | `tempos.set` | C/S | A | 新增/替换 tempo，排序及 tick 0 锚点 |
| P2-TOOL-050 | `tempos.delete` | C/S | A | 禁止删除 tick 0，缺失点 no-op |
| P2-TOOL-051 | `time_signatures.set` | C/S | A | bar、分子/分母范围、排序及 bar 0 锚点 |
| P2-TOOL-052 | `time_signatures.delete` | C/S | A | 禁止删除 bar 0，缺失点 no-op |
| P2-TOOL-053 | `master.set_control` | C/S | A | gain/pan/mute/solo，非有限值拒绝 |
| P2-TOOL-054 | `history.undo` | C/S | A | 显式 document/revision，空栈语义与单 revision |
| P2-TOOL-055 | `history.redo` | C/S | A | 显式 document/revision，分支与 savepoint 语义 |

## 4. L2 文件、播放与完整创作（32）

### 4.1 文件权限、文档与格式（6）

| 追踪号 | 公共工具 | 类型 | 底座 | 文件/任务与本期要点 |
|---|---|---|---|---|
| P2-TOOL-056 | `automation.get_file_access` | Q/S | N | 只读返回 canonical read/write roots 与 session grants |
| P2-TOOL-057 | `documents.new` | C/S | B | 公共编排 `commit_new`；显式 reject/discard 未保存策略 |
| P2-TOOL-058 | `documents.open` | C/A | B | 受限读路径；解析、包解析、原子换代与失败保留旧 session |
| P2-TOOL-059 | `documents.import` | C/A | B | 受限读路径；格式路由、tempo/拍号和 merge enum |
| P2-TOOL-060 | `documents.save` | C/S | A/B | 受限写路径、无路径规则、overwrite 与 savepoint |
| P2-TOOL-061 | `formats.list` | Q/S | A/B | 打开/导入/导出格式、可用性和各格式 option Schema |

### 4.2 音频片段与导出（8）

| 追踪号 | 公共工具 | 类型 | 底座 | 文件/任务与本期要点 |
|---|---|---|---|---|
| P2-TOOL-062 | `audio_clips.import` | C/A | B | 受限读路径；解码、创建 clip、`client_ref` 与 revision 复检 |
| P2-TOOL-063 | `audio_clips.import_batch` | C/A | B | 多路径；`atomic`/`best_effort` 明确失败策略与单次提交 |
| P2-TOOL-064 | `audio_clips.relocate` | C/S | A/B | canonical 受限读路径，History、类型与 no-op |
| P2-TOOL-065 | `audio_clips.confirm_path` | C/S | A/B | canonical 受限读路径，候选路径和文档版本 |
| P2-TOOL-066 | `exports.midi.start` | C/A | A/B | 受限写路径、类型化 options、overwrite、TaskAccepted |
| P2-TOOL-067 | `exports.audio.get_capabilities` | Q/S | B | 格式、采样率、声道/混音、导出源与依赖关系 |
| P2-TOOL-068 | `exports.audio.preview` | Q/S | A/B | 类型化计划/diagnostics，无文件写入和任务分配 |
| P2-TOOL-069 | `exports.audio.start` | C/A | A/B | 受限写路径、类型化 options、渲染任务、错误与清理 |

### 4.3 提取、推理与任务（9）

| 追踪号 | 公共工具 | 类型 | 底座 | 文件/任务与本期要点 |
|---|---|---|---|---|
| P2-TOOL-070 | `extract.get_capabilities` | Q/S | B | clip 提取器、模型、范围、模块准备状态与 option Schema |
| P2-TOOL-071 | `extract.pitch.start` | C/A | A/B | 类型化 options、TaskId、取消和 revision/generation 写回门禁 |
| P2-TOOL-072 | `extract.midi.start` | C/A | A/B | 类型化 options、音符原子写回、`client_ref`/TaskId |
| P2-TOOL-073 | `inference.get_capabilities` | Q/S | B | scope、stage、provider/GPU/模型/声音和模块准备状态 |
| P2-TOOL-074 | `inference.start` | C/A | B | 公共 orchestrator 调用内部 apply/refresh 流水线，不公开写回步骤 |
| P2-TOOL-075 | `inference.reset_stage` | C/S | B | 稳定 scope/stage 语义，组合内部 invalidate/rebuild 能力 |
| P2-TOOL-076 | `tasks.list` | Q/S | A* | 由 `operations.list` 改名；当前 generation、分页、状态/kind 过滤 |
| P2-TOOL-077 | `tasks.get` | Q/S | A* | 由 `operations.get` 改名；结构化进度、结果、错误与创建者诊断 |
| P2-TOOL-078 | `tasks.cancel` | C/S | A* | 由 `operations.cancel` 改名；取消中/终态/提交点稳定语义 |

### 4.4 播放与循环（9）

| 追踪号 | 公共工具 | 类型 | 底座 | 并发与本期要点 |
|---|---|---|---|---|
| P2-TOOL-079 | `playback.get` | Q/S | A | 状态、位置、loop 与可播放性快照 |
| P2-TOOL-080 | `playback.play` | C/S | A | playback scope 串行、设备/模块/busy 错误 |
| P2-TOOL-081 | `playback.pause` | C/S | A | 状态转换、重复调用 no-op |
| P2-TOOL-082 | `playback.stop` | C/S | A | 停止与恢复位置语义、重复调用 no-op |
| P2-TOOL-083 | `playback.set_position` | C/S | A | 有限非负 tick、状态版本与无文档 revision 副作用 |
| P2-TOOL-084 | `playback.set_last_position` | C/S | A | 停止恢复位置、边界与 no-op |
| P2-TOOL-085 | `playback.set_loop` | C/S | A | 显式区间、History/revision 与错误优先级 |
| P2-TOOL-086 | `playback.set_loop_enabled` | C/S | A | 已有/空 loop 区间、History/revision 与 no-op |
| P2-TOOL-087 | `playback.clear_loop` | C/S | A | 清除 loop、单 History/revision、空 loop no-op |

## 5. Connector 固定桥接工具（不计入 87）

| 工具 | 固定职责 |
|---|---|
| `connector.get_status` | 无参返回 connector/editor/bootstrap/MCP/Manifest/exposure 事实 |
| `connector.reconnect` | 主动重新 discover/watch、连接和 Manifest 握手 |
| `editor.tools.list` | 分页列出通过 exposure 的 editor 实际目标 |
| `editor.tools.search` | 按名称、说明和 category 搜索实际目标 |
| `editor.tools.describe` | 返回一个实际目标的 Schema、版本、权限和可用性 |
| `editor.tools.invoke` | 按 editor 当前真实 Schema 泛化调用通过 exposure 的目标 |

六个桥接工具在 connector 生命周期内始终发布。它们不能观察或调用 InternalOnly、未注册 L3
或未来条件工具，也不能绕过 connector exposure 与 editor Access Policy。

## 6. 明确不注册的集合

- L3 GUI、设置、设备、Recent、包、路径和预设工具；只保留 profile/descriptor/CLI 基础。
- `documents.list`、`windows.list`、`documents.close` 等未来多窗口/多文档条件工具。
- `application.request_exit/restart` 和公共编排器使用的全部内部 commit/apply/cache/cleanup
  operation。
- `--headless`、`/automation/v1` 对应的任何工具或路由。

实现阶段必须有集合测试证明这些名称不出现在 Public Manifest、editor `tools/list`、connector
类型化工具或泛化 list/search/describe/invoke 结果中。

