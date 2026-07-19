# 主题语义 Token Map（初稿）

> 版本：v1 基线，已完成第一轮协同审查。当前共 **129 个 semantic token**。
>
> 本稿确定职责和状态接口。`lite-dark/colors.json` 中的值主要来自现有视觉基线，新增状态使用
> 临近颜色占位，均不代表最终配色已经定稿。

## 第一轮审查结果

- `surface.popup` 仅用于菜单、下拉列表等可交互浮层，Toast 和 ToolTip 改用组件 token；
- 删除职责不明确的 `surface.canvas`；音乐编辑画布统一使用 `editor.canvas`；
- 文本收缩为 `primary / secondary / emphasis / onAccent / link` 五类；
- `control` 只收录当前确实存在独立表现的 fill、border、foreground 状态；
- Timeline 的 running 状态拆成正弦脉动所需的 low/high 两个端点；
- 原 `mixer.mute/solo` 改成轨道、片段和混音台可共享的按钮变体。

第一版不追求预先覆盖所有状态。迁移中发现新的独立语义时允许增量增加 token，但不能用已有
token 表达不相干的职责。

## 命名与分层

`colors.json` 只有两个顶层对象：

```json
{
    "palette": {
        "neutral.800": "#1D1F26",
        "accent.500": "oklch(78% 0.105 260)"
    },
    "tokens": {
        "surface.sunken": "{neutral.800}",
        "control.border.focus": "{accent.500}"
    }
}
```

- `palette` 是主题内部的 primitive，名称可以表达色阶，只允许颜色字面量；
- `tokens` 是稳定接口，允许颜色字面量或精确别名 `{name}`；
- QSS 使用 `${surface.sunken}`，禁止 `${neutral.800}`；
- v1 不支持颜色计算表达式。浅色与深色主题分别显式给出 token 值。

## 通用 UI Token

### Surface

| Token | 职责 |
|---|---|
| `surface.window` | 应用主窗口和最高层背景 |
| `surface.panel` | 卡片、侧栏、工具面板等一级容器 |
| `surface.raised` | 工具条分组、凸起控件、选项块 |
| `surface.sunken` | 表格、列表、时间轴等下沉区域 |
| `surface.input` | 获得焦点的单行输入框，以及常驻多行编辑区背景 |
| `surface.popup` | 菜单、ComboBox 下拉列表等可交互的不透明浮层 |

Toast、ToolTip 不使用 `surface.popup`，见后文组件 token。

### Text 与 Icon

| Token | 职责 |
|---|---|
| `text.primary` | 绝大多数内容和控件文本 |
| `text.secondary` | 说明、提示、占位文字 |
| `text.emphasis` | 透明控件悬停等需要更高对比度的文本 |
| `text.onAccent` | 实色强调背景上的文本 |
| `text.link` | 可点击文本链接 |
| `icon.primary` | 默认图标 |
| `icon.secondary` | 次级图标 |
| `icon.disabled` | 禁用图标 |
| `icon.onAccent` | 实色强调背景上的图标 |

禁用文本属于控件状态，使用 `control.foreground.disabled`，不再设置 `text.disabled`。

### Global Border

| Token | 职责 |
|---|---|
| `border.subtle` | 分割线、低对比边界 |
| `border.default` | 非交互容器的普通边界 |
| `border.strong` | 面板轮廓、需要明显分组的边界 |
| `border.focus` | 非标准控件或编辑区域的焦点边界 |

标准控件边框使用下面的 `control.border.*`。全局 `border.*` 不承担控件状态。

### Control State Matrix

通用中性控件从填充、描边、前景三个视觉维度描述。状态词含义如下：

| 状态 | 含义 |
|---|---|
| `default` | 未交互、未选中 |
| `hover` | 未选中且指针悬停 |
| `pressed` | 未选中且按下 |
| `disabled` | 未选中且禁用 |
| `checked` | 选中/开启 |
| `checkedHover` | 选中且悬停；当前只在填充中需要独立值 |
| `checkedPressed` | 选中且按下；当前填充和前景需要独立值 |
| `checkedDisabled` | 选中且禁用；当前填充和前景需要独立值 |

第一版 token 为：

- `control.fill.default / hover / pressed / disabled / checked / checkedHover / checkedPressed /
  checkedDisabled`；
- `control.border.default / hover / pressed / focus / disabled / checked`；
- `control.foreground.default / pressed / disabled / checked / checkedPressed / checkedDisabled`。

没有独立 token 的状态直接复用最接近的已有状态。例如 hover 前景复用 `default`，
checkedHover 前景复用 `checked`。只有设计或现有样式要求不同颜色时才增加新 token。

特殊控件可以直接使用矩阵的一部分，也可以在组件层覆盖。例如 TagButton 可覆盖 checked 描边，
Mute/Solo 可覆盖 checked 色相；未覆盖的状态继续使用通用矩阵。

### Accent、Selection、Status 与 Layer

| Token | 职责 |
|---|---|
| `accent.default` | 主题主强调色 |
| `accent.hover` | 实色强调元素悬停 |
| `accent.pressed` | 实色强调元素按下 |
| `accent.muted` | 中等透明/低强调版本 |
| `accent.subtle` | 选择提示等极弱强调版本 |
| `selection.fill` | 通用选择区域填充 |
| `selection.hoverFill` | 已选择区域悬停时的填充 |
| `selection.border` | 通用选择区域边框 |
| `selection.text` | 原生文本选择前景 |
| `selection.textFill` | 原生文本选择的实色背景 |
| `selection.inactiveFill` | 非活动窗口或非焦点选择填充 |
| `status.info` | 信息/处理中反馈 |
| `status.success` | 成功反馈 |
| `status.warning` | 警告反馈 |
| `status.error` | 错误、危险操作反馈 |
| `shadow.popup` | 浮层、Toast、ToolTip 阴影 |
| `overlay.scrim` | 模态遮罩、范围外压暗层 |

`accent.*` 描述强调色阶，不替代控件状态。控件可以通过别名把 checked 状态连接到 accent。

### Toast 与 ToolTip

| Token | 职责 |
|---|---|
| `toast.background` | Toast 的轻量半透明背景 |
| `toast.border` | Toast 的半透明描边 |
| `toast.text` | Toast 消息文本，可引用 `text.emphasis` |
| `tooltip.background` | ToolTip 背景 |
| `tooltip.border` | ToolTip 描边 |
| `tooltip.text` | ToolTip 辅助说明文本；标题仍可使用 `text.emphasis` |

### 通用组件补充

| Token | 职责 |
|---|---|
| `toolButton.fill.checked` | 工具按钮中性 checked 填充，不表达主题强调 |
| `toolButton.fill.checkedHover` | 中性 checked 工具按钮的悬停填充 |
| `toolButton.fill.pressed` | 工具按钮按下填充 |
| `toolButton.border.checked` | 中性 checked 工具按钮的描边 |
| `checkBox.fill.unchecked` | 未选中复选框的默认填充 |
| `checkBox.fill.uncheckedHover` | 未选中复选框的悬停填充 |
| `checkBox.fill.uncheckedPressed` | 未选中复选框的按下填充 |
| `scrollbar.handle` | 滚动条手柄默认填充 |
| `scrollbar.handleHover` | 滚动条手柄悬停填充 |
| `scrollbar.handlePressed` | 滚动条手柄按下填充 |
| `scrollbar.handleEmphasis` | 需要高对比度的滚动条手柄填充 |
| `slider.track.inactive` | 滑杆未激活轨道 |
| `slider.track.inactiveSubtle` | 低对比度滑杆未激活轨道 |
| `slider.track.active` | 滑杆已激活轨道 |
| `slider.thumb.fill` | 滑杆手柄填充 |
| `slider.thumb.border` | 滑杆手柄描边 |
| `slider.thumb.borderStrong` | 高对比度滑杆手柄描边 |
| `window.closeButton.hover` | 系统关闭按钮悬停填充 |
| `window.closeButton.pressed` | 系统关闭按钮按下填充 |
| `playback.control.playingFill` | 播放进行状态填充 |
| `playback.control.pausedFill` | 暂停状态填充 |

## 编辑领域 Token

### Editor 与 Piano

| Token | 职责 |
|---|---|
| `editor.canvas` | 轨道、钢琴卷帘和参数编辑器主画布 |
| `editor.canvasAlternate` | 相邻行、黑键行或局部层的交替底色 |
| `editor.grid.bar` | 小节主网格线 |
| `editor.grid.beat` | 拍网格线 |
| `editor.grid.subdivision` | 细分网格线 |
| `editor.playhead` | 当前播放位置 |
| `editor.lastPlayhead` | 上次/辅助播放位置 |
| `editor.loop` | 启用的循环标记 |
| `editor.loopDisabled` | 禁用的循环标记 |
| `editor.selection.border` | 编辑器框选边框 |
| `editor.selection.fill` | 编辑器框选填充 |
| `editor.splitLine` | 切分/破坏性编辑预览线 |
| `piano.key.white` | 左侧钢琴白键 |
| `piano.key.black` | 左侧钢琴黑键 |
| `piano.key.divider` | 钢琴键分隔线 |

编辑器选择不直接复用通用 `selection.*`：它画在高信息密度画布上，需要独立控制对比度。

### Curve 与 Meter

| Token | 职责 |
|---|---|
| `curve.guide` | 参数刻度、毕业线、关键帧辅助线 |
| `curve.original` | 原始/参考曲线 |
| `curve.edited` | 编辑后曲线 |
| `curve.anchor` | 普通锚点和锚点曲线 |
| `curve.anchorSelected` | 选中锚点和预览锚点 |
| `meter.dimmed` | 电平表未点亮段 |
| `meter.safe` | 安全电平段 |
| `meter.warning` | 已进入警告阈值、但尚未临界的电平段 |
| `meter.critical` | 临界电平段 |
| `meter.clipped` | 削波指示 |
| `meter.value` | 当前值和峰值保持线 |
| `meter.background` | 电平读数背板 |

电平色保留领域命名，不直接等同于 `status.*`。当前 `warnColor` 实际为绿色，其含义是电平
区间，而不是全局警告。

### Timeline Task

| Token | 职责 |
|---|---|
| `timeline.task.pending` | 尚未执行的任务片段 |
| `timeline.task.runningLow` | 运行中脉动的低强度端点 |
| `timeline.task.runningHigh` | 运行中脉动的高强度端点 |
| `timeline.task.success` | 成功完成的任务片段 |
| `timeline.task.failed` | 执行失败的任务片段 |

实现动画时使用 `t = (sin(phase) + 1) / 2` 在两个端点间插值，建议采用 OKLab 插值。

### Mute / Solo Button Variant

未选中状态直接使用通用 `control.*`。组件 token 只覆盖带业务色的 checked 状态：

| 前缀 | 后缀 |
|---|---|
| `button.mute` | `checked.foreground`、`checked.fill`、`checked.border`、`checkedHover.fill`、`checkedHover.border`、`checkedPressed.fill`、`checkedPressed.border` |
| `button.solo` | `checked.foreground`、`checked.fill`、`checked.border`、`checkedHover.fill`、`checkedHover.border`、`checkedPressed.fill`、`checkedPressed.border` |

这套变体供轨道、片段和混音台共享，不绑定到 `mixer` 命名空间。禁用状态先回退到通用
`checkedDisabled`，只有发现 Mute/Solo 需要特殊禁用表现时才增加组件 token。

## 暂不进入 Token Map

- `app-color-palette.json` 的 12 个轨道、音符、说话人业务色；
- 尺寸、圆角、间距、字体等非颜色属性；
- 根据业务色动态计算的“色块上前景”。当前白色选择边框先按过渡项迁移，后续单独设计
  对比度策略；
- 自动生成 hover/pressed 的公式。v1 使用显式值，等深浅主题稳定后再评估 recipe 层；
- Timeline 脉动过程中的中间色；主题只提供 low/high 端点。

## 已确认的迁移原则

1. `text.emphasis` 表示比普通正文更需要视觉优先级的文本，可用于歌手标题和透明控件悬停
   前景；
2. Control token 按当前实际状态收录，后续设计出现新差异时再增量扩展；
3. Mute/Solo 是通用按钮的 checked 变体，未选中和禁用状态复用 `control.*`；
4. 保留 `meter.warning`：LevelMeter 本身存在 safe、warn、critical 三个阈值区间，颜色不决定
   区间语义；
5. QSS 迁移保持整体视觉层级和状态差异，不要求像素级一致；职责相同的历史近似色可以归并，
   统一配色留到后续调色阶段。
