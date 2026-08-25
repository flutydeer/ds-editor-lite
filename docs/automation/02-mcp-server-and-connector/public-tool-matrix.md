# 二期公共 MCP 工具矩阵

## 1. 冻结口径

二期工具面按业务域组织。Editor 提供 **128** 个公共工具，DS Connector Lite 提供 **6** 个桥接工具，合计 **134** 个。MCP tool name 是跨 Contract、Registry、Manifest、Editor 与 Connector 的稳定身份，不使用依赖表内顺序的编号。

公共工具集维持 v1：`toolsetVersion = 1`，134 个工具各自的 current version、introduced version、minimum compatible version 均为 1。版本是全表不变量，因此工具清单不设置横向版本列。

标记说明：

- `Q/S`：同步 Query。
- `C/S`：同步 Command。
- `C/A`：接受后返回任务句柄的异步 Command。
- Profile 列表示工具契约声明的最低开放层级；Meta 始终参与握手、发现和诊断。

矩阵按被查询或修改的主状态所有者归域，不按 Profile 分组。编辑工具以可整体 Undo/Redo 的历史记录为原子边界；同类多对象可批量提交，不能共同撤销的属性保持独立。创建输入限制嵌套深度：轨道与歌声片段只创建空容器，音符作为叶节点可携带完整初始数据。

## 2. Editor 域汇总

| 域 | 数量 | 核心职责 |
|---|---:|---|
| 应用 | 1 | 产品与构建身份 |
| 自动化与安全边界 | 4 | 状态、Manifest、动态选项、文件授权事实 |
| 文档与工程 | 8 | 文档生命周期、保存、导入与工程快照 |
| 格式 | 2 | 格式能力与导入前检查 |
| 轨道 | 15 | 轨道查询、细粒度编辑、语言与声音 |
| 总线 | 5 | Master 总线查询与细粒度控制 |
| 片段 | 16 | 片段查询、几何、属性与声音继承 |
| 音频素材 | 5 | 音频查询、导入和路径解析 |
| 声库 | 2 | 可用声库发现与描述 |
| Speaker Mix | 9 | 固定/动态混合与关键帧 |
| 音符、歌词、语言、发音与音素 | 19 | 叶节点创建、几何、歌词、语言、发音和音素 |
| 参数曲线与锚点 | 10 | 曲线能力、采样和锚点编辑 |
| 时间轴 | 5 | Tempo 与拍号 |
| 历史记录 | 3 | 历史记录状态、Undo、Redo |
| 播放 | 8 | 播放状态、定位与循环 |
| 导出 | 6 | MIDI/音频能力、预览与任务 |
| 提取 | 3 | 音高/MIDI 提取能力与任务 |
| 推理 | 4 | 能力、状态、启动与阶段重置 |
| 异步任务 | 3 | 任务列表、详情与取消 |
| **Editor 合计** | **128** | **36 Q/S + 82 C/S + 10 C/A** |

## 3. Editor 公共工具

### 3.1 应用（1）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `application.get_info` | Meta | Q/S | 返回产品名、版本、平台与构建身份 |

### 3.2 自动化与安全边界（4）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `automation.get_status` | Meta | Q/S | Editor 实例、host、profile、Manifest、当前 document/window 摘要 |
| `automation.get_manifest` | Meta | Q/S | descriptor、Schema、版本摘要、digest 与分页 |
| `automation.get_options` | Meta | Q/S | 按目标字段与上下文解析动态候选，继承目标权限 |
| `automation.get_file_access` | L2 | Q/S | 返回 canonical 读写根与会话授权事实 |

### 3.3 文档与工程（8）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `documents.get` | L1 | Q/S | 文档状态、路径、dirty/savepoint 与 revision |
| `project.get` | L1 | Q/S | 有序轨道/片段工程快照与稳定对象 ID |
| `documents.new` | L2 | C/S | 未保存策略、模板与原子文档换代 |
| `documents.open` | L2 | C/A | 受控读路径、格式选项与异步换代 |
| `documents.save` | L2 | C/S | 当前路径保存、覆盖策略与 savepoint |
| `documents.save_as` | L2 | C/S | 受控写路径、扩展名与覆盖策略 |
| `documents.import` | L2 | C/A | 单文件导入计划、格式选项与任务写回 |
| `documents.import_batch` | L2 | C/A | 批量导入、失败策略与任务写回 |

### 3.4 格式（2）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `formats.list` | L2 | Q/S | 格式、扩展名、用途、可用性与 option Schema |
| `formats.inspect` | L2 | Q/S | 文件格式、来源、诊断与稳定 plan digest |

### 3.5 轨道（15）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `tracks.list` | L1 | Q/S | 有序轨道列表与分页 |
| `tracks.get` | L1 | Q/S | 单轨属性与对象统计 |
| `tracks.insert` | L1 | C/S | 只创建可指定标题/颜色的空轨道；显式索引与 client reference |
| `tracks.remove` | L1 | C/S | 批量 ID、完整预检与单条历史记录 |
| `tracks.move` | L1 | C/S | 显式目标索引与稳定顺序 |
| `tracks.rename` | L1 | C/S | 名称单字段命令 |
| `tracks.set_color` | L1 | C/S | 调色板索引边界 |
| `tracks.set_gain` | L1 | C/S | 增益单字段命令 |
| `tracks.set_pan` | L1 | C/S | 声像范围与单字段命令 |
| `tracks.set_mute` | L1 | C/S | 静音单字段命令 |
| `tracks.set_solo` | L1 | C/S | 独奏单字段命令 |
| `tracks.set_default_language` | L1 | C/S | 默认语言与实际声音能力校验 |
| `tracks.get_voice_context` | L1 | Q/S | 自有/有效声音、继承和语言上下文 |
| `tracks.set_voice` | L1 | C/S | 稳定 singer 引用；speaker 可空，多个 speaker 时显式选择 |
| `tracks.clear_voice` | L1 | C/S | 清除轨道声音上下文 |

### 3.6 总线（5）

总线域在 descriptor 中使用 category `bus`，公开 operation ID 保持 `master.*`。

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `master.get` | L1 | Q/S | Master gain/pan/mute/solo 与电平相关可用状态 |
| `master.set_gain` | L1 | C/S | Master 增益单字段命令 |
| `master.set_pan` | L1 | C/S | Master 声像单字段命令 |
| `master.set_mute` | L1 | C/S | Master 静音单字段命令 |
| `master.set_solo` | L1 | C/S | Master 独奏单字段命令 |

### 3.7 片段（16）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `clips.list` | L1 | Q/S | 按轨道、类型、范围筛选并分页 |
| `clips.get` | L1 | Q/S | 单片段属性与稳定 ID |
| `clips.insert` | L1 | C/S | 只创建可指定轨道/位置/长度/名称的空歌声片段 |
| `clips.duplicate` | L1 | C/S | 深复制既有片段的音符、参数、声音和相对布局；请求不接受对象树 |
| `clips.remove` | L1 | C/S | 批量 ID 与单条历史记录 |
| `clips.move` | L1 | C/S | 逐项目标轨道与起点 |
| `clips.resize_left` | L1 | C/S | 左边界与内容偏移语义 |
| `clips.resize_right` | L1 | C/S | 右边界与最短长度 |
| `clips.rename` | L1 | C/S | 名称单字段命令 |
| `clips.set_gain` | L1 | C/S | 增益单字段命令 |
| `clips.set_mute` | L1 | C/S | 静音单字段命令 |
| `clips.set_default_language` | L1 | C/S | 片段语言与有效声音能力校验 |
| `clips.get_voice_context` | L1 | Q/S | 继承、自有与有效声音上下文 |
| `clips.use_track_voice` | L1 | C/S | 恢复轨道声音继承 |
| `clips.set_voice` | L1 | C/S | 设置片段自有声音；speaker 可空，多个 speaker 时显式选择 |
| `clips.clear_voice` | L1 | C/S | 清除片段自有声音 |

### 3.8 音频素材（5）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `audio_clips.get` | L2 | Q/S | 路径状态、候选、hash 与音频元数据 |
| `audio_clips.import` | L2 | C/A | 单文件读授权、解码与片段任务 |
| `audio_clips.import_batch` | L2 | C/A | 多文件授权、批量上限与失败策略 |
| `audio_clips.relocate` | L2 | C/S | 新路径校验、解码、hash 与最终写回组成一项同步 Mutation |
| `audio_clips.confirm_path` | L2 | C/S | 候选校验、重新授权与最终写回组成一项同步 Mutation |

### 3.9 声库（2）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `voices.list` | L1 | Q/S | 可用 SingerRef、显示信息、筛选与分页 |
| `voices.describe` | L1 | Q/S | 特定声库的说话人、语言、G2P、默认值与混合能力 |

声库域只负责列出可用声库和描述特定声库；应用 voice 的命令分别保留在轨道域和片段域。

### 3.10 Speaker Mix（9）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `speaker_mix.get` | L1 | Q/S | 轨道/片段目标的混合与关键帧快照 |
| `speaker_mix.set_fixed` | L1 | C/S | 固定权重混合与归一化 |
| `speaker_mix.enable_dynamic` | L1 | C/S | 启用动态混合 |
| `speaker_mix.disable_dynamic` | L1 | C/S | 关闭动态混合并保持确定状态 |
| `speaker_mix.set_dynamic_bypass` | L1 | C/S | 动态混合 bypass 单字段命令 |
| `speaker_mix.keyframes.insert` | L1 | C/S | 位置、权重与稳定关键帧 ID |
| `speaker_mix.keyframes.move` | L1 | C/S | 批量稳定 ID 移动 |
| `speaker_mix.keyframes.set_weights` | L1 | C/S | 单关键帧权重替换与归一化 |
| `speaker_mix.keyframes.remove` | L1 | C/S | 批量稳定 ID 删除 |

### 3.11 音符、歌词、语言、发音与音素（19）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `notes.get` | L1 | Q/S | 有序音符快照与分页 |
| `notes.search` | L1 | Q/S | 歌词查询、匹配模式与大小写/正则选项 |
| `notes.insert` | L1 | C/S | 叶节点完整初始 draft，不要求 voice context；批量形成一条历史记录 |
| `notes.duplicate` | L1 | C/S | 深复制音符及关联参数，保持相对布局并整体撤销 |
| `notes.remove` | L1 | C/S | 片段归属与批量原子删除 |
| `notes.move` | L1 | C/S | 批量时间/音高增量 |
| `notes.resize_left` | L1 | C/S | 批量左边界调整 |
| `notes.resize_right` | L1 | C/S | 批量右边界调整 |
| `notes.split_at` | L1 | C/S | 显式局部位置与稳定拆分结果 |
| `notes.quantize` | L1 | C/S | 封闭量化值与起点/长度开关 |
| `notes.set_lyric` | L1 | C/S | 已有音符的歌词单字段命令，不与长度等属性捆绑 |
| `notes.set_language` | L1 | C/S | 批量语言与声音能力校验 |
| `notes.set_pronunciation` | L1 | C/S | 发音值与来源 |
| `notes.reset_pronunciation` | L1 | C/S | 恢复自动发音 |
| `notes.set_phonemes` | L1 | C/S | 音素名称序列替换 |
| `notes.set_phoneme_offsets` | L1 | C/S | 音素边界序列校验 |
| `notes.reset_phoneme_offsets` | L1 | C/S | 重置所选词根，并无弹窗地纳入恢复后会重叠的右邻级联闭包；整体形成一条历史记录 |
| `notes.reset_phonemes` | L1 | C/S | 恢复自动音素与边界 |
| `notes.fill_lyrics` | L1 | C/S | 批量歌词填充与分词/语言选项 |

### 3.12 参数曲线与锚点（10）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `parameters.get_capabilities` | L1 | Q/S | 参数层、范围、步长、曲线与插值能力 |
| `parameters.get` | L1 | Q/S | draw/anchor 曲线与稳定 curve/anchor ID |
| `parameters.replace` | L1 | C/S | 指定参数层的完整曲线替换 |
| `parameters.draw` | L1 | C/S | 局部采样绘制与 merge mode |
| `parameters.erase` | L1 | C/S | 局部区间擦除 |
| `parameters.bake` | L1 | C/S | 原始曲线烘焙与可选区间 |
| `parameters.insert_anchors` | L1 | C/S | 批量锚点插入与稳定 ID |
| `parameters.move_anchors` | L1 | C/S | 批量稳定 ID 移动位置和值 |
| `parameters.remove_anchors` | L1 | C/S | 批量稳定 ID 删除 |
| `parameters.set_anchor_interpolation` | L1 | C/S | 批量锚点插值更新 |

### 3.13 时间轴（5）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `timeline.get` | L1 | Q/S | Tempo 与拍号的有序快照 |
| `tempos.set` | L1 | C/S | 新增/替换 Tempo 与零点锚规则 |
| `tempos.delete` | L1 | C/S | Tempo 删除与零点锚规则 |
| `time_signatures.set` | L1 | C/S | 新增/替换拍号与 bar 0 规则 |
| `time_signatures.delete` | L1 | C/S | 拍号删除与 bar 0 规则 |

### 3.14 历史记录（3）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `history.get_state` | L1 | Q/S | Undo/Redo 名称、能力与 savepoint |
| `history.undo` | L1 | C/S | 显式历史记录导航与 revision |
| `history.redo` | L1 | C/S | 显式历史记录导航与 revision |

### 3.15 播放（8）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `playback.get` | L2 | Q/S | 播放状态、位置、循环与 state version |
| `playback.play` | L2 | C/S | 播放状态转换与能力检查 |
| `playback.pause` | L2 | C/S | 暂停状态转换 |
| `playback.stop` | L2 | C/S | 停止与恢复位置语义 |
| `playback.seek` | L2 | C/S | 有限非负位置与 state version |
| `playback.set_loop` | L2 | C/S | 设置持久循环区间，形成一条历史记录并递增文档 revision |
| `playback.set_loop_enabled` | L2 | C/S | 设置持久循环开关，形成一条历史记录并递增文档 revision |
| `playback.clear_loop` | L2 | C/S | 清除持久循环状态，形成一条历史记录并递增文档 revision |

`playback.play/pause/stop/seek` 只修改瞬时播放状态；三个循环工具修改工程持久状态，同时遵守文档 revision 与播放 state version 冲突检查，并可由 `history.undo/redo` 整体导航。

### 3.16 导出（6）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `exports.midi.get_capabilities` | L2 | Q/S | MIDI 来源、格式与 option Schema |
| `exports.midi.preview` | L2 | Q/S | 目标计划、诊断与 plan digest |
| `exports.midi.start` | L2 | C/A | 受控写路径、覆盖策略与任务 |
| `exports.audio.get_capabilities` | L2 | Q/S | 格式、采样率、声道、混音和来源 |
| `exports.audio.preview` | L2 | Q/S | 目标计划与阻断诊断 |
| `exports.audio.start` | L2 | C/A | 受控写路径、渲染任务与失败清理 |

### 3.17 提取（3）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `extract.get_capabilities` | L2 | Q/S | 音频来源、模型、语言、范围与 option Schema |
| `extract.pitch.start` | L2 | C/A | 音高提取任务与目标片段写回 |
| `extract.midi.start` | L2 | C/A | MIDI 提取任务与目标位置写回 |

### 3.18 推理（4）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `inference.get_capabilities` | L2 | Q/S | scope、stage、provider、device 与 model |
| `inference.get_status` | L2 | Q/S | 各阶段状态、原因与关联任务 |
| `inference.start` | L2 | C/A | 作用域、阶段、执行选项与任务 |
| `inference.reset_stage` | L2 | C/S | 作用域内阶段重置 |

### 3.19 异步任务（3）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `tasks.list` | L2 | Q/S | 当前 document generation、筛选与分页 |
| `tasks.get` | L2 | Q/S | 状态、进度、结果、错误与创建者归因 |
| `tasks.cancel` | L2 | C/S | 排队、运行、提交点与终态取消语义 |

## 4. DS Connector Lite 桥接工具（6）

| 工具 | 类型 | 固定职责 |
|---|---|---|
| `connector.get_status` | Q/S | 返回 Connector、Bootstrap、Editor、MCP、Manifest、兼容和 exposure 事实 |
| `connector.reconnect` | C/S | 主动刷新 QLocal 观察、上游握手、工具目录与 Manifest 摘要，并返回当前状态 |
| `editor.tools.list` | Q/S | 分页列出通过 exposure 的 Editor 实际工具 |
| `editor.tools.search` | Q/S | 按 ID、标题、说明和域搜索实际工具 |
| `editor.tools.describe` | Q/S | 返回目标 Schema、版本、权限、兼容与可用性 |
| `editor.tools.invoke` | 继承目标 | 按 Editor 当前真实 Schema 调用获准目标 |

六个桥接工具在 Connector 进程生命周期内保持固定。Exposure 的同一判定同时约束已知类型化工具，以及 `list/search/describe/invoke` 可观察、可调用的实际 Editor 目标。

## 5. 集合与契约门禁

实现和测试必须证明以下集合恒等式：

```text
Editor PublicToolDefinitions
= Editor Public Registry bindings
= Editor tools/list names
= Public Automation Manifest operation IDs
= Connector 构建时已知 Editor 类型化工具 names

Connector bridge definitions
= Connector downstream 固定桥接工具 names

128 + 6 = 134
```

每个 Editor 工具必须具备唯一 operation ID、域、最低 profile、Query/Command、同步模式、严格 input/output Schema、`value_sources`、历史记录/file/host/concurrency/conflict/safety descriptor 和执行 binding。每次调用在实际 dispatch 前重新执行 profile/Custom、Schema、动态值、File Guard 与 Admission 检查。
