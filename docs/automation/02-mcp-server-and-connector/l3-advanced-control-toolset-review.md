# L3 进阶控制工具集（评审稿）

| 领域 | 工具 | 类型 | 契约要点 |
|---|---|---|---|
| 工作区布局 | `workspace.get` | Q/S | 返回轨道面板、底部面板、当前底部页面及可用页面 ID |
| 工作区布局 | `workspace.set_panel_visibility` | C/S | 部分更新轨道面板和底部面板可见性；至少保留一个主编辑面板 |
| 工作区布局 | `workspace.show_bottom_panel_page` | C/S | 显示指定底部页面并在需要时展开底部面板 |
| 轨道视图 | `track_view.get` | Q/S | 返回视口中心、横纵缩放和自动翻页状态 |
| 轨道视图 | `track_view.set_viewport` | C/S | 部分更新中心 tick、中心轨道索引及横纵缩放 |
| 轨道视图 | `track_view.reveal_clips` | C/S | 定位并完整显示指定轨道或片段集合，不修改工程数据 |
| 轨道视图 | `track_view.set_auto_page_turn` | C/S | 开启或关闭轨道视图播放自动翻页 |
| 钢琴窗 | `piano_roll.get` | Q/S | 返回活动片段、视口、编辑模式、量化和自动翻页状态 |
| 钢琴窗 | `piano_roll.set_active_clip` | C/S | 打开指定歌声片段，或关闭当前钢琴窗片段 |
| 钢琴窗 | `piano_roll.set_viewport` | C/S | 部分更新中心 tick、中心音高及横纵缩放 |
| 钢琴窗 | `piano_roll.reveal_notes` | C/S | 定位并完整显示指定片段中的音符集合，不修改工程数据 |
| 钢琴窗 | `piano_roll.set_edit_mode` | C/S | 切换选择、绘制、音高编辑等当前可用编辑模式 |
| 钢琴窗 | `piano_roll.set_quantize` | C/S | 部分更新量化分度与量化启用状态 |
| 钢琴窗 | `piano_roll.set_auto_page_turn` | C/S | 开启或关闭钢琴窗播放自动翻页 |
| 参数编辑器 | `parameter_editor.get` | Q/S | 返回当前参数、前景参数、编辑工具和视口状态 |
| 参数编辑器 | `parameter_editor.set_active_parameter` | C/S | 切换当前编辑参数并显示参数编辑页面 |
| 参数编辑器 | `parameter_editor.set_foreground_parameter` | C/S | 设置用于对照或交换的前景参数 |
| 参数编辑器 | `parameter_editor.set_tool` | C/S | 切换绘制、擦除、锚点等当前可用参数工具 |
| 参数编辑器 | `parameter_editor.set_viewport` | C/S | 部分更新参数视图的时间范围和值域范围 |
| 选择与焦点 | `selection.get` | Q/S | 返回当前轨道、片段和音符选择及键盘焦点归属 |
| 选择与焦点 | `selection.set_track` | C/S | 选择一条轨道，或清除轨道选择 |
| 选择与焦点 | `selection.set_clips` | C/S | 原子替换有序片段选择集合 |
| 选择与焦点 | `selection.set_notes` | C/S | 在指定歌声片段内原子替换音符选择集合 |
| 选择与焦点 | `selection.clear` | C/S | 按 track、clips、notes 或 all 范围清除选择 |
| 通用设置 | `general_settings.get` | Q/S | 返回“应用”和“歌唱”组的已开放设置、有效值与可选值 |
| 通用设置 | `general_settings.update_application` | C/S | 部分更新“应用”组；当前仅包含界面语言 |
| 通用设置 | `general_settings.update_singing` | C/S | 部分更新默认歌唱语言及按语言保存的默认歌词 |
| 外观设置 | `appearance_settings.get` | Q/S | 返回主题、字体、窗口、动画和触控组的已开放设置与候选值 |
| 外观设置 | `appearance_settings.update_theme` | C/S | 部分更新“主题”组；失败返回结构化错误且不弹窗 |
| 外观设置 | `appearance_settings.update_font` | C/S | 部分更新“字体”组；支持恢复平台默认字体 |
| 外观设置 | `appearance_settings.update_window` | C/S | 部分更新“窗口”组；需重启的字段只返回 restart_required，不弹窗 |
| 外观设置 | `appearance_settings.update_animation` | C/S | 部分更新动画启用状态和持续时间倍率 |
| 外观设置 | `appearance_settings.update_touch` | C/S | 在宿主支持时部分更新直接操控触控能力 |
| 音频设置 | `audio_settings.get` | Q/S | 返回音频驱动和设备、播放、文件缓存设置及依赖候选值 |
| 音频设置 | `audio_settings.update_device` | C/S | 部分更新驱动、设备、缓冲、采样率、热插拔、增益和声像；失败回滚且不弹窗 |
| 音频设置 | `audio_settings.update_playback` | C/S | 部分更新“播放”组的播放头停止行为 |
| 音频设置 | `audio_settings.update_file_caching` | C/S | 部分更新“文件缓存”组的预读样本数 |
| MIDI 设置 | `midi_settings.get` | Q/S | 返回 MIDI 输入和合成器设置、设备与波形候选值 |
| MIDI 设置 | `midi_settings.update_input` | C/S | 部分更新“MIDI 输入”组的设备；失败返回结构化错误且不弹窗 |
| MIDI 设置 | `midi_settings.update_synthesizer` | C/S | 部分更新波形、振幅、ADSR、A4 频率和工程音分偏移联动 |
| 推理设置 | `inference_settings.get` | Q/S | 返回设备、渲染、歌手会话保留设置及有效候选值 |
| 推理设置 | `inference_settings.update_device` | C/S | 部分更新执行提供程序和 GPU；需重启时只返回 restart_required |
| 推理设置 | `inference_settings.update_render` | C/S | 部分更新采样步数、深度、Vocoder CPU、自动推理、前瞻和音高平滑 |
| 推理设置 | `inference_settings.update_singer_session_retention` | C/S | 部分更新歌手会话容量和空闲释放时间 |
| 推理缓存 | `inference_cache.get` | Q/S | 扫描并返回缓存目录、文件数、总大小和可清理大小 |
| 推理缓存 | `inference_cache.clear_unused` | C/A | 清理当前工程、撤销历史和播放未占用的缓存；不触发确认弹窗 |
| 包路径设置 | `package_settings.get` | Q/S | 返回有序包搜索路径及重启生效状态 |
| 包路径设置 | `package_settings.update_search_paths` | C/S | 替换有序搜索路径；路径受读取根约束，仅标记重启生效，不立即加载 |
| 包信息 | `packages.list` | Q/S | 返回已索引包、版本、供应方、路径和所含声库摘要 |
| 包信息 | `packages.describe` | Q/S | 返回指定包的元数据、许可、说明和声库明细 |
| 包信息 | `packages.validate` | Q/S | 校验读取根内的包目录或包文件并返回结构化诊断 |
| Speaker Mix 预设 | `speaker_mix_presets.list` | Q/S | 返回预设 ID、名称、适用声库、来源和权重 |
| Speaker Mix 预设 | `speaker_mix_presets.save` | C/S | 新建或按 ID 更新一个预设，名称冲突时原子失败 |
| Speaker Mix 预设 | `speaker_mix_presets.delete` | C/S | 按稳定 ID 删除一个预设；不存在时返回 not_found |
