# Clipboard System Design Plan

## Context

项目当前缺少剪贴板功能。早期存在一个 `ClipboardController` 的实现框架，但关键部分（序列化、粘贴逻辑）全部被注释或留空。菜单中 Ctrl+X/C/V 快捷键已注册但回调为空。需要为**音符**和**剪辑**两个层面设计完整的剪贴板功能。

## Early Design Evaluation

早期设计的优点：
- Singleton `ClipboardController` + PIMPL 模式，符合项目风格
- 自定义 MIME 类型已在 `ControllerGlobal.h` 定义
- 主菜单 QAction 和快捷键已注册

早期设计的问题：
- `Note::serialize()` 返回空 `{}`，无法序列化
- `NotesParamsInfo` 无序列化方法，无参数数据
- `ClipController::copySelectedNotesWithParams()` 的 MIME data 从未被填充
- `paste()` 中反序列化代码全部注释
- 完全不支持剪辑级别的复制粘贴
- 菜单不感知当前面板（钢琴卷帘 vs 编排视图）

**结论：保留 ClipboardController 框架并重构，不新建控制器。**

## Architecture

```
MainMenuViewPrivate::onCopy/onCut/onPaste
    ↓ (根据 m_panelType 分发)
ClipboardController::copyNotes / copyClips / pasteNotes / pasteClips
    ↓
    ├─ ClipEditor  → ClipController (音符级别)
    └─ TracksEditor → TrackController (剪辑级别)
```

## Serialization Format

不使用 opendspx 格式，采用独立的 QJsonObject 序列化。粘贴时不复制 ID，由 `IdGenerator` 分配新 ID。

### Note JSON

```json
{
  "localStart": 480, "length": 480, "keyIndex": 60, "centShift": 0,
  "lyric": "la", "language": "cmn",
  "pronunciation": { "original": "la", "edited": "" },
  "pronCandidates": ["la"],
  "phonemes": { ... },
  "lineFeed": false,
  "workspace": { ... }
}
```

### Clip JSON

```json
{
  "clips": [
    {
      "type": "singing",
      "name": "...", "start": 1920, "length": 3840,
      "clipStart": 0, "clipLen": 3840, "gain": 0.0, "mute": false,
      "defaultLanguage": "cmn",
      "notes": [ ... ],
      "workspace": { ... }
    },
    {
      "type": "audio",
      "name": "...", "start": 0, "length": 9600,
      "clipStart": 0, "clipLen": 9600, "gain": 0.0, "mute": false,
      "path": "C:/audio/vocal.wav",
      "workspace": { ... }
    }
  ],
  "trackIndexOffsets": [0, 0]
}
```

## Copy/Cut/Paste Behavior

### Notes (Piano Roll)

| Operation | Behavior |
|-----------|----------|
| Copy | 序列化选中音符 → 存入系统剪贴板（MIME: `application/vnd.ds-editor-lite.notewithparams`） |
| Cut | Copy + 通过 `NoteActions` 删除选中音符（可撤销） |
| Paste (菜单栏 Ctrl+V) | 从剪贴板反序列化 → 将播放光标位置量化对齐网格 → 偏移到量化后位置 → 通过 `NoteActions::insertNotes()` 插入（可撤销） |
| Paste (右键菜单) | 从剪贴板反序列化 → 将**右键点击位置**量化对齐网格 → 偏移到量化后位置 → 通过 `NoteActions::insertNotes()` 插入（可撤销） |

**粘贴位置策略**：
- **菜单栏 Ctrl+V**：所有音符相对最早音符的 `localStart` 保留相对位置，最早音符对齐到播放光标位置**经 `snapNearest` 量化后**的本地位置。
- **右键菜单粘贴**：粘贴位置为右键点击处的 tick 位置（经 `snapNearest` 量化），而非播放光标位置。这更符合用户直觉——在哪里右键就粘贴在哪里。

量化使用当前钢琴卷帘的 quantize 设置，通过 `TimelineSnapUtils::snapNearest(tick, quantizeStep)` 对齐网格。

### Clips (Arrangement)

| Operation | Behavior |
|-----------|----------|
| Copy | 序列化选中剪辑 → 存入系统剪贴板（MIME: `application/vnd.ds-editor-lite.clip`） |
| Cut | Copy + 通过 `ClipActions::removeClips()` 删除（可撤销） |
| Paste | 从剪贴板反序列化 → 将播放光标位置量化对齐网格 → 偏移到量化后位置和当前选中轨道 → 通过 `ClipActions::insertClips()` 插入（可撤销） |

**粘贴位置策略**：最早剪辑对齐到播放光标位置**经量化后**的位置（使用编排视图的 quantize，通过 `TimelineSnapUtils::snapNearest()` 对齐），其余保持相对偏移。轨道方面，以 `appStatus->selectedTrackIndex` 为基准轨道。

## Key Design Decisions

1. **Note 位置使用 `localStart`（相对剪辑）序列化**，粘贴时按偏移计算
2. **不复制 ID**，粘贴的元素获得新 ID
3. **不复制推理结果**，粘贴的 SingingClip 需要重新推理
4. **AudioClip 浅拷贝**，只复制文件路径
5. **粘贴的 SingingClip 默认 `useTrackSingerInfo = true`**，继承目标轨道声线。不复制来源 Clip 的 singer/speaker 信息
6. **Cut = Copy + 可撤销的 Delete**
7. **使用系统剪贴板 (QClipboard)**，不用内部缓冲
8. **V1 不复制参数曲线**（pitch/energy 等），但音符本身的所有属性（音素、发音、workspace 等）完整复制。参数曲线需要按选中范围切片，复杂度高，作为后续增强
9. **右键菜单粘贴使用点击位置**，菜单栏粘贴使用播放光标位置

## Known Issues

- **歌手与语言不匹配**：粘贴 SingingClip 到其他轨道时，默认继承目标轨道声线。如果目标轨道歌手不支持源 Clip 中音符的语言（如源 Clip 音符使用日语但目标轨道歌手只支持中文），会导致推理失败或发音异常。目前不做处理，后续可考虑：粘贴时校验语言兼容性并警告用户、自动切换到目标轨道歌手支持的默认语言
9. **右键菜单粘贴使用点击位置**，菜单栏粘贴使用播放光标位置

## Menu Enable/Disable

重构 `enterClipEditorState()` / `exitClipEditorState()`，新增 `enterTracksEditorState()` / `exitTracksEditorState()`：

| Action | TracksEditor | ClipEditor |
|--------|-------------|------------|
| Cut/Copy | 有选中剪辑时启用 | 有选中音符时启用 |
| Paste | 剪贴板有剪辑数据时启用 | 剪贴板有音符数据时启用 |
| Delete | 有选中剪辑时启用 | 有选中音符时启用 |
| SelectAll | 轨道有剪辑时启用 | 剪辑有音符时启用 |

通过 `QClipboard::dataChanged()` 信号动态更新 Paste 按钮状态。

## Implementation Phases

### Phase 1: Serialization Foundation ✅ (commit e230ad31)
- ✅ 实现 `Note::serialize()` / `Note::deserialize()` — 保留 edited pronunciation、edited phoneme names；不保留 phoneme offsets
- ✅ 完善 `NotesParamsInfo` 的 JSON 序列化

### Phase 2: Note Clipboard ✅ (commit e230ad31)
- ✅ 修复 `ClipController::copySelectedNotesWithParams()` — 使用真实序列化
- ✅ 实现 `ClipController::pasteNotesWithParams()` — 创建新音符、`snapNearest` 量化粘贴位置、通过 NoteActions 插入（可撤销）
- ✅ 修复 `ClipboardController::paste()` — 反序列化并调用粘贴
- ✅ 连接 `MainMenuViewPrivate::onCopy/onCut/onPaste` 到 ClipEditor 分支
- ✅ 更新 `enterClipEditorState()` / `exitClipEditorState()` 管理 Cut/Copy/Paste 启用状态

### Phase 2.5: Piano Roll Context Menus for Clipboard ✅

**Context**: 钢琴卷帘缺少右键菜单的剪贴板操作。需要在两处添加：
1. 右键点击音符时的上下文菜单 — 添加"剪切"和"复制"
2. 音符编辑模式下右键点击背景空白处 — 新增上下文菜单，包含"粘贴"

三套菜单（主菜单栏、音符上下文菜单、背景上下文菜单）的 Cut/Copy/Paste 状态需要正确同步。

#### 已完成

- ✅ `contextMenuEvent` 扩展模式判断至 5 种模式（Select, IntervalSelect, DrawNote, EraseNote, SplitNote）
- ✅ `buildNoteContextMenu` 添加 Cut 和 Copy 菜单项（布局：Ctrl+X/C/V, 填词.../搜索歌词... --- 分割音符 --- 剪切/复制/删除 --- 属性）
- ✅ 新增 `buildBackgroundContextMenu` 方法（含 Paste，根据剪贴板数据启用/禁用）
- ✅ `buildBackgroundContextMenu(const QPoint &pos)` 接收右键位置，计算 tick 并量化，Paste 触发时调用 `clipController->pasteNotesWithParams(info, tick)`，粘贴到右键点击位置而非播放光标位置
- ✅ `PianoRollGraphicsView_p.h` 声明 `buildBackgroundContextMenu(const QPoint &pos)`
- ✅ 添加必要 includes（ClipboardController.h, ControllerGlobal.h, QGuiApplication, QClipboard, QMimeData, QJsonDocument, NotesParamsInfo.h）
- ✅ 构建通过，已提交（commits: `bb6cb350`, `6b53b6e0`）

#### 音符上下文菜单布局

```
填词...
搜索歌词...
---
分割音符
---
剪切
复制
删除
---
属性
```

#### 状态同步策略

上下文菜单是每次点击时动态创建的，因此直接在创建时查询当前状态即可，不需要信号连接：
- **音符菜单中的 Cut/Copy**: 始终启用（右键点击音符时该音符已被选中）
- **背景菜单中的 Paste**: 创建时检查 `QGuiApplication::clipboard()->mimeData()` 是否包含 `NoteWithParams` MIME 类型
- **主菜单栏**: 不变，继续使用信号驱动

### Phase 3: Clip Clipboard ✅
- ✅ 创建 `ClipsInfo` 数据模型及序列化（新文件 `src/app/Model/ClipboardDataModel/ClipsInfo.h/cpp`）
  - 序列化/反序列化公共属性（name, start, length, clipStart, clipLen, gain, mute, workspace）
  - SingingClip：额外序列化 defaultLanguage、notes（每个音符完整序列化）
  - AudioClip：额外序列化 path
  - trackIndexOffsets：记录多轨偏移
- ✅ 在 `TrackController` 中实现 `copySelectedClips()` / `cutSelectedClips()` / `pasteClips()`
  - 粘贴通过 `ClipActions::insertClips()` 执行，可撤销
  - 粘贴后 SingingClip 从目标轨道继承歌手/说话人信息（`useTrackSingerInfo` 默认 true）
- ✅ `AppStatus` 中添加 `selectedClips` 属性 + `clipSelectionChanged` 信号
- ✅ `TracksGraphicsView` 中添加 `syncClipSelectionToAppStatus()`，在选区变化时同步 `selectedClips` 和 `selectedTrackIndex`
- ✅ `ClipboardController` 填充 `Clip` case：`copyCutClips()` / `paste()` Clip 分支
- ✅ `ClipActions` 新增多轨 `insertClips(clips, tracks)` 重载
- ✅ `MainMenuView` 新增 `enterTracksEditorState()` / `exitTracksEditorState()`
- ✅ `onCopy/onCut/onPaste/onDelete` 添加 TracksEditor 分发
- ✅ 菜单状态：Cut/Copy/Delete 有选中剪辑时启用，Paste 始终可用

### Phase 4: Polish
- 边界情况处理（无活动剪辑/轨道时粘贴、粘贴到项目边界外等）
- ✅ `QClipboard::dataChanged` 动态更新 Paste 按钮状态
  - `MainMenuView` 构造函数中连接 `QClipboard::dataChanged`
  - `updatePasteActionState()` 根据当前面板类型检查对应 MIME 类型是否可用
- ✅ 编排视图右键菜单添加 Cut/Copy/Paste 项
  - 空白处：Paste（使用右键位置作为目标轨道和 tick，量化对齐网格）
  - 剪辑上：Cut / Copy（操作当前选区）
  - 同时清理了不再使用的 `m_backgroundMenu` 成员
- **粘贴预览**：右键菜单 Paste 获得焦点（鼠标悬停/键盘导航）时，在钢琴卷帘上显示半透明"幽灵音符"预览粘贴位置和内容。通过 `QAction::hovered` 信号驱动，临时创建低 opacity 的 NoteView，菜单关闭时清理。预估 ~30-50 行代码，不涉及架构改动

## Critical Files

**修改：**
- `src/app/Model/AppModel/Note.h/cpp` — 实现 serialize/deserialize
- `src/app/Controller/ClipboardController.h/cpp/_p.h` — 面板感知分发，实现双路径
- `src/app/Controller/ClipController.h/cpp` — 修复 copySelectedNotesWithParams, 实现 pasteNotesWithParams
- `src/app/Controller/TrackController.h/cpp` — 新增 copySelectedClips, pasteClips
- `src/app/UI/Views/MainTitleBar/MainMenuView.cpp/_p.h` — 实现 onCut/onCopy/onPaste，重构状态管理
- `src/app/UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.cpp/_p.h` — 右键菜单
- `src/app/Model/ClipboardDataModel/NotesParamsInfo.h/cpp` — 完善序列化
- `src/app/Model/AppStatus/AppStatus.h` — 可能新增 selectedClips

**新建：**
- `src/app/Model/ClipboardDataModel/ClipsInfo.h/cpp` — 剪辑剪贴板数据模型

## Verification

1. **音符剪贴板（菜单栏）**：选中若干音符 → Ctrl+C → 移动播放光标 → Ctrl+V → 确认音符粘贴到播放光标量化位置 → Ctrl+Z 撤销 → 确认音符消失
2. **音符剪贴板（右键）**：选中若干音符 → 右键复制 → 在空白处右键粘贴 → 确认音符粘贴到**右键点击位置**（量化后）而非播放光标位置
3. **音符剪切**：选中若干音符 → Ctrl+X → 确认音符消失 → Ctrl+V → 确认粘贴成功 → Ctrl+Z 撤销粘贴 → Ctrl+Z 撤销剪切 → 确认原始音符恢复
4. **剪辑剪贴板**：选中一个/多个剪辑 → Ctrl+C → 选择目标轨道 → 移动播放光标 → Ctrl+V → 确认剪辑粘贴到正确轨道和位置
5. **AudioClip 粘贴**：复制音频剪辑 → 粘贴 → 确认波形正常显示
6. **菜单状态**：无选中时 Cut/Copy 灰色 → 选中后启用 → 剪贴板有数据时 Paste 启用
