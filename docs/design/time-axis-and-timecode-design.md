# 时间（秒）轨与标题栏时间码弹层 — 设计契约

> 状态：✅ 实施完成（`feat/time-axis-and-timecode`）

## 行为约定

### 时间（秒）轨

- 轨道窗与钢琴卷帘的标尺正下方各多一条 **28px 信息轨**（`TimeAxisLaneView`），用来读**秒坐标**：同一段工程在这个轴上按 `0:02.6` 这样的秒值定位，与以 tick 计量的小节/拍坐标互为对照。它是曲速轨、拍号轨之外的第三条可选信息轨，行内顺序排在最前，紧跟原有标尺。
- 这一行的长相向**标尺**看齐，而不是向其它信息轨看齐：背景透明（与标尺同底）、标签用标尺的小节号/拍号色、刻度线用标尺的 bar tick / beat tick / subdivision 色，刻度从行底向上生长（整分钟最高，整秒次之，亚秒最短），标签靠左上方。它因此读起来是「标尺的第二行」，而不是一条 chip 列表。行顶仍保留与其它信息轨相同的 1px 分隔线，左侧标题与右侧轨道在同一高度断行。
- 这一行**不叠小节/拍网格**，只画自己的秒刻度。刻度按缩放自适应从 `100ms` 逐级放宽到 `1h`（见「刻度算法」）。步长 ≥ 1s 时省略十分位（`0:15`），否则带十分位（`0:00.5`）。选档本身已保证相邻标签不会重叠，因此标签**不做 elide**：靠视口边缘时由绘制裁切掉超出部分，标签本身仍是完整的。
- **默认隐藏**，两个窗口各管各的，和曲速轨、拍号轨一致：
  - 轨道窗：列表头「轨道显示」菜单第一项「时间（秒）轨」（`actionTimeLane`，排第一是为了和行内顺序一致）只切换本窗口，不写设置；
  - 钢琴卷帘：「设置 → 外观 → 钢琴卷帘 → 显示时间（秒）轨」写入 `AppearanceOption::showTimeLane`（默认 `false`），热生效；
  - 钢琴卷帘没有激活片段时强制隐藏（与曲速轨、拍号轨一致）。
- 纯展示：**不做吸附、不做 seek、不产生命中态**，双击与右键在这一行无动作。滚动/缩放只经既有滚轮信号转发给画布，不改画布网格。
- 与曲速轨、拍号轨一样，这一行同样接收 `setTimeRange` 同步、绘制播放头实线与上一位虚线，并转发滚轮（Ctrl 横缩、Alt 纵缩、Shift 横滚、无修饰纵滚）。
- 不新增颜色 token：秒轨把 `InfoLaneView` 已有的 qproperty 映射到标尺用的 token，另加一个 `minorTextColor` 给非整分钟标签。

### 标题栏时间码弹层

- `elTime` 由纯 `InlineEditLabel` 换成 `TimeComboBox`（`objectName` 仍是 `elTime`）：**单击开弹层、双击内联编辑、右键开合**，与曲速、拍号字段一致。
- 弹层（`TimePopupWidget`，标题 `Time`）内容：左边 `小节:拍:tick`、右边秒坐标，两框之间是链环指示；下方「显示模式」三段选择 `小节:节拍:刻 / 分:秒:毫秒 / 同时显示`（英文 `Bar:Beat:Tick / Min:Sec.Ms / Both`）。
- 两个输入框**永远描述同一个位置**：改任一侧回车/失焦即换算成 tick 并移动播放头，另一侧立刻回算。非法输入不移动播放头，字段被改回当前播放头位置。
- 播放头在弹层打开期间继续变化（播放、或别处 seek）时两框跟随刷新；**正在输入的那个框不覆盖**（由 `QLineEdit::textEdited` 标记活跃框，回车/失焦后解除）。
- 「显示模式」直接决定标题栏时间码自己的格式，热生效并持久化到 `AppearanceOption::timecodeFormat`（默认 `barBeatTick`，即现状）。三种格式：
  - `barBeatTick`：`001:01:240`（`Timeline::getBarBeatTickTime`）
  - `time`：`01:14.514`（完整时间码，与改动前一致）
  - `both`：`001:01:240 · 01:14.514`
- 位置编辑不是文档变更：不写文档、不产生 Undo、不改 `documentVersion`，只可能改设置 JSON。

## 格式契约

秒坐标只保留一位小数（十分之一秒），分钟不补前导零，这样 28px 行与标题栏都够短。UI 要更细的精度时，解析端仍接受最多三位小数，代价是显示只会四舍五入到十分位。

固定格式走 C Locale（`docs/guides/localization.md` 的例外条款），单位换算全部落在 `MusicBase`：

| 接口 | 行为 |
|----|----|
| `MusicTimeConverter::formatSecondsLabel(ms, includeTenths = true)` | 标尺秒轨用的**简洁**写法：`%lld:%02lld.%lld`，即 `0:02.6`；先四舍五入到十分位再分解秒与分（`59.96s` → `1:00.0`，`359.9994s` → `6:00.0`）；`includeTenths = false` 时输出 `%lld:%02lld`（`1:14`），同样先取整到十分位再截去小数，因此 `74.514s` 不会变成 `1:15`；负数与 `NaN` 钳到 `0:00.0`；分钟不补零、不设上限 |
| `MusicTimeConverter::formatTimecode(ms)` | 标题栏时间码用的**完整**写法：`%02lld:%02lld.%03lld`，即 `01:14.514`；四舍五入到整毫秒后分解（`59.9996s` → `01:00.000`）；负数与 `NaN` 钳到 `00:00.000`；分钟补到两位、超过 99 自然变 3 位 |
| `MusicTimeConverter::parseMinSec(QStringView, double *ms)` | 两种写法共用一个解析器：接受 `mm:ss.mmm` / `m:ss.d` / `m:ss` / `ss.d`；小数 1~3 位按毫秒右补齐（`.6` → 600，`.614` → 614）；秒可大于 59（`0:75.0`）；归一化全角冒号；空串、负数、超长小数、多个冒号一律 `false` 且不写输出参数 |
| `TimecodeFormatUtils::formatPosition(timeline, tick, format)` | 三种显示格式的唯一生成处，负 tick 钳 0 |
| `TimecodeFormatUtils::parsePosition(timeline, text, format, *tick)` | 三种格式的唯一解析处；`both` 取 `·` 之前那半段，含 `.` 的输入按秒坐标解析，否则按小节:拍:tick |
| tick 反解 | `qRound(timeline.msToTick(ms))`，结果 `<0` 或超 int 范围判为非法 |

`both` 的渲染与剥离共用同一个分隔符（`TimecodeFormatUtils` 内部的 `combinedSeparator()`），标题栏与弹层不会各写一套。

显示精度只有十分位，所以「格式化再解析」得到的是 100ms 以内的最近值，不是原值。这一点由 `TestMusicTime::musicTimelineMinSecFormatting` 的往返断言固定。

## 关键实现

| 部分 | 文件 |
|----|----|
| 秒坐标格式化/解析 | `src/libs/MusicBase/MusicTimeConverter.{h,cpp}`、`src/app/UI/Utils/TimecodeFormatUtils.{h,cpp}` |
| 时间（秒）轨 | `src/app/UI/Views/TrackEditor/InfoLane/TimeAxisLaneView.{h,cpp}` |
| 信息轨绘制钩子 | `src/app/UI/Views/TrackEditor/InfoLane/InfoLaneView.{h,cpp}`（`paintLaneBackground` / `paintLaneContent`） |
| 轨道窗接入 | `TrackEditorView.{h,cpp}`、`TrackListHeaderView.{h,cpp}` |
| 钢琴卷帘接入 | `PianoRollView.{h,cpp}` |
| 时间码字段与弹层 | `PlaybackView.{h,cpp}`、`TimeComboBox.{h,cpp}`、`TimePopupWidget.{h,cpp}` |
| 弹层共用开合逻辑 | `src/app/UI/Views/Common/PopupInlineEditLabel.{h,cpp}`（`TempoComboBox`、`TimeSignatureComboBox`、`TimeComboBox` 三者共用） |
| 选项四层 | `AppearanceOption.{h,cpp}` / `SettingsAutomationFacade.{h,cpp}` / `AppOptionsAutomationAdapter.cpp` / `AppearancePage.{h,cpp}` |
| 主题 | `lite-dark/track-editor.qss`、`lite-light/track-editor.qss` 的 `TimeAxisLaneView` 段，以及 `title-bar.qss` 的 `TimePopupWidget` 段 |

新文件无需改 CMake（`src/app/CMakeLists.txt` 用 `GLOB_RECURSE CONFIGURE_DEPENDS`）。

## `InfoLaneView` 绘制钩子契约

`paintEvent` 固定为「背景 → 内容 → 播放头」三段，前两段抽成受保护虚函数：

- `paintLaneBackground(QPainter *)`：默认调 `drawTimeline`，即画布同款小节/拍/细分网格；秒轨置空。
- `paintLaneContent(QPainter *)`：默认画 chip 行；秒轨改画秒刻度。

新增信息轨只需覆盖这两个钩子，播放头、主题色、滚轮、`setTimeRange` 同步全部免费。同时把 `textColor()`、`barLineColor()`、`beatLineColor()`、`commonLineColor()` 与 `visibleStartTick()`/`visibleEndTick()` 提到 `protected`，供非 chip 轨使用。

## 刻度算法

- 候选步长（ms）：`100/200/500/1000/2000/5000/10000/15000/30000/60000/120000/300000/600000/900000/1800000/3600000`，后一档整除前一档，缩放时只增删线不重排。
- 选档：按**可见区内最慢的速度**算像素密度，取 `step * 像素每毫秒 >= 64` 的最小档，超出则取最粗档。像素每毫秒 = `(width / tickSpan) * bpm * ticksPerQuarterNote / 60000`，慢速段取值最小。标签在慢速段最先挤在一起，而一个只占少量 tick、却占掉大半毫秒的慢速段会让「整窗平均密度」高估好几倍，所以平均密度不能用来选档（`TestApplicationGui::timeAxisLaneStepKeepsLabelsApartAcrossTempos` 固定这一条）。
- 定位：可见 tick 窗口先经 `timeline.tickToMs` 求 ms 区间，每个刻度再 `timeline.msToTick` 回到 tick、经 `tickToX` 落到像素——**变速点自动正确**，刻度永远落在该秒值真实对应的位置，而不是按平均速度线性铺。
- 上限：单行最多 2000 个刻度（防御性边界，正常缩放远达不到）。

## 显隐

- 轨道窗的秒轨是**本窗口状态**：`actionTimeLane` 直接切换轨道与其左侧标题的可见性，不写选项，与曲速轨、拍号轨完全一致。
- 钢琴卷帘的秒轨读**外观选项** `AppearanceOption::showTimeLane`（默认 `false`），在 `updateInfoLanesVisibility()` 里与另两条轨一起处理，`anyLaneVisible` 也把它算进去，底部那条分隔线随三条轨一起显隐。
- 两边互不影响：在轨道窗勾选秒轨不改变钢琴卷帘的设置，反之亦然。

## 弹层开合的可复用语义（`PopupInlineEditLabel`）

- 左键单击启动 `QApplication::doubleClickInterval()` 计时器，超时开弹层；双击停表并进内联编辑；右键切换弹层开合。
- `popupVisible` 动态属性用于 QSS 高亮；弹层 `Hide`/`Close` 时若鼠标仍在字段上按下，置 `m_ignoreNextShow` 吞掉随之而来的重开，避免「点一下关掉又立刻弹回」。
- 子类只实现 `preparePopup()`（刷新弹层内容）与 `presentPopup(QPoint)`（转调各自弹层的 `showAt`，各自保留屏幕边缘钳制）。

## 主题

- 秒轨在 `track-editor.qss` 里以 `TimeAxisLaneView` 规则覆盖 `InfoLaneView` 的底色与线条色：背景归零，标签映射到 `${editor.playhead}` 与 `${timeline.ruler.beatScale}`，刻度映射到 `${timeline.ruler.barTick}`、`${timeline.ruler.beatTick}`、`${timeline.ruler.subdivisionTo}`。规则必须排在 `InfoLaneView` 之后，二者特异性相同，靠顺序取胜。行顶分隔线不由本类声明，而由 `InfoLaneView, InfoLaneHeaderView` 的共享规则统一提供，因此轨道体与左侧标题不会各画一条。
- `TimePopupWidget` 复用现有的 `surface.popup`、`control.border.default`、`control.border.focus`、`surface.input`、`text.secondary`、`text.primary`，**不新增颜色 token**，`docs/theme/theme-token-map.md` 无需改动。
- 弹层两个输入框就是普通的 `LineEdit` 控件，不加任何弹层专属 QSS：用的是 `controls.qss` 里全局 `QLineEdit` 的样式（浅底色 + 1px 边框 + 4px 圆角 + 更重的底边，聚焦时换成 `control.border.focus` 底边），与设置页等处的文本输入一致。
- 三段选择器直接用全局 `QPushButton[panelSegment="true"]` 规则（`:checked` 即 `selection.fill` 底 + `text.accent` 字），弹层只额外把整条的高度固定为标准控件高度，与底部的「编辑 / 混音」切换按钮一致。
- 弹层保持不透明（`WA_StyledBackground`，不带 `WA_TranslucentBackground`），Windows 11 用 DWM 圆角——与曲速、拍号弹层同一约定，避免合成器卡死。

## 测试

| 覆盖 | 用例 |
|----|----|
| 格式与解析 | `TestMusicTime::musicTimelineMinSecFormatting`（标尺简洁写法与时间码完整写法、进位、>99 分钟、负数/NaN、无十分位写法、两种写法的往返精度、变速 Timeline 往返） |
| 选项存取 | `TestPreferences::appearanceTimecodeFormatAndTimeLane`（默认值、三值往返、未知值回落、非 bool 忽略） |
| 弹层交互 | `TestApplicationGui::timePopupEditsPositionAndDisplayFormat`（单击开层、两框一致、秒值改位置、非法输入回滚、播放头跟随、模式切换落盘、内联编辑两种格式、文档不变） |
| 轨道窗显隐与顺序 | `TestApplicationGui::trackHeaderLaneMenuTogglesBothLanes`（三个菜单项、秒轨排第一、默认隐藏、勾选只切换本窗口且不动钢琴卷帘选项、取消隐藏） |
| 变速下的刻度密度 | `TestApplicationGui::timeAxisLaneStepKeepsLabelsApartAcrossTempos`（慢速段占比小但毫秒多时，相邻标签仍不小于 64px） |
| 设置页 | `TestApplicationGui::appearanceInputsPersistAcrossReopening`（新开关存在且落盘） |
| 钢琴卷帘三轨 | `TestNativeDesktop::rhiPianoNavigationInputsReachTheActiveViewport`（三轨都可见、滚轮都转发给画布） |
| 设置自动化 | `TestApplicationServices::appearanceSettings`（新字段往返、越界枚举 `InvalidArgument`） |

## 明确不做

- 标尺本身不切成秒显示，也不提供「标尺右键切换」。
- 不按秒吸附，秒轨上点击/双击不 seek。
- 「显示模式」只作用于标题栏时间码，不改曲速对话框头部等其他位置的时间显示。
- 秒轨没有除菜单项以外的显隐状态，不记忆「每窗口各自打开」。
- 不碰 DSPX 序列化与文档状态，位置编辑不产生 Undo。
