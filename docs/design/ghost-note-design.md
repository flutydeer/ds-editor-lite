# 钢琴卷帘 Ghost Note（显示其他轨道音符）— 设计契约

> 状态：✅ 实施完成（`40a8eb29` 初版、`bacbd4f7` 视觉修正、`700143e9` 无效重建治理、`afdf79b7` RHI 保留图层与主题刷新修复）

## 行为约定

- 钢琴卷帘在渲染当前 clip 之外，以**矮条**（琴键行高的 20%，最小 2 逻辑像素）绘制**其他轨道**全部 singing clip 的音符，仅供参考、完全不可交互（鼠标/触摸穿透，无命中测试、无歌词发音文本）。
- 开关在「设置 → 外观 → 钢琴卷帘 → 显示其他轨道的音符」（`AppearanceOption::showGhostNotes`，默认开启），热生效无需重启。
- 同轨道的其他 clip 不画；宿主轨道整体排除（宿主轨道解析失败时退化为只排除宿主 clip 本身）。
- 矮条在琴键行内垂直居中，横向内缩普通音符边框半宽——同音高首尾相接的音符保持与普通音符一致的间隙，不粘连（`GhostNoteStyle::barRect`，过窄时退化为 `noteMinimumVisualWidth` 而不是不画）。
- 颜色 = 源轨道 `colorIndex` 的 `AppColorPalette::noteBackground` 乘 alpha 0.45（`GhostNoteStyle::fillColor`），不引入新主题 token。
- z 序：Legacy `GhostNoteOverlay` 取 `-0.5`（时间网格 `-1` 之上、音符 `0` 之下）；RHI 靠顶点顺序（插在时间线之后、音符之前，画家算法）。
- 主题热切换后矮条颜色必须立即跟随（见下文「主题与颜色」——RHI 侧需要显式接线，非免费行为）。

## 关键实现

| 部分 | 文件 |
|----|----|
| 共享数据源 + 样式 | `src/app/UI/Views/ClipEditor/PianoRoll/GhostNoteSource.{h,cpp}` |
| Legacy 渲染 | `src/app/UI/Views/ClipEditor/PianoRoll/GhostNoteOverlay.{h,cpp}` |
| RHI 渲染 | `src/app/UI/Views/ClipEditor/PianoRoll/GhostNoteLayer.{h,cpp}` + `PianoRollRhiWidget.cpp` 的 `rebuildSnapshot()` |
| 选项四层 | `AppearanceOption.{h,cpp}` / `SettingsAutomationFacade.h` / `AppOptionsAutomationAdapter.cpp` / `AppearancePage.{h,cpp}` |
| 接入 | `PianoRollGraphicsView.cpp`（Legacy）、`PianoRollRhiWidget.cpp` `Private`（RHI） |

新文件无需改 CMake（`src/app/CMakeLists.txt` 用 `GLOB_RECURSE CONFIGURE_DEPENDS`）。

## GhostNoteSource 契约

两后端各持一份实例（`PianoRollView` 按开发者选项二选一创建后端，两份不会同时存活）。统一产出**绝对 tick**（`globalStart`，可为负），由渲染端减去 `clip->start()`。

- 数据结构：`QList<GhostNote>{globalStart, length, keyIndex, colorIndex}`，按 `globalStart` 升序维护；`maxLength()` 供渲染端用 `lower_bound` 定位可见扫描起点（音符有长度，需从 `可见起点 - maxLength` 起扫）。
- 触发源：`AppOptions::optionsChanged`（按枚举过滤 Appearance/All）、`AppModel::modelChanged`/`trackChanged`、以及逐对象连接的 `Track::propertyChanged`/`Track::clipChanged`、`Clip::propertyChanged`、`SingingClip::noteChanged`。
- **变更类型过滤**：`noteChanged` 只对 `Insert`/`Remove`/`TimeKeyPropertyChange` 调度重建——歌词、拼音、音素变更不影响矮条几何，忽略（否则每次打歌词都全量重建 + 整帧重绘）。
- **宿主排除**：宿主 clip 既不入列表也不建连接（其编辑与 ghost 无关）；宿主轨道整体跳过连接。
- **连接维护为增量式**（与最初"整体 disconnect 再重连"的草案不同）：以 `m_connectedTracks`/`m_connectedClips` 记账，新增对象才 connect；用 `destroyed()` 证人同步移除记账（账本永不持悬空指针，disconnect 只发生在存活对象上）。移出模型的 clip 不主动断连——Qt 在对象析构时自动断连，残留连接只是无害的多余触发。
- **无 diff 不通知**：重建写入临时列表，排序后与现列表逐元素比较（`GhostNote` 有 defaulted `operator==`），连同 `enabled`/`maxLength` 全部未变则不发 `changed()`——模型信号可以因"与 ghost 无关的编辑"触发重建，此时必须保持安静。
- 所有触发经 `scheduleRebuild()`（标志 + `QTimer::singleShot(0)`）合并；开关关闭时 `clearConnections()` 停止监听模型。

## Legacy 渲染契约（GhostNoteOverlay）

单视口跟随 `TimeOverlayView`（`setTransparentMouseEvents(true)`），**不做**逐音符 item——工程可含上万外部音符，逐个建 item 会拖垮场景与命中测试。`updateRectAndPos()` 随可见区变化移动并 `update()`（legacy 滚动本就整视口重绘，overlay 的边际成本只有绘制循环）。

- `paint()` 以 `lower_bound(visibleStart - maxLength)` 起扫、`globalStart > visibleEnd` 即 break。
- tick→item 与 scene-y→item 均为仿射：paint 开头取一次 `itemXPerTick()`（`TimeOverlayView` 新增的受保护访问器）与两个原点系数，循环内每条矮条只做乘加，不做逐音符 `mapFromScene`。
- 颜色经 `GhostNoteStyle::ColorTable` 帧内查表（构造时遍历一次 `AppColorPalette`，约 16 项），不做跨帧缓存——`AppColorPalette` 非 QObject、无变更信号，帧内构造让主题切换自动生效。

## RHI 渲染契约（GhostNoteLayer）

`GhostNoteLayer` 是**保留式顶点缓存**，仅 RHI 使用。矮条顶点烘在场景坐标系（`x = localTick * pixelsPerTick + tickToSceneX(0)`，`y = (127 - keyIndex) * rowHeight`），相机偏移由投影矩阵处理——**纯滚动（水平/垂直）不使几何失效**。

- 失效条件：`markDirty()`（ghost 列表变化、主题切换、宿主切换）、`pixelsPerTick`/`rowHeight`/`clipStart`/`tickToSceneX(0)`/DPR 变化、启用态翻转、可视范围超出缓存窗口。
- 窗口策略：发射可视范围 ±0.25 视口（水平与垂直同）。**余量必须保持小**：每帧都整窗 splice 并上传，宽余量（±1 视口实测）会让顶点量/上传/编码全线涨约 2.5 倍，抵消收益。滚动每越界一次重发一次，摊销成本可忽略。
- `rebuildSnapshot()` 在 `appendTimeline` 与 `appendNotes` 之间 `vertices.append(ghostLayer.ensureUpToDate(...))`，绘制顺序保证矮条压在音符下。
- `Private` 中 `GhostNoteSource::changed → ghostLayer.markDirty() + scheduleSnapshot()`。

## 主题与颜色

- `AppColorPalette` 自订阅 `ThemeManager::themeChanged` 拉新调色板；矮条颜色链路 = palette → 帧内 ColorTable → 顶点，legacy 每次重绘、RHI 每次重建时自动取到当前主题。
- **RHI 画布控件必须自连 `ThemeManager::themeChanged → scheduleSnapshot()`**（`PianoRollRhiWidget` 与 `TracksRhiWidget` 均已接）：themeChanged 触发的 QSS repolish 只会让 `QRhiWidget` 重绘——即 blit 上一次的场景纹理——**不会**触发快照重建。缺这条接线时主题热切换后整个画布（不只 ghost）保持旧配色直到下一次交互。这是实测发现并修复的缺陷（`afdf79b7`），新增 RHI 画布控件时必须照做。
- 已知遗留：`PianoRollRhiWidget::whiteKeyColor` 等 Q_PROPERTY 颜色 setter 全仓库无调用方，键位行底色实为写死默认值，与本特性无关，待单独处理。

## 性能实测（16 轨 × 2000 音符合成工程，Release）

- RHI：整帧 CPU 重细分基线即 ~0.35-0.9ms（60fps 满帧）；保留图层复测与基线持平——其价值在"视口不动、只有交互态变化"的帧（拖拽增量、编辑）与低端设备，不在开发机的纯滚动数字。
- Legacy：滚动 Paint ≈ 30-40% CPU（Debug 约 70%），大头是 `NoteView` 逐音符绘制与网格，ghost 占比小；本特性的 legacy 收益主要在编辑触发链的消除（见 GhostNoteSource 契约的无 diff 与类型过滤）。
- 加载大工程会触发 G2P 推理爆发（无歌手也跑），期间 UI 冻结；做渲染性能测量需避开启动后约 1 分钟。

## 明确不做

- 同轨道其他 clip 的音符不画。
- ghost 上不绘制歌词 / 发音文本。
- ghost 不参与命中测试、框选、自动翻页、历史聚焦。
- 不为 ghost 引入新主题 token（颜色 = 轨道色降 alpha）。
