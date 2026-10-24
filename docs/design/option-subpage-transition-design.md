# 选项子页转场（滑入 + 视差 + 变暗）

> 状态：已实现。本文是 `OptionSubPageStack` 的动画与尺寸契约。
> 实现位置：`src/app/UI/Dialogs/Options/Pages/OptionSubPageStack.{h,cpp}`
> 首个接入点：`AutomationPage`（访问控制 ↔ 自定义工具集）

## 1. 背景

子页之间原本是 `show()`/`hide()` 直接切换，切进去没有任何过渡。目标是手机设置那种 push/pop：
新页从右侧滑入，旧页左移一小段并变暗（视差由两页位移差产生），返回时方向镜像。

## 2. 结构

`OptionSubPageStack` 是一个**手动管几何**的容器：

```
OptionSubPageStack
├ 子页 A（几何 = 容器 rect）
├ 子页 B
├ SubPageTransitionScrim     半透明黑遮罩，转场时挂到「被盖住的那一页」下
├ SubPageTransitionBackdrop  页面底色块，转场时压到「上层那一页」内容之下
└ SubPageTransitionShield    透明输入屏蔽层，转场时铺满容器
```

子页**刻意不放进 layout**：`pos` 动画必须自己拥有位置，否则布局会把 widget 拽回原位。
子控件默认被父控件裁剪，所以「从右滑入」天然被容器右缘切掉，视觉正确。

## 3. 为什么不用 QGraphicsOpacityEffect

用 `QGraphicsOpacityEffect` 做真透明淡出是教科书做法，但本仓库有一条硬限制：**嵌套图形效果会让整棵子树渲染失败**
（父控件与任意子控件同时持有 effect，日志出现成对的 `A paint device can only be painted by one painter at a time`
与 `QWidgetEffectSourcePrivate::pixmap: Painter not active`，控件完全不可见，详见
`references/qt-graphics-effect-pitfalls.md`）。

`Button` 曾经通过 `DROP_SHADOW_EFFECT` 宏自带一个按需安装的 `QGraphicsDropShadowEffect`
（只要 QSS 里给某个 `Button` 配上 `qproperty-dropShadow` 就会装），所以任何页面级透明度动画都是地雷。
该宏已于 `5ca69dd4` 移除，但本设计仍然选择**零 effect**方案：变暗用一层自绘遮罩，而不是透明度动画。

## 4. 两个方向

| | 入页（`to`） | 出页（`from`） | 遮罩 | 层级 |
|---|---|---|---|---|
| **Forward**（进子页） | `pos.x` `+width → 0` | `0 → −28%width` | 挂在出页，`dim 0 → 0.35` | 入页在上 |
| **Backward**（返回） | `pos.x` `−28%width → 0` | `0 → +width` | 挂在入页，`dim 0.35 → 0` | 出页在上 |

两个方向的位移是严格镜像。遮罩始终挂在「被盖住的那一页」上，所以它以子控件的身份跟随那一页位移，
不需要第二条位移动画。返回时出页在上、入页从下面逐渐揭开，被揭开的部分随遮罩变亮同步恢复。

## 5. 尺寸契约

栈把**当前页**的尺寸提示转发给父级，外层选项页（`QScrollArea` + `widgetResizable`）因此始终按
「真正可见的那一页」定尺：

```cpp
QSize sizeHint()        { return m_current->sizeHint(); }
QSize minimumSizeHint() { return m_current->minimumSizeHint(); }
bool  hasHeightForWidth() const override { return true; }
int   heightForWidth(int w) const {
    return m_current->hasHeightForWidth() ? m_current->heightForWidth(w)
                                         : m_current->minimumSizeHint().height();
}
```

`hasHeightForWidth()` 恒真是刻意的：`IOptionPage::syncContentMinimum()` 依赖它决定是否钉住页面最小值，
而没有 hfw 的页（「固定头 + 内层滚动区」那种填充型页）回退到 `minimumSizeHint()`，正好等于它需要占的高度。

**转场期间**两页尺寸规则不同：

- **滑入页跟随容器**。它就是容器的新状态，外层滚动区在切换瞬间就 refit 到目标高度。
- **滑走页保持自身高度**。内容尺寸的页（报 `heightForWidth`）如果被压进容器的新高度，内容会被挤到变形、
  底部露出空档，实测就是这个症状。填充型页（无 hfw）本身跟随容器，照旧跟随。

两页高度不同时，滑走页尾部会被容器底边裁掉。这与滚动页被裁的样子一致，是可接受的表现。

## 6. 参数与动画档位

```
kTransitionMs          = 300      // push/pop 位移 + 遮罩时长
kOutgoingShiftRatio    = 0.28     // 出页位移占页宽比例（视差幅度）
kScrimPeakDim          = 0.35     // 遮罩峰值不透明度
EasingCurve            = OutCubic // 与 OverlayScrollBar / SmoothScroller 一致
```

时长经 `IAnimatable::getEffectiveAnimationTime()` 换算，「外观 → 动画」关闭时返回 0，
此时直接落到终态（不播放动画，也不留半途状态）。动画设置中途变化时同样立刻落位。

## 7. 边界与取舍

- **背景填充**：选项页本身是透明底（露出的是对话框底色），所以滑动的那一页必须垫一层不透明底色块
  `SubPageTransitionBackdrop`，颜色取 `surface.input`（`QDialog` 规则里用的页面底色 token，跟随主题）。
  底色块挂在**上层页**内容之下：push 时是入页，pop 时是出页。否则下面那一页的内容会从它的空白区透出来。
- **输入屏蔽**：转场期间页面在动，点击会落在滑过来的控件上，因此用 `SubPageTransitionShield`
  吞掉鼠标、滚轮与双击事件。转场只有 300ms。
- **语言切换**：`IOptionPage::changeEvent` 走 `initializePage()` 重建整个内容 widget，栈与动画对象随之销毁，
  转场状态自然丢弃，不需要额外清理。
- **复用**：容器与三个辅助控件都不含业务语义，其他有二级子页的选项页可以直接换用。
