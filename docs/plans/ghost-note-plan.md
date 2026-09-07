# 钢琴卷帘 Ghost Note 实现方案（显示其他轨道音符）

> 状态：📝 方案待评审，尚未实施。2026-09-08（v2：补齐 RHI 后端）

## Goal

歌声剪辑编辑器（钢琴卷帘）目前只渲染**当前 clip** 内的音符。编曲时看不到其他轨道在同一时间段的音符，跨轨道对齐（和声、对位、避免撞音）只能靠反复切换 clip 判断。

在钢琴卷帘中以**矮矩形（音符全高的 20%）**绘制其他轨道上的音符作为 ghost note，仅供参考、不可交互。在「设置 → 外观」提供开关，默认开启。

**两套渲染后端（Legacy QGraphicsScene / RHI 实验性）都要实现**，视觉表现一致。

## 设计决策

| 项 | 决策 |
|----|------|
| 渲染后端 | Legacy（`PianoRollGraphicsView`）与 RHI（`PianoRollRhiWidget`）**均实现** |
| 数据来源 | 抽出共享的 `GhostNoteSource`（QObject），两后端各持一份实例，负责遍历模型 + 监听变更 + 增量重建 |
| Legacy 承载方式 | **单个跟随视口的 overlay item**（`GhostNoteOverlay`），而非每个 ghost 一个 QGraphicsItem |
| RHI 承载方式 | 在 `rebuildSnapshot()` 里新增一批 `appendGhostNotes()` 矩形，插在 `appendTimeline()` 与 `appendNotes()` 之间 |
| 基类（Legacy） | `TimeOverlayView`（已提供 `startTick()`/`endTick()`/`tickToItemX()` 且自动跟随视口） |
| 范围 | 其他**轨道**上的 singing clip，同轨道的其他 clip 不画 |
| 配色 | 按各自轨道 `colorIndex` 取 `AppColorPalette::noteBackground(ci)` 并降 alpha，不引入新主题 token |
| 位置 | 矮条在所在琴键行内**垂直居中** |
| 交互 | Legacy `setTransparentMouseEvents(true)` 完全穿透。RHI 侧本就只是绘制批次，命中测试只认 `clip->notes()`。不画歌词/发音 |
| z 序 | Legacy `-0.5`（时间网格 `-1` 之上、音符 `0` 之下）。RHI 靠绘制顺序（画家算法） |
| 选项 | `AppearanceOption::showGhostNotes`，默认 `true`，热生效无需重启 |

**为什么不逐音符建 item（Legacy）**：工程里其他轨道音符可能上万，逐个建 `QGraphicsItem` 会让场景膨胀、拖垮命中测试与布局。overlay 只在 `paint()` 中绘制可见 tick 区间内的矮条 —— 与 `ClipRangeOverlay`、`PianoRollBackground` 同模式。

**为什么不复用 `NoteView`**：`NoteView` 的颜色来自静态成员 `s_trackColorIndex`（`NoteView.cpp:20`），全局唯一，无法为不同轨道的 ghost 分别着色。

**为什么抽 `GhostNoteSource`**：「遍历所有轨道 → 过滤 → 换算 → 排序」以及「监听 track/clip/note/option 变更并重连信号」这套逻辑两后端完全相同，且是本特性里最容易写错的部分（悬空连接）。共享一份，两个后端只各自负责绘制。

## 现状（排查结论）

### 渲染后端有两套

`PianoRollView.cpp:36-44` 按开发者选项 `DeveloperOption::EditorRenderBackend::RhiExperimental` 二选一：

| 后端 | 类 | 状态 |
|----|----|----|
| Legacy | `PianoRollGraphicsView` + `NoteView` items | **默认** |
| RHI | `PianoRollRhiWidget`（3500+ 行） | 实验性 |

两者互斥创建（`m_graphicsView` / `m_rhiView` 只有一个非空），所以两份 `GhostNoteSource` 实例不会同时存在。

### 坐标系

两后端的场景 X 都以**当前 clip 起点**为原点：

- Legacy：`PianoRollGraphicsView_p.h` 的 `m_offset = clip->start()`（`PianoRollGraphicsView.cpp:1429,1668`），`NoteView` 用 `rStart`（clip 内相对 tick）定位（`NoteView.cpp:257-270`）
- RHI：`viewport.tickToSceneX(note->localStart())`（`PianoRollRhiWidget.cpp:2237`），`startTick()` 才加回 `clip->start()`

因此 `GhostNoteSource` 统一产出 **`globalStart`（绝对 tick）**，由各后端自行减去 `clip->start()`。相对值**可为负**（其他轨道音符早于当前 clip 起点）。

### 已有 ghost 先例

- Legacy：`PianoRollGraphicsView::showPianoRollPastePreview()`（`PianoRollGraphicsView.cpp:311-338`）用 `setOpacity(0.35)` + `setAcceptedMouseButtons(Qt::NoButton)` 做粘贴预览
- RHI：`Private::appendPastePreview()`（`PianoRollRhiWidget.cpp:2291-2340`）对一组颜色统一 `applyOpacity(0.35)` 后走同样的矩形批次

视觉弱化的思路可直接沿用。ghost 用 0.45 的 alpha 系数与矮条形状区分于粘贴预览。

### 跨轨道监听先例

`TracksRhiWidget::rebuildModelConnections()`（`TracksRhiWidget.cpp:886-915`）已经在做「遍历 tracks/clips，先 `disconnect(obj, nullptr, this, nullptr)` 再重连」，`GhostNoteSource` 照抄这个模式即可，天然避免悬空连接。

### 选项写入路径有四层

设置页**不直接写** `appOptions`，而是走 Automation facade。任何新字段必须同时加进 DTO 和 adapter，否则 `restoreAppearance()` 会把它覆盖回默认值。

| 层 | 文件 |
|----|----|
| 字段 | `Model/AppOptions/Options/AppearanceOption.{h,cpp}` |
| DTO | `Automation/SettingsAutomationFacade.h:29-38`（`AppearanceSettingsDto`） |
| 映射 | `Automation/AppOptionsAutomationAdapter.cpp:66-86`（`captureAppearance`/`restoreAppearance`） |
| UI | `UI/Dialogs/Options/Pages/AppearancePage.{h,cpp}` |

变更通知只有单一全局信号 `AppOptions::optionsChanged(AppOptionsGlobal::Option)`，消费方自行按枚举过滤。

### 主题切换

两后端的颜色都在绘制时向 `AppColorPalette::instance()` 取值（`NoteView.cpp:157`、`PianoRollRhiWidget.cpp:2207`），主题切换后 QSS 重新 polish 触发重绘/重建快照，ghost 颜色自动跟随，**无需额外接线**。

## 实现

### 1. 新增选项 `showGhostNotes`

- `AppearanceOption.h` — 加 `bool showGhostNotes = true;` 与 `const QString showGhostNotesKey = "showGhostNotes";`
  （该文件是手写 key + load/save 风格，不用 `LITE_OPTION_ITEM` 宏，保持一致）
- `AppearanceOption.cpp` — `load()` 加 `if (object.contains(showGhostNotesKey)) showGhostNotes = object.value(showGhostNotesKey).toBool();`，`save()` 加一行 `insert`
- `SettingsAutomationFacade.h` — `AppearanceSettingsDto` 加 `bool showGhostNotes = true;`
  （`operator==` 是 `= default`，自动参与变更检测）
- `AppOptionsAutomationAdapter.cpp` — `captureAppearance()` / `restoreAppearance()` 各加一行

内部选项，无需改 `PublicAutomationRegistry` / MCP schema（当前仅 `appearance.themeId` 对外暴露）。

### 2. 设置页开关

`AppearancePage.{h,cpp}`：新增成员 `SwitchButton *m_swShowGhostNotes;`，照 `m_swAnimationEnabled`（`AppearancePage.cpp:135-136`）三步写法：

1. `createContentWidget()` 里 `new SwitchButton(option->showGhostNotes)`
2. `connect(..., &SwitchButton::toggled, this, &AppearancePage::modifyOption)`
3. `modifyOption()` 里 `settings.showGhostNotes = m_swShowGhostNotes->value();`

新建 `OptionListCard(tr("Piano Roll"))`，用 `addItem(title, description, control)` 三参重载：

- 标题 `tr("Show notes from other tracks")`
- 副标题 `tr("Displayed as thin bars for reference only")`

加进 `mainLayout`（放在 animation card 之后）。

> `IOptionPage.cpp:38-51` 在 `LanguageChange` 时整页重建，所以指针必须在 `createContentWidget()` 创建、在 `modifyOption()` 无条件读取。热生效，**不要**用 `RestartDialog`。

翻译补到 `Resources/translate/translation_zh_CN.ts` 的 `AppearancePage` context（约 483 行起），或跑 `update_translations` 目标生成。

### 3. 新建共享数据源 `GhostNoteSource`

`src/app/UI/Views/ClipEditor/PianoRoll/GhostNoteSource.{h,cpp}`
（新文件**无需改 CMake**，`src/app/CMakeLists.txt:4-7` 用 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)`）

```cpp
struct GhostNote {
    int globalStart = 0;   // 绝对 tick
    int length = 0;
    int keyIndex = 60;
    int colorIndex = 0;    // 源轨道 colorIndex
};

namespace GhostNoteStyle {
    inline constexpr double heightRatio = 0.2;  // 占琴键行高比例
    inline constexpr double minHeight = 2.0;    // 逻辑像素下限
    inline constexpr double opacity = 0.45;     // 相对 noteBackground 的 alpha 系数
    QColor fillColor(int colorIndex);           // noteBackground(ci) 降 alpha
}

class GhostNoteSource : public QObject {
    Q_OBJECT
public:
    explicit GhostNoteSource(QObject *parent = nullptr);
    void setHostClip(SingingClip *clip);            // nullptr = 清空
    [[nodiscard]] bool enabled() const;             // 选项 && 有 host clip
    [[nodiscard]] const QList<GhostNote> &notes() const;  // 按 globalStart 升序
signals:
    void changed();
};
```

行为：

- 构造时连 `AppOptions::optionsChanged`，过滤条件写成
  `option == AppOptionsGlobal::Appearance || option == AppOptionsGlobal::All`
  （`AppOptions` 构造时会发 `All`，参照 `TracksRhiWidget.cpp:220-226`）
- 连 `AppModel::modelChanged`（换工程）、`AppModel::trackChanged`（增删轨道）
- `rebuildConnections()` 照 `TracksRhiWidget::rebuildModelConnections()`：遍历 `appModel->tracks()`，每个 track 先 `disconnect(track, nullptr, this, nullptr)` 再连 `Track::propertyChanged`（换色）、`Track::clipChanged`（增删 clip，回调里重连一次）。每个 singing clip 先 disconnect 再连 `Clip::propertyChanged`（拖动改 `start()`）与 `SingingClip::noteChanged`
- 关闭开关时不建立模型连接（只保留 option 连接），零开销
- `rebuild()` 逻辑：
  1. 选项关 或 无 host clip → 清空、`emit changed()`、返回
  2. `appModel->findClipById(hostClip->id(), trackRef)` 取宿主轨道
  3. 遍历 `appModel->tracks()`，跳过宿主轨道（宿主轨道解析失败时退化为只跳过 host clip 本身），对每个 `Track::clips()` 中 `clipType() == Clip::Singing` 的 clip 遍历 `notes()`，产出 `{ note->globalStart(), note->length(), note->keyIndex(), track->colorIndex() }`
  4. 按 `globalStart` 排序，`emit changed()`
- 所有触发都走 `scheduleRebuild()`：置标志 + `QTimer::singleShot(0, ...)`，合并同一事件循环内的多次触发（与 RHI 的 `scheduleSnapshot()` 同模式）

### 4. Legacy：新建 `GhostNoteOverlay`

`src/app/UI/Views/ClipEditor/PianoRoll/GhostNoteOverlay.{h,cpp}`，继承 `TimeOverlayView`。

接口：

```cpp
void setGhostNotes(const QList<GhostNote> *notes);  // 只持指针，源存活期由 view 保证
void setOffset(int offset);                         // = clip->start()
```

`paint()`：

- `painter->setPen(Qt::NoPen)`，关闭抗锯齿（与 `NoteView::drawRectOnly()` 一致）
- 行高 `h = noteHeight * scaleY()`，条高 `barH = std::max(GhostNoteStyle::minHeight, h * GhostNoteStyle::heightRatio)`
- Y：`sceneY = (127 - keyIndex) * h`，再 `sceneYToItemY(sceneY) + (h - barH) / 2`（行内居中，照 `PianoRollBackground.cpp:31-33` 的映射写法）
- X：`tickToItemX(globalStart - m_offset)`，宽度 `tickToItemX(...+length) - x`，最小 1px
- 颜色 `GhostNoteStyle::fillColor(colorIndex)`
- `painter->fillRect(...)`

`updateRectAndPos()` 照抄 `ClipRangeOverlay.cpp:28-33`。

**性能**：列表已按 `globalStart` 升序，`paint()` 中用 `std::lower_bound` 跳到 `startTick()` 之前一段（因为音符有长度，需从 `startTick() - maxLength` 起扫，实现上取「首个 `globalStart >= startTick() + offset` 的位置再回退」不安全 —— 改为记录源列表中的 `maxLength`，用 `lower_bound(globalStart >= visibleStartGlobal - maxLength)` 作为起点），遇到 `globalStart > visibleEndGlobal` 即 break。

### 5. Legacy：接入 `PianoRollGraphicsView`

`PianoRollGraphicsView_p.h` 加成员：

```cpp
GhostNoteOverlay *m_ghostOverlay = nullptr;
GhostNoteSource *m_ghostSource = nullptr;
```

构造函数中（紧邻 `m_clipRangeOverlay` 创建处，`PianoRollGraphicsView.cpp:122-124`）：

```cpp
d->m_ghostSource = new GhostNoteSource(d);
d->m_ghostOverlay = new GhostNoteOverlay;
d->m_ghostOverlay->setZValue(-0.5);   // 网格 -1 之上、音符 0 之下
d->m_ghostOverlay->setGhostNotes(&d->m_ghostSource->notes());
scene->addCommonItem(d->m_ghostOverlay);
d->m_ghostOverlay->setTransparentMouseEvents(true);
connect(d->m_ghostSource, &GhostNoteSource::changed, d->m_ghostOverlay, [d] {
    d->m_ghostOverlay->setVisible(d->m_ghostSource->enabled());
    d->m_ghostOverlay->update();
});
```

接入点：

| 位置 | 动作 |
|----|----|
| `moveToSingingClipState()`（`:1414`） | `m_ghostSource->setHostClip(clip); m_ghostOverlay->setOffset(m_offset);` |
| `moveToNullClipState()`（`:1394`） | `m_ghostSource->setHostClip(nullptr);` |
| `onClipPropertyChanged()`（`:1665`） | `m_ghostOverlay->setOffset(m_offset);`（当前 clip 被拖动） |

`setGhostNotes` 传的是 `GhostNoteSource` 内部列表的地址。`m_ghostSource` 以 `d` 为 parent，生命周期覆盖 overlay 的使用期（overlay 由 scene 拥有，随 view 销毁）。

### 6. RHI：接入 `PianoRollRhiWidget`

`Private` 新增成员：

```cpp
GhostNoteSource ghostSource;
```

构造函数（`PianoRollRhiWidget.cpp:212` 起的 `Private(PianoRollRhiWidget *q)`）里连：

```cpp
QObject::connect(&ghostSource, &GhostNoteSource::changed, q, [this] { scheduleSnapshot(); });
```

`Private::setDataContext()`（`:305`）末尾加 `ghostSource.setHostClip(newClip);`
（`clip` 的 `propertyChanged` 已经在同函数里连了 `scheduleSnapshot()`，`clip->start()` 变化时 ghost 的 local 换算随快照重建自然更新，无需额外接线。）

`rebuildSnapshot()`（`:1954`）在 `appendTimeline(...)` 与 `appendNotes(...)` 之间插入：

```cpp
appendGhostNotes(localStart, localEnd);
```

新增私有方法：

```cpp
void appendGhostNotes(const double localStart, const double localEnd) {
    const auto &ghosts = ghostSource.notes();
    if (ghosts.isEmpty())
        return;
    const auto rowHeight = noteHeight * verticalScale();
    const auto barHeight = std::max(GhostNoteStyle::minHeight,
                                    rowHeight * GhostNoteStyle::heightRatio);
    const auto offset = clip->start();
    for (const auto &ghost : ghosts) {
        const auto start = ghost.globalStart - offset;
        if (start > localEnd)
            break;                       // 已按 globalStart 升序
        if (start + ghost.length < localStart)
            continue;
        const auto top = viewport.unitToSceneY(127 - ghost.keyIndex)
                         + (rowHeight - barHeight) * 0.5;
        appendLogicalRect(QRectF(viewport.tickToSceneX(start), top,
                                 std::max(1.0, ghost.length * pixelsPerTick()), barHeight),
                          GhostNoteStyle::fillColor(ghost.colorIndex));
    }
}
```

与 Legacy 共用 `GhostNoteStyle` 常量与配色函数，保证两后端视觉一致。RHI 侧同样可以先 `std::lower_bound` 定位起点，但由于快照重建本就 O(可见音符)，且这里只是一次线性扫描 + 提前 break，先按上面的简单写法实现，若实测大工程掉帧再补二分。

## Verification

1. 构建：优先加载 `cmake-build` skill 走 CLion（`clion_execute_run_configuration`，`configurationName="DsEditorLite"`），回退 `cmake --preset debug && cmake --build --preset debug`
2. 建工程 → 建 2~3 条轨道，各放 singing clip 并写入不同音高、时间上重叠的音符
3. **两个后端各测一遍**（开发者选项切换 `editorRenderBackend`：Legacy / RhiExperimental）：
   - 默认即为开启，进入某个 clip 的钢琴卷帘
   - 其他轨道音符显示为矮条，高度约为琴键行高的 20%，颜色随各自轨道
   - 设置 → 外观里关闭开关后矮条消失、重新打开后恢复，均无需重启
   - 两后端的矮条位置/高度/配色一致
4. 交互确认：矮条**不响应**点击、框选、右键，鼠标穿透到网格
5. 边界场景（两后端）：
   - 拖动当前 clip 改变 `start()` 后，ghost 相对位置正确
   - 其他轨道增删音符 / 增删 clip / 增删轨道 → ghost 实时更新
   - 修改其他轨道颜色 → ghost 颜色跟随
   - 深浅主题切换下均可辨识（颜色来自 `AppColorPalette`，随主题走）
   - 数千音符时横向滚动、缩放无明显掉帧
   - 切到无 clip / audio clip 状态、关闭工程（`modelChanged`）不崩溃、不残留
6. 回归测试：
   - `TestThemeColors`、`TestThemeIcons` 直接构造 `AppearanceOption` —— 加带默认值的 bool 源码兼容
   - `TestAutomationL3ApplicationDomains` 会 round-trip `SettingsSnapshotDto::appearance` —— capture/restore 两侧都加了字段才能通过，务必跑一遍
   - `TestPianoRollInteractions`、`TestPianoRollNoteCommit` 涉及钢琴卷帘构造，确认新成员不影响

## 明确不做

- 同轨道其他 clip 的音符不画。
- ghost 上不绘制歌词 / 发音文本。
- ghost 不参与命中测试、框选、自动翻页、历史聚焦。
