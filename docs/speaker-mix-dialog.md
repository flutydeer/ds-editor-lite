# Speaker Mix Dialog (声线混合对话框)

## 概述

`SpeakerMixDialog` 用于配置同一歌手模型内多个 speaker 的固定混合比例。当前已从
debug/demo 对话框推进到固定混合持久化小闭环：

- 从当前 `SingingClip` 的 effective singer 读取 speaker 列表
- 在剪辑编辑器工具栏通过临时 `Speaker Mix` 按钮打开对话框
- OK 后写入 `SingingClip::SpeakerMixData`
- 通过 `ReplaceSpeakerMixAction` 支持 undo/redo
- 保存到 `workspace["ds-editor-lite"]["speakerMix"]`，重新打开工程可恢复
- 暂不写 opendspx 官方 mix 结构；官方 `sources` 仍保持当前单声线 fallback

后续它会继续聚焦 **Fixed Mix**：作为固定混合配置和用户混合预设管理界面；Dynamic Mix
关键帧权重只在 `SpeakerMixEditorView` 中编辑，不再通过对话框编辑。

当前 Dynamic Mix 模型接入已完成。对话框仍是 `ClipEditorToolBarView` 上临时
`Speaker Mix` 按钮打开的 Fixed Mix 入口，短期保留用于验证 fixedWeights 和 Dynamic Mix
开关之间的切换关系。

对话框位于 `src/app/UI/Dialogs/SpeakerMix/`：

- `SpeakerMixDialog.h/.cpp` — 继承 `OKCancelDialog`，组合 tag 选择区 + 列表 + 比例条
- `SpeakerMixList.h/.cpp` — `QListWidget`，管理 speaker 行（拖拽排序、ComboBox、位置标签）
- `SpeakerMixBar.h/.cpp` — 自绘比例条（拖拽分割点调比例、彩色 segment、标签）

样式注意：

- 对话框应使用 `Dialog::globalParent()` 作为 parent 打开，避免沿用工具栏父级后丢失全局对话框样式。
- 顶部声线 tag 与 OK/Cancel 按钮依赖全局 dialog / controls QSS，不应在局部重新覆盖基础样式。

相关模型与动作：

- `Model/AppModel/SpeakerMixData.h/.cpp` — speaker mix 数据结构与权重规范化
- `SingingClip` — 持有 `SpeakerMixData`，提供 `speakerMixData()` / `setSpeakerMixData()`
- `Controller/Actions/AppModel/SpeakerMix/` — `ReplaceSpeakerMixAction` / `SpeakerMixActions`
- `DspxProjectConverter` — DS workspace 中读写 `speakerMix`

## 对话框布局

1. **Preset bar（后续）** — 预设 ComboBox + 新建 / 保存 / 另存为 / 删除 / 重置
2. **Tag 选择区** — `FlowLayout + TagButton`，每个 speaker 一个 tag；checked = 参与混合
3. **SpeakerMixList** — 仅显示已勾选的 speaker
4. **SpeakerMixBar** — 比例条，拖拽分割点调整比例
5. **OK / Cancel** — OK 写出 FixedMix 数据，Cancel 不修改模型

```
┌───────────────────────────────────────┐
│  预设: [Bright Blend ▼] [+] [S] [...]  │
├───────────────────────────────────────┤
│  [Luna] [Azure] [Umbra]               │  ← Tag 选择区（显示 speaker name）
├───────────────────────────────────────┤
│  = ● 声线: [Luna ▼]   位置: 0%        │  ← 列表行（内部仍使用 speaker id）
│  = ● 声线: [Azure ▼]  位置: 33%       │
│  = ● 声线: [Umbra ▼]  位置: 67%       │
├───────────────────────────────────────┤
│  █Luna 33%█  █Azure 33%█  █Umbra 34%█ │
└───────────────────────────────────────┘
```

## 当前数据流

1. 用户选中歌声剪辑。
2. `ClipEditorToolBarView` 的 `Speaker Mix` 按钮判断当前 effective singer 至少有 2 个 speakers。
3. 点击按钮后打开 `SpeakerMixDialog(singerInfo, clip->speakerMixData(), Dialog::globalParent())`。
4. Dialog 从 `SingerInfo::speakers()` 构造可选 speaker 列表。
5. OK 时生成 `SpeakerMixData { mode = FixedMix, sources, fixedWeights }`。
6. `SpeakerMixActions::replaceSpeakerMix()` 创建 old/new 快照 action。
7. `SingingClip::setSpeakerMixData()` 规范化数据，触发 `speakerMixChanged` 并 bump inference revision。
8. 保存工程时写入 DS workspace；打开工程时在 singer/speaker 恢复后解析 `speakerMix`。

## 预设入口（后续）

固定混合预设会集成到现有 singer/speaker 二级菜单中：

```text
Luna
Azure
Umbra
────────────
Luna + Azure 50/50
Bright Blend
Soft Verse Mix
────────────
新建混合预设...
管理混合预设...
```

- 选择单 speaker：当前 clip 进入 `Single`
- 选择混合预设：复制 preset 的 `sources + fixedWeights` 到当前 clip，进入 `FixedMix`
- 新建 / 管理混合预设：打开 `SpeakerMixDialog`
- 预设入口闭环后，`ClipEditorToolBarView` 上临时打开本对话框的 `Speaker Mix` 按钮可以移除

预设语义：

- 预设只保存 Fixed Mix，不保存 Dynamic Mix keyframes
- 应用预设是复制，不是引用；clip 后续修改不会自动反写 preset
- 工程可在 DS workspace 辅助记录 presetId/name，但实际恢复仍以展开后的 `SpeakerMixData` 为准
- 如果用户基于某 preset 修改比例，Dialog 可显示 `Custom` 或 `PresetName *` 状态；只有显式保存才覆盖 preset

## 数据结构

当前 AppModel 内部表达的是“一个 effective singer 下的多个 speaker embedding 混合”，不按
opendspx 的平铺 singer source 建模。

```cpp
enum class SingerSourceMode { Single, FixedMix, DynamicMix };

struct SpeakerMixSource {
    SpeakerInfo speaker;
};

struct SpeakerMixKeyframe {
    int tick = 0;
    QVector<double> weights; // N-1，最后一个 source 隐式补足
};

struct SpeakerMixData {
    SingerSourceMode mode = SingerSourceMode::Single;
    QList<SpeakerMixSource> sources;
    QVector<double> fixedWeights;
    QList<SpeakerMixKeyframe> dynamicKeyframes;
};
```

约束：

- 当前固定混合使用 `mode = FixedMix`
- `sources.size() >= 2`
- 权重采用 N-1 存储，最后一个 speaker 权重为 `1 - sum(weights)`
- 权重 clamp 到 `[0, 1]`
- `fixedWeights` 与 `dynamicKeyframes` 可以同时存在；`mode` 只表示当前生效模式
- 无效数据（speaker 缺失、长度不匹配、speaker 不属于当前 singer）降级为 Single
- effective singer 改变时，已有 Fixed/Dynamic mix 重置为 Single

## 显示名与业务值

当前 UI 已改为显示 speaker name，但业务逻辑仍使用 speaker id：

- `SpeakerMixDialog` 从 `SingerInfo::speakers()` 构造 `speaker.id() -> speaker.name()` 映射
- `TagButton` 文本显示 name，`speakerName` property 保存 id
- `SpeakerMixList` 行内 ComboBox 文本显示 name，`Qt::UserRole` 保存 id
- `SpeakerMixList::getLabels()` 仍返回 id，用于保存与查找
- `SpeakerMixBar` segment label 显示 name
- name 为空时回退显示 id

这样可以保持持久化、去重、替换 speaker 的逻辑都按 id 走，同时 UI 对用户更友好。

## SpeakerMixList 接口要点

| 方法 / 信号 | 说明 |
|------------|------|
| `setSpeakerDisplayNames(idToName)` | 设置显示名映射；内部业务值仍为 id |
| `addSpeaker(const QString &id)` | 添加行，保留原有 speaker 比例，新 speaker 从当前比例中分配份额 |
| `removeSpeaker(const QString &id)` | 移除行，被移除 speaker 的比例按剩余项原比例重新分配 |
| `speakerChanged(oldId, newId)` | 行内 ComboBox 切换时发射 |
| `setDoubleValues(values)` | 用 double 百分比恢复比例条与行位置 |
| `getMixBar()` | 返回 `SpeakerMixBar` 指针 |
| `getValues() / getLabels()` | 获取当前整数显示比例与 speaker id 列表 |
| `SpeakerMixColorResolver` | 根据 singer speaker 稳定顺序派生 Dialog/tag/bar 共用颜色 |

## DynamicMix 状态下的打开语义

`SpeakerMixDialog` 是 Fixed Mix 底座入口。即使当前 clip 的 `mode == DynamicMix`，打开对话框时也按
`SpeakerMixData::sources` 的保存顺序恢复列表，并优先使用 `fixedWeights` 恢复固定比例。

若当前数据没有有效的 `fixedWeights`，但存在 `dynamicKeyframes`，对话框使用第一帧 keyframe weights
作为兜底显示。只有没有可恢复的有效 sources 时，才回退到当前 singer 的全部 speakers 默认顺序。

## SpeakerMixBar 实现要点

- 内部使用 double 比例，避免反复增删、拖动时整数误差累积
- public `getValues()` 返回总和为 100 的整数百分比
- 显示与拖动吸附到整数百分比：拖动分割线时按 1% 粒度吸附
- 默认拖动分割线只调整相邻两个 speaker 的边界，其余 speaker 比例不变
- 按住 `Alt` 拖动时，分割线左侧组与右侧组按组内原比例等比例压缩/扩展
- 拖动计算基于按下时的比例快照，避免累计误差
- 允许相邻块压缩到 0%，整体比例始终 clamp 到非负并保持总和为 100

## 持久化

`DspxProjectConverter` 在 clip 的 DS workspace 中写入：

```json
{
  "speakerMix": {
    "mode": "fixed",
    "sources": [
      { "id": "luna", "name": "Luna" },
      { "id": "azure", "name": "Azure" },
      { "id": "umbra", "name": "Umbra" }
    ],
    "fixedWeights": [0.33, 0.33]
  }
}
```

说明：

- `mode == Single` 时省略 `speakerMix`
- Fixed Mix 使用 `fixedWeights`
- Dynamic Mix 使用 `dynamicKeyframes`
- 为了支持 Dynamic Mix 开关，`fixedWeights` 与 `dynamicKeyframes` 现在允许同时写入与恢复
- 读取时使用当前 effective singer 的 `speakers()` 严格解析 source
- 官方 opendspx `sources` 仍写当前单 effective singer/speaker，作为推理和第三方兼容 fallback

## 已完成提交

- `1535ee3a` — Add fixed speaker mix persistence
- `0f837b8b` — Show speaker names in mix dialog

当前已用 debug preset 构建通过。构建方式应遵循 `.agents/skills/cmake-build/SKILL.md`。

更早的 UI / 交互基础提交：

- `c45ae64b` — Add speaker tag selection area with TagButton control
- `324168da` — Add FlowLayout from Qt official example
- `dde3f61f` — Tone down QListWidget item hover/selected colors to match track list
- `b95ed2c9` — Move SpeakerMix controls into main app as debug dialog
- `2b6ac398` — Improve speaker mix ratio editing
- `bf8eb7ed` — Align speaker mix split dragging
- `d63c0b81` — Use custom tooltip for speaker mix hover
- `fc377933` — Disable used speaker mix options

## 临时状态

- `ClipEditorToolBarView` 中的 `Speaker Mix` 按钮是临时测试入口
- 用于默认进入 Speaker Mix 页的本地测试改动已移除；现在仍可通过参数前景下拉框手动切到 Speaker Mix 页验证 Dynamic Mix
- 本轮不实现动态混合推理；推理仍使用当前单 speaker fallback
- 本轮不写 opendspx 官方 mix 结构
- 当前 Fixed Mix 对话框、Dynamic Mix 编辑视图和参数页 toolbar 已统一使用 `SpeakerMixColorResolver` 派生颜色。
- 当前对话框在 `mode == DynamicMix` 时仍主要作为 Fixed Mix 临时入口使用；打开时恢复 `sources + fixedWeights` 这份固定底座。

## 下一步

结合 `speaker-mix-plan.md`，Dynamic Mix 模型接入已完成，下一步建议按下面顺序推进：

1. 整理正式入口
   - `ClipEditorToolBarView` 的 `Speaker Mix` 按钮短期继续作为固定混合对话框入口
   - 参数编辑器中的 Speaker Mix 页继续作为动态混合编辑入口
   - 等 preset 菜单入口闭环后，移除剪辑工具栏上的临时按钮
   - 固定混合配置从 singer/speaker 菜单中的 preset 新建/管理入口进入

2. 增加 Fixed Mix preset
   - 新增用户级 fixed mix preset store
   - 对话框顶部增加 preset ComboBox 和新建、保存、另存为、删除、重置按钮
   - 选择 preset 时复制为当前 clip 的 Fixed Mix 配置，不引用 preset

3. 后续再评估 opendspx 官方 mix 结构
   - 当前 AppModel 坚持“一个 singer 下多个 speaker”语义
   - 等确认 opendspx 对 speaker/mix 的官方表达后，只调整 converter，不反向污染 AppModel
