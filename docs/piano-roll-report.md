# 钢琴卷帘状态管理与事件分发审计报告

## 架构概述

钢琴卷帘采用 **Strategy + Pimpl** 模式：

- `PianoRollGraphicsView`（公开 Widget）接收所有 Qt 输入事件
- `PianoRollGraphicsViewPrivate`（Pimpl）持有全部可变状态
- `PianoRollEditHandler`（抽象策略基类）— 6 个具体 Handler 注册在 `QHash<PianoRollEditMode, Handler*>` 中
- 3 个音高模式（`DrawPitch`、`ErasePitch`、`FreezePitch`）**没有 Handler**，通过 `PitchEditorView` 的 `setTransparentMouseEvents` 机制直接处理事件

### Handler 注册表

| 编辑模式 | Handler 实例 | 备注 |
|---|---|---|
| `Select` | `SelectNoteHandler` | |
| `IntervalSelect` | `SelectNoteHandler`（独立实例） | 同一个类的两个实例 |
| `DrawNote` | `DrawNoteHandler` | |
| `EraseNote` | `EraseNoteHandler` | |
| `SplitNote` | `SplitNoteHandler` | |
| `EditPitchAnchor` | `EditPitchAnchorHandler` | |
| `DrawPitch` | 无（`nullptr`） | 依赖 `PitchEditorView` Z 序 |
| `ErasePitch` | 无（`nullptr`） | 依赖 `PitchEditorView` Z 序 |
| `FreezePitch` | 无（`nullptr`） | 未实现 |

### 模式切换信号链

```
ClipEditorToolBarView::editModeChanged
    → PianoRollView::onEditModeChanged
        → PianoRollGraphicsView::setEditMode
            → deactivate 旧 handler
            → 查找并 activate 新 handler
            → 配置 DragBehavior 和音高编辑叠加层
```

---

## A. 设计问题

### A1. `mousePressEvent` 中 Handler 分发与内联逻辑混合

**文件**：`PianoRollGraphicsView.cpp:184-260`

Select 模式的 `mousePressEvent` 流程复杂：先调用 `SelectNoteHandler::mousePressEvent`（处理音符选择/准备），然后外层函数还会调用 `TimeGraphicsView::mousePressEvent`（启动框选）。Handler 的返回值 `false` 表示"我没有完全处理，让父类继续"——但这一契约是隐式的。DrawNote/Erase/Split 模式使用相同模式但行为不一致。

**建议**：统一 Handler 返回值语义，或者让 Select 模式下的框选逻辑也纳入 Handler。

### A2. `DrawNoteHandler` 在 `commit()`/`discard()` 中直接修改 `d->m_currentHandler`

**文件**：`DrawNoteHandler.cpp:86,95`

```cpp
d->m_currentHandler = d->m_handlers.value(d->m_editMode, nullptr);
```

Handler 不应直接修改 Pimpl 的 Handler 指针。这仅因双击 Select 模式创建音符时临时覆盖了 `m_currentHandler` 才需要这样做（`PianoRollGraphicsView.cpp:430-434`）。这是分层违规——Handler 指针的管理应由 View 负责。

### A3. `splitNoteAtPosition` 存在两套重复实现

- `SplitNoteHandler.cpp:72`（工具栏切割工具使用）
- `PianoRollGraphicsView.cpp:1210`（右键菜单使用）

两份几乎完全相同的切割逻辑，应提取为共享方法。

### A4. `anchorEditMode` 标志语义重载

**文件**：`EditPitchAnchorHandler.cpp`

`setAlwaysVisible` 设置 `m_state.anchorEditMode = visible`，但 `activate()` 也将 `anchorEditMode` 设为 `true`。该标志同时承担两个含义："锚点是否应该可见"和"用户是否处于锚点编辑模式"。如果调用 `setAlwaysVisible(false)`，会意外破坏已激活编辑模式的可见性。

**建议**：拆分为 `anchorVisible` 和 `anchorEditActive` 两个独立标志。

### A5. `DrawPitch`/`ErasePitch` 没有 Handler，完全依赖 Z 序

这些模式之所以能工作，是因为 `setPitchEditMode` 使 `PitchEditorView` 变为非透明，其 Z 值（2）高于音符（1）。但 `m_currentHandler` 为 `nullptr`，因此这些模式没有模式专属行为。`contextMenuEvent` 不处理这些模式——直接穿透到 `TimeGraphicsView::contextMenuEvent`。

### A6. 锚点 Handler 的 `commit()` 绕过了正常的 Action 模式

`EditPitchAnchorHandler::commit()` 直接调用 `clipController->onParamEdited`。其他 Handler 的提交经由 `PianoRollGraphicsView::commitAction()` → Handler `commit()`。但锚点 Handler 在创建/删除/拖拽/合并/插值切换时独立自动提交。`commitAction()` 实际上从未调用锚点 Handler 的 `commit()`——Handler 的生命周期与 View 的脱钩了。

---

## B. 代码质量 / 可维护性问题

### B1. Handler 返回值语义不一致

`mousePressEvent`/`mouseMoveEvent`/`mouseReleaseEvent` 的 `bool` 返回值在不同 Handler 中含义不同：

| Handler | `mousePress` 返回值 | 含义 |
|---|---|---|
| `SelectNoteHandler` | `false` | "我没处理，父类继续" |
| `EraseNoteHandler` | `true` | "我已处理，停止" |
| `DrawNoteHandler` | `true`/`false` | 绘制时为 true，编辑音符时为 false |
| `EditPitchAnchorHandler` | `true` | "我已处理，停止" |

基类默认返回 `true`。无文档说明此契约。

### B2. `event()` 覆盖在 Handler 之前处理 Escape

**文件**：`PianoRollGraphicsView.cpp:152-156`

`event()` 覆盖无条件地在 KeyPress Escape 时调用 `discardAction()`。但锚点 Handler 也在 `keyPressEvent` 中处理 Escape（退出编辑状态）。两条路径都会执行——`event()` 先执行，然后 `keyPressEvent` 通过 `TimeGraphicsView::event()` 被调用。导致 `discardAction()` 和 Handler 的 Escape 逻辑**双重触发**。

### B3. 音符查找使用线性扫描

`noteViewAt()`、`pronViewAt()`、`findNoteViewById()`、`selectedNoteItems()`、`findNextNoteView()` 均对 `noteViews` 进行线性扫描。对于包含大量音符的项目，开销叠加——尤其是 `mouseMoveEvent` 在每次鼠标移动时都调用 `noteViewAt`。

### B4. `cancelRequested` 来自基类，与 Pimpl 状态混合

`cancelRequested` 在 `mousePressEvent`、`mouseMoveEvent`、`mouseReleaseEvent`、`discardAction` 中被使用，但它声明在 `TimeGraphicsView` 基类中。与 Pimpl 中的 `m_mouseDown` 混合使用，增加了理解难度。

### B5. `m_selecting` 标志近乎遗留

Pimpl 中的 `m_selecting`（`_p.h:59`）在 `mousePressEvent` 中设为 `true`，在 `commitAction`/`discardAction` 中重置。但 `TimeGraphicsView` 自身也有框选逻辑（通过 `setDragBehavior(RectSelect)` 触发）。两者独立运作，Pimpl 的 `m_selecting` 实际上并不控制框选是否发生——它仅用于音符选择提交逻辑，功能退化为一个信号量。

### B6. 锚点 Handler 使用 `mouseMoveEvent` 检测悬停而非 `hoverMoveEvent`

锚点 Handler 在 `mouseMoveEvent` 中通过 `!(buttons & LeftButton)` 检测悬停，但 `hoverMoveEvent` 才是 Qt 的正确模式（`SplitNoteHandler` 使用的就是它）。`onHoverMove` 仅在 `SplitNote` 模式下分发给 Handler——锚点模式被跳过，因为 `m_isEditPitchMode` 为 `true`，`onHoverMove` 直接返回。

当前的实现能正常工作，是因为 `activate()` 中启用了 `setMouseTracking(true)`，使得即使没有按键，`mouseMoveEvent` 也会被调用。但这偏离了 Qt 推荐的事件处理模式。

---

## C. 总结

| 编号 | 严重度 | 分类 | 问题 |
|---|---|---|---|
| A1 | 设计 | 架构 | `mousePressEvent` 中 Handler 分发与内联逻辑混合 |
| A2 | 设计 | 分层 | `DrawNoteHandler` 直接修改 `d->m_currentHandler` |
| A3 | 设计 | DRY | `splitNoteAtPosition` 存在两套重复实现 |
| A4 | 设计 | 语义 | `anchorEditMode` 标志语义重载 |
| A5 | 设计 | 完整性 | `DrawPitch`/`ErasePitch` 无 Handler，依赖 Z 序 |
| A6 | 设计 | 生命周期 | 锚点 Handler 自动提交，脱离 `commitAction()` 流程 |
| B1 | 质量 | 契约 | Handler 返回值语义不一致 |
| B2 | 质量 | 逻辑 | Escape 在 `event()` 和 `keyPressEvent` 中双重处理 |
| B3 | 质量 | 性能 | 音符查找使用线性扫描 |
| B4 | 质量 | 清晰度 | `cancelRequested` 来自基类，与 Pimpl 状态混用 |
| B5 | 质量 | 清晰度 | `m_selecting` 标志功能退化 |
| B6 | 质量 | 模式 | 锚点 Handler 用 `mouseMoveEvent` 代替 `hoverMoveEvent` 检测悬停 |

以上问题均不影响当前功能正确性，可在后续重构中逐步改进。建议优先关注：

1. **A4**（标志拆分）— 防止后续功能引入隐性 Bug
2. **A2**（Handler 指针管理）— 理清分层职责
3. **B1**（返回值语义）— 统一契约可降低后续开发心智负担
