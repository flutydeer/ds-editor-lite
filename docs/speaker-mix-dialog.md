# Speaker Mix Dialog (声线混合对话框)

## 概述

`SpeakerMixDialog` 用于配置同一歌手模型内多个 speaker 的固定混合比例。当前已从
debug/demo 对话框推进到 Fixed Mix preset 管理界面：

- 从当前 `SingingClip` 的 effective singer 读取 speaker 列表
- 从 track/clip 的 singer/speaker 二级菜单中新建或管理 Fixed Mix preset
- 顶部 preset bar 支持选择、新建、保存、另存为、删除、重置
- OK 后把当前 Fixed Mix 数据应用到目标 track/clip
- track/clip 应用 preset 均通过 action 支持 undo/redo
- track/clip 的实际 `SpeakerMixData` 保存到 `workspace["ds-editor-lite"]["speakerMix"]`，重新打开工程可恢复
- preset 自身保存到应用常规设置 `GeneralOption::speakerMixPresets`，不进入工程文件
- 暂不写 opendspx 官方 mix 结构；官方 `sources` 仍保持当前单声线 fallback

后续它会继续聚焦 **Fixed Mix**：作为固定混合配置和用户混合预设管理界面；Dynamic Mix
关键帧权重只在 `SpeakerMixEditorView` 中编辑，不再通过对话框编辑。Dynamic Mix 是 clip
自定义自动化；Follow Track clip 需要通过参数页显式启用并复制当前轨道配置后才能编辑。

当前 Dynamic Mix 模型接入已完成。对话框只编辑 Fixed Mix 底座；Dynamic Mix keyframes
不进入 preset，也不在对话框内编辑。

对话框位于 `src/app/UI/Dialogs/SpeakerMix/`：

- `SpeakerMixDialog.h/.cpp` — 继承 `OKCancelDialog`，组合 tag 选择区 + 列表 + 比例条
- `SpeakerMixList.h/.cpp` — `QListWidget`，管理 speaker 行（拖拽排序、ComboBox、位置标签）
- `SpeakerMixBar.h/.cpp` — 自绘比例条（拖拽分割点调比例、彩色 segment、标签）

样式注意：

- 对话框应使用 `Dialog::globalParent()` 作为 parent 打开，避免沿用工具栏父级后丢失全局对话框样式。
- 顶部声线 tag 与 OK/Cancel 按钮依赖全局 dialog / controls QSS，不应在局部重新覆盖基础样式。

相关模型与动作：

- `Model/AppModel/SpeakerMixData.h/.cpp` — speaker mix 数据结构与权重规范化
- `Track` — 持有 track 级 `SpeakerMixData`，通过 `speakerMixChanged` 同步跟随中的 clips
- `SingingClip` — 自有/track 缓存两份 speaker mix；`useTrackSingerInfo` 统一表示 singer/speaker/speaker mix 全部跟随 track
- `Model/SpeakerMixPreset/` — 用户级 Fixed Mix preset 数据结构与 store
- `Controller/Actions/AppModel/SpeakerMix/` — clip/track speaker mix preset 应用 action 与替换 action
- `DspxProjectConverter` — DS workspace 中读写 track/clip `speakerMix`

## 对话框布局

1. **Preset bar** — 预设 ComboBox + 新建 / 保存 / 另存为 / 删除 / 重置
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

1. 用户在 track 或 clip 的 singer/speaker 二级菜单中展开 multi-speaker singer。
2. 菜单显示该 singer 当前 `packageId + singerId + packageVersion` 精确匹配的 Fixed Mix presets。
3. 点击 preset：复制 preset 的 `sources + fixedWeights` 到目标 track/clip，进入 `FixedMix`。
4. 点击“新建混合预设...”或“管理混合预设...”：打开 `SpeakerMixDialog(singerInfo, initialMix, Dialog::globalParent())`。
5. Dialog 从 `SingerInfo::speakers()` 构造可选 speaker 列表；preset 下拉只显示当前 singer 版本的 presets。
6. Save/Save As 只保存 Fixed Mix 的 `sources + fixedWeights`，不保存 Dynamic Mix keyframes。
7. OK 时生成 `SpeakerMixData { mode = FixedMix, sources, fixedWeights }` 并应用到目标 track/clip。
8. 保存工程时写入展开后的 track/clip DS workspace；preset 列表保存在应用常规设置中。

## 预设入口

固定混合预设已集成到现有 singer/speaker 二级菜单中：

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

- 选择单 speaker：当前 track/clip 进入 `Single`
- 选择混合预设：复制 preset 的 `sources + fixedWeights` 到当前 track/clip，进入 `FixedMix`
- 新建 / 管理混合预设：打开 `SpeakerMixDialog`
- `ClipEditorToolBarView` 上临时打开本对话框的 `Speaker Mix` 按钮已移除

预设语义：

- 预设只保存 Fixed Mix，不保存 Dynamic Mix keyframes
- 预设按 `packageId + singerId + packageVersion` 精确匹配
- 应用预设是复制，不是引用；track/clip 后续修改不会自动反写 preset
- 工程恢复以展开后的 `SpeakerMixData` 为准；preset 自身不进入工程文件
- 如果 package 更新导致 `packageVersion` 变化，旧版本 preset 不显示、不自动迁移、不删除
- 当前 preset 管理能力完整但偏重，后续需要讨论是否简化交互
- Dynamic Mix keyframes 不进入 preset；Dynamic Mix 的 bypass 状态保存在工程的展开后 `SpeakerMixData` 中。

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

`DspxProjectConverter` 在 track/clip 的 DS workspace 中写入：

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
- 为了支持 Dynamic Mix Bypass，`fixedWeights` 与 `dynamicKeyframes` 现在允许同时写入与恢复
- 读取时使用当前 effective singer 的 `speakers()` 严格解析 source
- clip 处于 Follow Track 时不写自有 `speakerMix`；打开工程后从 track 缓存继承
- 旧工程的 `useTrackSpeakerInfo` 兼容读取；新语义用 `useTrackSingerInfo` 表示 singer/speaker/speaker mix 统一跟随
- 官方 opendspx `sources` 仍写当前单 effective singer/speaker，作为推理和第三方兼容 fallback

Preset store 存在应用常规设置中：

```json
{
  "speakerMixPresets": {
    "schemaVersion": 1,
    "presets": [
      {
        "id": "uuid",
        "name": "Bright Blend",
        "packageId": "package",
        "singerId": "singer",
        "packageVersion": "1.2.0",
        "sources": [
          { "id": "luna", "name": "Luna" },
          { "id": "azure", "name": "Azure" }
        ],
        "fixedWeights": [0.5],
        "createdAt": "2026-06-21T10:00:00.000Z",
        "updatedAt": "2026-06-21T10:00:00.000Z"
      }
    ]
  }
}
```

## 已完成提交

- 当前提交 — Add fixed speaker mix presets
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

## 当前限制与观察

- `ClipEditorToolBarView` 中的临时 `Speaker Mix` 按钮已移除
- 用于默认进入 Speaker Mix 页的本地测试改动已移除；现在仍可通过参数前景下拉框手动切到 Speaker Mix 页验证 Dynamic Mix
- 本轮不实现动态混合推理；推理仍使用当前单 speaker fallback
- 本轮不写 opendspx 官方 mix 结构
- 当前 Fixed Mix 对话框、Dynamic Mix 编辑视图和参数页 toolbar 已统一使用 `SpeakerMixColorResolver` 派生颜色。
- 当前对话框在 `mode == DynamicMix` 时作为 Fixed Mix 底座入口使用；打开时恢复 `sources + fixedWeights`，不会保存 dynamic keyframes 到 preset。
- Dynamic Mix 已改为显式启用；Follow Track clip 启用时会复制当前 effective singer/speaker/mix 到 clip 自有状态，再进入关键帧编辑。
- preset 管理交互偏复杂，后续需要讨论是否简化。
- preset 应用后的外部 UI 状态仍不完整：track/clip 的 singer/speaker 组合框与 clip 标签当前会显示第一个 speaker，而不是 preset 名称或混合状态；多级 speaker 菜单也缺少选中态样式。下一轮需要先讨论 preset 当前值的显示语义，再实现菜单选中态。

## 下一步

结合 `speaker-mix-plan.md`，Dynamic Mix 模型接入已完成，下一步建议按下面顺序推进：

1. 回归 Dynamic Mix 显式启用与 Bypass
   - 覆盖 Follow Track + track preset、clip 自定义 FixedMix、Bypass/Resume、Stop Dynamic
   - 回归关键帧添加、删除、拖拽、undo/redo 与保存恢复

2. 简化 Fixed Mix preset 管理
   - 讨论是否保留当前完整 preset bar
   - 评估更轻量的保存/覆盖/删除流程

3. 讨论 Fixed Mix preset 的当前状态 UI
   - 组合框和 clip 标签需要表达 preset / Fixed Mix，而不是退化成第一个 speaker
   - speaker 二级菜单需要补充选中态样式，与 ComboBox 的当前项反馈保持一致

4. 后续再评估 opendspx 官方 mix 结构
   - 当前 AppModel 坚持“一个 singer 下多个 speaker”语义
   - 等确认 opendspx 对 speaker/mix 的官方表达后，只调整 converter，不反向污染 AppModel
