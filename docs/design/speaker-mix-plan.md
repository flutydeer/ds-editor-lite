# 声线混合（Speaker Mix）— 设计文档

> 状态：✅ 全部实施完成（Fixed Mix / Fixed Mix Preset / Dynamic Mix 模型、UI、持久化、推理接入均已落地；opendspx 官方 mix 结构暂缓）
> 本文是最终设计说明；分阶段计划与验收过程已移除。

## 三种 clip 声源模式

- `Single`：当前单歌手/单 speaker 行为，保持兼容。
- `Fixed Mix`：同一歌手模型内多个 speaker 的固定比例混合，可保存为本地用户预设。
- `Dynamic Mix`：同一组 speaker 的比例随时间变化（SpeakerMixEditorView 关键帧编辑）。

推理链路已全通：Duration 阶段使用静态基座 speaker mix；Pitch / Variance / Acoustic 阶段使用当前 piece 时间范围采样后的 speaker mix。

## 数据模型

`SingingClip::SpeakerMixData` 是独立的声源混合模型，不塞进 `ParamInfo`：

- `SingerSourceMode { Single, FixedMix, DynamicMix }`
- `SpeakerMixSource { SpeakerInfo speaker }`
- `SpeakerMixKeyframe { int tick, QVector<double> weights }`，weights 存 **N-1 项**（最后一个 speaker 权重隐式 = 1 - sum），保持总和恒为 1
- `SpeakerMixData { mode, sources, fixedWeights, dynamicKeyframes }`；`mode` 表示当前生效/旁路状态
- 发出独立 `speakerMixChanged()` 信号，并 bump inference revision

关键语义：

- 添加 speaker：现有权重按比例压缩，总和始终 100%。
- 删除 speaker：权重按比例分摊，总和始终 100%，无负数。
- 排序 speaker（Fixed Mix 下）：顺序变更后 Dynamic Mix keyframe 同步重排。
- Fixed → Dynamic 切换：第一个关键帧自动继承 Fixed Mix 比例。
- Dynamic → Fixed 切换：Fixed Mix 比例保持不变。
- Dynamic Mix 下 speaker 列表锁定，不允许增删/排序。
- **Bypass 不删除动态关键帧**（保留 keyframes，推理用 `fixedWeights`）；**Stop Dynamic 会删除动态关键帧**并回到 Fixed Mix。
- 颜色不写入模型，由 `SpeakerMixColorResolver` 在 UI 层派生；同一 speaker 在同一 singer 内颜色稳定（Fixed Mix 对话框、参数页工具栏、Dynamic Mix 图一致）。

## DSPX 序列化策略

- `Single`：沿用当前一个 `SingleSinger`。
- `Fixed Mix` / `Dynamic Mix`：**暂不写 opendspx 官方 mix 结构**，官方 `sources` 仍写一个 `SingleSinger` 作为兼容 fallback。
- DS workspace 保存完整 `SpeakerMixData`（speakerId、显示名、固定权重、动态关键帧）。
- 读取优先恢复 DS workspace；无 DS workspace 的旧工程按 `Single` 打开。
- opendspx 官方结构的 `MixedSinger` / `Sources::mix` 后续只在 converter 层评估，**不反向影响 AppModel 语义**。

## 预设（Preset）

- 用户级 preset store，保存同一 singer 版本下的 speaker 列表和固定比例（**不含动态关键帧**），持久化到 `GeneralOption::speakerMixPresets`。
- 预设按 `packageId + singerId + packageVersion` 精确匹配；package 升级后旧版本 preset 不显示、不自动迁移、不删除。
- 入口：speaker 选择二级菜单（单个 speaker 列表下方分隔线 + preset 列表；底部"新建混合预设..." / "管理混合预设..."）+ `SpeakerMixDialog` 顶部 preset bar（ComboBox + 新建/保存/另存为/删除/重置）。
- 预设全局可用，track 和 clip 均可选择；选预设 = 进入 Fixed Mix 模式。
- 工程保存展开后的实际 `SpeakerMixData` + `sourcePresetId/sourcePresetName/sourcePresetDirty`（DAW/VSTi 风格 dirty 状态，编辑后显示 dirty，不按内容反推 preset）。
- **应用预设是复制，不是引用**：track/clip 后续修改不自动反写 preset，除非显式覆盖保存。
- 剪辑工具栏上的临时 `Speaker Mix` 按钮已移除（正式入口为 singer/speaker 二级菜单）。

## 推理接入

- Duration 使用 `effectiveSpeakerMixForFixedInference()` 静态 fallback，不做动态采样（避免 duration 前无法获得实际音素范围的蛋鸡问题）。
- `InferSpeakerMixModel::effectiveSpeakerMixFromData()` 在 Dynamic Active 时按 piece tick 范围采样 keyframes，输出等长权重。
- Dynamic Bypassed / Stop Dynamic 后使用 `fixedWeights`。
- timing 不足、动态数据无效或 speaker source 无效时降级到 Fixed Mix / Single fallback。
- `InferenceApplyGate` 使用 piece 上的 speaker mix signature，避免动态 mix 的异步推理结果误应用。
- dsinfer speaker mix 比例按 `0.0~1.0` 处理。

## UI 约定

- `SpeakerMixDialog` 只负责 Fixed Mix 底座（speaker 列表 + 固定比例 + preset 管理）；Dynamic Mix keyframe 权重编辑只在参数编辑器 `SpeakerMixEditorView` 内完成（不再规划单独关键帧属性对话框）。
- 参数页显式 Dynamic Mix 启用入口；启用后工具栏提供 Bypass / Resume / Stop Dynamic 命令。
- 前景参数列表中的 Speaker Mix 使用 `ParamInfo::SpeakerMix`，不再借用 `ParamInfo::Unknown`。
- Dynamic Mix 状态下打开 Fixed Mix 对话框：列表按保存的 `sources` 顺序恢复，比例优先使用 `fixedWeights`，不回退到 singer 默认顺序。

## Follow-up Notes（未完成项）

- [ ] 下一轮优先补 `SpeakerMixData` 和 `InferSpeakerMixModel` 的**纯数据测试**。
- [ ] Bypass 当前是文字按钮 + 轻量状态提示；后续可参考 DAW 的 automation/plugin bypass 样式设计更明确的旁路视觉语言。
- [ ] preset 管理功能完整但操作偏重，后续讨论是否简化（更轻量的保存/覆盖/删除流程）。
- [ ] 动态推理当前以 piece 范围采样为主，不做音符/音素级动态规划（更细粒度后续再评估）。
- [ ] `MixedSinger` / `Sources::mix` 官方结构导入暂缓，确认 opendspx 官方表达后只调整 converter。

## 关键文件

- `src/libs/ProjectModel/AppModel/SingingClip.*`（`SpeakerMixData`）
- `src/app/Controller/Actions/AppModel/SpeakerMix/SpeakerMixActions.*`（`ReplaceSpeakerMixAction` 等，undo/redo）
- `src/app/UI/Dialogs/SpeakerMix/SpeakerMixDialog.*`（Fixed Mix + preset 管理）
- `src/app/UI/Views/ClipEditor/ParamEditor/`（`SpeakerMixEditorView` 关键帧编辑）
- `src/app/Modules/Inference/Models/InferSpeakerMixModel.*`（推理采样）
- `src/app/UI/Views/TrackEditor/TrackControlView.cpp`、`ClipEditor/ToolBar/ClipEditorToolBarView.cpp`（二级菜单入口）
