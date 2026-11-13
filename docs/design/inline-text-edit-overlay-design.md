# QWidget 覆盖层就地编辑 — 行为约定

## 背景

编辑器内所有就地编辑（轨道名、剪辑名与轨道窗画布里的剪辑标题、钢琴卷帘歌词/发音、速度/拍号/播放位置/声像/增益数值入口）统一由 `InlineTextEditOverlay`（普通子 `QWidget` 覆盖层 + 专用 `QLineEdit`）+ `InlineEditLabel` 实现，不使用顶层窗口标志、不使用 `QGraphicsProxyWidget`。

## 固定行为约定

- 编辑器内部文本操作、输入法和自身右键菜单**不属于**"外部事件"。
- 滚动、缩放、选择/工具/数据上下文变化、目标移动或删除、宿主隐藏或换父、最小化（`WindowStateChange`）都属于**外部事件**：立即提交并退出编辑。
- 窗口尺寸/位置变化与窗口失活**不属于**外部事件：Windows 弹出触摸键盘时会 resize 前台窗口（本机实测无失活事件），失活也可能来自键盘宿主短暂夺焦点，编辑必须存活；Qt 在 resize 时保持滚动值不变，锚点不会脱离目标。取证与细节见 `touch-and-pen-input-design.md` 第十七节。
- 普通重绘和 QSS 刷新不触发提交。
- 目标已失效且无法提交时安全取消。
- 提交前执行 `trimmed()`；无变化不提交；一次有效修改只产生一次 action。
- **轨道名和剪辑名允许为空**；空歌词恢复语言默认歌词；空发音恢复原始发音。
- 非法或不完整输入关闭覆盖层并保持当前模型值，不转换为 `0`，不发送业务修改。
- Enter 提交、Esc 取消、FocusOut 提交（reason 为 `ActiveWindowFocusReason` 的失活失焦除外）、重复结束保护（`m_submitted`）。
- 编辑会话绑定开始编辑时的目标 id（如 clip id / note id）；切换活动剪辑和外部属性刷新前先提交，禁止写入错误目标。
- 数值入口验证规则：速度仅接受有限正数；拍号接受正整数 `分子/分母` 且分母为 2 的幂；播放位置验证合法 `小节:拍:tick` 范围；声像支持 `Lxx/Rxx/C` 和数值百分比；增益支持有限数值及 `-∞`，提交后沿用滑块范围截断并规范化显示。

## 关键实现

- `InlineTextEditOverlay` / `InlineEditLabel`：`src/libs/GUI/Controls/`
- 钢琴卷帘共享覆盖层宿主：`PianoRollGraphicsView`（viewport 覆盖编辑器 + 稳定目标 note id）与 `PianoRollRhiWidget`
- 轨道窗剪辑标题共享会话：`ClipTitleInlineEdit`（`src/app/UI/Views/Common/`），Legacy 的 `TracksGraphicsView` 与 RHI 的 `TracksRhiWidget` 各持有一个，后端只提供视口坐标下的标题带矩形
- 编辑态 QSS：`clip-editor.qss` 等主题文件中的 `InlineEditLabel` / overlay 角色样式
- FillLyric 模块自己的同名 `EditLabel` 控件保留，不在统一范围内

## 轨道窗剪辑标题

剪辑标题的就地重命名由双击触发，两套编排后端共用同一套判定与提交：

- **命中区是标题带**，不是整条剪辑。带高 `EditorItemGeometry::clipTitleHeight`（20 逻辑像素），从剪辑绘制矩形上沿算起，横向取整条可见宽度。双击标题带**以外**的剪辑实体仍是原有手势（切到剪辑编辑器并把钢琴卷帘居中到落点）。
- **输入框跟着文本走，不铺满标题带**：宽度按当前文本的 `QFontMetricsF` 宽度加固定内边距算，下限 `ClipTitleInlineEdit::minimumEditorWidth`（96）、上限 `maximumEditorWidth`（320），并且不超过所在标题带的宽度，除非标题带本身窄到放不下下限（此时保留下限、允许溢出）。打字过程中宽度随内容实时增减。规则是静态纯函数 `ClipTitleInlineEdit::editorWidthFor(text, font, bandWidth)`，可脱离会话单测。
- **只有标题被画成独立标题条时才接管双击**，即 `EditorItemGeometry::clipHasTitleStrip`（底部预览高 ≥ `clipTitleStripMinPreviewHeight`，32 逻辑像素）。竖缩小到标题改为居中绘制时没有标题带，双击整条剪辑仍走导航，否则短剪辑上导航手势会彻底失联。
- 标题带的几何、标题高与阈值只允许来自 `EditorItemGeometry`。Legacy 的 `AbstractClipView::paint` 与 RHI 的 `TracksRhiWidget::appendClip` / `clipPreviewRect` 都从这两个常量推导，物理像素与逻辑像素之间用 `EditorItemGeometry::scaledRect` 换算。
- **会话只记剪辑 id**，提交前重新 `findClipById`。目标已删则静默丢弃，文档换代时取消而不是提交（新工程可能复用同一个 id），RHI 回退 Legacy 前先提交。
- 提交体与剪辑编辑器工具条同一条路：`Clip::ClipCommonProperties args(*clip)` 改 `name` 后调 `trackController->onClipPropertyChanged(args)`，一次修改一条 undo。名称 `trimmed()` 后与当前值相同则不产生 action，空名允许。
- 编辑期间后端不再绘制该剪辑的标题（Legacy `setTitleEditing`、RHI `ClipSnapshot::titleEditing`），覆盖层是旧名的唯一显示位置。

## 触摸行为

落在激活覆盖层上的触摸流由**画布触摸控制器持有并转发**，覆盖层不自行接管投递（Qt 隐式触摸抓取对非画布控件不持久化，三种自接管形态实测全部丢失 update/end——机制与取证见 `touch-and-pen-input-design.md` 第十五节）：

- `EditorTouchTarget::touchRelayTextBegin/Move/End/Cancel`：控制器在手势机分派 Pressed 前逐点询问，编辑器上的点整条流转发，对手势机不可见。
- 覆盖层 `relayTouchBegin/Move/End/Cancel(globalPos)` 把流转译为发给 line edit 的鼠标事件：点按定位光标、拖动选择；点按编辑器外照常触发"点击外部提交"。
- 触摸长按 450 ms（12 px 漂移取消）**松手时**弹出行内编辑菜单，与画布长按菜单同一时序；流被平台吞掉则不弹。
- 新流开始时若上一条流未收到 End，先补发 release 再接管（自愈）。
- 真机回归清单见 `touch-and-pen-input-design.md` 第十五节。
