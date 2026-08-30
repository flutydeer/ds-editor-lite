# 二期公共 MCP 工具矩阵

## 1. 冻结口径

二期工具面按业务域组织。Editor 提供 **177** 个公共工具，DS Connector Lite 提供 **6** 个桥接工具，合计 **183** 个。MCP tool name 是跨 Contract、Registry、Editor 与 Connector 的稳定身份，不使用依赖表内顺序的编号。

公共工具集维持 v1：`toolset_version = 1`。每个工具只声明自己的
`minimum_toolset_version`，当前均为 1；兼容性只由这两个版本字段决定。Schema 不一致属于
同一工具集版本下的实现缺陷，由 MCP 输入校验和契约测试发现，不参与 Connector 的子集证明。

标记说明：

- `Q/S`：同步 Query。
- `C/S`：同步 Command。
- `C/A`：接受后返回任务句柄的异步 Command。
- Profile 列表示工具契约声明的最低开放层级；L0 是固有层，始终参与发现与执行，不能通过 Profile、Custom 或 Connector exclude 禁用。

矩阵按被查询或修改的主状态所有者归域，不按 Profile 分组。编辑工具以可整体 Undo/Redo 的历史记录为原子边界；同类多对象可批量提交，不能共同撤销的属性保持独立。创建输入限制嵌套深度：轨道与歌声剪辑只创建空容器，音符作为叶节点可携带完整初始数据。

## 2. Editor 域汇总

| 域 | 数量 | 核心职责 |
|---|---:|---|
| 应用 | 5 | 产品与构建身份、运行状态、生命周期闭环、文件授权事实 |
| 文档与工程 | 8 | 文档状态与统计、最近项目、生命周期、保存与导入 |
| 格式 | 2 | 格式能力与导入前检查 |
| 轨道 | 14 | 轨道查询、细粒度编辑、语言与声音 |
| 总线 | 5 | Master 总线查询与细粒度控制 |
| 剪辑 | 15 | 剪辑查询、几何、属性与声音继承 |
| 音频剪辑 | 5 | 音频查询、导入和路径解析 |
| 声库 | 2 | 可用声库发现与描述 |
| Speaker Mix | 13 | 固定/动态混合、关键帧与预设 |
| 音符、歌词、语言、发音与音素 | 19 | 叶节点创建、几何、歌词、语言、发音和音素 |
| 参数曲线与锚点 | 12 | 有界曲线查询、采样和显式锚点曲线编辑 |
| 时间线 | 5 | Tempo 与拍号 |
| 历史记录 | 3 | 历史记录状态、Undo、Redo |
| 播放 | 8 | 播放状态、定位与循环 |
| 导出 | 6 | MIDI/音频能力、预览与任务 |
| 提取 | 3 | 音高/MIDI 提取能力与任务 |
| 推理 | 4 | 能力、状态、启动与阶段重置 |
| 异步任务 | 3 | 任务列表、详情与取消 |
| 工作区布局 | 2 | 主编辑面板布局、可见性与焦点归属 |
| 轨道面板 | 7 | 面板状态、视口、选择、焦点与自动翻页 |
| 剪辑编辑器 | 16 | 共享时间视口、钢琴与参数子区域的显示、选择和工具状态 |
| 设置 | 10 | 允许公开的应用设置查询、稀疏更新、候选值与生效状态 |
| 包信息 | 3 | 已安装包查询、详情与异步刷新 |
| 歌词规则 | 7 | splitter/tagger 规则管理与只读流水线测试 |
| **Editor 合计** | **177** | **41 Q/S + 123 C/S + 13 C/A** |

## 3. Editor 公共工具

### 3.1 应用（5）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `application.get_info` | L0 | Q/S | 返回产品名、版本、平台与构建身份 |
| `application.get_status` | L0 | Q/S | Editor 实例、host、profile、工具集版本与当前 document/window 摘要 |
| `application.request_exit` | L0 | C/S | 请求优雅退出；脏工程默认以 `busy` 拒绝，`discard_changes: true` 时无弹窗丢弃改动后退出 |
| `application.request_restart` | L0 | C/S | 请求以当前可执行文件和参数优雅重启；未保存策略与退出相同，不开放任意进程启动 |
| `application.get_file_access` | L2 | Q/S | 返回 canonical 读写根与会话授权事实 |

两个生命周期工具只接受可选布尔字段 `discard_changes`，默认 `false`；不提供 `force`、
`validate_only` 或幂等键。成功响应先确认请求已接受，再由既有文档工作流完成关闭。公共调用在工程
繁忙或存在未授权丢弃的改动时返回结构化错误，绝不进入 GUI 保存确认；菜单、窗口关闭等 GUI 路径
仍保留原有保存询问。两项均为不可禁用、不可排除且不显示在 Custom 设置中的 L0 固有能力。

### 3.2 文档与工程（8）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `documents.get` | L1 | Q/S | 文档状态、路径、dirty/savepoint、revision、工程长度及轨道/剪辑分类统计 |
| `documents.list_recent` | L2 | Q/S | 从应用设置列出最近项目的路径、文件名与当前存在状态 |
| `documents.new` | L2 | C/S | 未保存策略、模板与原子文档换代 |
| `documents.open` | L2 | C/A | 受控读路径、已验证输入快照、格式选项与异步换代 |
| `documents.save` | L2 | C/S | 当前路径保存；显式覆盖策略优先，省略时使用默认覆盖行为并更新 savepoint |
| `documents.save_as` | L2 | C/S | 受控写路径、扩展名与同目录排他发布 |
| `documents.import` | L2 | C/A | 单文件导入计划、已验证输入快照、格式选项与任务写回 |
| `documents.import_batch` | L2 | C/A | 逐项输入快照、失败策略与任务写回 |

### 3.3 格式（2）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `formats.list` | L2 | Q/S | 格式、扩展名、用途、可用性与 option Schema |
| `formats.inspect` | L2 | Q/S | 64 MiB 内的有界快照、格式、来源、诊断与稳定 plan digest |

### 3.4 轨道（14）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `tracks.list` | L1 | Q/S | 有序轨道列表与分页 |
| `tracks.get` | L1 | Q/S | 单轨属性、对象统计、自有/有效声音及默认语言上下文 |
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
| `tracks.set_voice` | L1 | C/S | 稳定 singer 引用；speaker 可空，多个 speaker 时显式选择 |
| `tracks.clear_voice` | L1 | C/S | 清除轨道声音上下文 |

### 3.5 总线（5）

总线域在 descriptor 中使用 category `bus`，公开 operation ID 保持 `master.*`。

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `master.get` | L1 | Q/S | Master gain/pan/mute/solo 与电平相关可用状态 |
| `master.set_gain` | L1 | C/S | Master 增益单字段命令 |
| `master.set_pan` | L1 | C/S | Master 声像单字段命令 |
| `master.set_mute` | L1 | C/S | Master 静音单字段命令 |
| `master.set_solo` | L1 | C/S | Master 独奏单字段命令 |

### 3.6 剪辑（15）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `clips.list` | L1 | Q/S | 按轨道、类型、范围筛选并分页 |
| `clips.get` | L1 | Q/S | 单个剪辑属性、稳定 ID；歌声剪辑同时返回继承、自有与有效声音上下文 |
| `clips.insert` | L1 | C/S | 只创建可指定轨道/位置/长度/名称的空歌声剪辑 |
| `clips.duplicate` | L1 | C/S | 深复制既有剪辑的音符、参数、声音和相对布局；请求不接受对象树 |
| `clips.remove` | L1 | C/S | 批量 ID 与单条历史记录 |
| `clips.move` | L1 | C/S | 逐项目标轨道与起点 |
| `clips.resize_left` | L1 | C/S | 左边界与内容偏移语义 |
| `clips.resize_right` | L1 | C/S | 右边界与最短长度 |
| `clips.rename` | L1 | C/S | 名称单字段命令 |
| `clips.set_gain` | L1 | C/S | 增益单字段命令 |
| `clips.set_mute` | L1 | C/S | 静音单字段命令 |
| `clips.set_default_language` | L1 | C/S | 剪辑语言与有效声音能力校验 |
| `clips.use_track_voice` | L1 | C/S | 恢复轨道声音继承 |
| `clips.set_voice` | L1 | C/S | 设置剪辑自有声音；speaker 可空，多个 speaker 时显式选择 |
| `clips.clear_voice` | L1 | C/S | 清除剪辑自有声音 |

### 3.7 音频剪辑（5）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `audio_clips.get` | L2 | Q/S | 路径状态、候选、hash 与音频元数据 |
| `audio_clips.import` | L2 | C/A | 单文件读授权、解码与剪辑任务；创建前按完整请求幂等去重 |
| `audio_clips.import_batch` | L2 | C/A | 多文件授权、批量上限与失败策略；与单项入口共享 Task 幂等语义 |
| `audio_clips.relocate` | L2 | C/A | 新路径授权、后台快照/解码/hash、源摘要复核与 Task 最终写回 |
| `audio_clips.confirm_path` | L2 | C/A | 候选授权、后台快照/解码/hash、源摘要复核与 Task 最终写回 |

### 3.8 声库（2）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `voices.list` | L1 | Q/S | 列出包含 package ID、package version 与 singer ID 的完整 SingerRef、显示信息、筛选与分页 |
| `voices.describe` | L1 | Q/S | 按完整 SingerRef 精确描述特定并存版本的说话人、语言、G2P、默认值与混合能力 |

声库域只负责列出可用声库和描述特定声库；应用 voice 的命令分别保留在轨道域和剪辑域。L1/L2 的发现、动态候选、设置与回读均使用版本完整的 SingerRef，不依赖 L3 包信息域。

### 3.9 Speaker Mix（13）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `speaker_mix.get` | L1 | Q/S | 轨道/剪辑目标的混合、关键帧及来源预设/脏状态快照 |
| `speaker_mix.set_fixed` | L1 | C/S | 固定权重混合与归一化 |
| `speaker_mix.enable_dynamic` | L1 | C/S | 启用动态混合 |
| `speaker_mix.disable_dynamic` | L1 | C/S | 关闭动态混合并保持确定状态 |
| `speaker_mix.set_dynamic_bypass` | L1 | C/S | 动态混合 bypass 单字段命令 |
| `speaker_mix.keyframes.insert` | L1 | C/S | 位置、权重与稳定关键帧 ID |
| `speaker_mix.keyframes.move` | L1 | C/S | 批量稳定 ID 移动 |
| `speaker_mix.keyframes.set_weights` | L1 | C/S | 单关键帧权重替换与归一化 |
| `speaker_mix.keyframes.remove` | L1 | C/S | 批量稳定 ID 删除 |
| `speaker_mix.presets.list` | L2 | Q/S | 列出应用级预设，可按 singer 过滤；不读取或修改文档历史记录 |
| `speaker_mix.presets.save` | L2 | C/S | 新建或按稳定 ID 更新应用级预设，名称冲突时原子失败 |
| `speaker_mix.presets.delete` | L2 | C/S | 删除应用级预设，不改变引用该预设的既有文档混合值 |
| `speaker_mix.presets.apply` | L2 | C/S | 将预设值应用到轨道/剪辑，形成一条文档历史记录并保留来源元数据 |

### 3.10 音符、歌词、语言、发音与音素（19）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `notes.list` | L1 | Q/S | 有序音符快照与分页 |
| `notes.search` | L1 | Q/S | 歌词查询、匹配模式与大小写/正则选项 |
| `notes.insert` | L1 | C/S | 叶节点完整初始 draft，不要求 voice context；批量形成一条历史记录 |
| `notes.duplicate` | L1 | C/S | 深复制音符及关联参数；锚点仅在所选范围内有界采样，保持相对布局并整体撤销 |
| `notes.remove` | L1 | C/S | 剪辑归属与批量原子删除 |
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

### 3.11 参数曲线与锚点（12）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `parameters.get_capabilities` | L1 | Q/S | 参数层、范围、步长、曲线与插值能力 |
| `parameters.get` | L1 | Q/S | 按可选半开时间范围和点数上限查询曲线；锚点完整保留，采样曲线确定性降采样并报告原始/返回点数 |
| `parameters.replace` | L1 | C/S | 指定参数层的完整曲线替换 |
| `parameters.draw` | L1 | C/S | 局部采样绘制与 merge mode |
| `parameters.erase` | L1 | C/S | 局部区间擦除 |
| `parameters.bake` | L1 | C/S | 原始曲线烘焙与可选区间；局部烘焙在锚点采样前执行点数与时间轴上界预检 |
| `parameters.create_anchor_curve` | L1 | C/S | 以至少两个初始锚点显式创建一条不重叠曲线并返回稳定 ID |
| `parameters.insert_anchors` | L1 | C/S | 向显式 `curve_id` 批量插入锚点，不隐式创建或合并曲线 |
| `parameters.move_anchors` | L1 | C/S | 批量稳定 ID 移动位置和值；不得隐式跨曲线合并或制造重叠 |
| `parameters.remove_anchors` | L1 | C/S | 批量稳定 ID 删除 |
| `parameters.set_anchor_interpolation` | L1 | C/S | 批量锚点插值更新 |
| `parameters.merge_anchor_curves` | L1 | C/S | 显式合并同一参数层内相邻且不重叠的完整锚点曲线 |

### 3.12 时间线（5）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `timeline.get` | L1 | Q/S | Tempo 与拍号的有序快照 |
| `tempos.set` | L1 | C/S | 新增/替换 Tempo 与零点锚规则 |
| `tempos.remove` | L1 | C/S | Tempo 删除与零点锚规则 |
| `time_signatures.set` | L1 | C/S | 新增/替换拍号与 bar 0 规则 |
| `time_signatures.remove` | L1 | C/S | 拍号删除、bar 0 规则与删除后的时间线投影上界 |

### 3.13 历史记录（3）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `history.get_state` | L1 | Q/S | Undo/Redo 名称、能力与 savepoint |
| `history.undo` | L1 | C/S | 显式历史记录导航与 revision |
| `history.redo` | L1 | C/S | 显式历史记录导航与 revision |

### 3.14 播放（8）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `playback.get_state` | L2 | Q/S | 播放状态、位置、循环与当前可播放性 |
| `playback.play` | L2 | C/S | 播放状态转换与能力检查 |
| `playback.pause` | L2 | C/S | 暂停状态转换 |
| `playback.stop` | L2 | C/S | 停止与恢复位置语义 |
| `playback.seek` | L2 | C/S | 设置有限非负播放位置 |
| `playback.set_loop` | L2 | C/S | 设置持久循环区间，形成一条历史记录并递增文档 revision |
| `playback.set_loop_enabled` | L2 | C/S | 设置持久循环开关，形成一条历史记录并递增文档 revision |
| `playback.clear_loop` | L2 | C/S | 清除持久循环状态，形成一条历史记录并递增文档 revision |

`playback.play/pause/stop/seek` 只修改瞬时播放状态，以目标状态/位置为准且可安全重复调用，不使用状态版本；三个循环工具修改工程持久状态，遵守文档 revision 并可由 `history.undo/redo` 整体导航。

### 3.15 导出（6）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `exports.midi.get_capabilities` | L2 | Q/S | MIDI 来源、格式与 option Schema |
| `exports.midi.preview` | L2 | Q/S | 目标计划、诊断与 plan digest |
| `exports.midi.start` | L2 | C/A | 受控写路径、同目录排他发布与任务 |
| `exports.audio.get_capabilities` | L2 | Q/S | 格式、采样率、声道、混音和来源 |
| `exports.audio.preview` | L2 | Q/S | 目标计划与阻断诊断 |
| `exports.audio.start` | L2 | C/A | 暂存渲染、发布前精确 revision 复核、受控写路径与失败清理 |

### 3.16 提取（3）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `extract.get_capabilities` | L2 | Q/S | 音频来源、模型、语言、范围与 option Schema |
| `extract.pitch.start` | L2 | C/A | 哈希音频快照、源身份复核、音高提取任务与目标剪辑写回 |
| `extract.midi.start` | L2 | C/A | 哈希音频快照、源身份复核、MIDI 提取任务与目标位置写回 |

### 3.17 推理（4）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `inference.get_capabilities` | L2 | Q/S | scope、stage、provider、device 与 model |
| `inference.get_status` | L2 | Q/S | 各阶段状态、原因与关联任务 |
| `inference.start` | L2 | C/A | 作用域、阶段、执行选项与任务 |
| `inference.reset_stage` | L2 | C/S | 作用域内阶段重置 |

### 3.18 异步任务（3）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `tasks.list` | L2 | Q/S | 按 document/application scope 筛选分页；进度更新不使稳定成员游标失效 |
| `tasks.get` | L2 | Q/S | 按 scope 返回状态、进度、结果、错误与创建者归因 |
| `tasks.cancel` | L2 | C/S | 按 scope 执行排队、运行、提交点与终态取消语义 |

### 3.19 工作区布局（2）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `workspace.get_state` | L3 | Q/S | 返回主编辑面板可见性、布局与当前键盘焦点所属面板 |
| `workspace.set_panel_visibility` | L3 | C/S | 稀疏更新轨道面板与剪辑编辑器可见性；至少保留一个主编辑面板 |

### 3.20 轨道面板（7）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `track_panel.get_state` | L3 | Q/S | 返回视口、自动翻页、当前轨道、有序剪辑选择与 primary item |
| `track_panel.set_viewport` | L3 | C/S | 稀疏更新中心 tick、中心轨道索引与横纵缩放 |
| `track_panel.reveal_clips` | L3 | C/S | 完整显示目标轨道或剪辑集合，不修改工程 |
| `track_panel.set_auto_page_turn` | L3 | C/S | 设置轨道面板自动翻页 |
| `track_panel.select_track` | L3 | C/S | 选择或清除当前轨道，并显示、激活轨道面板 |
| `track_panel.select_clips` | L3 | C/S | 原子替换有序剪辑选择与 primary clip，并显示、激活轨道面板 |
| `track_panel.clear_selection` | L3 | C/S | 按 track/clips/all 清除选择，并显示、激活轨道面板 |

### 3.21 剪辑编辑器（16）

钢琴和参数子区域共享时间位置与横向缩放；钢琴另有音高纵向视口，参数另有值域纵向视口。焦点事实与选择归入各自面板/子区域，不建立平行的选择域。GUI Command 不要求文档 revision；显示和激活目标区域是成功条件，键盘焦点只作尽力获取。

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `clip_editor.get_state` | L3 | Q/S | 返回活动剪辑、当前子区域、共享时间视口、自动翻页、钢琴与参数状态 |
| `clip_editor.set_active_clip` | L3 | C/S | 设置活动歌声剪辑，或关闭当前活动剪辑 |
| `clip_editor.set_time_viewport` | L3 | C/S | 稀疏更新钢琴与参数共享的中心 tick 和横向缩放 |
| `clip_editor.set_auto_page_turn` | L3 | C/S | 设置剪辑编辑器自动翻页 |
| `clip_editor.show_region` | L3 | C/S | 显示、展开并激活 piano 或 parameters 子区域 |
| `clip_editor.piano.set_pitch_viewport` | L3 | C/S | 稀疏更新中心音高与纵向缩放 |
| `clip_editor.piano.reveal_notes` | L3 | C/S | 在活动歌声剪辑中完整显示指定音符 |
| `clip_editor.piano.set_edit_mode` | L3 | C/S | 设置钢琴窗受支持的编辑模式 |
| `clip_editor.piano.set_quantize` | L3 | C/S | 稀疏更新量化分度与启用状态 |
| `clip_editor.piano.select_notes` | L3 | C/S | 原子替换有序音符选择与 primary note，并显示、激活钢琴子区域 |
| `clip_editor.piano.clear_selection` | L3 | C/S | 清除活动剪辑音符选择，并显示、激活钢琴子区域 |
| `clip_editor.parameters.set_foreground` | L3 | C/S | 设置前景参数 |
| `clip_editor.parameters.set_background` | L3 | C/S | 设置背景参数或 none |
| `clip_editor.parameters.swap` | L3 | C/S | 原子交换前景与背景；不可交换时不产生部分变化 |
| `clip_editor.parameters.set_tool` | L3 | C/S | 设置绘制、擦除、烘焙或锚点等受支持工具 |
| `clip_editor.parameters.set_value_viewport` | L3 | C/S | 稀疏更新归一化值域中心与纵向缩放，不改变共享时间视口 |

### 3.22 设置（10）

设置工具只公开明确允许的应用选项，不公开自动化/MCP 自配置、开发者选项、窗口/动画/触控、文件缓存、MIDI、合成器、G2P 优先级、推理缓存、最近文件清理或未列出的设置。

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `settings.query` | L3 | Q/S | 按可选 domain 返回配置值、生效值、候选/范围、重启要求与不可用原因 |
| `settings.ui_language.update` | L3 | C/S | 稀疏更新 UI 语言并立即应用 |
| `settings.singing.update` | L3 | C/S | 稀疏更新默认歌唱语言及按语言保存的默认歌词 |
| `settings.theme.update` | L3 | C/S | 稀疏更新颜色主题并立即应用 |
| `settings.audio_device.update` | L3 | C/S | 稀疏更新驱动、设备、缓冲、采样率、热插拔、增益与声像；失败回滚且不弹窗 |
| `settings.playback_behavior.update` | L3 | C/S | 稀疏更新播放头停止行为 |
| `settings.compute_device.update` | L3 | C/S | 稀疏更新执行提供程序与 GPU；需要重启时只返回事实 |
| `settings.render.update` | L3 | C/S | 稀疏更新采样步数、深度、Vocoder CPU、自动推理、前瞻与音高平滑 |
| `settings.singer_session_retention.update` | L3 | C/S | 稀疏更新会话容量与空闲释放时间 |
| `settings.package_search_paths.update` | L3 | C/S | 替换有序读取根内路径；配置值持久化并报告重启后生效 |

### 3.23 包信息（3）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `packages.list` | L3 | Q/S | 列出包、版本、供应方、受读取根约束的 canonical path 与声库摘要 |
| `packages.describe` | L3 | Q/S | 返回指定包元数据、许可、说明与声库明细 |
| `packages.refresh` | L3 | C/A | 使用当前 effective 搜索路径后台扫描，完成后原子切换索引；前序提交被拒绝时等待调用重新扫描 |

`packages.refresh` 创建 application-scoped Task；不伪造 `document_id`，也不参与工程 revision 或历史记录。

### 3.24 歌词规则（7）

| 工具 | Profile | 类型 | 契约要点 |
|---|---|---|---|
| `lyric_rules.list` | L3 | Q/S | 列出内置/自定义 splitter、tagger 的稳定 ID、内容、启用状态与分类顺序 |
| `lyric_rules.create` | L3 | C/S | 创建自定义 splitter 或 tagger，不允许伪造内置规则 |
| `lyric_rules.update` | L3 | C/S | 按稳定 rule ID 稀疏更新自定义规则；内置内容不可修改 |
| `lyric_rules.delete` | L3 | C/S | 删除自定义规则；内置规则不可删除 |
| `lyric_rules.set_enabled` | L3 | C/S | 单独启停内置或自定义规则 |
| `lyric_rules.move` | L3 | C/S | 在 splitter/tagger 各自序列中原子移动规则 |
| `lyric_rules.test` | L3 | Q/S | 对给定文本只读运行 splitter→tagger，返回逐阶段结果 |

## 4. DS Connector Lite 桥接工具（6）

| 工具 | 类型 | 固定职责 |
|---|---|---|
| `connector.get_status` | Q/S | 返回 Connector、Bootstrap、Editor、MCP、工具集兼容和 exposure 的缓存事实 |
| `connector.reconnect` | C/S | 主动刷新 QLocal 观察、上游握手与工具目录，并返回当前状态 |
| `editor.tools.list` | Q/S | 分页列出通过 exposure 的 Editor 实际工具摘要 |
| `editor.tools.search` | Q/S | 按 ID、标题、说明和域搜索实际工具摘要 |
| `editor.tools.describe` | Q/S | 返回目标 Schema、版本、权限、兼容与可用性 |
| `editor.tools.invoke` | 继承目标 | 按 Editor 当前真实 Schema 调用获准目标 |

六个桥接工具在 Connector 进程生命周期内保持固定。Exposure 的同一判定同时约束已知类型化工具，以及 `list/search/describe/invoke` 可观察、可调用的实际 Editor 目标。

## 5. 集合与契约门禁

实现和测试必须证明以下集合恒等式：

```text
Editor PublicToolDefinitions
= Editor Public Registry bindings
= Editor tools/list names
= Connector 构建时已知 Editor 类型化工具 names

Connector bridge definitions
= Connector downstream 固定桥接工具 names

177 + 6 = 183
```

每个 Editor 工具必须具备唯一 operation ID、域、最低 profile、Query/Command、同步模式、严格
input/output Schema、必要的 `value_sources`、标准 MCP annotations、
`minimum_toolset_version` 和执行 binding。动态候选由 `value_sources` 指向同层级可达的领域查询，
业务调用不自动回查 provider；每次调用在实际 dispatch 前重新执行 profile/Custom、输入 Schema、File Guard 与
Admission 检查。输出 Schema 由确定性契约测试覆盖，运行时不逐次 assert。
