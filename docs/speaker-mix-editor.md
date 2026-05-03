# Speaker Mix Editor (声线混合编辑器)

## 概述

在钢琴卷帘底部的参数编辑器（ParamEditorView）中新增 speaker mix（声线混合）编辑功能。Speaker mix 控制不同 speaker（说话人/声线）混合比例随时间的变化，所有 speaker 权重之和始终为 100%，通过关键帧插值实现平滑过渡。

## 与现有参数的差异

| 维度 | 现有参数 | Speaker mix |
|------|---------|------------|
| 值维度 | 单值曲线（每 tick 一个值） | 多值（每 tick N 个权重，总和=100%） |
| 渲染方式 | 单色填充/曲线 | 堆叠面积图，每个 speaker 一个颜色 |
| 编辑方式 | 手绘 DrawCurve | 关键帧锚点 |
| 数据模型 | `Param` → `QList<Curve*>` | 独立模型：关键帧列表，每帧含 N 个权重 |

## 分阶段计划

### 第一阶段：视图层功能（当前）

目标：实现 speaker mix 的渲染和交互，数据临时存放在视图内部，不涉及文档模型。

### 第二阶段：模型集成（后续）

目标：将 speaker mix 数据纳入文档模型，支持序列化/反序列化、撤销/重做。

---

## 架构设计

### 集成方式：共存叠加

`SpeakerMixEditorView` 作为独立图层放在 `ParamEditorGraphicsView` 中，与现有的前景/背景 `CommonParamEditorView` 共存。

```
ParamEditorGraphicsView
├── m_background: CommonParamEditorView  [z=1, 仅展示]   — 保持不变
├── m_foreground: CommonParamEditorView  [z=2, 可交互]   — 保持不变
└── m_speakerMixView: SpeakerMixEditorView [新增]        — speaker mix 专用
```

切换逻辑：
- 选择普通参数作为前景 → 显示 `m_foreground`，隐藏 `m_speakerMixView`
- 选择 speaker mix 作为前景 → 隐藏 `m_foreground`，显示 `m_speakerMixView`
- `m_background` 始终可用，不受 speaker mix 模式影响

### 数据模型（第一阶段，视图内部临时存储）

```cpp
struct SpeakerInfo {
    QString name;    // e.g. "Opencpop", "夏叶子", "绮萱"
    QColor color;
};

struct SpeakerMixKeyframe {
    int tick;
    QList<double> weights;  // 与 speakers 列表一一对应，总和 = 1.0
};
```

### 新增/修改组件

| 组件 | 说明 |
|------|------|
| `SpeakerMixEditorView` | 新建。`TimeOverlayView` 子类，负责堆叠面积图渲染、关键帧锚点渲染与交互 |
| `ParamEditorGraphicsView` | 修改。新增 `m_speakerMixView` 成员，`setForeground` 中增加判断分支 |
| `ParamEditorToolBarView` | 修改。前景参数 ComboBox 中添加 "Speaker Mix" 选项 |

### 渲染逻辑

堆叠面积图绘制：
1. 在每个可见像素 x 位置，对相邻关键帧做插值，得到各 speaker 的权重
2. 从底部开始，依次堆叠各 speaker 的面积区域
3. 每个区域用对应 speaker 颜色的半透明填充
4. 关键帧位置绘制锚点圆点
5. Hover 时显示 tooltip，列出各 speaker 名称和权重百分比

### 交互逻辑（第一阶段简化版）

- 点击空白：在该 tick 位置添加关键帧（权重取插值值）
- 拖拽锚点：上下拖拽调整某 speaker 在该关键帧的权重（其余 speaker 等比调整以保持总和=100%）
- 右键/Delete：删除关键帧
- Hover 锚点：显示各 speaker 比例的 tooltip

---

## UI 参考

工具栏在 speaker mix 模式下的布局（参考效果图）：

```
参数 ✏ 声线▼  👁 (无)▼  包络 实参  ▶ ✏ ◆ ◆ ◀ ▶  ● Opencpop▼  ● 夏叶子▼  ● 绮萱▼
```

---

## 待讨论事项

- [ ] 工具栏 UI：speaker mix 模式下的工具栏与普通参数模式差异较大，是否需要独立的工具栏布局？
- [ ] 拖拽权重分配策略：拖一条 speaker 时，其余 speaker 等比缩放，还是其他方式？
- [ ] Speaker 列表来源：第一阶段硬编码测试数据，还是有配置机制？

---

## 进度

- [x] 调研现有参数编辑系统架构
- [x] 确定 speaker mix 与现有参数的差异
- [x] 确定集成方式：共存叠加（非互斥切换）
- [x] 确定 `SpeakerMixEditorView` 独立于 `CommonParamEditorView`
- [ ] 讨论待讨论事项
- [ ] 实现 `SpeakerMixEditorView`
- [ ] 修改 `ParamEditorGraphicsView` 集成 speaker mix
- [ ] 修改工具栏支持 speaker mix 选项
