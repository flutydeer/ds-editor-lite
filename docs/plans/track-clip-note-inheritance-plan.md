# Track/Clip/Note 层级继承设计方案

## 需求概述

- **Track** 和 **Clip** 级别可选择 **spk** (speaker)
- **Track**、**Clip**、**Note** 级别可选择 **语种** (language)
- 未手动指定时，默认跟随父级值（继承机制）

---

## 当前架构分析

### 现有数据结构

```
Track
├── m_singerInfo: SingerInfo     # 歌手包
├── m_speakerInfo: SpeakerInfo   # 当前选中的 speaker
├── m_defaultLanguage: QString   # 默认语种
└── clips: OverlappableSerialList<Clip>
         ├── AudioClip
         └── SingingClip
                  ├── m_singerInfo: SingerInfo (可独立)
                  ├── m_speakerInfo: SpeakerInfo (可独立)
                  ├── m_defaultLanguage: QString (可独立)
                  ├── useTrackSingerInfo: bool (default true)
                  ├── useTrackSpeakerInfo: bool (default true)
                  ├── m_trackSingerInfo: SingerInfo (来自 track)
                  ├── m_trackSpeakerInfo: SpeakerInfo (来自 track)
                  └── notes: OverlappableSerialList<Note>
                           └── Note
                                    ├── m_language: QString (default "unknown")
                                    # ❌ 无 speaker 字段
```

### 继承模式现状

SingingClip 已实现 `useTrackSpeakerInfo` 机制：
- `true` → 使用 track 的 speakerInfo
- `false` → 使用 clip 自己存储的 speakerInfo

Note 只有 `language` 字段，无 speaker。

---

## 设计方案

### 核心原则

1. **默认跟随父级** - UI 下拉框第一项固定为"跟随父级"，默认选中
2. **手动锁定** - 用户选择其他选项后，使用该值，不再变化
3. **无需开关** - 通过值是否为"跟随父级"来判断是否继承

### 下拉框选项顺序

```
[跟随父级] ← 默认选中，值为特殊标记
─────────────
option-1
option-2
...
```

选择"跟随父级"时：动态获取父级 (parent.effectiveXXX)
选择具体选项时：使用该选项的值

### 实体属性一览

| 级别 | SingerInfo | Speaker | Language | 继承机制 |
|------|----------|---------|----------|----------|
| **Track** | ✅ singerInfo | ✅ speakerInfo | ✅ defaultLanguage | 无父级 |
| **SingingClip** | ✅ 双字段 | ✅ 双字段 | ✅ 双字段 | **用空值表示跟随父级** |
| **Note** | — | 始终继承 clip | ✅ language (已有) | 无 |

- SingingClip 保持现有 `m_singerInfo` + `m_trackSingerInfo` 等双字段结构
- getter 用 **空值** 判断是否跟随父级（复用现有 useTrackSpeakerInfo 逻辑）

### 2. 继承逻辑（新）

```cpp
// SingingClip 的有效值获取
QString SingingClip::effectiveSpeaker() const {
    if (m_selectedSpeakerOption == "跟随父级") {
        return track()->effectiveSpeaker();
    }
    return m_speakerInfo.id();  // 用户选择的值
}

QString SingingClip::effectiveLanguage() const {
    if (m_selectedLanguageOption == "跟随父级") {
        return track()->effectiveLanguage();
    }
    return m_defaultLanguage;  // 用户选择的值
}
```

- 存储用户选择的选项（m_selectedSpeakerOption, m_selectedLanguageOption）
- 值为 `"跟随父级"` 时动态获取父级，否则使用存储值
- 无需 Boolean 开关

### 3. 数据模型变更

#### 3.1 Track 级别
无需变更。

#### 3.2 SingingClip 级别
修改 getter，复用现有字段（`m_singerInfo`, `m_speakerInfo`, `m_defaultLanguage`）：

```cpp
// 当前已有字段：
Property<SingerInfo> m_singerInfo;       // 独立 singer
Property<SingerInfo> m_trackSingerInfo;  // 来自 track
Property<SpeakerInfo> m_speakerInfo;    // 独立 speaker
Property<SpeakerInfo> m_trackSpeakerInfo; // 来自 track
Property<QString> m_defaultLanguage;     // 独立 language

// 修改后的 getter - 用空值表示"跟随父级"
SpeakerInfo SingingClip::speakerInfo() const {
    if (m_speakerInfo->isEmpty()) {
        return m_trackSpeakerInfo.get();  // 跟随父级
    }
    return m_speakerInfo.get();  // 独立值
}

QString SingingClip::defaultLanguage() const {
    if (m_defaultLanguage->isEmpty()) {
        return track()->defaultLanguage();  // 跟随父级
    }
    return m_defaultLanguage.get();  // 独立值
}
```

- 字段值为空 → 跟随父级
- 字段值非空 → 使用该值
- UI 设置时：设为非空值即锁定为独立，设为空即恢复跟随父级

#### 3.3 Note 级别
- **Language**：Note 已有 `m_language` 字段，用户手动设置，无自动继承逻辑（每个 note 独立设置）
- **Speaker**：无字段，始终通过 `clip()->effectiveSpeaker()` 获取

### 4. UI 交互设计

#### 4.1 Track 面板
- **Singer 选择器**：下拉框，加载可用 SingerInfo 列表
- **Speaker 选择器**：依赖 Singer 自动加载 speakers，第一项固定为"跟随父级"
- **Language 选择器**：依赖 Singer 自动加载 languages，第一项固定为"跟随父级"

#### 4.2 Clip 属性面板
- **Speaker 选择器**：
  - 第一项："跟随父级" ← 默认选中
  - 其余：track.singerInfo.speakers 列表
- **Language 选择器**：
  - 第一项："跟随父级" ← 默认选中
  - 其余：track.singerInfo.languages 列表

#### 4.3 Note 属性面板
- **Language 显示**（只读，显示继承来的值）
  - 若手动设置，显示值 + "已设置" 标记
  - 若继承，显示 "继承自 Clip: xxx"
- **Speaker 显示**：无需显示，始终继承自 clip

### 5. 序列化格式

```json
// Track 序列化 (已有)
{
    "singerInfo": { ... },
    "speakerInfo": { "id": "speech" },
    "defaultLanguage": "zh-CN"
}

// SingingClip 序列化
{
    "defaultLanguage": "ja-JP",
    "speakerInfo": { "id": "voice2" }
}
// Note: 当 defaultLanguage/speakerInfo 有值时意为独立选择，为空时意为"跟随父级"

### 6. 需要完成的 TODO

| 位置 | 当前状态 | 需要完成 |
|------|----------|----------|
| `TrackController.cpp:61` | `// TODO: set default singer and speaker here` | 实现新 Track 创建时的默认值设置 |
| `AppModel.cpp:141` | `// TODO: set default singer and speaker here` | 同上 |
| `SingingClip.cpp:260-312` | 注释掉的 re-segmentation 代码 | 实现 singer/speaker 变更时的 piece 重建 |

---

## 实现计划

### Phase 1: 修改继承逻辑 (1 个文件)

1. **SingingClip.cpp** - 修改 `speakerInfo()` / `defaultLanguage()` getter，用空值判断是否跟随父级

### Phase 2: UI 选择器 (1 个文件)

2. **ClipPropertyWidget** - 添加 language 选择器，第一项固定为"跟随父级"

### Phase 3: 控制器逻辑 (2 个文件)

3. **TrackController.cpp** - 新建 Track 时设置默认值 TODO
4. **AppModel.cpp** - 加载时的默认值 TODO

---

## 设计原则

- **跟随父级默认** - 下拉框第一项固定为"跟随父级"，默认选中
- **动态获取** - 值为"跟随父级"时动态查询父级有效值
- **手动锁定** - 用户选择具体值后使用该值，不再继承
- **最小变更** - 复用现有 m_speakerInfo / m_defaultLanguage 字段，无新增布尔开关