# L3 进阶控制工具集（评审稿）

| 领域 | 工具 | 类型 | 契约要点 |
|---|---|---|---|
| 工作区布局 | `workspace.get` | Q/S | 返回轨道面板与片段编辑器的可见性、整体布局和当前键盘焦点所属面板；子区域状态由所属面板返回 |
| 工作区布局 | `workspace.set_panel_visibility` | C/S | 部分更新轨道面板和片段编辑器可见性；至少保留一个主编辑面板 |
| 轨道面板 | `track_panel.get` | Q/S | 返回视口、自动翻页、当前轨道及有序片段选择和 primary item |
| 轨道面板 | `track_panel.set_viewport` | C/S | 部分更新轨道面板的中心 tick、中心轨道索引及横纵缩放 |
| 轨道面板 | `track_panel.reveal_clips` | C/S | 定位并完整显示指定轨道或片段集合，不修改工程数据 |
| 轨道面板 | `track_panel.set_auto_page_turn` | C/S | 开启或关闭轨道面板播放自动翻页 |
| 轨道面板 | `track_panel.select_track` | C/S | 选择一条轨道或清除轨道选择，并将键盘焦点切换到轨道面板 |
| 轨道面板 | `track_panel.select_clips` | C/S | 原子替换有序片段选择集合和 primary clip，并将键盘焦点切换到轨道面板 |
| 轨道面板 | `track_panel.clear_selection` | C/S | 按 track、clips 或 all 清除轨道面板选择，并将键盘焦点切换到轨道面板 |
| 片段编辑器 | `clip_editor.get` | Q/S | 以同一快照返回活动片段、当前及聚焦子区域、共享时间视口和自动翻页，以及含音符选择的钢琴状态与参数状态 |
| 片段编辑器 | `clip_editor.set_active_clip` | C/S | 打开指定歌声片段，或关闭当前片段编辑器中的活动片段 |
| 片段编辑器 | `clip_editor.set_time_viewport` | C/S | 部分更新钢琴与参数子区域共享的中心 tick 和横向缩放 |
| 片段编辑器 | `clip_editor.set_auto_page_turn` | C/S | 开启或关闭片段编辑器共享时间轴的播放自动翻页 |
| 片段编辑器 | `clip_editor.show_region` | C/S | 显示并聚焦 piano 或 parameters 子区域，并在需要时展开片段编辑器 |
| 钢琴子区域 | `clip_editor.piano.set_pitch_viewport` | C/S | 只更新钢琴子区域的中心音高和纵向缩放，不重复修改共享时间视口 |
| 钢琴子区域 | `clip_editor.piano.reveal_notes` | C/S | 切换到钢琴子区域并完整显示指定活动片段中的音符集合，不修改工程数据 |
| 钢琴子区域 | `clip_editor.piano.set_edit_mode` | C/S | 切换选择、绘制、音高编辑等当前可用编辑模式 |
| 钢琴子区域 | `clip_editor.piano.set_quantize` | C/S | 部分更新量化分度与量化启用状态 |
| 钢琴子区域 | `clip_editor.piano.select_notes` | C/S | 在活动歌声片段内原子替换有序音符选择和 primary note，并显示及聚焦钢琴子区域 |
| 钢琴子区域 | `clip_editor.piano.clear_selection` | C/S | 清除活动片段的音符选择，并显示及聚焦钢琴子区域 |
| 参数子区域 | `clip_editor.parameters.set_foreground` | C/S | 切换当前编辑的前景参数并显示参数子区域 |
| 参数子区域 | `clip_editor.parameters.set_background` | C/S | 设置用于对照的背景参数，允许设为 none |
| 参数子区域 | `clip_editor.parameters.swap` | C/S | 交换前景与背景参数；当前状态不可交换时原子失败 |
| 参数子区域 | `clip_editor.parameters.set_tool` | C/S | 切换绘制、擦除、锚点等当前可用参数工具 |
| 参数子区域 | `clip_editor.parameters.set_value_viewport` | C/S | 只更新参数子区域的值域范围或纵向缩放，不重复修改共享时间视口 |
| 设置查询 | `settings.query` | Q/S | 按可选 domains 过滤返回全部已开放设置的配置值、生效值、候选值或范围、重启要求及不可用原因；省略过滤时返回完整公开快照，不包含自动化安全配置、开发者设置或未开放路径 |
| UI 语言 | `settings.ui_language.update` | C/S | 部分更新界面语言并立即应用；合法值由 `settings.query` 返回 |
| 歌唱 | `settings.singing.update` | C/S | 部分更新默认歌唱语言及按语言保存的默认歌词 |
| 主题 | `settings.theme.update` | C/S | 部分更新颜色主题；失败返回结构化错误且不弹窗 |
| 音频驱动和设备 | `settings.audio_device.update` | C/S | 部分更新驱动、设备、缓冲、采样率、热插拔、增益和声像；失败回滚且不弹窗 |
| 播放行为 | `settings.playback_behavior.update` | C/S | 部分更新播放头停止行为 |
| 计算设备 | `settings.compute_device.update` | C/S | 部分更新执行提供程序和 GPU；需重启时只返回 restart_required |
| 渲染 | `settings.render.update` | C/S | 部分更新采样步数、深度、Vocoder CPU、自动推理、前瞻和音高平滑 |
| 歌手会话保留 | `settings.singer_session_retention.update` | C/S | 部分更新歌手会话容量和空闲释放时间 |
| 包搜索路径 | `settings.package_search_paths.update` | C/S | 替换有序包搜索路径；路径受读取根约束，仅标记重启生效，不立即加载 |
| 包信息 | `packages.list` | Q/S | 返回已索引包、版本、供应方、路径和所含声库摘要 |
| 包信息 | `packages.describe` | Q/S | 返回指定包的元数据、许可、说明和声库明细 |
| 包信息 | `packages.refresh` | C/A | 重新扫描当前包搜索路径并返回刷新任务；完成后原子切换索引并报告新增、更新、移除和失败项 |
| 歌词规则 | `lyric_rules.list` | Q/S | 返回内置与自定义 splitter/tagger 规则的稳定 ID、类型、内容、启用状态和分类型顺序 |
| 歌词规则 | `lyric_rules.create` | C/S | 新建一条自定义 splitter 或 tagger 规则并返回稳定 rule_id；不允许伪造内置规则 |
| 歌词规则 | `lyric_rules.update` | C/S | 按稳定 rule_id 稀疏修改一条自定义规则；内置规则内容不可修改 |
| 歌词规则 | `lyric_rules.delete` | C/S | 删除一条自定义规则；内置规则不可删除 |
| 歌词规则 | `lyric_rules.set_enabled` | C/S | 单独启停一条内置或自定义规则 |
| 歌词规则 | `lyric_rules.move` | C/S | 在 splitter 或 tagger 各自的有序规则集中移动一条规则 |
| 歌词规则 | `lyric_rules.test` | Q/S | 用给定文本运行当前 splitter → tagger 流水线并返回逐阶段结果，不写配置 |
