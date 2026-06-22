# Speaker Mix Editor (声线混合编辑器)

## 概述

在钢琴卷帘底部的参数编辑器（ParamEditorView）中新增 speaker mix（声线混合）编辑功能。Speaker mix 控制不同 speaker（说话人/声线）混合比例随时间的变化，所有 speaker 权重之和始终为 100%，通过关键帧插值实现平滑过渡。

当前模型接入已完成，`SpeakerMixEditorView` 只负责 Dynamic Mix 关键帧编辑；Fixed Mix 的
speaker 组合和固定比例由 `SpeakerMixDialog` / preset 管理入口负责。

Phase 6 后，Fixed Mix 的正式入口已迁移到 track/clip 的 singer/speaker 二级菜单；剪辑工具栏上的临时
`Speaker Mix` 按钮已移除。Dynamic Mix 仍通过参数编辑器中的 Speaker Mix 页进入。

## 与现有参数的差异

| 维度 | 现有参数 | Speaker mix |
|------|---------|------------|
| 值维度 | 单值曲线（每 tick 一个值） | 多值（每 tick N 个权重，总和=100%） |
| 渲染方式 | 单色填充/曲线 | 堆叠面积图，每个 speaker 一个颜色 |
| 编辑方式 | 手绘 DrawCurve | 关键帧锚点 |
| 数据模型 | `Param` → `QList<Curve*>` | 独立模型：关键帧列表，每帧含 N 个权重 |

## 分阶段计划

### 第一阶段：视图层功能（完成）

目标：实现 speaker mix 的渲染和交互，数据临时存放在视图内部，不涉及文档模型。

### 第二阶段：模型集成（完成）

目标：将 speaker mix 数据纳入文档模型，支持序列化/反序列化、撤销/重做。

当前 Dynamic Mix 已改为显式启用流程：

- 未启用：`mode = FixedMix` 且 `dynamicKeyframes` 为空，参数页显示说明和启用按钮
- 启用：`mode = DynamicMix`，使用 `dynamicKeyframes`，允许编辑关键帧
- Bypassed：`mode = FixedMix` 且 `dynamicKeyframes` 非空，显示并允许编辑关键帧，但实际生效使用 `fixedWeights`
- Stop Dynamic：清空 `dynamicKeyframes`，回到 clip 自定义 Fixed Mix
- `fixedWeights` 与 `dynamicKeyframes` 可以同时保留；`mode` 表示当前是否旁路动态自动化

---

## 关键文件索引

| 文件 | 说明 |
|------|------|
| `UI/Views/ClipEditor/ParamEditor/SpeakerMixEditorView.h/.cpp` | Speaker mix 编辑视图（堆叠面积图渲染 + 关键帧 + 交互） |
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
| `UI/Utils/AppColorPalette.h/.cpp` | 应用基础配色池（12 色循环），speaker mix 颜色派生来源 |
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

### 数据模型（当前实现）

```cpp
struct SpeakerInfo {
    QString name;    // e.g. "Opencpop", "夏叶子", "绮萱"
    QColor color;    // UI 派生颜色，不进入 SpeakerMixData
};

struct SpeakerMixKeyframe {
    int tick;
    QList<double> weights;  // 长度 = speaker 数 - 1，最后一个 speaker 的权重 = 1.0 - sum(weights)
};
```

**Speaker 数据来源：** 当前来自 `SingingClip::SpeakerMixData::sources`。显示名优先使用
`SpeakerInfo::name()`，为空时回退 `speaker.id()`。

**Keyframe 数据来源：**

- Dynamic Mix 启用或 Bypassed：显示并编辑 `dynamicKeyframes`
- Dynamic Mix 未启用：显示参数页空状态；背景可显示由 `fixedWeights` 构造的只读平直比例

**颜色来源：** 当前通过 `SpeakerMixColorResolver` 基于 `SingingClip::singerInfo().speakers()`
的稳定顺序派生；找不到 singer 上下文或 speaker id 时按当前 sources 下标回退。

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

#### 关键帧与分割点

每个关键帧在每两个相邻 speaker 之间的分界线上各有一个**分割点**（圆点）。3 个 speaker 时每个关键帧有 2 个分割点。分割点不能独立增删，增删操作作用于整个关键帧。

#### 初始关键帧

开启 Dynamic Mix 时，时间轴开头会有一个 **tick 0 关键帧**作为初始锚点。首次开启且没有
`dynamicKeyframes` 时，它由当前 `fixedWeights` 创建。该关键帧不可水平移动、不可删除，但分界点可编辑。
当前通过判断 `kf.tick == 0` 识别初始关键帧。

#### 添加关键帧

**双击**空白区域在该 tick 位置添加关键帧，权重取当前插值值。单击保留给选择操作。

#### 选中与多选

| 操作 | 行为 |
|------|------|
| 单击分割点 | 选中该分割点，同时关键帧视为已选中 |
| 在空白处拖动 | 区间选择（框选），范围内的所有关键帧被选中（y 值忽略，参考钢琴卷帘音符区间选择） |

不支持同时选中多个分割点。

#### 删除关键帧

- 选中关键帧/分割点后按 **Delete** 键删除该关键帧
- **右键菜单**中选择"删除"
- 初始关键帧不可删除

#### 拖拽分割点

| 操作 | 行为 |
|------|------|
| 直接上下拖动分割点 | 仅改变该分割点相邻的两个 speaker 的比例，其余 speaker 不变 |
| Alt + 上下拖动分割点 | 等比例压缩/拉伸分割点两边所有 speaker 的比例 |

#### 插值模式

当前仅支持 **线性插值**。

Speaker mix 的权重满足 w_i >= 0 且 Σw_i = 1，构成 N 维概率单纯形（probability simplex）约束。Hermite / Catmull-Rom 等曲线插值在此约束下会因 overshoot 破坏约束（权重变负或总和不等于 1），且独立权重插值与累积值插值均存在交叉影响问题。

**未来平滑插值方案**（待后续实现）：
- **Softmax 插值**：在无约束空间（logit 空间）做 Hermite 插值，再 softmax 映射回概率空间
- **Log-ratio 插值**：对权重做对数比变换，在变换空间中平滑插值
- **Stick-breaking 插值**：将权重分解为条件概率序列，逐维独立插值

#### 边界行为

- **第一个关键帧之前**：由初始关键帧覆盖（初始关键帧位于 clip 开头，不可移动）
- **最后一个关键帧之后**：保持最后一帧的值不变，绘图时填充到视口右边缘

#### Hover 信息

鼠标悬停在分割点上时显示 tooltip，列出该关键帧所有 speaker 的名称和权重百分比。

#### 关键帧导航

工具栏的"上一个/下一个关键帧"按钮：
- 滚动视口，通过 `TimeGraphicsView::centerAt(tick)` 将目标关键帧居中
- 同时移动播放头到目标关键帧位置

#### 交互实现方案（待实现）

##### 状态模型

在 `SpeakerMixEditorView` 中添加交互状态：

```cpp
struct {
    SpeakerMixKeyframe *hoveredKeyframe = nullptr;
    int hoveredSplitIndex = -1;           // 哪个分割点被 hover（0 ~ n-2）
    SpeakerMixKeyframe *selectedKeyframe = nullptr;
    int selectedSplitIndex = -1;

    bool dragging = false;
    QPointF dragStartPos;
    double dragStartWeight = 0;           // 拖拽开始时该分割点的累积权重
    bool altDrag = false;

    bool selecting = false;
    QRectF selectionRect;
    QList<SpeakerMixKeyframe *> selectedKeyframes;  // 区间选择的结果
} m_state;
```

##### 需要 override 的方法

- `mousePressEvent` — 单击选中 / 开始区间选择
- `mouseMoveEvent` — 拖拽分割点 / 更新区间选择框
- `mouseReleaseEvent` — 结束拖拽 / 结束区间选择
- `mouseDoubleClickEvent` — 双击添加关键帧
- `hoverMoveEvent` — 更新 hover 状态（需要 `setAcceptHoverEvents(true)`，已由 `setTransparentMouseEvents(false)` 启用）
- `keyPressEvent` — Delete 删除选中关键帧
- `contextMenuEvent` — 右键菜单（删除 / 切换插值模式）

##### 命中检测

参考 `EditPitchAnchorHandler::anchorNodeAt()` 的模式：遍历所有关键帧，将每个分割点的 (tick, 累积权重) 转为 item 坐标，计算与鼠标位置的欧氏距离，半径 6px 内视为命中。返回命中的 keyframe 指针 + split index。

```cpp
struct HitResult {
    SpeakerMixKeyframe *keyframe = nullptr;
    int splitIndex = -1;  // 命中的分割点序号，-1 表示未命中
};
HitResult hitTest(const QPointF &itemPos) const;
```

##### 事件流

| 事件 | 条件 | 行为 |
|------|------|------|
| `mousePressEvent` 左键 | 命中分割点 | 选中该分割点，记录 dragStartPos |
| `mousePressEvent` 左键 | 未命中 | 开始区间选择，设 selecting=true |
| `mouseMoveEvent` | dragging | 移动距离 > 3px 后进入拖拽；检测 Alt 键；计算新权重并更新关键帧 |
| `mouseMoveEvent` | selecting | 更新 selectionRect，找出范围内的关键帧 |
| `mouseReleaseEvent` | dragging | 结束拖拽，确认权重 |
| `mouseReleaseEvent` | selecting | 结束区间选择，记录 selectedKeyframes |
| `mouseDoubleClickEvent` | 任意位置 | 在该 tick 插入新关键帧，权重取插值值 |
| `hoverMoveEvent` | — | 更新 hoveredKeyframe / hoveredSplitIndex，触发 update() |
| `keyPressEvent` Delete | 有选中 | 删除选中的关键帧（初始帧除外） |
| `contextMenuEvent` | 命中分割点 | 弹出菜单：删除等轻量操作 |

##### 拖拽权重计算

**直接拖（无 Alt）：** 拖动分割点 i，只改变 speaker[i] 和 speaker[i+1] 的权重。将鼠标 y 转为新的累积权重值，clamp 到上下相邻分割点范围内，差值分配给 weights[i]，反向差值分配给下一个 speaker。

**Alt 拖：** 等比例压缩/拉伸分割点两侧。计算拖拽产生的累积权重偏移 delta，上方所有权重等比缩放，下方所有权重等比缩放，保持总和=100%。

##### 渲染增强

在 `paint()` 中根据状态额外绘制：
- hover 分割点：放大圆点 + 外圈（参考 `PitchAnchorEditorView` 的 hover ring，半径 6px）
- 选中分割点：高亮色 `(155, 186, 255)`
- 区间选择框：蓝色半透明圆角矩形 `(155, 186, 255, 64)` 填充 + `(155, 186, 255, 200)` 描边，同锚点编辑器样式

##### 右键菜单

参考 `EditPitchAnchorHandler::contextMenuEvent` 实现：
- 创建 `Menu`（CMenu 子类），添加删除等轻量操作
- `menu->exec(event->globalPos())` 同步显示
- `menu->deleteLater()` 清理

##### 精确编辑策略

动态声线混合不再规划单独的关键帧属性对话框。

原因：

- 经过实际测试，`SpeakerMixEditorView` 本身的分割点拖拽精度已经足够。
- 再把当前关键帧接到 `SpeakerMixDialog` 会形成第二套比例编辑入口，增加状态同步和 undo 粒度复杂度。
- `SpeakerMixDialog` 后续聚焦 Fixed Mix 的 speaker 列表和固定比例配置；Dynamic Mix 的 keyframe 权重编辑只在参数编辑器内完成。

##### 参考的交互模式

| 关注点 | 参考来源 | 文件 |
|--------|---------|------|
| 命中检测 | `EditPitchAnchorHandler::anchorNodeAt()` | `PianoRoll/EditPitchAnchorHandler.cpp` |
| 鼠标状态机 | `EditPitchAnchorHandler` 的 press/move/release | 同上 |
| 区间选择框渲染 | `PitchAnchorEditorView::drawSelectionRect()` | `PianoRoll/PitchAnchorEditorView.cpp` |
| Hover 圆点样式 | `PitchAnchorEditorView::drawAnchorCurves()` | 同上 |
| 右键菜单 | `EditPitchAnchorHandler::contextMenuEvent()` | `PianoRoll/EditPitchAnchorHandler.cpp` |
| 拖拽阈值 | Manhattan distance > 3px | 同上 |

### 工具栏

两个工具栏根据前景参数类型互斥显示：

| 条件 | 显示的工具栏 |
|------|------------|
| 前景为普通参数 | `ParamEditorToolBarView`（现有） |
| 前景为 speaker mix | `SpeakerMixToolBarView`（新增） |

`SpeakerMixToolBarView` 包含：
- Active 状态：Bypass、停止使用动态混合、关键帧导航、Speaker 列表
- Bypassed 状态：恢复、停止使用动态混合、关键帧导航、Speaker 列表
- 未启用状态：仅显示 Speaker 列表，启用入口在参数页空状态中

Dynamic Mix 显式启用行为：

- 仅在当前 clip 具备 Fixed Mix sources（至少 2 个 speaker）时可启用
- Follow Track clip 启用时，显式复制当前轨道配置到 clip 并停止跟随
- 首次启用时，如果 `dynamicKeyframes` 为空，用 `fixedWeights` 初始化第一个关键帧
- Bypass 只切换 `mode = FixedMix`，保留 keyframes；Resume 切回 `mode = DynamicMix`
- Stop Dynamic 清空 keyframes 并回到 FixedMix

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
- [x] Speaker 列表来源：当前 clip 的 `SpeakerMixData::sources`，颜色通过 `SpeakerMixColorResolver` 按 singer speaker 稳定顺序派生
- [x] 添加关键帧：双击空白区域添加，权重取插值值
- [x] 删除关键帧：Delete 键 + 右键菜单两种方式
- [x] 选中机制：单击选中分割点（关键帧视为已选中）；区间框选多个关键帧（y 值忽略）
- [x] 初始关键帧：clip 开头放置不可移动/删除的关键帧，值为默认比例（均分）
- [x] 插值模式：当前仅线性插值（Hermite 在概率单纯形约束下不可行，未来考虑 Softmax/Log-ratio/Stick-breaking）
- [x] 边界行为：初始关键帧覆盖前方；最后关键帧之后保持不变，填充到视口右边缘
- [x] 关键帧导航：滚动视口 centerAt(tick) + 移动播放头
- [x] 不再为 Dynamic Mix 关键帧提供单独属性对话框；权重编辑只在 `SpeakerMixEditorView` 内完成
- [x] Dynamic Mix 需要显式启用；Bypass 时回退 Fixed Mix 且不删除 keyframes
- [x] `SpeakerMixEditorView` 绑定 `SingingClip::SpeakerMixData`，移除硬编码 speaker/keyframes
- [x] Dynamic Mix 关键帧编辑通过 `ReplaceSpeakerMixAction` 提交
- [x] `fixedWeights` 和 `dynamicKeyframes` 可同时保留，`mode` 只表示当前生效模式

## 待讨论事项

（暂无）

---

## 已修复问题

- [x] 指针失效：交互状态中的 `SpeakerMixKeyframe*` 改为 int index，避免 QList 插入/删除后悬空指针
- [x] 拖拽无响应：`startDrag` 设 `dragging=false` 但 `mouseMoveEvent` 只在 `dragging==true` 时调用 `updateDrag`，形成死锁。修复：条件改为 `dragging || selectedKeyframeIndex >= 0`
- [x] 工具栏覆盖：独立的 `SpeakerMixToolBarView` 替换了参数选择工具栏，导致无法切回其他参数。修复：将 speaker mix 控件（导航+speaker列表）嵌入 `ParamEditorToolBarView` 内部作为可显隐区段，不再使用独立的 `SpeakerMixToolBarView`
- [x] 初始关键帧右键菜单：之前对 tick==0 直接 return 不显示菜单。修复：显示插值模式切换，仅隐藏删除选项
- [x] Hermite 插值交叉影响：独立权重和累积值两种 Hermite 插值均因概率单纯形约束导致交叉影响。决定：移除 Hermite，当前只支持线性插值
- [x] 区间选择检查了 y 范围：改为只按 x 范围选，选择框高度 = 视口高度（beam 模式）
- [x] Delete 键无响应：QGraphicsItem 未设 `ItemIsFocusable`，添加后在 mousePressEvent 中 setFocus
- [x] 区间选中样式：选中的关键帧竖线变蓝，圆点变蓝；选择框为 beam 模式（只有左右边线）
- [x] 区间选中实时更新：拖拽过程中实时更新 selectedKeyframeIndices，不等松手
- [x] 批量删除：Delete 键可删除区间选中的所有关键帧（跳过初始帧）
- [x] 圆点半径调整：当前外圈半径 `kDotRadius = 6px`，内圈半径 `kInnerDotRadius = 4px`
- [x] 右键菜单：`QMenu` 改为项目 `Menu` 类，`popup()` 改为 `exec()` + `deleteLater()`，初始关键帧显示菜单但禁用删除
- [x] Speaker Mix 工具栏闪烁：关键帧编辑提交后会刷新 toolbar speaker chips。修复：`setSpeakers()`、Dynamic 开关 enabled/checked 更新增加幂等判断，数据未变化时不重建控件

## 待修复问题

- [x] 初步验收发现：开启 Dynamic Mix 后，双击空白区域存在无法创建关键帧的情况。已改为显式启用 Dynamic Mix；Follow Track clip 通过启用按钮复制为 clip 自定义状态后再编辑。
- [ ] Fixed Mix preset 管理功能当前完整但偏复杂，需要后续讨论是否压缩为更轻量的“保存/另存/删除”流程，减少用户在 dialog 内的状态判断成本。
- [ ] Fixed Mix preset 应用后的 UI 表达需要重新设计：组合框和 clip 标签不应显示第一个 speaker 造成误导；speaker 二级菜单需要补充当前选中 preset 的状态样式，避免用户无法确认当前 preset。

---

## 进度

- [x] 调研现有参数编辑系统架构
- [x] 确定 speaker mix 与现有参数的差异
- [x] 确定集成方式：共存叠加（非互斥切换）
- [x] 确定 `SpeakerMixEditorView` 独立于 `CommonParamEditorView`
- [x] 确定工具栏方案：speaker mix 控件嵌入 `ParamEditorToolBarView` 内部（不再使用独立 `SpeakerMixToolBarView`）
- [x] 确定拖拽策略：分割点直接拖 / Alt 等比拖
- [x] 确定数据来源：当前 clip 的 `SpeakerMixData::sources` / `dynamicKeyframes`
- [x] 确定详细操作逻辑（添加/删除/选中/拖拽/插值/边界/导航）
- [x] 实现 `SpeakerMixEditorView`（数据模型 + 堆叠面积图渲染 + 关键帧竖线/圆点）
- [x] 修改 `ParamEditorGraphicsView` 集成 speaker mix（m_speakerMixView，前景切换）
- [x] 修改 `ParamEditorToolBarView` 添加 Speaker Mix 选项（index 映射到 ParamInfo::Unknown）
- [x] 修改 `ParamEditorInfoArea` 添加 clearParamProperties()
- [x] 修改 `ParamEditorView` 处理 speaker mix 模式下的 info area
- [x] 实现交互逻辑（命中检测 + 选中 + 拖拽 + 双击添加 + 删除 + 右键菜单 + 区间选择 + hover）
- [x] 实现 speaker mix 工具栏控件（关键帧导航 + speaker 列表，嵌入 ParamEditorToolBarView）
- [x] 修复指针失效：交互状态改用 index
- [x] 修复拖拽状态机死锁
- [x] 修复工具栏覆盖：嵌入式集成
- [x] 修复初始关键帧右键菜单
- [x] 简化为线性插值（移除 Hermite）
- [x] 修复区间选择忽略 y 范围 + beam 模式选择框
- [x] 修复 Delete 键无响应（ItemIsFocusable）
- [x] 区间选中样式 + 实时更新 + 批量删除
- [x] 圆点半径调整（当前外圈 6px，内圈 4px）
- [x] 右键菜单改用项目 Menu 类（exec + deleteLater，初始帧禁用删除而非不显示菜单）
- [x] 关键帧水平移动：点击竖线（非分割点）可左右拖拽移动关键帧位置，初始帧不可移动，不可越过相邻帧
- [x] 接入真实模型：`SpeakerMixEditorView` 绑定 `SingingClip::SpeakerMixData`
- [x] 移除硬编码 speaker/keyframes
- [x] Dynamic Mix 显式启用入口接入参数页空状态
- [x] 启用 Dynamic：无 keyframe 时由 fixedWeights 创建 tick 0 首帧；已有 keyframes 时切回 active
- [x] Bypass Dynamic：回到 fixedWeights，不删除 dynamicKeyframes
- [x] Stop Dynamic：清空 dynamicKeyframes；fixedWeights 无效时用首个 dynamic keyframe 补齐
- [x] 编辑权重、添加/删除/移动 keyframe 后通过 `ReplaceSpeakerMixAction` 提交，支持 undo/redo
- [x] workspace 同时读写 `fixedWeights` 与 `dynamicKeyframes`
- [x] 修复关键帧编辑后工具栏 speaker 指示器闪烁
- [x] Fixed Mix preset 正式入口接入 track/clip singer 二级菜单
- [x] Track 新增 `SpeakerMixData`，Follow Track 统一跟随 singer/speaker/speaker mix
- [x] 剪辑工具栏临时 `Speaker Mix` 按钮移除
- [ ] 进一步优化编辑体验
- [x] 统一 Fixed Mix 对话框与 Dynamic Mix 编辑视图的 speaker 颜色规则
- [x] 整理 Fixed Mix 对话框在 DynamicMix 状态下的初始化语义

### 当前实现细节备忘

- `SpeakerMixEditorView` 继承 `TimeOverlayView`，当前带 `Q_OBJECT`，用于发出 `speakerMixEdited`
- 填充色使用 `AppColorPalette::speakerMixParamFill(index)`，颜色下标由 `SpeakerMixColorResolver` 解析
- 分割线白色 `(220, 220, 220, 200)`，关键帧竖线 `(220, 220, 220, 160)`
- Speaker mix 在前景 ComboBox 中使用 `ParamInfo::Unknown` 作为标识值（index + 1 自然映射到 Unknown=10）
- Swap 按钮在 speaker mix 模式下禁用（不允许交换到背景）
- 数据结构：`SpeakerMixSpeaker`（避免与 `PackageManager/Models/SpeakerInfo` 同名冲突）
- 交互状态全部使用 int index 引用关键帧，不使用指针（避免 QList 操作后失效）
- Speaker mix 工具栏控件嵌入 `ParamEditorToolBarView`，通过 `setSpeakerMixMode(bool)` 控制显隐，包含 Bypass/Resume/Stop Dynamic、关键帧导航和 speaker 指示器
- 拖拽状态机：`startDrag` 设 `dragging=false`（待定），`mouseMoveEvent` 中 `selectedKeyframeIndex >= 0` 时也调用 `updateDrag`，超过阈值后 `dragging=true`
- 插值：仅线性插值，`SpeakerMixKeyframe` 不含 interpMode 字段，右键菜单只有删除
- 键盘事件：构造函数设 `ItemIsFocusable`，mousePressEvent 中 `setFocus()`
- 命中检测：统一矩形判断（|dx| ≤ r 且 |dy| ≤ r），分割点优先；竖线命中排除分割点附近 y 区域
- 拖拽模式：dragSplitIndex >= 0 时上下拖改权重，dragSplitIndex == -1 时左右拖移动关键帧
- 光标：分割点 SizeVerCursor，竖线 SizeHorCursor，无命中 ArrowCursor
- 选择框：beam 模式（全高，只有左右边线），实时更新选中状态
- 选中样式：区间选中的关键帧竖线和圆点都变蓝 `(155, 186, 255)`
- 当前颜色：Fixed Mix 对话框、Dynamic Mix 编辑视图和参数页工具栏都通过 `SpeakerMixColorResolver` 解析同一 speaker 的稳定颜色下标
- Fixed Mix 对话框在 `DynamicMix` 状态下仍恢复固定底座：列表按 `SpeakerMixData::sources` 顺序，比例优先使用 `fixedWeights`，缺失时才用第一帧 `dynamicKeyframes` 兜底
