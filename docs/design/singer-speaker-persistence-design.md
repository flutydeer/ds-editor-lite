# Singer & Speaker Info Persistence Design

## Overview

DS Editor Lite 支持在轨道（Track）和歌唱剪辑（SingingClip）上独立设置歌手（Singer）和声线（Speaker），并通过 "Follow Track" 继承机制让剪辑默认跟随轨道的歌手/声线设置。这些信息在保存 `.dspx` 工程文件时持久化，加载时恢复。

## Data Model

### Track

轨道自身持有歌手和声线信息，没有父级可继承：

| 字段 | 类型 | 说明 |
|------|------|------|
| `m_singerInfo` | `SingerInfo` | 轨道的歌手（含 identifier、name、speakers 列表、languages 列表） |
| `m_speakerInfo` | `SpeakerInfo` | 轨道的声线（id、name、toneMin、toneMax） |
| `m_defaultLanguage` | `QString` | 轨道默认语言 |

### SingingClip

剪辑有两套 singer/speaker 字段：一套是“本地值”（剪辑自己选的），一套是“轨道缓存”（从轨道同步过来用于继承展示）。通过 `useTrackSingerInfo` / `useTrackSpeakerInfo` 两个布尔标志决定 getter 返回哪一套值。

当前交互和模型 API 将 singer/speaker 作为强关联的一组状态处理：正常运行时只产生“完全 Follow Track”或“完整独立 singer/speaker pair”两种状态，不再分别设置或分别通知 singer 与 speaker。保留两个 flag 是为了持久化兼容旧工程；加载旧的半继承状态时会归一化为完整 pair。

| 字段 | 类型 | 说明 |
|------|------|------|
| `m_singerInfo` | `Property<SingerInfo>` | 剪辑本地歌手 |
| `m_trackSingerInfo` | `Property<SingerInfo>` | 轨道歌手缓存（由 Track 推送） |
| `m_speakerInfo` | `Property<SpeakerInfo>` | 剪辑本地声线 |
| `m_trackSpeakerInfo` | `Property<SpeakerInfo>` | 轨道声线缓存 |
| `useTrackSingerInfo` | `Property<bool>` | `true` = 继承轨道歌手，默认 `true` |
| `useTrackSpeakerInfo` | `Property<bool>` | `true` = 继承轨道声线，默认 `true` |

**getter 逻辑**（`SingingClip::singerInfo()` / `speakerInfo()`）：

```cpp
SingerInfo SingingClip::singerInfo() const {
    if (useTrackSingerInfo)
        return m_trackSingerInfo.get();  // 返回轨道缓存
    return m_singerInfo.get();           // 返回本地值
}
```

这些字段使用 `Property<T>` 模板实现值变更回调。每次 `operator=` 赋值时若值变化，触发 `onChanged` 回调发出 Qt 信号，驱动 UI 同步更新。

### 继承链路

```
Track
├── singerInfo / speakerInfo          ← 轨道上用户选择的实际值
└── SingingClip
    ├── m_trackSingerInfo / m_trackSpeakerInfo  ← 从 Track 同步的缓存（仅用于继承展示）
    ├── m_singerInfo / m_speakerInfo            ← 剪辑本地独立选择
    └── useTrackSingerInfo / useTrackSpeakerInfo ← 控制 getter 返回哪个
```

Track 的 `singerOrSpeakerChanged` 信号 → `setTrackSingerAndSpeakerForClip()` → 更新剪辑的 `m_trackSingerInfo` / `m_trackSpeakerInfo`。当 `useTrackSingerInfo = true` 时，剪辑的 `singerInfo()` 自动返回更新后的轨道歌手。

## Save/Load Format

### Track（`workspace["ds-editor-lite"]`）

Track 在 opendspx 中没有官方 singer 字段，所有信息保存在 `<track>.workspace["ds-editor-lite"]` 内：

```json
{
  "schemaVersion": 1,
  "singer": {
    "identifier": { "singerId": "...", "packageId": "...", "packageVersion": "..." },
    "name": "Singer Name",
    "defaultLanguage": "cmn"
  },
  "speaker": { "id": "speaker-id", "name": "Speaker Name", "toneMin": "C3", "toneMax": "C5" },
  "defaultLanguage": "cmn",
  "colorIndex": 0
}
```

### SingingClip（`sources.singers[]` + `workspace["ds-editor-lite"]`）

剪辑有两个写入位置：

1. **官方字段** `<clip>.sources.singers[0]`：保存 effective singer（`singingClip->singerInfo()` 的实际值），确保其他 opendspx 编辑器能读取。`id` 写 singerId，`extra` 写 packageId、packageVersion、speakerId。

2. **DS workspace** `<clip>.workspace["ds-editor-lite"]`：保存行为标志和 DS 私有字段：

```json
{
  "schemaVersion": 1,
  "useTrackSingerInfo": true,
  "useTrackSpeakerInfo": true,
  "speaker": { "id": "...", "name": "..." },
  "defaultLanguage": "cmn"
}
```

**设计决策**：即使 `useTrackSingerInfo = true`，仍将轨道歌手的实际值写入 `sources.singers[0]`。这是为了兼容第三方 opendspx 编辑器——它们不识别 `useTrackSingerInfo` 标志，但能读取 official singer 字段得到正确的歌手 ID。DS Lite 加载时通过 workspace 中的 flag 恢复继承语义。

### 加载兼容策略

| 文件类型 | `useTrackSingerInfo` 默认值 | 说明 |
|----------|---------------------------|------|
| DS Lite 新文件（有 workspace 标志） | 按 workspace 中保存的值 | 正常恢复 |
| 旧 DS Lite 文件（无 sources、无 workspace） | `true` | 继承轨道，与旧行为一致 |
| 第三方 opendspx（有 sources，无 workspace） | `false` | 使用 sources 中的独立歌手 |

## UI Design

### TwoLevelComboBox

通用的两级下拉控件，用于同时展示和选择歌手+声线。菜单结构：

```
[Follow Track]          ← 仅在剪辑编辑器上显示，选中后按钮显示实际有效歌手名
─────────────
(No singer)             ← 清空选择
─────────────
Singer A /
  Speaker A1
  Speaker A2
Singer B                ← 单声线歌手，不分子菜单
```

`ComboBoxItemData` 结构：

```cpp
struct ComboBoxItemData {
    QString text;              // 菜单项显示文字
    SingerInfo singer;         // 完整歌手信息
    SpeakerInfo speaker;       // 完整声线信息
    QAction *action = nullptr;
    bool isInheritItem = false; // 是否为 "Follow Track" 条目
};
```

**"Follow Track" 条目**：`isInheritItem = true`，`text = "Follow Track"`。被选中后，singer/speaker 字段通过 `setCurrentData(singer, speaker, preferInherit=true)` 动态更新为轨道的实际歌手/声线，使按钮显示 `Follow Track (Singer / Speaker)`。`currentText()` 会保留 Follow Track 状态提示，同时展示当前有效 singer/speaker；工具栏 tooltip 同步为同一文本，避免按钮宽度不足时看不到完整信息。

**包刷新时保留选中**：`setItems()` 重新构建菜单前保存当前状态（是否选中"Follow Track"、之前的 singer/speaker），构建后用对应逻辑恢复。

### TrackControlView

轨道的 `cbSinger`（`TwoLevelComboBox`）不需要 `setShowInheritItem(true)`——轨道无父级，没有继承概念。

选中 singer/speaker 后直接调用 `Track::setSingerAndSpeakerInfo()`，Track 发出 `singerOrSpeakerChanged` 信号，所有继承该轨道的剪辑自动更新 `m_trackSingerInfo` / `m_trackSpeakerInfo`。

### ClipEditorToolBarView

剪辑编辑器的 `cbSinger` 启用 `setShowInheritItem(true)`。

**选中逻辑**（`onSingerEdited()`）：

- **"Follow Track"** → `m_singingClip->useTrackSingerAndSpeaker()`：同时设两个 flag = true，getter 返回轨道缓存 pair
- **具体歌手/声线** → `m_singingClip->setOwnSingerAndSpeaker(singer, speaker)`：原子设置本地 pair，同时设两个 flag = false

**状态恢复**（`onClipSingerChanged()` / `setPianoRollToolsEnabled()`）：

- `useTrackSingerInfo == true` → `setCurrentData(singer, speaker, preferInherit=true)`：选中 "Follow Track" 条目并更新其显示数据
- `useTrackSingerInfo == false` → `setCurrentData(singer, speaker, preferInherit=false)`：按 singer+speaker 匹配合并项

## Pair-level Singer/Speaker API

### 问题

Singer 和 speaker 是同一套渲染身份的两个部分。若把它们拆成独立 setter 或独立信号，UI、推理和持久化都可能观察到 singer 已变但 speaker 尚未更新的中间状态，导致显示错配或推理任务使用不完整数据。

### 解决方案

`SingingClip` 只暴露成对的运行时状态变更方法：

```cpp
void SingingClip::setOwnSingerAndSpeaker(const SingerInfo &singer, const SpeakerInfo &speaker) {
    const auto oldSingerInfo = this->singerInfo();
    const auto oldSpeakerInfo = this->speakerInfo();
    m_singerSpeakerBatching = true;
    m_speakerInfo = speaker;
    m_singerInfo = singer;
    useTrackSpeakerInfo = false;
    useTrackSingerInfo = false;
    m_singerSpeakerBatching = false;
    updateDefaultG2pId(m_defaultLanguage);
    if (oldSingerInfo != this->singerInfo() || oldSpeakerInfo != this->speakerInfo())
        Q_EMIT singerOrSpeakerChanged();
}
```

`setTrackSingerAndSpeakerInfo()` 同时更新轨道缓存 pair，`useTrackSingerAndSpeaker()` 同时切回 Follow Track。三者都只在 effective singer/speaker pair 实际变化时发出一次 `singerOrSpeakerChanged()`，避免独立剪辑因轨道 singer/speaker 改动而触发无意义推理。

### 方法速查

| 方法 | 用途 | 调用方 |
|------|------|--------|
| `setOwnSingerAndSpeaker(s, spk)` | 原子设置本地 singer+speaker，并切到独立状态 | Clip 编辑器选择具体歌手、Converter 加载独立 Clip、Paste 独立 Clip |
| `setTrackSingerAndSpeakerInfo(s, spk)` | 同时更新轨道缓存 singer+speaker | Track 推送、ValidationController、Converter 加载、跨轨移动 |
| `useTrackSingerAndSpeaker()` | 同时切回轨道 singer+speaker 继承 | Clip 编辑器选择 Follow Track、Converter 加载 Follow Track Clip |
| `Track::setSingerAndSpeakerInfo(s, spk)` | 原子设置轨道 singer+speaker，并推送给继承 Clip | TrackControlView |

## Clipboard and Move Semantics

- 复制/粘贴 Follow Track Clip：新 Clip 跟随目标 Track，粘贴后显示目标 Track 的 effective singer/speaker。
- 复制/粘贴独立 Clip：剪贴板保存源 Clip 的 effective singer/speaker 和继承 flag，粘贴后恢复为完整独立 pair，不受目标 Track singer/speaker 影响。
- 跨轨移动 Follow Track Clip：移动到新 Track 后刷新轨道缓存，显示并使用新 Track 的 singer/speaker；撤销移动时恢复旧 Track 的缓存。
- 跨轨移动独立 Clip：只刷新轨道缓存，不改变 effective 本地 pair，因此不会触发无意义 singer/speaker 变更。

## Key Files

| 文件 | 职责 |
|------|------|
| `src/app/Model/AppModel/SingingClip.h/.cpp` | 歌手/声线模型，继承逻辑，原子 setter |
| `src/app/Model/AppModel/Track.h/.cpp` | 轨道歌手/声线，信号推送 |
| `src/app/UI/Controls/TwoLevelComboBox.h/.cpp` | 歌手/声线下拉控件，"Follow Track" 条目 |
| `src/app/UI/Views/ClipEditor/ToolBar/ClipEditorToolBarView.cpp` | 剪辑编辑器工具栏，歌手选择交互 |
| `src/app/UI/Views/TrackEditor/TrackControlView.cpp` | 轨道控制面板，歌手选择交互 |
| `src/app/Modules/ProjectConverters/DspxProjectConverter.cpp` | dspx 文件保存/加载中的 singer/speaker 序列化 |
| `src/app/Modules/Inference/InferController.cpp` | 歌手变更时重建推理任务 |
