# Clipboard System — 设计文档

> 状态：✅ 实施完成（Phase 1 序列化、Phase 2 音符剪贴板、Phase 2.5 钢琴卷帘右键菜单、Phase 3 剪辑剪贴板均已提交；Phase 4 Polish 为后续增强）
> 本文是最终设计说明，实施阶段记录已移除。

## Context

系统剪贴板覆盖**音符**（钢琴卷帘）和**剪辑**（编排视图）两级。早期存在 `ClipboardController` 实现框架（Singleton + PIMPL，符合项目风格；MIME 类型已定义；菜单 QAction 和快捷键已注册），但序列化、粘贴逻辑全部缺失。最终结论：**保留 ClipboardController 框架并重构**，不新建控制器。

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

不使用 opendspx 格式，采用独立 QJsonObject 序列化。粘贴时不复制 ID，由 `IdGenerator` 分配新 ID。

### Note JSON（MIME: `application/vnd.ds-editor-lite.notewithparams`）

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

### Clip JSON（MIME: `application/vnd.ds-editor-lite.clip`）

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

## Copy / Cut / Paste Behavior

### Notes (Piano Roll)

| Operation | Behavior |
|-----------|----------|
| Copy | 序列化选中音符 → 系统剪贴板 |
| Cut | Copy + `NoteActions` 删除选中音符（可撤销） |
| Paste (菜单栏 Ctrl+V) | 反序列化 → 播放光标位置量化对齐网格 → 偏移 → `NoteActions::insertNotes()`（可撤销） |
| Paste (右键菜单) | 反序列化 → **右键点击位置**量化对齐网格 → 偏移 → 插入（可撤销） |

**粘贴位置策略**：所有音符相对最早音符的 `localStart` 保留相对位置；菜单栏粘贴时最早音符对齐到播放光标位置经 `snapNearest` 量化后的本地位置；右键粘贴时对齐到点击处 tick（经量化）。量化使用当前钢琴卷帘的 quantize 设置，通过 `TimelineSnapUtils::snapNearest(tick, quantizeStep)` 对齐。

### Clips (Arrangement)

| Operation | Behavior |
|-----------|----------|
| Copy | 序列化选中剪辑 → 系统剪贴板 |
| Cut | Copy + `ClipActions::removeClips()`（可撤销） |
| Paste | 反序列化 → 播放光标量化位置 + 当前选中轨道 → `ClipActions::insertClips()`（可撤销） |

**粘贴位置策略**：最早剪辑对齐到播放光标位置（编排视图 quantize，`TimelineSnapUtils::snapNearest()`），其余保持相对偏移；轨道以 `trackIndexOffsets` 相对当前选中轨道。

## Key Design Decisions

1. Note 位置用 `localStart`（相对剪辑）序列化，粘贴时按偏移计算。
2. **不复制 ID**，粘贴的元素获得新 ID。
3. **不复制推理结果**，粘贴的 SingingClip 需要重新推理。
4. **AudioClip 浅拷贝**，只复制文件路径。
5. 粘贴的 SingingClip 默认 `useTrackSingerInfo = true`，继承目标轨道声线，不复制来源 singer/speaker。
6. **Cut = Copy + 可撤销的 Delete**。
7. 使用系统剪贴板（`QClipboard`），不用内部缓冲。
8. **V1 不复制参数曲线**（pitch/energy 等）；音符本身所有属性（音素、发音、workspace）完整复制。参数曲线需按选中范围切片，复杂度高，作为后续增强（Phase 4）。
9. 右键菜单粘贴用点击位置，菜单栏粘贴用播放光标位置。

## Known Issues

- **歌手与语言不匹配**：粘贴 SingingClip 到其他轨道时默认继承目标轨道声线。若目标轨道歌手不支持源 Clip 音符的语言（如源日语、目标仅支持中文），会导致推理失败或发音异常。当前为已知限制，未做自动语言迁移。

## Menu Enable/Disable

通过 `enterClipEditorState()` / `enterTracksEditorState()` 区分面板：

| Action | TracksEditor | ClipEditor |
|--------|-------------|------------|
| Cut/Copy | 有选中剪辑时启用 | 有选中音符时启用 |
| Paste | 剪贴板有剪辑数据时启用 | 剪贴板有音符数据时启用 |
| Delete | 有选中剪辑时启用 | 有选中音符时启用 |
| SelectAll | 轨道有剪辑时启用 | 剪辑有音符时启用 |

Paste 状态通过 `QClipboard::dataChanged()` 信号动态更新。

## 关键文件

- `src/app/Controller/ClipboardController.cpp/.h`（入口分发）
- `src/app/Model/ClipboardDataModel/NotesParamsInfo.h/.cpp`、`ClipsInfo.h/.cpp`（序列化模型）
- `src/app/Controller/TrackController.cpp`（clip 粘贴）、`src/app/Controller/ClipController.cpp`（note 粘贴）
- `src/app/UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.cpp`（右键菜单）
- `src/app/UI/Views/MainTitleBar/MainMenuViewPrivate`（菜单分发）
