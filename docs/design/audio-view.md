# 音频查看视图（对标钢琴卷帘）— 设计契约

> 状态：✅ 实施完成（分支 `feat/audio-clip-view`）

## 背景

底部「编辑」页原先只服务 singing clip。选中音频 clip 时 `ClipEditorView::moveToAudioClipState` 只是把钢琴卷帘藏起来、显示占位文案，而 `hasActiveClip` 的取值让工具栏一并消失，于是**剪辑连重命名都做不到**。

同时 `EditorAutomationFacade::setActiveClip` 用 `objectResolver.singingClip(...)` 解析目标，音频 clip 一律解析失败，`appStatus->activeClipId` 永远停在 -1，选中音频时整条链路（工具栏、底部面板页、视图状态）都不会被激活。这是"没有工具栏"的真正根因，先修它，再补一个能看音频的视图。

## 行为约定

- 选中音频 clip 后，底部「编辑」页在**同一个页面内**切换到音频查看视图（不是新开页），工具栏只剩分组 `clipInfoGroup`（剪辑名编辑），音符工具组、量化组、自动翻页组连同它们两侧的分隔一起隐藏。
- 音频视图**纯查看**：不接受鼠标点击、不产生编辑、不显示右键菜单，波形上方的滚轮行为与钢琴卷帘一致（滚轮横向滚动、Ctrl+滚轮横向缩放）。
- **多声道分道显示**：单声道一条车道，立体声上下两条（左/右），超过两条按序号。车道数 = `max(1, audioInfo.channels)`，车道高度 = 画布高度 / 车道数，始终铺满视口，没有纵向缩放和纵向滚动。
- 左右声道各自独立采样，不复用轨道剪辑的**下混**峰值缓存。下混缓存在轨道编辑器继续使用，行为不变。
- 左侧固定宽度列显示 **dB 标尺**（`audioChannelAxisWidth = 64`，与钢琴卷帘的键盘列同宽，切换 clip 类型时画布不横向跳动）。标尺按车道分带，每带画自己的刻度与声道标记（`L` / `R` / `1` / `2`…）。
- 时间标尺（`TimelineView`）与钢琴卷帘共用同一份实现，横向缩放/滚动由后端统一驱动。
- 播放位置指示线跟着 `playbackController` 走，与钢琴卷帘、轨道列表同一路信号（`visualPositionChanged` / `lastPositionChanged`）。进入音频视图时还会补推一次 `position()` / `lastPosition()`，否则播放停止状态下切过来的视图会停在上一帧的旧位置。

## dB 标尺算法

波形本身保持**线性**振幅映射（`WaveformRenderUtils::mapAmplitude` 默认 `Linear`），dB 只是刻度标注，不改变波形形状。因此某个 dB 值 V 到车道中心的距离是

```
distanceFromCenter = pow(10, V / 20) * halfLaneHeight
```

等 dB 步长在屏幕上**几何收缩**，越靠近中心越挤，固定 3 dB 阶梯必然在某处重叠。生成方式（`AudioDbAxis::buildTicks`）：

1. 阶梯 `0, -3, -6, … , -60`，先放入 `0`（距离 = `halfLaneHeight`）。
2. 逐项算 `d = pow(10, db/20) * halfLaneHeight`：`d < minLabelSpacing` 时**停止**（后面只会更挤），否则与上一个保留项比，`lastKept.d - d >= minLabelSpacing` 才保留。
3. 末尾追加静音端 `{infinite = true, distanceFromCenter = 0}`，标签为负无穷（参考混音器，用 `U+2212` + `U+221E` 拼，不用字面量）。

`minLabelSpacing = max(audioDbAxisMinLabelSpacing, fontMetrics().height() + 2)`（下限 12）。保留项的间隔自然从 3 dB 涨到 6/9/12 dB，与 Au 的阶梯一致：

| 半车道高 | 保留的有限刻度 |
|----|----|
| 200 | 0 -3 -6 -9 -12 -15 -21 |
| 100 | 0 -3 -6 -9 -15 |
| 60 | 0 -3 -6 -12 |
| 25 | 0 -6 |
| 15 | 0 |
| < 10 | 空（只留中心线） |

半车道高 25 时 `-3` 离 `0` 只有 7.3 px、放不下标签，于是**跳过**它取下一个能放下的 `-6`——靠跳过稀疏化，不靠挤压。

实际运行时标签间距取字体行高 + 2（默认字体行高 17，即间距下限 19）。立体声剪辑把画布均分成两条车道，每条车道半高 72，画出来是 0、-3、-9 和负无穷，被跳过的 3 dB 刻度退化成短刻度。

刻度与标签都以静音线为轴上下镜像，同一个 dB 值在静音线上下各出现一次（`0 / -3 / -6 / -12` 在上，`-12 / -6 / -3 / 0` 在下），读数左右对称。标签各自钳在所属车道带内，所以满量程那个 `0` 贴着车道的上下边缘。

负无穷标签和其它刻度一样，以静音线为基线垂直居中。标尺列里的静音线只画到右端的刻度列，不横穿文字——否则标签只能往下让位，就比别的刻度低半行了。

轴显示的是**素材原始振幅**，不含 clip 增益。车道过矮（`halfLaneHeight < kMinHalfLaneHeight = 10`）时不画刻度，只画中心线与分隔线。

## 文件

| 部分 | 文件 |
|----|----|
| 车道几何（唯一来源） | `src/app/UI/Views/ClipEditor/AudioView/AudioLaneGeometry.h` |
| dB 标尺 | `AudioDbAxis.{h,cpp}` |
| 左侧标尺控件 | `AudioChannelAxisView.{h,cpp}` |
| 视图数据快照 | `AudioViewContent.h` |
| Legacy 后端 | `AudioViewGraphicsScene.{h,cpp}` / `AudioViewGraphicsView.{h,cpp}` / `AudioViewBackground.{h,cpp}` / `AudioWaveformLaneView.{h,cpp}` |
| RHI 后端 | `AudioViewRhiWidget.{h,cpp}` |
| 容纳两后端的容器 | `AudioView.{h,cpp}` |
| 接线 | `ClipEditorView.{h,cpp}` / `ToolBar/ClipEditorToolBarView.{h,cpp}` |

新增文件无需改 CMake（`src/app/CMakeLists.txt` 用 `GLOB_RECURSE CONFIGURE_DEPENDS`）。

## 坐标系与车道几何

视图偏移 = `clip->start()`，因此 `startTick()` / `endTick()` 是**全局** tick，场景 x 是**素材局部** tick `0..clip->length()`。采样时传 `materialStartTick = content.start`、`visibleStartTick = content.contentStartTick()`（= `start + clipStart`）。

`AudioLaneGeometry` 是车道行几何的唯一来源，标尺和波形必须用它算同一份矩形，否则左右列会错位：

- `laneCount(channels)`：`max(1, channels)`
- `laneHeight(totalHeight, channels)`：整除后把余数补给**最后一条**车道，保证铺满
- `laneRect(bounds, channels, lane)` / `laneHalfHeight(bounds, channels)`
- `channelTag`：立体声 `L` / `R`，其余按 `lane + 1`

## 后端契约

两套后端由开发者选项 `editorRenderBackend` 二选一，行为等价，切换后视图状态保持（`AudioView::fallbackToLegacy` 用 `viewState()` 恢复时间视口）：

- Legacy：`AudioViewGraphicsView` 派生 `TimeGraphicsView`，`AudioViewBackground` 派生 `TimeGridView` 画车道底色与分隔，每条车道一个 `AudioWaveformLaneView`（`setAcceptedMouseButtons(Qt::NoButton)`），位置由 `AudioLaneGeometry` 给出。
- RHI：`AudioViewRhiWidget` 派生 `EditorRhiWidget` 与 `EditorTouchTarget`。视口用 `setVerticalContent(0.0, 1.0)` + `setScaleBounds(…, 1.0, 1.0)` 把纵向缩放钉死为 1、纵向偏移钉死为 0（`setEnsureContentFillsViewport(true, false)` 只让横向铺满）。时间网格走共享的 `TimelineLineEmitter`，波形走抽出来的 `WaveformRhiUtils::appendWaveform`。触摸只导航（`touchFingerEdits()` 返回 `false`、`touchContentAt()` 恒为 `None`、`touchBlankDragAction()` 为 `Pan`）。

## 视图状态

音频模式下 `ClipEditorView`：

- `viewState()` 返回音频后端的 `centerTick` / `scaleX`，`centerKeyIndex = 60`、`verticalScale = 1`、`editMode = Select`——钢琴卷帘的垂直语义在音频视图没有意义，用常量占位，切回 singing clip 时会被真实值覆盖。
- `focusedRegion()` 返回 `Region::None`（没有可编辑面）。
- `regionVisible()` 仍转发给 `PianoRollEditorView` 的内部标志，不随可见性变化，因此 `captureEditorViewState` / `restoreEditorViewState` 往返后钢琴卷帘的布局不受影响。
- `moveToAudioClipState` 调 `EditorViewController::clearClipEditorEditTarget()`，把活动编辑目标降为 `None`，剪切/复制/粘贴/删除不会落到已经隐藏的钢琴卷帘上。
- 音频视图沿用 `Region::PianoRoll` 的画布归属，不新增 `Region` / `EditorInteraction::Target` 枚举值。

## 采样器通道分支

`AudioWaveformSampler::Request` 增加 `channel`（默认 -1 = 下混，轨道编辑器行为不变），`CacheKey` 一并带上（否则弹道间会命中彼此的缓存）。`selectPeakData(info, highResolution, channel)` 先看 `peakCacheChannels` / `peakCacheMipmapChannels` 对应车道是否非空，空则退回下混缓存——旧文档和测试里手搓的 `AudioInfoModel` 没有分道缓存，必须能退化而不是画空白。`sampleSubChunkPeakMode` 与 `sampleCurveMode` 直接读 `m_ioBuffer[offset + lane]` 而不做声道平均。

峰值缓存由 `DecodeAudioTask` 在一次解码里同时产出下混与分道两份，写入 `AudioInfoModel`，并纳入自动化草稿哈希与 `audioInfoEqual` 比较。代价是一个 10 分钟 44.1 kHz 立体声文件多约 1.7 MB 常驻内存，接受不做懒加载。

## 主题

颜色全部走 QSS token（`lite-dark` / `lite-light` 的 `clip-editor.qss`），QSS 改动需重新构建（资源编译进 `lite_res.qrc`）：

| 属性 | token |
|----|----|
| `laneBackgroundColor`（所有车道同色，不交替） | `${editor.canvasAlternate}` |
| `AudioChannelAxisView` 的 `laneBackgroundColor` / 左上角占位 | `${surface.panel}` |
| `laneSeparatorColor` | `${border.subtle}` |
| `waveformCenterLineColor` | `${piano.roll.octaveDivider}` |
| `clipRangeOverlayColor` | `${piano.roll.overlay}` |
| `barLineColor` / `beatLineColor` / `commonLineColor` | `${editor.grid.bar}` / `${editor.grid.beat}` / `${editor.grid.subdivision}` |
| `playPosIndicatorColor` / `lastPlayPosIndicatorColor` | `${editor.playhead}` / `${editor.lastPlayhead}` |
| 标尺刻度 / 标签 / 负无穷 / 分隔 / 声道标记 | `${timeline.ruler.barTick}` / `${timeline.ruler.beatScale}` / `${control.foreground.disabled}` / `${border.subtle}` / `${text.secondary}` |

车道底色不分奇偶交替，所有声道统一用 `editor.canvasAlternate`，与轨道编辑器放剪辑那块区域（`TracksGraphicsView` / `TracksRhiWidget` 的 background）同色——交替会让第一路（L）看起来比第二路暗。声道之间靠分隔线与左侧的 `L` / `R` 标记区分。左侧标尺列用面板色 `surface.panel`，与车道底色拉开一档，再加一条右边缘竖线，读起来是一条独立的面板列而不是画布的一部分。

波形颜色取 `AppColorPalette::instance()->baseColor(trackColorIndex)`（轨道基色，与钢琴卷帘的音符填充同源），`ClipEditorView::applyTrackColor` 一并下发给音频视图。**不能用 `clipForeground`**：它按基色明度返回纯黑或纯白，那是给轨道剪辑浅色底板上的文字和波形用的，画在深色画布上会变成看不见的黑块。

## 测试

- `TestApplicationGui/tst_audio_view.cpp`：`audioDbAxisTickLadderFollowsTheLaneHeight` 覆盖上表的阶梯稀疏化、线性位置、负无穷标签与过矮车道，`audioWaveformSamplerUsesTheRequestedChannelLane` 用两份已知比例的峰值缓存验证分道采样、左右声道不对称性、缺失车道回退下混、以及 `channel = -1` 行为未变，`audioViewDrawsThePlaybackPosition` 经控制器改播放位置后要求画布上出现一条贯穿整高的新竖线。
- `tst_track_editing.cpp`：音频 clip 拖动后 `activeClip()` / `appStatus->activeClipId` 的期望值由"不激活"翻转为"激活"。
