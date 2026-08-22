# 一期 Automation Facade operation 静态测试清单

## 1. 审计口径与结论

本清单以 `OperationIds.h` 中的 122 个唯一 operation 为唯一分母，并与真实
`CoreRuntime` 构造出的 `OperationCatalog` 契约表逐项对照。它只描述仓库当前已有的
**静态自动化覆盖结构**，不是构建、CTest、GUI 或真实服务的执行报告；任何一行的
`P` 都不能解释为本轮测试通过，也不能替代 `test-report.md` 中由执行证据产生的结论。

`TestAutomationCatalogContract` 为 122 个 operation 各保留一个稳定
`AFC-CATALOG-001～122` 关联，并核对 descriptor 全字段、顺序与唯一性；这属于
Catalog 契约覆盖，不算 handler 行为覆盖。`TestAutomationArchitecture` 对集中 ID、
in-process 版本后缀、History、revision、generation 与 Facade 边界做静态守卫，也不调用业务
handler。下文只在测试实际进入 Facade/dispatcher/task 路径并断言该 operation 的结果、
错误或副作用时，才记为 operation 级行为覆盖。

| 等级 | 含义 | 当前数量 |
|---|---|---:|
| `P` | 有直接 operation 结果断言，但仍缺至少一个适用维度 | 122 |
| `E` | 只有该 operation 的直接错误/拒绝路径 | 0 |
| `I` | handler 被调用，但没有该 operation 的结果断言 | 0 |
| `B` | 只有 Catalog/架构/名册等基础契约，没有行为场景 | 0 |

因此当前静态分类为 **122P / 0E / 0I / 0B**。这表示所有 operation 都至少有一个
直接行为场景；不表示任何 operation 已满足 `test-outline.md` 的全部适用维度，也不
陈述这些测试的最终执行结果。

## 2. 测试目标与场景维度

### 2.1 已注册 Automation 目标

`src/tests/CMakeLists.txt` 当前注册以下九个 Automation 测试目标：

| 简称 | 测试目标 | 静态职责 |
|---|---|---|
| `Core` | `TestAutomationCore` | 通用 dispatcher、选定 Facade、DTO、任务和旧有端到端顺序回归 |
| `Catalog` | `TestAutomationCatalogContract` | 122 项 descriptor 全字段、顺序、唯一性和稳定 Catalog 场景 ID |
| `Idem` | `TestAutomationIdempotency` | 同步/异步重放、16/64 路并发、冲突、失败释放、文档与 generation 键空间 |
| `Races` | `TestAutomationTaskRaces` | Task 状态机、取消/提交点、重复完成、revision/对象/generation 竞态 |
| `Doc` | `TestAutomationDocumentLifecycle` | new/open/import/save、回滚、savepoint、generation 清理与错误优先级 |
| `Edit` | `TestAutomationEditingDomains` | project/tracks/clips/notes/parameters/speaker mix/timeline/history |
| `Runtime` | `TestAutomationRuntimeDomains` | application/playback/editor/settings/recent files/packages/presets/宿主失败 |
| `Async` | `TestAutomationAsyncFileDomains` | inference/audio clips/import/file/export/extract/task list 与服务不可用 |
| `Arch` | `TestAutomationArchitecture` | 集中 ID、无 in-process 版本后缀、Facade/History/revision/generation 源码边界 |
| `NoteGUI` | `TestPianoRollNoteCommit` | GUI 音符插入/拆分的真实 created ID、revision 与失败无副作用契约 |

`TestEditorViewController` 与 `TestUndoRedoController` 是控制器层补充回归，分别补充
GUI host 转发和 History focus 两阶段导航；它们不改变下表的 operation 分母，也不以
Catalog 存在性代替 Facade 行为。

### 2.2 适用维度简写

基础编号沿用 `test-outline.md`：`Q1～Q6` 为 Query，`C1～C10` 为同步 Command，
`A1～A7` 为异步 Command。descriptor 未声明的 History、revision、幂等或文件行为不
强行套用。

| Profile | 适用维度 |
|---|---|
| `Q-A` | 应用级 Query：`Q1、Q2、Q4～Q6`，并验证适用的宿主不可用 |
| `Q-D` | 文档 Query：`Q1～Q6`，含显式 DocumentId、旧 generation 与无副作用 |
| `Q-DW` | 文档与窗口 Query：`Q1～Q6`，同时覆盖 DocumentId、WindowId 隔离 |
| `C-A` | 应用级 Command：适用的 `C1～C4、C7`，含 no-op、validate-only、持久化/宿主失败 |
| `C-W` | GUI Command：适用的 `C1～C4、C7、C9`，含有效/未知 WindowId |
| `C-D` | 文档同步 Command：适用的 `C1～C8`，按 descriptor 验证 History、revision 与幂等 |
| `C-DR` | 文档替换 Command：`C1～C5、C7` 加 generation 原子替换、失败保留旧 Session |
| `C-DW` | GUI 文档 Command：适用的 `C1～C7、C9`，显式 DocumentId/WindowId |
| `A-D` | 文档异步 Command：适用的 `C1～C8` 加 `A1～A7` 任务状态、取消、竞态与 generation |
| `+REF` | 创建类 `C10`：`client_ref` 顺序、唯一性、重放以及失败/预检不分配 |
| `+FILE` | 路径、格式、覆盖策略、临时文件、I/O 失败和清理 |
| `+TASK` | TaskId、过滤、稳定终态、未知/旧 TaskId 与不可取消提交点 |
| `+CACHE` | 可重建写回、输入快照、对象/版本复检以及不应增加 History/revision 的路径 |

`Catalog` 与 `Arch` 的基础保护对每行都适用，逐项表只列直接行为目标和场景。

## 3. 逐 operation 映射

### application（3）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `application.get_info` | `Q-A`；DTO、宿主可用性 | `Runtime: APP-Q-INFO-SNAPSHOT`、`HOST-APPLICATION-UNAVAILABLE` | P |
| `application.request_exit` | `C-W`；validate-only、单次 host、拒绝 | `Runtime: APP-C-EXIT-VALIDATE`、`APP-C-EXIT-COMMIT`、`APP-C-HOST-REJECT`、`HOST-APPLICATION-UNAVAILABLE` | P |
| `application.request_restart` | `C-W`；模式、窗口隔离、拒绝 | `Runtime: APP-C-RESTART-COMMIT`、`APP-C-WINDOW-ISOLATION`；`Core` restart 预检/提交 | P |

### documents（5）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `documents.commit_import` | `C-D+REF`；空导入、原子提交、History | `Doc: AFC-DOC-LIFECYCLE-003`；`Async: validate-commit-no-op-invalid` | P |
| `documents.commit_new` | `C-DR+REF`；untitled、revision 0、清 generation 状态 | `Doc: AFC-DOC-LIFECYCLE-002/005/008`；`Races: new generation replaces a queued extraction`、`generation replacement supersedes a task at the commit point`；`Idem: generationsHaveIndependentKeySpaces` | P |
| `documents.commit_open` | `C-DR+REF`；路径/savepoint、失败回滚、旧任务失效 | `Doc: AFC-DOC-LIFECYCLE-004/005/006/008/010`；`Races: open generation replaces a running extraction` | P |
| `documents.get` | `Q-D`；身份、路径、lifecycle、busy、savepoint | `Doc: AFC-DOC-LIFECYCLE-001/004/008` 与状态摘要；`Core` 保存/替换快照 | P |
| `documents.save` | `C-D+FILE`；save/save-as、savepoint、幂等、I/O 失败 | `Doc: AFC-DOC-LIFECYCLE-005/007`；`Async: validate-save-failure-and-unavailable`；`Idem: saveDoesNotClearCache` | P |

### project / tracks / clips / audio_clips / imports（18）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `project.get` | `Q-D`；有序强类型快照、旧 generation | `Edit: ordered-value-snapshot`；`Core` 创建后 DTO 快照 | P |
| `tracks.insert` | `C-D+REF`；边界、预检、不分配、单提交 | `Edit: validation-and-create`；`Core` client_ref 绑定 | P |
| `tracks.move` | `C-D`；边界、no-op、undo | `Edit: preview-noop-commit-undo` | P |
| `tracks.remove` | `C-D`；重复/空集合、预检、子对象、undo | `Edit: duplicates-preview-undo` | P |
| `tracks.set_color` | `C-D`；非 History revision、no-op、错误优先级 | `Edit: non-history-state`；`Doc: AFC-DOC-LIFECYCLE-009/010` | P |
| `tracks.set_default_language` | `C-D`；Unicode、空值、no-op、非 History | `Edit: unicode-and-noop` | P |
| `tracks.set_properties` | `C-D`；NaN、Unicode、多字段原子、no-op | `Edit: atomic-properties` | P |
| `clips.insert` | `C-D+REF`；空/非法、预检、绑定 | `Edit: empty-invalid-preview-create`；`Core` 歌声/音频/复制保真 | P |
| `clips.remove` | `C-D`；重复/空集合、预检、undo | `Edit: duplicates-preview-undo`；`Races` 对象删除交错 | P |
| `clips.set_default_language` | `C-D`；校验、no-op、非 History revision | `Edit: validation-revision-no-history`；`Core` no-op | P |
| `clips.set_properties` | `C-D`；移动+属性原子、边界、no-op、undo | `Edit: move-and-edit-atomically` | P |
| `audio_clips.apply_decode_cache` | `C-D+CACHE`；预检、no-op、路径快照 | `Async: derived-preview-commit-no-op-and-stale-path`；`Core` cache 写回 | P |
| `audio_clips.apply_resolved_path` | `C-D+CACHE`；预检、no-op、旧/空路径 | `Async: derived-path-resolution-and-empty-target`；`Core` 解析路径写回 | P |
| `audio_clips.confirm_path` | `C-D`；预检、no-op、类型、revision | `Async: state-commit-preview-no-op-and-wrong-type`；`Core` wrong-type | P |
| `audio_clips.relocate` | `C-D+FILE`；预检、History、no-op、类型 | `Async: history-commit-preview-no-op-and-invalid` | P |
| `audio_clips.set_hash` | `C-D+CACHE`；预检、no-op、空 hash | `Async: derived-hash-and-empty-input`；`Core` hash 写回 | P |
| `audio_clips.set_path_status` | `C-D+CACHE`；预检、no-op、旧路径 | `Async: derived-status-and-validation`；`Core` 状态写回 | P |
| `imports.commit_batch` | `C-D+REF`；预检、重复 ref、单 revision | `Async: validate-atomic-commit-and-duplicate-ref`；`Core` 有序绑定与整批 undo | P |

### notes（10）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `notes.get` | `Q-D`；类型/归属、有序值快照 | `Edit: typed-ordered-snapshot`；`Core` client_ref 不持久化 | P |
| `notes.insert` | `C-D+REF`；空/非法、预检、重叠、绑定、undo | `Edit: empty-invalid-preview-overlap`；`Core` 重复 ref 原子失败；`NoteGUI` 真实 ID/失败无副作用 | P |
| `notes.move` | `C-D`；边界、重复、no-op、重叠、undo | `Edit: bounds-duplicates-noop-undo`；`Core` 零位移/批移动 | P |
| `notes.quantize` | `C-D`；网格、几何、no-op、错误优先级 | `Edit: commit-noop-process-isolation` 与 `dispatch/error-priority-matrix` | P |
| `notes.remove` | `C-D`；重复/空集合、预检、undo | `Edit: duplicates-preview-undo` | P |
| `notes.resize_left` | `C-D`；最短长度、重复、预检、空集合 | `Edit: clamp-preview-commit` | P |
| `notes.resize_right` | `C-D`；最短长度、预检、空集合 | `Edit: clamp-preview-commit` | P |
| `notes.set_phoneme_offsets` | `C-D`；数量、单调、预检、no-op、清空 | `Edit: shape-order-preview-noop` | P |
| `notes.set_word_properties` | `C-D`；重复、Unicode、级联清理、原子 no-op | `Edit: unicode-cascade-atomic-noop` | P |
| `notes.split` | `C-D+REF`；边界、预检、不分配、绑定、undo | `Edit: invalid-preview-create-undo`；`NoteGUI` child ID/失败无副作用 | P |

### parameters / speaker_mix（10）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `parameters.get` | `Q-D`；空曲线、参数类型、值快照 | `Edit: empty-invalid-wrong-type`；`Core` edited Pitch 快照 | P |
| `parameters.replace` | `C-D`；draw/anchor、step、预检、no-op、undo | `Edit: validate-roundtrip-noop-undo`；`Core` 复制保真 | P |
| `speaker_mix.clip.apply` | `C-D`；归一化、preset 元数据 | `Edit: apply-normalized-preset` | P |
| `speaker_mix.clip.enable_dynamic` | `C-D`；动态 keyframe 排序 | `Edit: dynamic-keyframes` | P |
| `speaker_mix.clip.replace` | `C-D`；保留自有 voice、替换 mix | `Edit: preserve-owned-voice` | P |
| `speaker_mix.clip.select_single` | `C-D`；切换自有 context | `Edit: owned-context`；`Core` 继承→自有 | P |
| `speaker_mix.clip.use_track` | `C-D`；恢复继承、no-op | `Edit: inherit-noop`；`Core` 自有→继承 | P |
| `speaker_mix.track.apply` | `C-D`；fixed mix 归一化 | `Edit: normalized-fixed-mix` | P |
| `speaker_mix.track.replace` | `C-D`；保留 voice、排序动态 key | `Edit: preserve-voice-change-mix` | P |
| `speaker_mix.track.select_single` | `C-D`；预检、提交、no-op | `Edit: preview-commit-noop` | P |

### timeline / tempos / time_signatures / master（6）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `timeline.get` | `Q-D`；tempo/拍号锚点、版本、旧 ID | `Edit: anchors-and-version` | P |
| `tempos.delete` | `C-D`；tick 0、缺失 no-op、预检、undo | `Edit: anchor-missing-preview-undo` | P |
| `tempos.set` | `C-D`；非法值、排序/替换、预检、no-op | `Edit: invalid-preview-sorted-replace-noop`；`Core` stale revision/undo/redo | P |
| `time_signatures.delete` | `C-D`；bar 0、缺失 no-op、预检、undo | `Edit: anchor-missing-preview-undo` | P |
| `time_signatures.set` | `C-D`；分子/分母、排序/替换、预检、no-op | `Edit: invalid-preview-sorted-replace-noop` | P |
| `master.set_control` | `C-D`；非有限值、预检、no-op、undo/redo | `Edit: invalid-preview-noop-undo` | P |

### history（3）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `history.get_state` | `Q-D`；空栈、名称、savepoint | `Edit: empty-and-named-state`；`Doc` generation/savepoint；`Core` 栈快照 | P |
| `history.redo` | `C-D`；预检、空栈、revision、分支清除 | `Edit: preview-commit-branch-clear`；`Core` redo/no-op；`UndoCtl` 两阶段 focus | P |
| `history.undo` | `C-D`；预检、空栈、revision、focus fallback | `Edit: preview-commit-empty`；`Core` 多域 undo；`UndoCtl` 可见性/滚动/门禁/fallback | P |

### inference（12）

`Async` 的 `validate-success-no-op` 数据矩阵对下列 12 种
`InferenceMutationKind` 逐项断言 centralized ID、descriptor revision policy、预检预测、
单次 apply、revision 策略与 no-op。`apply_pitch` 另有对象/错误优先级、兄弟 rebase、
generation 与服务不可用场景。

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `inference.apply_acoustic` | `C-D+CACHE`；不增 revision | `Async: validate-success-no-op`；`Core` acoustic cache 写回 | P |
| `inference.apply_duration` | `C-D+CACHE`；增 revision | `Async: validate-success-no-op` | P |
| `inference.apply_phoneme_names` | `C-D+CACHE`；增 revision | `Async: validate-success-no-op` | P |
| `inference.apply_pitch` | `C-D+CACHE`；对象/版本复检、rebase | `Async: validate-success-no-op`、`document-revision-object-priority`、`generation-and-sibling-writeback`、`unavailable-service` | P |
| `inference.apply_pronunciations` | `C-D+CACHE`；增 revision | `Async: validate-success-no-op` | P |
| `inference.apply_variance` | `C-D+CACHE`；增 revision | `Async: validate-success-no-op`；作为 sibling writeback | P |
| `inference.invalidate_clip` | `C-D+CACHE`；check-only revision | `Async: validate-success-no-op` | P |
| `inference.rebuild_original_params` | `C-D+CACHE`；增 revision | `Async: validate-success-no-op` | P |
| `inference.refresh_param_input` | `C-D+CACHE`；check-only revision | `Async: validate-success-no-op` | P |
| `inference.refresh_speaker_mix` | `C-D+CACHE`；增 revision | `Async: validate-success-no-op` | P |
| `inference.resegment_clip` | `C-D+CACHE`；check-only revision | `Async: validate-success-no-op` | P |
| `inference.reset_stage` | `C-D+CACHE`；预检、增 revision | `Async: validate-success-no-op`；`Core` sibling rebase/generation 拒绝 | P |

### extract（2）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `extract.midi.start` | `A-D+TASK+REF`；类型、成功提交、失败、取消 | `Async: success-backend-failure-and-typed-input`；`Core` 成功/运行取消/失败；`Races` Task 状态边界 | P |
| `extract.pitch.start` | `A-D+TASK`；预检、写回、取消、revision/generation 竞态 | `Async: validate-success-typed-and-service-errors`、`unavailable-extraction-service`；`Races` 受控调度/竞态矩阵；`Core` 重放/成功/取消/编辑后拒绝 | P |

### exports / formats（5）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `exports.audio.cleanup` | `C-D+FILE+TASK`；一次清理、重复、未知 TaskId | `Async: cleanup-once-and-unknown-task`；`Core` 终态清理 | P |
| `exports.audio.preview` | `Q-D+FILE`；类型化预览、DocumentId、服务不可用 | `Async: typed-preview-document-and-service-errors`；`Core` 格式/warning/路径拒绝 | P |
| `exports.audio.start` | `A-D+FILE+TASK`；预检、任务、warning、成功/失败 | `Async: validate-queue-success-warning-and-failure`；`Idem` 异步重放/失败释放；`Core` 取消/generation | P |
| `exports.midi.start` | `C-D+FILE`；预检、幂等、路径/格式/覆盖/I/O | `Async: validate-idempotent-write-and-path-errors`；`Core` 路径与后端错误 | P |
| `formats.list` | `Q-A+FILE`；类型化快照、服务不可用 | `Async: success-and-unavailable`；`Core` 格式能力 | P |

### operations（3）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `operations.cancel` | `C-D+TASK`；预检、重复、终态、提交点、未知/旧 ID | `Core` cancel 预检/重复/终态/Committing；`Races: task manager state boundaries`、`cancel versus commit-point barrier stress`、generation replacement | P |
| `operations.get` | `Q-D+TASK`；各状态、progress/error/mutation、未知/旧 ID | `Core` queued/running/succeeded/failed/canceled 快照；`Races` 状态边界、old TaskId/generation；`Async` 导出/提取终态查询 | P |
| `operations.list` | `Q-D+TASK`；generation 范围、终态快照、错误 | `Async: queued-terminal-and-wrong-document`；`Races` 单记录稳定终态 | P |

### playback（9）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `playback.clear_loop` | `C-D`；单 revision、no-op | `Runtime: PLAY-C-LOOP-ENABLE-CLEAR` | P |
| `playback.get` | `Q-D`；状态/位置/loop、DocumentId、宿主 | `Runtime: PLAY-Q-SNAPSHOT`、`PLAY-Q-DOCUMENT-ISOLATION`、`HOST-PLAYBACK-UNAVAILABLE` | P |
| `playback.pause` | `C-D`；状态转换、重复 no-op、幂等不支持 | `Runtime: PLAY-C-PAUSE-STOP-NOOP`、`PLAY-C-IDEMPOTENCY-UNSUPPORTED` | P |
| `playback.play` | `C-D`；预检、no-op、busy、设备/宿主失败 | `Runtime: PLAY-C-STATE-VALIDATE-NOOP`、`PLAY-C-BUSY`、`PLAY-C-DEVICE-FAILURE`、`HOST-PLAYBACK-UNAVAILABLE` | P |
| `playback.set_last_position` | `C-D`；预检、no-op、边界 | `Runtime: PLAY-C-LAST-POSITION` | P |
| `playback.set_loop` | `C-D`；History/revision、预检、no-op、优先级 | `Runtime: PLAY-C-LOOP-REVISION`、`PLAY-C-LOOP-VALIDATION`、`PLAY-C-ERROR-PRIORITY`、`HOST-PLAYBACK-UNAVAILABLE` | P |
| `playback.set_loop_enabled` | `C-D`；已有/空区间、no-op、revision | `Runtime: PLAY-C-LOOP-ENABLE-CLEAR`、`PLAY-C-LOOP-VALIDATION` | P |
| `playback.set_position` | `C-D`；预检、no-op、NaN/Inf/负值、宿主 | `Runtime: PLAY-C-POSITION`、`PLAY-C-POSITION-BOUNDARIES`、`HOST-PLAYBACK-UNAVAILABLE` | P |
| `playback.stop` | `C-D`；状态转换、重复 no-op、单次 host | `Runtime: PLAY-C-PAUSE-STOP-NOOP` | P |

### editor（15）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `editor.center_piano_roll` | `C-W`；预检、no-op、边界、窗口/宿主 | `Runtime: EDITOR-C-CENTER-PIANO`、`EDITOR-C-VIEW-HOST-UNAVAILABLE`、`HOST-EDITOR-UNAVAILABLE`；`EditorCtl` 转发 | P |
| `editor.center_track_panel` | `C-W`；预检、no-op、边界、窗口/宿主 | `Runtime: EDITOR-C-CENTER-TRACK`；`EditorCtl` 转发 | P |
| `editor.get_capabilities` | `Q-A`；单 Session/Window、Catalog ID 列表 | `Runtime: EDITOR-Q-CAPABILITIES`；`Core` 与 `OperationIds::all()` 相等 | P |
| `editor.get_state` | `Q-DW`；view/selection/busy 快照、ID 隔离 | `Runtime: EDITOR-Q-STATE-SNAPSHOT`、`EDITOR-Q-OPTIONAL-VIEW`、`EDITOR-Q-ID-ISOLATION` | P |
| `editor.restore_view` | `C-W`；完整 round-trip、原子失败、窗口/宿主 | `Runtime: EDITOR-C-RESTORE-VIEW`；`EditorCtl` 完整/非法快照 | P |
| `editor.reveal` | `C-DW`；预检、类型/范围、fallback、host | `Runtime: EDITOR-C-REVEAL`、`EDITOR-C-REVEAL-FALLBACK`、`EDITOR-C-REVEAL-FAILURES`；`UndoCtl` focus fallback | P |
| `editor.set_active_clip` | `C-DW`；归属、no-op、错误优先级 | `Runtime: EDITOR-C-SELECTION-ROUNDTRIP`、`EDITOR-C-SELECTION-ERROR-PRIORITY` | P |
| `editor.set_auto_page_turn` | `C-W`；预检、目标枚举、no-op | `Runtime: EDITOR-C-AUTO-PAGE`；`Core` PianoRoll 目标 | P |
| `editor.set_panel_visibility` | `C-W`；至少一面板、no-op、窗口/host | `Runtime: EDITOR-C-PANEL-VISIBILITY`；`EditorCtl` 双隐藏/转发 | P |
| `editor.set_piano_roll_edit_mode` | `C-W`；枚举、预检、no-op、host | `Runtime: EDITOR-C-EDIT-MODE`；`EditorCtl` 转发 | P |
| `editor.set_piano_roll_scale` | `C-W`；有限正比例、预检、no-op、host | `Runtime: EDITOR-C-SCALE-PIANO`；`EditorCtl` 转发 | P |
| `editor.set_quantize` | `C-W`；网格、预检、no-op、宿主 | `Runtime: EDITOR-C-QUANTIZE`、`HOST-EDITOR-UNAVAILABLE`；`Core` 设置结果 | P |
| `editor.set_selection` | `C-DW`；track/clip/note 归属、去重、revision 不变 | `Runtime: EDITOR-C-SELECTION-ROUNDTRIP`；`Core` 三类 selection/stale revision | P |
| `editor.set_track_panel_scale` | `C-W`；有限正比例、预检、no-op、host | `Runtime: EDITOR-C-SCALE-TRACK`；`EditorCtl` 转发 | P |
| `editor.show_bottom_panel_page` | `C-W`；稳定 page ID、预检、no-op、host | `Runtime: EDITOR-C-BOTTOM-PAGE`；`EditorCtl` 合法/未知页 | P |

### settings / recent_files / package search paths（15）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `settings.get` | `Q-A`；八个域、更新后快照、宿主 | `Runtime: SETTINGS-Q-SNAPSHOT`、`SETTINGS-Q-UPDATED-SNAPSHOT`、`HOST-SETTINGS-UNAVAILABLE` | P |
| `settings.update_appearance` | `C-A`；no-op、预检、边界、持久化失败 | `Runtime: SETTINGS-C-APPEARANCE` | P |
| `settings.update_audio` | `C-A`；no-op、预检、设备边界、持久化失败 | `Runtime: SETTINGS-C-AUDIO` | P |
| `settings.update_developer` | `C-A`；枚举、no-op、预检、持久化失败 | `Runtime: SETTINGS-C-DEVELOPER` | P |
| `settings.update_fill_lyric` | `C-A`；Unicode 规则、重复/非法项、持久化 | `Runtime: SETTINGS-C-FILL-LYRIC`、`SETTINGS-C-LYRIC-RULE-VALIDATION` | P |
| `settings.update_g2p_language` | `C-A`；顺序、重复、no-op、持久化 | `Runtime: SETTINGS-C-G2P` | P |
| `settings.update_general` | `C-A`；no-op、预检、枚举、持久化/宿主失败 | `Runtime: SETTINGS-C-GENERAL`、`HOST-SETTINGS-UNAVAILABLE`；`Core` no-op | P |
| `settings.update_inference` | `C-A`；provider、数值、no-op、持久化失败 | `Runtime: SETTINGS-C-INFERENCE` | P |
| `settings.update_window` | `C-A`；no-op、预检、持久化失败 | `Runtime: SETTINGS-C-WINDOW` | P |
| `recent_files.add` | `C-A`；规范化、Unicode、去重、容量、持久化失败 | `Runtime: RECENT-C-ADD-NORMALIZE`、`RECENT-C-MAXIMUM-ORDER`、`RECENT-C-INVALID-PATH`、`RECENT-C-PERSISTENCE-FAILURE` | P |
| `recent_files.clear` | `C-A`；预检、空集合 no-op | `Runtime: RECENT-C-CLEAR` | P |
| `recent_files.list` | `Q-A`；空/有序快照、宿主 | `Runtime: RECENT-Q-EMPTY`、`RECENT-C-MAXIMUM-ORDER`、`HOST-SETTINGS-UNAVAILABLE` | P |
| `recent_files.remove` | `C-A`；规范化、缺失 no-op、非法路径 | `Runtime: RECENT-C-REMOVE`、`RECENT-C-INVALID-PATH` | P |
| `packages.get_search_paths` | `Q-A`；空/Unicode/有序快照、宿主 | `Runtime: PACKAGE-PATH-Q-EMPTY`、`PACKAGE-PATH-C-NORMALIZE`、`HOST-SETTINGS-UNAVAILABLE` | P |
| `packages.set_search_paths` | `C-A`；预检、规范化/去重、no-op、持久化失败 | `Runtime: PACKAGE-PATH-C-NORMALIZE`、`PACKAGE-PATH-C-PERSISTENCE-FAILURE` | P |

### packages（3）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `packages.list` | `Q-A`；detached DTO、singer、模块不可用 | `Runtime: PACKAGES-Q-LIST-SNAPSHOT`、`HOST-PACKAGES-UNAVAILABLE` | P |
| `packages.resolve_document_voices` | `C-D+CACHE`；预检、no-op、无 revision、优先级、幂等拒绝 | `Runtime: PACKAGES-C-RESOLVE-PREVIEW`、`PACKAGES-C-RESOLVE-NOOP`、`PACKAGES-C-RESOLVE-ERROR-PRIORITY`、`PACKAGES-C-RESOLVE-IDEMPOTENCY`、`HOST-PACKAGES-UNAVAILABLE` | P |
| `packages.validate` | `Q-A`；报告、空路径、backend/模块失败 | `Runtime: PACKAGES-Q-VALIDATE`、`PACKAGES-Q-VALIDATE-FAILURES`、`HOST-PACKAGES-UNAVAILABLE` | P |

### speaker_mix_presets（3）

| Operation | Profile / 重点 | 直接目标与场景 | 等级 |
|---|---|---|:---:|
| `speaker_mix_presets.delete` | `C-A`；缺失 no-op、预检、持久化失败、重复 | `Runtime: PRESETS-C-DELETE`、`PRESETS-C-DELETE-INVALID`、`HOST-PRESETS-UNAVAILABLE` | P |
| `speaker_mix_presets.list` | `Q-A`；空集合、detached 快照、宿主 | `Runtime: PRESETS-Q-EMPTY`、`PRESETS-Q-SNAPSHOT`、`HOST-PRESETS-UNAVAILABLE` | P |
| `speaker_mix_presets.save` | `C-A`；预检不分配、创建/更新、重复名、边界、持久化 | `Runtime: PRESETS-C-SAVE-PREVIEW`、`PRESETS-C-SAVE-COMMIT`、`PRESETS-C-UPDATE`、`PRESETS-C-DUPLICATE-NAME`、`PRESETS-C-VALIDATION`、`PRESETS-C-PERSISTENCE-FAILURE`、`HOST-PRESETS-UNAVAILABLE` | P |

## 4. 跨域契约覆盖

| 契约 | 已有静态自动化映射 | 当前边界 |
|---|---|---|
| document → revision → object/domain 错误优先级 | `Doc: AFC-DOC-LIFECYCLE-009/010` 完整证明 `tracks.set_color`；`Edit: dispatch/error-priority-matrix` 覆盖 35 个同步编辑命令的 document/revision 优先级；`Async` 对 `inference.apply_pitch` 覆盖 document/revision/clip/piece/note；`Runtime` 覆盖 playback loop、editor active clip 和 package voice resolve 的对应顺序 | 未对 122 项逐项跑完整四层矩阵 |
| no-op / History / revision | `Edit` 覆盖主要编辑、timeline、History；`Doc` 覆盖空 import/savepoint；`Runtime` 覆盖 playback/settings/recent/preset；`Async` 覆盖 inference、audio cache/import/export | 各 operation 的所有等价输入、redo 分支和 savepoint 组合仍未穷举 |
| `validate_only` | 创建类验证不分配对象，替换类不轮换 generation，异步类不分配 TaskId，GUI/settings/file 命令不调用 host 或写文件 | 并非每个 Command 都有独立预检场景；部分由同域代表覆盖 |
| 幂等 | `Idem` 覆盖串行、16/64 路同步并发、参数/operation 冲突、预检/校验/提交失败释放、文档/generation 隔离和异步接受/终态；`Core/Doc/Async` 覆盖 save、MIDI、audio export 的真实 Facade 路径 | `DocumentGeneration` 策略的每个 operation 尚未逐项做真实 handler 重放/并发 |
| 取消、提交点与竞态 | `Races` 覆盖 256 次 cancel/commit barrier、128 次重复完成、排队/运行取消、revision/删除/new/open 交错；`Core/Async` 覆盖实际 extract/audio export 生命周期 | 强竞态主要集中在 `extract.pitch.start` 和通用 Task 协议，尚未对全部异步 operation 做同构矩阵 |
| generation | `Doc` 覆盖 new/open 的原子轮换与 History/idempotency/task 清理；`Idem` 覆盖键空间重建；`Races` 覆盖旧 TaskId 和 late callback；`Async` 覆盖 inference late writeback | 多文档/多窗口不是一期能力，当前只验证单 Session/Window 契约 |
| host unavailable | `Runtime` 覆盖 application/playback/editor/settings/packages/presets；`Async` 覆盖 inference/document save/file/audio export/extraction 服务缺失 | 并非同一 host 下的每个 operation 都有独立缺失回调场景 |
| Catalog、集中 ID 与源码边界 | `Catalog` 精确对照 122 个 descriptor；`Arch` 禁止产品 operation 字符串散落、带版本号的 operation 后缀、History/revision/generation 绕行 | Catalog 只保存 descriptor；它本身不证明每个公开 Facade 方法都可成功调用 |

## 5. 仍有的自动化缺口

1. 122 项均已有直接行为断言，但全部仍为 `P`：没有 operation 完成
   `test-outline.md` 中全部适用的正常、边界、错误、无副作用、确定性与真实资格维度。
2. `Catalog` 的 122 个稳定场景 ID 是 descriptor 关联，不是 122 个 handler 用例 ID；目前
   `Core` 仍是大型顺序测试，`Runtime/Doc` 的部分场景也按跨 operation 契约分组。
3. 错误优先级、host unavailable、`validate_only` 和幂等都有跨域证明，但尚未对每个适用
   operation 数据驱动展开；特别是带 `DocumentGeneration` 幂等策略的同步编辑命令。
4. 异步强竞态已覆盖 Task 协议和 pitch extraction 代表路径，但 MIDI extraction 与 audio
   export 尚未各自完成取消/提交点/revision/对象删除/generation/重复 callback 的全排列。
5. 真实 codec、声库、模型、音频设备、持久化后重启和可见 GUI 结果属于资格/回归层；本清单
   不记录或推断这些环境执行结果。

## 6. 机器维护不变量

每次更新本清单都应静态检查以下不变量：

- `OperationIds.h` 为 122 条、122 个唯一值；`Catalog` 显式 descriptor 表也为 122 条、
  122 个唯一符号，两个集合完全相等。
- 本文逐 operation 表为 122 行、122 个唯一 ID，与 `OperationIds.h` 集合完全相等；
  等级计数之和必须为 122。
- Automation operation ID 范围内不存在带版本号的后缀；产品源码除 `OperationIds.h` 外
  不出现 122 个 ID 的字符串字面量，调用与注册使用集中常量。
- 文档不得包含本机绝对路径、用户名或执行结果；提交前运行 Markdown 集合/重复项检查、
  隐私扫描和 `git diff --check`。
