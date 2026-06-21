# 声线混合支持分阶段计划

## Summary

目标是把当前 Speaker Mix 从"视图 demo"推进到完整支持三种 clip 声源模式：

- `Single`：当前单歌手/单 speaker 行为，保持兼容。
- `Fixed Mix`：同一歌手模型内多个 speaker 的固定比例混合，可保存为本地用户预设。
- `Dynamic Mix`：同一组 speaker 的比例随时间变化，使用当前 Speaker Mix 关键帧编辑器。

第一版范围锁定为：模型、UI、撤销重做、DSPX 读写和工程恢复；推理链路先保持单 speaker 兼容，不在本轮完成动态混合合成。

当前进展：

- `Single` 继续沿用原有 singer/speaker 行为。
- `Fixed Mix` 已完成 AppModel、undo/redo、对话框、workspace 持久化与工程恢复闭环。
- `Fixed Mix Preset` 已完成用户级 store、track/clip singer 二级菜单入口、对话框 preset bar、track 级 speaker mix 与 undo/redo。
- `Dynamic Mix` 已完成模型接入：参数页绑定 `SingingClip::SpeakerMixData`，关键帧编辑通过 `ReplaceSpeakerMixAction` 提交。
- `Dynamic Mix` 开关已接入参数页工具栏：Fixed/Dynamic 共用 `sources`，`fixedWeights` 与 `dynamicKeyframes` 可以同时保留，`mode` 只表示当前生效模式。
- opendspx 官方 mix 结构暂不写入；当前只把 speaker mix 存入 DS workspace，官方 `sources` 保持单 effective singer/speaker fallback。

## Key Changes

- 在 `SingingClip` 增加独立的声源混合模型，不塞进 `ParamInfo`：
    - `SingerSourceMode { Single, FixedMix, DynamicMix }`
    - `SpeakerMixSource { SpeakerInfo speaker }`
    - `SpeakerMixKeyframe { int tick, QVector<double> weights }`，weights 存 N-1 项（最后一个 speaker 的权重隐式 = 1 - sum），UI 始终显示 N 个比例。
    - `SpeakerMixData { mode, sources, fixedWeights, dynamicKeyframes }`。`mode` 表示当前生效模式；`fixedWeights` 和 `dynamicKeyframes` 可以同时保留，用于 Dynamic Mix 开关在固定底座和动态关键帧之间切换。
    - 发出独立 `speakerMixChanged()` 信号，并 bump inference revision。

- DSPX 序列化策略：
    - `Single`：沿用当前一个 `SingleSinger`。
    - `Fixed Mix` / `Dynamic Mix`：当前暂不写 opendspx 官方 mix 结构，官方 `sources` 仍写一个 `SingleSinger` 作为兼容 fallback。
    - DS workspace 保存完整 `SpeakerMixData`，用于恢复 speakerId、显示名、固定权重和动态关键帧。
    - 读取时当前优先恢复 DS workspace；无 DS workspace 时仍按旧工程 `Single` 处理。
    - opendspx 官方结构的 `MixedSinger` / `Sources::mix` 后续只在 converter 层评估，不反向影响 AppModel 语义。

- UI 分阶段落地：
    - 已完成：把 `TestSpeakerMix` 抽象成正式比例控件，用于 Fixed Mix 配置。
    - 已完成：把 `SpeakerMixEditorView` 从硬编码数据改为绑定 `SingingClip::SpeakerMixData`，关键帧编辑通过 action 提交。
    - 已完成：参数页工具栏加入 Dynamic Mix 开关，支持 Fixed/Dynamic 生效模式切换。
    - 之后：增加清晰的声源模式入口：`Single / Fixed Mix / Dynamic Mix`。
    - 最后：整理 `ParamInfo::Unknown` 临时方案，保留 UI 上的 "Speaker Mix" 入口，但内部用明确的 speaker mix mode/state 分支，不让它被当成普通 Param。

- 本地预设：
    - 新增用户级 preset store，保存同一 singer 版本下的 speaker 列表和固定比例（不含动态关键帧）。
    - 预设按 `packageId + singerId + packageVersion` 精确匹配；package 升级后旧版本 preset 不显示、不自动迁移、不删除。
    - 预设集成到现有 speaker 选择二级菜单：单个 speaker 列表下方加分隔线和预设列表，选预设 = 进入 Fixed Mix 模式。
    - speaker 菜单底部提供“新建混合预设...”和“管理混合预设...”入口，打开 `SpeakerMixDialog` 管理固定混合预设。
    - `SpeakerMixDialog` 顶部增加 preset bar：预设 ComboBox，以及新建、保存、另存为、删除、重置等操作。
    - 预设全局可用，track 和 clip 均可通过二级菜单选择。
    - 工程文件保存的是展开后的实际 `SpeakerMixData`；预设保存在应用常规设置中，不进入工程文件。
    - 预设应用时复制到 track/clip，之后修改不自动反写预设，除非用户选择覆盖保存。
    - 预设菜单入口完成后，剪辑工具栏上的临时 `Speaker Mix` 按钮已移除。

## Design Decisions

> 本节记录讨论过程中确认的设计决策和待讨论项。每项标注状态：✅ 已决定 / 🔲 待讨论。

### 1. 权重存储维度与增删逻辑 ✅

**存储 N-1，UI 显示 N。** 与 opendspx `ratio` 字段对齐，避免冗余。

#### 新增 speaker ✅
- 按比例压缩现有权重：新 speaker 获得默认比例 p%，原有 speaker 各自按 `(1-p)` 等比缩放，保持相对比例不变。
- 在 N-1 存储下，新 speaker 不插在末尾（保持原最后一位 speaker 的隐式权重位置不变），UI 显示顺序通过 index mapping 与存储顺序解耦。

#### 删除 speaker ✅
- 按比例分摊：被删 speaker 的权重按剩余 speaker 的当前比例分摊给所有人。
- 删除最后一个 speaker 时：原倒数第二位变为新的隐式末位，其 stored weight 被移除。

#### 排序 speaker ✅
- Fixed Mix 下支持排序。
- 排序时同步重排 `sources` 列表和 `weights` 数组（保持对应关系）。
- 若存在 Dynamic Mix keyframe，所有 keyframe 的 weights 也同步重排。

#### Dynamic Mix 限制 ✅
- Dynamic Mix 下**不允许增删、换 speaker、排序**。所有 keyframe 的 weights 数组长度必须一致。
- speaker 列表的构成（哪些 speaker、什么顺序）由 Fixed Mix 阶段决定，进入 Dynamic Mix 后锁定。

### 2. UI "位置"字段 ✅

SpeakerMixList 上每行的"位置"是**纯展示的计算值**，基于各 speaker 占的比例换算（累积比例），不提供文本框编辑。

- 删除 spinbox 输入，仅保留比例条拖拽作为编辑方式。
- 理由：对话框宽度 400px+，比例条精度足够声线混合场景使用。

### 3. Fixed Mix ↔ Dynamic Mix 模式切换 ✅

- **Fixed → Dynamic：** 自动以当前 Fixed Mix 比例创建第一个关键帧（类比 DAW 自动化：以当前推子位置创建第一个自动化点）。
- **Dynamic → Fixed：** Fixed Mix 比例保持不变（不从 Dynamic keyframe 反写）。若之前设过 Fixed Mix 则用原值，若从未设过则取 Dynamic 第一个关键帧的值。
- **Dynamic 关闭不删除 keyframe：** 关闭后再开启，keyframe 数据恢复。
- `mode` 只表示当前生效的权重来源。为了支持 Dynamic Mix 开关，normalize 不能在 FixedMix 时清掉 `dynamicKeyframes`，也不能在 DynamicMix 时清掉 `fixedWeights`。

### 4. Dynamic Mix 第一个关键帧 ✅

第一个关键帧**时间锁定**（不可水平拖动），但**分界点可编辑**（权重可调）。

- 与 Fixed Mix 脱钩：编辑第一个关键帧不反写 Fixed Mix 比例。
- Fixed Mix 始终是"安全退路"，Dynamic Mix 是自由编辑区。

### 5. Action 粒度 ✅

采用粗粒度方案：**一个 `ReplaceSpeakerMixAction`，存 old/new 两个 `SpeakerMixData` 快照，全量替换。**

- Undo 粒度由 View 层控制：拖拽开始时记快照，拖拽结束时提交一次 action，不会产生逐帧 undo。
- 整体替换天然保证原子性（增删 speaker 同时改 sources + weights + keyframe 不会出现中间状态不一致）。
- `SpeakerMixData` 数据量小（几个 speaker + 几十个 keyframe），快照无性能压力。

### 6. Track 级 Fixed Mix 与统一 Follow Track ✅

Track 拥有自己的 `SpeakerMixData`，并与 track singer/speaker 一起构成 clip 的“跟随轨道”来源：

- `Track::speakerMixData()` 保存 track 当前 Fixed/Dynamic mix 数据；本阶段 track 菜单主要应用 Fixed Mix preset。
- track singer/speaker 改变时，track speaker mix 自动重置为 `Single`。
- `SingingClip::useTrackSingerInfo` 保留为唯一跟随标志：`true` 表示 singer、speaker、speaker mix 全部跟随 track。
- 旧工程中的 `useTrackSpeakerInfo` 兼容读取；若旧工程任一跟随标志为 false，则 clip 视为自定义。
- clip 选择具体 speaker 或 preset 时退出 Follow Track；选择 `Follow Track` 时恢复三者跟随。
- track 应用 preset 后，通过 `Track::speakerMixChanged` 同步所有仍处于 Follow Track 的 singing clips。
- track/clip 应用 preset 都通过 action 提交，undo/redo 能恢复 singer/speaker/mix 的整体状态。

### 7. Fixed Mix / Dynamic Mix 共享 speaker 列表 ✅

`SpeakerMixData` 中 `sources`（speaker 列表）在 Fixed Mix 和 Dynamic Mix 之间共享，weights 各自独立。

- Fixed Mix 有自己的 `fixedWeights`。
- Dynamic Mix 有自己的 `dynamicKeyframes`（每个 keyframe 含独立 weights）。
- 切换模式时 speaker 列表不变，只是切换当前生效的 weights 来源。
- 在 Fixed Mix 下增删/排序 speaker 时，所有 Dynamic keyframe 的 weights 同步调整。
- opendspx 序列化时，`MixedSinger.singers[]` 和 `Sources.singers[]` 写入同一份 speaker 列表。

### 8. Dynamic Mix 关键帧编辑入口 ✅

Dynamic Mix 的关键帧权重直接在 `SpeakerMixEditorView` 中编辑，不再接入 `SpeakerMixDialog`
或单独的关键帧属性对话框。

- `SpeakerMixEditorView` 的分割点拖拽精度已足够日常编辑。
- 保留一个动态编辑入口可以减少状态同步、dialog 临时数据和 undo 粒度复杂度。
- `SpeakerMixDialog` 后续只负责 Fixed Mix 的 sources / fixedWeights 配置。

### 9. Dynamic Mix 开关 ✅

Dynamic Mix 需要一个用户可见开关，用于控制动态关键帧是否生效。

- 开关建议放在参数编辑器的 Speaker Mix 页工具栏中。
- 关：使用当前 Fixed Mix / 预设比例，`mode = FixedMix`。
- 开：使用 `dynamicKeyframes`，`mode = DynamicMix`。
- 从 Fixed Mix 第一次开启 Dynamic Mix 时，如果没有 keyframe，则用 `fixedWeights` 创建第一个关键帧。
- 关闭 Dynamic Mix 不删除 `dynamicKeyframes`；再次开启时恢复之前编辑过的关键帧。
- Fixed Mix 是底座，Dynamic Mix 是自动化层；二者共享同一份 `sources`。

### 10. 混合预设入口与管理 ✅

混合预设作为 Fixed Mix 的用户模板存在，不保存 Dynamic Mix keyframes。

- 支持声线混合的 singer，在 speaker 二级菜单中显示：单 speaker 列表、分隔线、用户混合预设列表、管理入口。
- 选择单 speaker：进入 `Single`。
- 选择混合预设：复制预设的 `sources + fixedWeights` 到当前 clip，进入 `FixedMix`。
- `SpeakerMixDialog` 作为固定混合预设编辑/管理界面，顶部提供 preset ComboBox 和新建、保存、另存为、删除、重置等操作。
- 应用预设是复制，不是引用。track/clip 后续修改不自动反写 preset。
- preset 自身保存在应用常规设置中，不进入工程文件；工程恢复以展开后的 `SpeakerMixData` 为准。

## Phases

- Phase 1：正式化比例控件 — **基本完成**
    - ✅ 从 `TestSpeakerMix` 提炼为 `SpeakerMixList`（列表）和 `SpeakerMixBar`（比例条）。
    - ✅ 新增 `SpeakerMixDialog`，组合 Tag 选择区 + 列表 + 比例条。
    - ✅ 新增 `TagButton` 控件和 `FlowLayout`，实现 speaker 成员选择。
    - ✅ 接入真实 `SingerInfo::speakers()`，不再使用硬编码测试 speaker。
    - ✅ 固定混合对话框接入 `SingingClip::SpeakerMixData`。
    - ✅ 对话框显示 speaker name，内部业务值仍使用 speaker id。
    - ✅ Dynamic Mix 接入后，固定模式显示只读平直权重，动态模式允许编辑 keyframes。

- Phase 2：Clip 模型与撤销重做 — **Fixed Mix / Dynamic Mix 已完成**
    - ✅ 在 `SingingClip` 增加 `SpeakerMixData`。
    - ✅ 新增 `ReplaceSpeakerMixAction` / `SpeakerMixActions`。
    - ✅ Fixed Mix 对话框 OK 后通过 action 提交。
    - ✅ 调整 normalize 语义：FixedMix 保留有效 `dynamicKeyframes`，DynamicMix 保留有效 `fixedWeights`，让 Dynamic Mix 开关可恢复 inactive 数据。
    - ✅ `SpeakerMixEditorView` 的关键帧编辑通过 action 提交，不再只改 view 内数据。

- Phase 3：DSPX / workspace 读写 — **workspace 已完成**
    - ✅ `Single` 继续按旧逻辑保存。
    - ✅ Fixed Mix 写入 `workspace["ds-editor-lite"]["speakerMix"]`。
    - ✅ workspace 读取时严格按当前 effective singer 的 speakers 解析。
    - ✅ Dynamic Mix 写入/读取 `dynamicKeyframes`，并允许同时保存 inactive 的 `fixedWeights`。
    - 🔲 opendspx 官方 mix 结构暂缓。

- Phase 4：Dynamic Mix 编辑闭环 — **基本完成**
    - ✅ Speaker Mix 参数页显示当前 clip 的真实 speaker 列表和 keyframes。
    - ✅ 从 Fixed Mix 进入 Dynamic Mix 时，以当前 fixedWeights 创建第一个关键帧。
    - ✅ 在 Speaker Mix 参数页工具栏增加 Dynamic Mix 开关。
    - ✅ 编辑关键帧比例、添加/删除/移动关键帧后，通过 `ReplaceSpeakerMixAction` 提交。
    - ✅ 不再规划单独关键帧属性对话框；Dynamic Mix 权重编辑只在 `SpeakerMixEditorView` 内完成。
    - ✅ 动态关闭时不删除关键帧；重新开启时恢复。
    - ✅ 工具栏 speaker 指示器刷新已做幂等处理，避免每次关键帧提交后重建造成闪烁。
    - ✅ Fixed Mix 对话框、Dynamic Mix 编辑视图、参数页工具栏共用 `SpeakerMixColorResolver`，同一 speaker 在同一 singer 内颜色稳定。
    - ✅ Fixed Mix 对话框在当前 `mode == DynamicMix` 时从 `sources + fixedWeights` 恢复固定底座；没有有效 `fixedWeights` 时才使用第一帧 `dynamicKeyframes` 兜底。

- Phase 6：Fixed Mix 预设与正式入口 — **已完成**
    - ✅ 新增用户级 fixed mix preset store，持久化到 `GeneralOption::speakerMixPresets`。
    - ✅ preset 精确绑定 `packageId + singerId + packageVersion`，同 singer 同版本内名称去重，跨版本允许同名。
    - ✅ 在 track/clip 的 speaker 二级菜单中加入 preset 列表和新建/管理入口。
    - ✅ `SpeakerMixDialog` 顶部增加 preset ComboBox 和新建、保存、另存为、删除、重置按钮。
    - ✅ 选择 preset 时复制为当前 track/clip 的 Fixed Mix 配置，不引用 preset。
    - ✅ Track 新增 `SpeakerMixData`，并通过统一 Follow Track 语义同步到跟随中的 clips。
    - ✅ 新增 track/clip preset 应用 action，覆盖 undo/redo。
    - ✅ 移除剪辑工具栏上的临时 `Speaker Mix` 按钮。

- Phase 5：推理接入预研
    - 明确当前引擎是否能在同一 singer/model 内按 speaker embedding 做时间变化混合。
    - 若支持，再设计 InferPiece 如何携带 speaker mix anchors；若不支持，UI 暂时标注为工程/编辑能力，合成仍使用当前主 speaker 或固定 fallback。

## Test Plan

- 单歌手旧工程打开、保存、再打开后 singer/speaker 不变。
- Fixed Mix：创建 3 个 speaker、调整比例、保存 DSPX、重新打开后比例和顺序一致。
- Dynamic Mix：创建关键帧、拖动比例、保存 DSPX、重新打开后 anchors 一致。
- 动态关闭后保存再打开：固定比例生效，隐藏动态关键帧仍可恢复。
- Dynamic Mix 开关：Fixed → Dynamic 创建首个关键帧，Dynamic → Fixed 回到 fixedWeights，再开启后 keyframes 恢复。
- 应用 Fixed Mix preset：clip 进入 FixedMix，保存再打开后展开后的 speaker/weights 一致。
- 应用 Fixed Mix preset：track 进入 FixedMix，跟随中的 clip 自动刷新，自定义 clip 不受影响。
- 预设版本匹配：`packageId/singerId` 相同但 `packageVersion` 不同时，菜单只显示当前版本 preset。
- Follow Track：clip 选择 preset 后退出跟随；重新选择 Follow Track 后 singer/speaker/speaker mix 都跟随 track。
- preset 修改：clip 已应用的旧配置不随 preset 自动变化，除非用户显式覆盖应用。
- 添加 speaker：现有权重按比例压缩，总和始终 100%。
- 删除 speaker：权重按比例分摊，总和始终 100%，无负数。
- 排序 speaker（Fixed Mix 下）：顺序变更后 Dynamic Mix keyframe 同步重排。
- Fixed → Dynamic 切换：第一个关键帧自动继承 Fixed Mix 比例。
- Dynamic → Fixed 切换：Fixed Mix 比例保持不变。
- DynamicMix 状态下打开 Fixed Mix 对话框：列表按保存的 `sources` 顺序恢复，比例优先使用 `fixedWeights`，不会回退到 singer 默认顺序。
- 同一 speaker 在 Fixed Mix 对话框、参数页 toolbar、Dynamic Mix 图中的 accent 颜色一致。
- Undo/redo 覆盖：模式切换、比例拖拽、添加/删除 keyframe、增删/排序 speaker、应用 preset。
- 兼容读取：无 DS workspace 但有 `MixedSinger` 或 `Sources::mix` 的 opendspx 文件能尽力导入。

## Follow-up Notes

- 颜色仍不写入 `SpeakerMixData`，只在 UI 层派生。`AppColorPalette` 提供应用基础色池和 speaker mix 角色色，`SpeakerMixColorResolver` 负责把 speaker id 映射到稳定 palette index。
- speaker 颜色优先按 `SingerInfo::speakers()` 的稳定顺序派生；找不到 singer 上下文或找不到 speaker id 时，按当前 sources 下标兜底。
- Fixed Mix 对话框是固定底座入口：即使当前 clip 处于 `DynamicMix`，打开时也恢复保存的 `sources + fixedWeights`，不会根据 Dynamic Mix 当前帧重建 speaker 顺序。
- 初步验收发现：开启 Dynamic Mix 后，双击添加关键帧存在无法创建的情况，需要后续联调 `SpeakerMixEditorView` 的事件命中/焦点路径。
- 当前 preset 管理功能完整但操作偏重，后续需要讨论是否简化为更轻量的保存/覆盖/删除流程。

## Assumptions

- 第一版只支持同一歌手模型内的 speaker 混合；跨 singer/model 不支持编辑。
- opendspx 的平铺 singer 表达会通过 DS workspace 补充 speaker 语义。
- Dynamic Mix 下 speaker 列表锁定，不允许增删/排序。
- 动态开关关闭不会删除动态关键帧。
- Fixed Mix 预设只保存 speaker 列表和固定比例，不保存动态关键帧。
- 应用预设是复制，不是引用。
- 本轮不实现动态混合推理，只保证工程数据和编辑体验闭环。
