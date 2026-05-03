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

## 关键文件索引

| 文件 | 说明 |
|------|------|
| `UI/Views/ClipEditor/ParamEditor/ParamEditorView.h/.cpp` | 顶层参数编辑器，组合工具栏+信息区+图形视图 |
| `UI/Views/ClipEditor/ParamEditor/ParamEditorToolBarView.h/.cpp` | 参数选择工具栏（前景/背景 ComboBox + 交换按钮） |
| `UI/Views/ClipEditor/ParamEditor/ParamEditorGraphicsView.h/.cpp` | 图形视图，持有前景/背景两个 CommonParamEditorView 实例 |
| `UI/Views/ClipEditor/ParamEditor/ParamEditorGraphicsScene.h/.cpp` | 图形场景 |
| `UI/Views/ClipEditor/ParamEditor/ParamEditorInfoArea.h/.cpp` | 左侧刻度标签 |
| `UI/Views/ClipEditor/CommonParamEditorView.h/.cpp` | 通用参数编辑视图（手绘/擦除 + DrawCurve 渲染） |
| `UI/Views/ClipEditor/PianoRoll/PitchAnchorEditorView.h/.cpp` | 音高锚点渲染（可参考锚点绘制方式） |
| `UI/Views/ClipEditor/PianoRoll/EditPitchAnchorHandler.h/.cpp` | 音高锚点交互逻辑（可参考锚点交互模式） |
| `Model/AppModel/Params.h/.cpp` | ParamInfo::Name 枚举 + Param 数据模型 |
| `Model/AppModel/ParamProperties.h/.cpp` | 参数属性定义（值域、显示模式等） |
| `Utils/ParamUtils.h/.cpp` | 参数名称/属性查询单例 |
| `UI/Utils/TrackColorPalette.h/.cpp` | 轨道颜色池（12 色循环），speaker 颜色来源 |
| `docs/音高参数编辑系统.md` | 音高参数编辑系统架构文档（含曲线模型、提交流程等） |

以上路径均相对于 `src/app/`，除 docs 外。

---

## 架构设计

### 集成方式：共存叠加

选择共存叠加而非互斥切换，是为了在 speaker mix 模式下仍然能在背景中叠加显示一个普通参数曲线，保持灵活性。

`SpeakerMixEditorView` 不复用 `CommonParamEditorView`，因为两者数据维度不同：`CommonParamEditorView` 围绕单值 `DrawCurve` 设计（渲染、手绘编辑），而 speaker mix 需要多维堆叠面积图和关键帧锚点交互，硬塞公共基类得不偿失。

在 `ParamEditorGraphicsView` 中新增独立的 `m_speakerMixView` 成员，而非改变 `m_foreground` 的类型，是为了最小化对现有代码的侵入——只需在 `setForeground` 里加判断分支即可。

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
    QColor color;    // 从 TrackColorPalette::baseColor(index) 按序号获取
};

struct SpeakerMixKeyframe {
    int tick;
    QList<double> weights;  // 长度 = speaker 数 - 1，最后一个 speaker 的权重 = 1.0 - sum(weights)
};
```

**Speaker 数据来源：** 第一阶段硬编码 3 条测试 speaker。颜色从 `TrackColorPalette`（12 色循环池）按序号取 `baseColor(index)`。

### 新增/修改组件

| 组件 | 说明 |
|------|------|
| `SpeakerMixEditorView` | 新建。`TimeOverlayView` 子类，负责堆叠面积图渲染、关键帧锚点渲染与交互 |
| `SpeakerMixToolBarView` | 新建。Speaker mix 专用工具栏，包含关键帧导航（上一个/下一个）和 speaker 列表（彩色圆点+名称） |
| `ParamEditorGraphicsView` | 修改。新增 `m_speakerMixView` 成员，`setForeground` 中增加判断分支 |
| `ParamEditorToolBarView` | 修改。前景参数 ComboBox 中添加 "Speaker Mix" 选项 |
| `ParamEditorView` | 修改。管理两个工具栏的显示/隐藏切换 |

### 渲染逻辑

堆叠面积图绘制：
1. 在每个可见像素 x 位置，对相邻关键帧做插值，得到各 speaker 的权重
2. 从底部开始，依次堆叠各 speaker 的面积区域
3. 每个区域用对应 speaker 颜色的半透明填充
4. 关键帧位置绘制锚点圆点
5. Hover 时显示 tooltip，列出各 speaker 名称和权重百分比

### 交互逻辑

#### 锚点与分割点

关键帧在每两个相邻 speaker 之间的分界线上各有一个**分割点**（圆点）。拖拽操作作用于分割点而非 speaker 本身。

#### 拖拽模式

| 操作 | 行为 |
|------|------|
| 直接上下拖动分割点 | 仅改变该分割点相邻的两个 speaker 的比例，其余 speaker 不变 |
| Alt + 上下拖动分割点 | 等比例压缩/拉伸分割点两边所有 speaker 的比例 |

#### 其他操作

- 点击空白：在该 tick 位置添加关键帧（权重取插值值）
- 右键/Delete：删除关键帧
- Hover 锚点：显示各 speaker 比例的 tooltip

### 工具栏

两个工具栏根据前景参数类型互斥显示：

| 条件 | 显示的工具栏 |
|------|------------|
| 前景为普通参数 | `ParamEditorToolBarView`（现有） |
| 前景为 speaker mix | `SpeakerMixToolBarView`（新增） |

`SpeakerMixToolBarView` 包含：
- 关键帧导航：上一个关键帧 / 下一个关键帧按钮
- Speaker 列表：彩色圆点 + 名称（每个 speaker 一项）

---

## UI 参考

工具栏在 speaker mix 模式下的布局（参考效果图）：

```
参数 ✏ 声线▼  👁 (无)▼  包络 实参  ▶ ✏ ◆ ◆ ◀ ▶  ● Opencpop▼  ● 夏叶子▼  ● 绮萱▼
```

---

## 已决定事项

- [x] 工具栏：需要独立的 `SpeakerMixToolBarView`，仅在前景参数为 speaker mix 时显示，包含关键帧导航和 speaker 列表
- [x] 拖拽策略：直接拖动分割点仅改变相邻两个 speaker 比例；Alt+拖动等比压缩/拉伸两侧
- [x] Speaker 列表来源：第一阶段硬编码 3 条测试数据，颜色从 `TrackColorPalette` 按序号取

## 待讨论事项

- [ ] 详细操作逻辑（添加/删除关键帧的具体交互方式）

---

## 进度

- [x] 调研现有参数编辑系统架构
- [x] 确定 speaker mix 与现有参数的差异
- [x] 确定集成方式：共存叠加（非互斥切换）
- [x] 确定 `SpeakerMixEditorView` 独立于 `CommonParamEditorView`
- [x] 确定工具栏方案：独立 `SpeakerMixToolBarView`
- [x] 确定拖拽策略：分割点直接拖 / Alt 等比拖
- [x] 确定数据来源：第一阶段硬编码 3 条 speaker，颜色从 TrackColorPalette 取
- [ ] 讨论详细操作逻辑
- [ ] 实现 `SpeakerMixEditorView`
- [ ] 修改 `ParamEditorGraphicsView` 集成 speaker mix
- [ ] 修改工具栏支持 speaker mix 选项
