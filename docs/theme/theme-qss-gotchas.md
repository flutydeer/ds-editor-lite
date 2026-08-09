# 主题 QSS 坑点记录

本文件记录在开发/维护主题 QSS 时遇到的坑，以及对应的规避方式。每一条都经过实测验证。

## QScrollBar：元素规则缺少 box 导致拖拽手柄跳变

### 现象

所有被 QSS 样式化的 `QScrollBar`（原生滚动条，不含 OverlayScrollBar 等自绘控件）在**按住手柄并移动**时，手柄会瞬间跳动十几像素，之后才"跟手"；滚动条接近两端时问题更明显，手柄会提前抵达两端，甚至一次轻微移动就直接触底/触顶。

### 触发条件

`QScrollBar` 元素规则（`QScrollBar { ... }`）中**没有声明任何 box 属性**（margin / padding / spacing），例如：

```css
QScrollBar {
    background-color: transparent;
    border-style: none;
}
```

注意：`background-color`、`border-*` 等**不参与 box 判定**，把它们改成任何值都无法避免此问题。

### 根因

Qt 的 `QStyleSheetStyle::subControlRect`（qstylesheetstyle.cpp）对滚动条各 subcontrol 使用了两套不一致的几何：

1. **SC_ScrollBarSlider**：只要存在 `QScrollBar::handle` 规则，就按 QSS 的 contentRect 计算——0 起点、全长（例如 600px 高的滚动条，手柄行程 600px）。
2. **SC_ScrollBarGroove**：仅当 `QScrollBar` 元素规则 `hasBox()` 为真（即声明了 margin/padding/spacing，见 qstylesheetstyle.cpp `extractBox` 分支）才按 QSS 计算；否则**回退到原生样式算法**（QCommonStyle），该算法为两端箭头按钮预留 `PM_ScrollBarExtent`（16px），轨道变成 `[16, 584]`。

`QScrollBarPrivate::pixelPosToRangeValue`（qscrollbar.cpp）在拖拽时把两者拼在一起换算 value：以 groove 的起点（16）为 0 点、以 `groove.bottom() - sliderLength`（584 - 60 = 524）为终点，而绘制和按下时的点击偏移却使用 slider 的 0 起点全长几何。两把"尺子"不一致导致：

```
实际换算：value = (mouseY - 点击偏移 - 16) / 509 × range     ← groove 尺子
应有换算：value = (mouseY - 点击偏移)     / 540 × range     ← slider 尺子
```

首次移动即按错误的坐标系换算 → 瞬间跳变；行程 509 vs 540 造成持续约 1.06 倍偏差，越拖越偏；手柄行程实际为 0~600，换算尺子却认为只有 16~584，因此两端提前触底。原生样式没有此问题，因为其 groove 与 slider 出自同一套 QCommonStyle 算法，坐标系天然一致。

与"隐藏滚动条两端按钮"（`QScrollBar::add-line/sub-line { width: 0; height: 0 }`）**无关**：删除这些规则跳变依旧。准确地说，当元素规则没有 box 时，QSS 引擎会"忘记"用户已用 QSS 接管整个滚动条，仍按原生方式为已不存在的按钮预留空间。

### 修复

在 `QScrollBar` 元素规则上显式声明 `margin: 0px`（或 `padding: 0px`），使 `hasBox()` 为真，groove 改用 QSS 全长几何，与 slider 统一坐标系：

```css
QScrollBar {
    background-color: transparent;
    border-style: none;
    margin: 0px;
}
```

### 验证

- 实测 Qt 6.11.1（MSVC 2022 64-bit）：修复后手柄精确跟手（鼠标 +30px 手柄 +30px），与原生样式行为一致。
- 此修复与 OBS Studio 2018 年对同类问题的处理相同（`QScrollBar { margin: 0px }`）。
