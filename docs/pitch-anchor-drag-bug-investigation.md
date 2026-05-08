# 音高锚点拖拽 Bug 调查

## 现象

某些情况下，拖动一段锚点曲线上的某个锚点时，移动的是另一个锚点。复现条件不明，非 100% 触发。

## 涉及文件

| 文件 | 路径 |
|------|------|
| EditPitchAnchorHandler.cpp | `src/app/UI/Views/ClipEditor/PianoRoll/EditPitchAnchorHandler.cpp` |
| EditPitchAnchorHandler.h | `src/app/UI/Views/ClipEditor/PianoRoll/EditPitchAnchorHandler.h` |
| PitchAnchorEditorView.cpp | `src/app/UI/Views/ClipEditor/PianoRoll/PitchAnchorEditorView.cpp` |
| PitchAnchorEditorView.h | `src/app/UI/Views/ClipEditor/PianoRoll/PitchAnchorEditorView.h` |
| AnchorCurve.h / .cpp | `src/app/Model/AppModel/AnchorCurve.h` `.cpp` |
| OverlappableSerialList.h | `src/app/Utils/OverlappableSerialList.h` |

## 核心逻辑流程

### 鼠标按下 (`mousePressEvent`)

1. 调用 `anchorNodeAt(scenePos)` 查找点击位置的锚点
2. 如果找到了节点且不在当前选中集中，调用 `selectNode(node)` 单选该节点
3. 如果节点已在选中集中，不改变选中集
4. 记录 `dragStartPos`，设置 `dragging = false`

### 鼠标移动 (`mouseMoveEvent`) —— 拖拽逻辑

1. 当移动超过 3px 曼哈顿距离后，设置 `dragging = true`
2. 首次进入拖拽：遍历 `selectedNodes`，为每个节点填充 `DragNodeInfo`（记录 `sourceCurve`、`startTick`、`startValue`）
3. 每帧处理：
   ```
   for each DragNodeInfo:
       sourceCurve->removeNode(node)
       node->setPos(startTick + deltaTick)
       node->setValue(startValue + deltaValue)
       sourceCurve->insertNode(node)
       node.targetCurve = anchorCurveAt(node.pos(), sourceCurve)  // 查找是否跨越到其他曲线
   ```

### 鼠标释放 (`mouseReleaseEvent`)

1. **第一遍循环**：对于 `targetCurve != sourceCurve` 的节点，从 source 移除，插入到 target
2. **第二遍循环**：对每个拖拽节点调用 `removeOverlappingNodes(finalCurve, node)` 删除同 tick 的重复节点
3. 清理空曲线
4. 调用 `commit()` 提交到模型

---

## 发现的潜在问题

### 问题一：命中测试返回"第一个匹配"而非"最接近" ⚠️ 最可能

**位置**：`EditPitchAnchorHandler::anchorNodeAt()` (L365-L377)

```cpp
AnchorNode *EditPitchAnchorHandler::anchorNodeAt(const QPointF &scenePos) const {
    for (auto *curve : anchorCurves()) {
        for (auto *node : curve->nodes().toList()) {   // 按 pos 升序遍历
            const auto x = q->tickToSceneX(node->pos());
            const auto y = d->m_anchorEditor->valueToSceneY(node->value());
            const auto dx = scenePos.x() - x;
            const auto dy = scenePos.y() - y;
            if (dx * dx + dy * dy <= kAnchorHitRadius * kAnchorHitRadius)
                return node;                            // 立即返回第一个在半径内的节点
        }
    }
    return nullptr;
}
```

**问题描述**：
- 遍历顺序按 `pos()` 升序（从左到右），命中半径 `kAnchorHitRadius = 6.0`（像素）
- 当两个锚点水平间距 ≤ 12px（2 × 命中半径）时，点击右侧节点可能命中左侧节点
- 因为左侧节点先被检查，且在命中半径内，直接返回左侧节点
- 用户以为在拖右侧节点，实际拖的是左侧节点

**模拟复现条件**：
- 同一曲线上两个锚点间距 ≤ 12 像素
- 用户点击较靠右的那个锚点

**修复思路**：改为遍历所有匹配节点，返回距离最近的那个，而非第一个匹配。

---

### 问题二：`removeOverlappingNodes` 中的悬垂指针访问 ⚠️ 严重

**位置**：`EditPitchAnchorHandler::mouseReleaseEvent()` (L154-L167)

```cpp
// 第一遍：将节点从 source 曲线迁移到 target 曲线
for (auto &info : m_state.dragNodeInfos) {
    if (info.targetCurve && info.targetCurve != info.sourceCurve) {
        info.sourceCurve->removeNode(info.node);
        info.targetCurve->insertNode(info.node);
        sourcesToCleanup.insert(info.sourceCurve);
    }
}

// 第二遍：删除同 tick 重叠的节点
for (auto &info : m_state.dragNodeInfos) {
    auto *finalCurve = info.targetCurve ? info.targetCurve : info.sourceCurve;
    removeOverlappingNodes(finalCurve, info.node);  // info.node 可能已被上一轮 delete
}
```

**问题场景**：
1. 曲线 1 有节点 A(tick=100)，曲线 2 有节点 B(tick=100)——同 tick，不同曲线
2. 同时选中 A 和 B，拖拽 `deltaTick` 相同 → 拖完后仍在同一 tick
3. 第一遍循环可能把 B 迁移到曲线 1
4. 第二遍循环中，A 的 `removeOverlappingNodes` 发现 B 在同一 tick，**执行 `delete B`**
5. 紧接着 B 的迭代中，`info.node` 指向已释放的内存 → **未定义行为**

`removeOverlappingNodes` 内部会访问 `keep->pos()`：
```cpp
void EditPitchAnchorHandler::removeOverlappingNodes(AnchorCurve *curve, AnchorNode *keep) {
    ...
    for (auto *node : curve->nodes().toList()) {
        if (node != keep && node->pos() == keep->pos())  // keep 可能是悬垂指针!
```

**复现条件**：
- 两条不同曲线在相同 tick 位置各有一个锚点
- 同时选中并拖拽

---

### 问题三：多选状态下单拖一个节点，所有选中节点都移动 🟡 UX 问题

**位置**：`EditPitchAnchorHandler::mousePressEvent()` (L47-L55)

```cpp
if (node) {
    if (!m_state.selectedNodes.contains(node))
        selectNode(node);   // 不在选中集 → 变为单选
    // 如果已在选中集 → 不改变选中集，拖拽时会移动所有已选节点
    m_state.dragStartPos = scenePos;
    m_state.dragging = false;
}
```

**问题描述**：
- 用户框选了 A、B、C 三个锚点
- 然后点击 A 并拖拽，期望只移动 A
- 实际：因为 A 已在 `selectedNodes` 中，`selectNode(A)` 不会被调用，selection 保持为 [A, B, C]
- 拖拽时全部三个节点一起移动
- 如果几条曲线有重叠的 tick 范围，`anchorCurveAt()` 可能将不同节点分配到不同的 targetCurve，造成视觉混乱

**可能的复现条件**：
- 框选 ≥ 2 个锚点
- 单击其中任意一个开始拖拽
- 预期只拖一个，实际拖了全部

---

## 数据结构说明

### AnchorNode

- 继承自 `Overlappable` + `UniqueObject`
- `pos()` / `setPos()`: tick 位置
- `value()` / `setValue()`: 音高值
- `interpMode()` / `setInterpMode()`: 插值模式 (Linear / Hermite / Cubic / None)
- `compareTo()`: 按 `pos()` 比较，相同 pos 无法区分
- `interval()`: 返回 `(pos, pos)` —— 退化区间

### AnchorCurve

- 继承自 `Curve`
- `nodes()`: 返回 `OverlappableSerialList<AnchorNode>`
- `insertNode(node)`: 调用 `m_nodes.add(node)`
- `removeNode(node)`: 调用 `m_nodes.remove(node)`

### OverlappableSerialList

- 底层使用 `std::set<T*, ItemCmp>` 存储，按 `compareTo()` 排序
- `ItemCmp`: 先比较 `compareTo()`，相同时回退到指针地址比较
- 同时维护 `interval_tree` 用于重叠检测

### AnchorOverlayState

- `selectedNodes`: `QList<AnchorNode*>` — 选中节点列表，顺序取决于选择方式（框选按曲线+pos 顺序，单击只含一个）
- `dragNodeInfos`: `QList<DragNodeInfo>` — 拖拽信息列表，在首次进入拖拽时从 `selectedNodes` 复制填充
- `currentCurve`: 当前编辑的曲线

---

## 拖拽过程中的渲染逻辑

### drawAnchorCurves（普通锚点绘制）

- 拖拽中：跳过 `targetCurve != null && sourceCurve == 当前曲线` 的节点
- 非拖拽：绘制所有节点

### drawDragPreviewCurve（拖拽预览绘制）

- 仅在 `targetCurves` 非空时绘制
- 按 targetCurve 分组绘制被拖拽的节点及其曲线段

---

## 后续追踪

- [ ] 确认问题一是否能复现（两个锚点靠很近时点击右侧锚点）
- [ ] 确认问题二是否可能导致随机崩溃
- [ ] 评估问题三是否属于设计意图还是 bug
- [ ] 尝试构造更小的复现用例
