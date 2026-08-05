# Speaker Mix 编辑器与对话框 — 设计文档

> 状态：✅ 实施完成（Dynamic Mix 关键帧编辑、Fixed Mix 对话框、preset 管理均已落地）
> 本文合并原 `speaker-mix-editor.md` 与 `speaker-mix-dialog.md`；分阶段记录与提交清单已移除。

## 职责划分

- **`SpeakerMixDialog`** 只负责 **Fixed Mix** 底座：speaker 列表 + 固定比例 + 用户预设管理。
- **`SpeakerMixEditorView`**（参数编辑器内）只负责 **Dynamic Mix** 关键帧权重编辑。
- Fixed Mix 正式入口在 track/clip 的 singer/speaker 二级菜单；剪辑工具栏的临时 `Speaker Mix` 按钮已移除。
- 不再规划单独的关键帧属性对话框：分割点拖拽精度已足够，再引入第二套比例编辑入口只会增加状态同步与 undo 粒度复杂度。

## 与现有参数的差异

| 维度 | 现有参数 | Speaker mix |
|------|---------|------------|
| 值维度 | 单值曲线（每 tick 一个值） | 多值（每 tick N 个权重，总和=100%） |
| 渲染方式 | 单色填充/曲线 | 堆叠面积图，每个 speaker 一个颜色 |
| 编辑方式 | 手绘 DrawCurve | 关键帧锚点 |
| 数据模型 | `Param` → `QList<Curve*>` | 独立模型：关键帧列表，每帧含 N 个权重 |

## 集成方式：共存叠加

`SpeakerMixEditorView` 不复用 `CommonParamEditorView`（数据维度不同：后者围绕单值 `DrawCurve` 设计）。在 `ParamEditorGraphicsView` 中新增独立成员，最小化对现有代码的侵入：

```
ParamEditorGraphicsView
├── m_background: CommonParamEditorView  [z=1, 仅展示]   — 保持不变
├── m_foreground: CommonParamEditorView  [z=2, 可交互]   — 保持不变
└── m_speakerMixView: SpeakerMixEditorView [新增]        — speaker mix 专用
```

- 前景为普通参数 → 显示 `m_foreground`，隐藏 `m_speakerMixView`
- 前景为 speaker mix → 隐藏 `m_foreground`，显示 `m_speakerMixView`
- `m_background` 始终可用

## 数据模型

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

- Speaker 来源：`SingingClip::SpeakerMixData::sources`；显示名优先 `SpeakerInfo::name()`，为空回退 `speaker.id()`。
- Keyframes：Dynamic Mix 启用或 Bypassed 时显示并编辑 `dynamicKeyframes`；未启用时显示参数页空状态，背景可显示由 `fixedWeights` 构造的只读平直比例。
- 颜色：`SpeakerMixColorResolver` 基于 `singerInfo().speakers()` 稳定顺序派生；找不到 singer 上下文或 speaker id 时按 sources 下标回退。

### Dynamic Mix 显式启用

- 未启用：`mode = FixedMix` 且 `dynamicKeyframes` 为空，参数页显示说明和启用按钮。
- 启用：`mode = DynamicMix`，使用 `dynamicKeyframes`，允许编辑关键帧。
- Bypassed：`mode = FixedMix` 且 `dynamicKeyframes` 非空，可编辑关键帧但生效使用 `fixedWeights`。
- Stop Dynamic：清空 `dynamicKeyframes`，回到 clip 自定义 Fixed Mix。
- 仅在当前 clip 具备 Fixed Mix sources（至少 2 个 speaker）时可启用；Follow Track clip 启用时显式复制当前轨道配置到 clip 并停止跟随；首次启用且 `dynamicKeyframes` 为空时用 `fixedWeights` 初始化第一个关键帧。

## 渲染逻辑（堆叠面积图）

1. 每个可见像素 x 位置对相邻关键帧插值，得到各 speaker 权重；
2. 从底部开始依次堆叠各 speaker 的面积区域；
3. 每个区域用对应 speaker 颜色半透明填充；
4. 关键帧位置绘制锚点圆点；
5. Hover 时 tooltip 列出各 speaker 名称和权重百分比。

## 交互逻辑（SpeakerMixEditorView）

### 关键帧与分割点

每个关键帧在每两个相邻 speaker 之间的分界线上各有一个**分割点**（圆点）。3 个 speaker 时每帧 2 个分割点。分割点不能独立增删，增删作用于整个关键帧。

**命中检测 z 序规则**（多关键帧/多分割点重叠时的优先级）：时间靠后的关键帧优先，speaker index 较大的分割点优先。顶部边缘分割点必须能再次拖下来（曾修复过 hit-test 使分割点卡在顶部的问题）。

### 初始关键帧

开启 Dynamic Mix 时，时间轴开头有 **tick 0 关键帧**作为初始锚点；首次开启且无 `dynamicKeyframes` 时由当前 `fixedWeights` 创建。该帧不可水平移动、不可删除（`kf.tick == 0` 识别），但分割点可编辑。

### 增删与选择

- 添加：**双击**空白区域在该 tick 添加，权重取当前插值值（单击保留给选择）。
- 选择：单击分割点选中（关键帧视为已选中）；空白处拖动为区间框选（y 忽略，参考钢琴卷帘音符区间选择）。不支持同时选中多个分割点。
- 删除：选中后按 **Delete** 或右键菜单"删除"；初始关键帧不可删除。

### 拖拽分割点

| 操作 | 行为 |
|------|------|
| 直接上下拖动 | 仅改变该分割点相邻的两个 speaker 的比例，其余不变（clamp 到上下相邻分割点范围内） |
| Alt + 上下拖动 | 等比例压缩/拉伸分割点两边所有 speaker 的比例（保持总和=100%） |

### 插值模式

当前仅支持**线性插值**。权重满足 `w_i >= 0 且 Σw_i = 1`（N 维概率单纯形约束），Hermite / Catmull-Rom 会 overshoot 破坏约束。

未来平滑插值方案（待实现，三选一）：Softmax 插值（logit 空间 Hermite + softmax 映射）、Log-ratio 插值、Stick-breaking 插值。

### 边界行为

- 第一个关键帧之前：由初始关键帧覆盖（位于 clip 开头，不可移动）。
- 最后一个关键帧之后：保持最后一帧值，填充到视口右边缘。
- Hover：分割点放大圆点 + 外圈（参考 `AnchorOverlayView` hover ring）；选中分割点高亮 `(155, 186, 255)`。
- 区间选择框：蓝色半透明圆角矩形 `(155, 186, 255, 64)` 填充 + `(155, 186, 255, 200)` 描边。

### 关键帧导航

工具栏"上一个/下一个关键帧"：`TimeGraphicsView::centerAt(tick)` 居中 + 播放头移动。

## 工具栏

两个工具栏按前景参数类型互斥显示：前景为普通参数 → `ParamEditorToolBarView`；前景为 speaker mix → `SpeakerMixToolBarView`。

`SpeakerMixToolBarView` 包含：Active 状态（Bypass、停止使用动态混合、关键帧导航、Speaker 列表）/ Bypassed 状态（恢复、停止使用动态混合、关键帧导航、Speaker 列表）/ 未启用状态（仅 Speaker 列表，启用入口在参数页空状态）。

Bypass 只切 `mode = FixedMix` 保留 keyframes；Resume 切回 `mode = DynamicMix`；Stop Dynamic 清空 keyframes 回 FixedMix。

---

## SpeakerMixDialog（Fixed Mix 配置 + preset 管理）

### 布局

1. **Preset bar** — 预设 ComboBox + 新建 / 保存 / 另存为 / 删除 / 重置
2. **Tag 选择区** — `FlowLayout + TagButton`，每个 speaker 一个 tag；checked = 参与混合
3. **SpeakerMixList** — 仅显示已勾选的 speaker（`QListWidget`，拖拽排序、ComboBox、位置标签；内部仍使用 speaker id）
4. **SpeakerMixBar** — 自绘比例条，拖拽分割点调比例、彩色 segment、标签
5. **OK / Cancel** — OK 写出 FixedMix 数据，Cancel 不修改模型

样式注意：对话框用 `Dialog::globalParent()` 作为 parent 打开（避免沿用工具栏父级丢失全局对话框样式）；顶部 tag 与 OK/Cancel 依赖全局 dialog / controls QSS。

### 数据流

1. track/clip 的 singer/speaker 二级菜单展开 multi-speaker singer。
2. 菜单显示当前 `packageId + singerId + packageVersion` 精确匹配的 Fixed Mix presets。
3. 点击 preset：复制 `sources + fixedWeights` 到目标，进入 `FixedMix`。
4. "新建混合预设..." / "管理混合预设..."：打开 `SpeakerMixDialog(singerInfo, initialMix, Dialog::globalParent())`。
5. Dialog 从 `SingerInfo::speakers()` 构造可选列表；preset 下拉只显示当前 singer 版本的 presets。
6. Save/Save As 只保存 `sources + fixedWeights`，**不保存 Dynamic Mix keyframes**。
7. OK 生成 `SpeakerMixData { mode = FixedMix, sources, fixedWeights }` 应用到目标。
8. 工程保存写入展开后的 track/clip DS workspace；preset 库在应用常规设置（`GeneralOption::speakerMixPresets`）。

### 预设菜单形态

```text
Luna
Azure
Umbra
────────────
Luna + Azure 50/50
Bright Blend
Soft Verse Mix
────────────
新建混合预设...
管理混合预设...
```

- 选单 speaker → `Single`；选 preset → `FixedMix`（复制，非引用）。
- 工程只保存 source preset id/name/dirty UI 元数据；编辑后显示 dirty，不按内容反推 preset。
- package 更新导致 `packageVersion` 变化：旧版本 preset 不显示、不自动迁移、不删除。

## 关键文件

- `src/app/UI/Views/ClipEditor/ParamEditor/SpeakerMixEditorView.h/.cpp`、`SpeakerMixToolBarView`
- `src/app/UI/Views/ClipEditor/ParamEditor/ParamEditorGraphicsView.h/.cpp`（`m_speakerMixView` 共存叠加）
- `src/app/UI/Dialogs/SpeakerMix/SpeakerMixDialog.h/.cpp`、`SpeakerMixList.h/.cpp`、`SpeakerMixBar.h/.cpp`
- `src/app/Model/AppModel/SpeakerMixData.h/.cpp`（数据模型与权重规范化）
- `src/app/Model/SpeakerMixPreset/`（用户级 preset store）
- `src/app/Controller/Actions/AppModel/SpeakerMix/`（preset 应用 / 替换 action，undo/redo）
- `src/app/Modules/ProjectConverters/DspxProjectConverter.cpp`（DS workspace 读写）
