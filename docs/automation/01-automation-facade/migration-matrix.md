# 一期 Automation Facade 迁移矩阵

## 1. 口径

本矩阵冻结一期已经接入的真实能力。Catalog 共 122 个 operation；每个条目都具备
类型化 C++ handler，并由 `Automation/OperationIds.h` 集中定义。当前列出的
测试是实现阶段保护测试；用户批准测试大纲后，才按每个 operation 的适用维度展开
全量确定性测试。

创建类 DTO 可携带请求内 `client_ref`；Facade 在实际分配对象 ID 后通过
`MutationResult.createdObjects` 返回有序绑定。该元数据不写入 Model 或工程文件，
`validate_only` 也不分配绑定。

以下路径均是生产适配入口。GUI 可以保留输入、绘制、hover、拖动预览和对话框，
但最终业务提交由对应 Facade 完成。

## 2. 入口、Handler 与保护测试

| 域 | 现有入口/适配器 | 唯一业务 Handler | 实现阶段保护 |
|---|---|---|---|
| 应用生命周期 | `AppController`、主菜单 | `ApplicationAutomationFacade` | info、WindowId、validate-only、退出/重启宿主调用 |
| 文档 | `DocumentWorkflowController` | `DocumentAutomationFacade` | generation 轮换、失败不替换、savepoint、revision、旧 ID |
| 轨道/片段/音频片段 | `TrackController`、`AudioDecodingController`、`DocumentImportController` | `ProjectAutomationFacade` | 对象解析、no-op、批提交、音频异步写回版本校验 |
| 音符/歌词/音素 | `ClipController`、音符交互 Controller | `NoteAutomationFacade` | 增删改、量化、拆分、词属性、音素偏移、单 History/revision |
| 参数与 Speaker Mix | 参数编辑器、Clip 工具栏、Track 控件 | `ParameterAutomationFacade` | 参数替换、Clip/Track 声线继承、归一化、运行期 ID |
| 时间线/Master | `AppController`、Tempo/拍号视图 | `TimelineAutomationFacade` | tempo、拍号锚点约束、Master、no-op、undo/redo |
| History | `UndoRedoController` | `HistoryAutomationFacade`、`CommandCommitter` | 空栈、单次 record/revision、focus 回放 |
| 推理写回 | `InferenceAutomationBridge`、推理 adapter | `InferenceAutomationFacade` | stage 分类、base revision、对象复检、原子写回 |
| 提取任务 | Pitch/MIDI Extract Controller | `ExtractionAutomationFacade` | TaskId、取消、提交点、旧 generation 丢弃 |
| 音频导出 | `AudioExporter` | `AudioExportAutomationFacade` | preview、start、cleanup、文件策略、任务状态 |
| 文件格式/MIDI 导出 | `AppController`、格式注册表 | `FileAutomationFacade` | 格式快照、显式文档版本、宿主失败 |
| 任务查询 | 音频导出及未来 adapter | `TaskAutomationFacade`、`AutomationTaskManager` | get/list/cancel、终态保留、generation 隔离 |
| 播放与 loop | `PlaybackController`、播放栏、时间线 | `PlaybackAutomationFacade` | 状态/位置不增 revision；持久化 loop 单 History/revision |
| 稳定 Editor 状态 | `EditorViewController`、Track/Clip Controller | `EditorAutomationFacade` | WindowId、显式 DocumentId、selection、reveal、量化、auto-page |
| 设置/Recent/搜索路径 | 设置页、Audio、FillLyric、DocumentWorkflow | `SettingsAutomationFacade` | 分域快照、更新/no-op、持久化回调、路径归一化 |
| Speaker Mix 预设 | `SpeakerMixPresetStore` | `PresetAutomationFacade` | list/save/delete、重复 ID、运行期元数据不入工程 |
| 包 | 包管理器、`ProjectPackageResolver` | `PackageAutomationFacade` | list/validate/resolve、模块不可用和文档版本 |

`TestAutomationArchitecture` 另以源码扫描守卫上述边界；`TestAutomationCore` 负责
Dispatcher、Session、幂等、任务、集中 operation 注册表和各 Facade 的实现级契约。

## 3. Catalog operation 清单（122）

### application（3）

```text
application.get_info
application.request_exit
application.request_restart
```

### documents（5）

```text
documents.commit_import
documents.commit_new
documents.commit_open
documents.get
documents.save
```

### project / tracks / clips / audio_clips / imports（18）

```text
project.get
tracks.insert
tracks.move
tracks.remove
tracks.set_color
tracks.set_default_language
tracks.set_properties
clips.insert
clips.remove
clips.set_default_language
clips.set_properties
audio_clips.apply_decode_cache
audio_clips.apply_resolved_path
audio_clips.confirm_path
audio_clips.relocate
audio_clips.set_hash
audio_clips.set_path_status
imports.commit_batch
```

### notes（10）

```text
notes.get
notes.insert
notes.move
notes.quantize
notes.remove
notes.resize_left
notes.resize_right
notes.set_phoneme_offsets
notes.set_word_properties
notes.split
```

### parameters / speaker_mix（10）

```text
parameters.get
parameters.replace
speaker_mix.clip.apply
speaker_mix.clip.enable_dynamic
speaker_mix.clip.replace
speaker_mix.clip.select_single
speaker_mix.clip.use_track
speaker_mix.track.apply
speaker_mix.track.replace
speaker_mix.track.select_single
```

### timeline / tempos / time_signatures / master（6）

```text
timeline.get
tempos.delete
tempos.set
time_signatures.delete
time_signatures.set
master.set_control
```

### history（3）

```text
history.get_state
history.redo
history.undo
```

### inference（12）

```text
inference.apply_acoustic
inference.apply_duration
inference.apply_phoneme_names
inference.apply_pitch
inference.apply_pronunciations
inference.apply_variance
inference.invalidate_clip
inference.rebuild_original_params
inference.refresh_param_input
inference.refresh_speaker_mix
inference.resegment_clip
inference.reset_stage
```

### extract（2）

```text
extract.midi.start
extract.pitch.start
```

### exports / formats（5）

```text
exports.audio.cleanup
exports.audio.preview
exports.audio.start
exports.midi.start
formats.list
```

### operations（3）

```text
operations.cancel
operations.get
operations.list
```

### playback（9）

```text
playback.clear_loop
playback.get
playback.pause
playback.play
playback.set_last_position
playback.set_loop
playback.set_loop_enabled
playback.set_position
playback.stop
```

### editor（15）

```text
editor.center_piano_roll
editor.center_track_panel
editor.get_capabilities
editor.get_state
editor.restore_view
editor.reveal
editor.set_active_clip
editor.set_auto_page_turn
editor.set_panel_visibility
editor.set_piano_roll_edit_mode
editor.set_piano_roll_scale
editor.set_quantize
editor.set_selection
editor.set_track_panel_scale
editor.show_bottom_panel_page
```

### settings / recent_files / package search paths（15）

```text
settings.get
settings.update_appearance
settings.update_audio
settings.update_developer
settings.update_fill_lyric
settings.update_g2p_language
settings.update_general
settings.update_inference
settings.update_window
recent_files.add
recent_files.clear
recent_files.list
recent_files.remove
packages.get_search_paths
packages.set_search_paths
```

### packages（3）

```text
packages.list
packages.resolve_document_voices
packages.validate
```

### speaker_mix_presets（3）

```text
speaker_mix_presets.delete
speaker_mix_presets.list
speaker_mix_presets.save
```

## 4. 明确不进入一期 Catalog

- 多真实 Session、DocumentRegistry、`documents.list`、跨文档 batch/复制/拖放；
- 多窗口创建、关闭、绑定、切换及 WindowRegistry；
- MCP、HTTP、JSON-RPC、权限 profile、Schema AST/digest 和 Headless bootstrap；
- 音频导出中尚无后端的 loop/selected-range 等 TODO；
- Issue 矩阵中仅规划、占位、无真实 handler 或当前 GUI 不可达的能力；
- 被动绘制读取、hover、动画、拖动过程中的临时 preview。

这些项不以 skipped operation 混入一期通过率。未来补齐真实能力时，必须同时增加
类型化 handler、集中 operation 注册、Catalog descriptor 和测试矩阵。
