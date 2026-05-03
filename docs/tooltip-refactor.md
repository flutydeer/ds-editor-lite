# ToolTip 重构方案

## 背景

`ToolTip` 和 `ToolTipFilter` 目前职责分布不合理：动画和屏幕定位逻辑放在 `ToolTipFilter` 中，而 `ToolTip` 本身缺少独立显示/隐藏的能力。新的 `SpeakerMixEditorView` 需要在 QGraphicsItem 的 hoverMoveEvent 中手动控制 ToolTip，无法使用 `ToolTipFilter`（它依赖 QWidget 的 Enter/Leave 事件）。

## 目标

将动画和屏幕定位能力下沉到 `ToolTip`，使其可以独立使用。`ToolTipFilter` 简化为事件转发层，内部调用 `ToolTip` 的新方法。

## 涉及文件

| 文件 | 路径 |
|------|------|
| `ToolTip.h` | `src/app/UI/Controls/ToolTip.h` |
| `ToolTip.cpp` | `src/app/UI/Controls/ToolTip.cpp` |
| `ToolTipFilter.h` | `src/app/UI/Controls/ToolTipFilter.h` |
| `ToolTipFilter.cpp` | `src/app/UI/Controls/ToolTipFilter.cpp` |

现有使用 `ToolTipFilter` 的文件（重构后不应有行为变化）：
- `UI/Views/ClipEditor/ToolBar/ClipEditorToolBarView.cpp`
- `UI/Views/TrackEditor/TrackListHeaderView.cpp`
- `UI/Views/MainTitleBar/ActionButtonsView.cpp`
- `UI/Views/Common/TabPanelTitleBar.cpp`
- `UI/Controls/LineEdit.cpp`
- `UI/Views/MainTitleBar/MainTitleBar.cpp`
- `UI/Controls/LevelMeter.h`

---

## 步骤

### 第一步：给 ToolTip 添加动画和定位能力

在 `ToolTip` 中新增以下成员和方法：

**新增私有成员：**
```cpp
QPropertyAnimation *m_opacityAnimation;
bool m_animationEnabled = true;
```

**新增公开方法：**
```cpp
void setAnimationEnabled(bool on);
bool animationEnabled() const;

// 在指定屏幕坐标处显示，自动做屏幕边缘修正
void showAt(const QPoint &screenPos);

// 带动画隐藏（动画结束后 hide）
void hideWithAnimation();
```

**`showAt` 实现要点：**
1. 接收屏幕坐标
2. 获取该坐标所在的 screen（`QApplication::screenAt`）
3. 用 screen 的 `availableGeometry` 对 tooltip rect 做边缘 clamp（搬自 `ToolTipFilter::adjustToolTipPos` 的逻辑）
4. `move` 到修正后的位置
5. 如果 `m_animationEnabled`，用 opacity 动画从当前值渐变到 1.0；否则直接 `setWindowOpacity(1)`
6. 调用 `show()`

**`hideWithAnimation` 实现要点：**
1. 如果 `m_animationEnabled`，用 opacity 动画渐变到 0，动画结束后调用 `hide()`
2. 否则直接 `setWindowOpacity(0)` + `hide()`

**构造函数修改：**
- 创建 `m_opacityAnimation`（target = this, property = "windowOpacity", duration = 150）
- connect `m_opacityAnimation::finished`：当 opacity == 0 时调用 `hide()`

**析构函数修改：**
- delete `m_opacityAnimation`

**移除：**
- `ToolTip` 中现有的未使用的 `m_timer` 成员和 `#include <QTimer>`（如果没有其他地方用到的话）

### 第二步：简化 ToolTipFilter

移除 `ToolTipFilter` 中已下沉到 `ToolTip` 的代码：

**移除私有成员：**
```cpp
QPropertyAnimation *m_opacityAnimation;  // 删除
bool m_animation;                        // 删除
```

**移除/简化方法：**
- 删除 `adjustToolTipPos()`——改为在 `showToolTip` 中调用 `m_tooltip->showAt(QCursor::pos())`
- 删除 `animation()` / `setAnimation()`——改为透传到 `m_tooltip->animationEnabled()` / `setAnimationEnabled()`

**简化 `showToolTip`：**
```cpp
void ToolTipFilter::showToolTip() const {
    m_tooltip->setTitle(m_parent->toolTip());
    m_tooltip->showAt(QCursor::pos());
}
```

**简化 `hideToolTip`：**
```cpp
void ToolTipFilter::hideToolTip() const {
    m_tooltip->hideWithAnimation();
}
```

**构造函数修改：**
- 移除 `m_opacityAnimation` 创建和 connect 代码
- `m_animation` 参数仍保留在构造函数签名中以保持 API 兼容，但内部转为调用 `m_tooltip->setAnimationEnabled(animation)`

**析构函数修改：**
- 移除 `delete m_opacityAnimation`

### 第三步：验证

1. 构建项目，确保编译通过
2. 手动测试以下场景，确认 ToolTip 行为无回归：
   - 鼠标悬停工具栏按钮（ClipEditorToolBarView、ActionButtonsView）
   - 鼠标悬停标签页标题（TabPanelTitleBar）
   - LevelMeter 的 tooltip

---

## 注意事项

- `ToolTipFilter` 的公开 API 签名不变，现有调用者不需要修改
- `ToolTip::showAt` 中的屏幕边缘修正逻辑直接搬自 `ToolTipFilter::adjustToolTipPos`，不做功能改动
- 动画 duration 保持 150ms 不变

---

## 附加修复：ToolTip 内容变更后尺寸不自适应

### 问题描述

`ToolTip` 实例在 `ToolTipFilter` 中只创建一次，不会随 hover 目标变化而销毁重建。当 tooltip 内容发生变更（`setTitle`、`setMessage`）时，窗口尺寸不会自动跟随更新。

原因：`ToolTip` 是顶层窗口（`Qt::ToolTip` flag），QLabel 文本变化后 layout 会重新计算 sizeHint，但顶层窗口不会自动 resize 到新的 sizeHint。

### 已尝试的方案

1. **`adjustSize()`**：只能放大不能缩小，顶层窗口的最小尺寸约束阻止了缩小。
2. **`setMinimumSize(QSize()) + layout invalidate/activate + resize(sizeHint())`**：`QSize()` 产生 (-1,-1) 导致 `QWidget::setMinimumSize: Negative sizes` 警告，且仍不能可靠缩小。

### 下一步方案：隐藏后销毁，显示前重建

修改 `ToolTipFilter`，在 `hideToolTip` 动画结束后销毁 `ToolTip` 实例，在 `showToolTip` 时重新创建。这样每次显示的 ToolTip 都是全新实例，不存在尺寸残留问题。

**实现思路：**
- `ToolTipFilter` 不再持有 `m_tooltip` 成员，改为在 `showToolTip` 中临时创建
- `hideWithAnimation` 的 `finished` 信号中 deleteLater 销毁自身
- 或者 `m_tooltip` 仍保留，但 `hideToolTip` 完成后 delete + 置 nullptr，`showToolTip` 时检查是否为 nullptr 再创建

### 验证

测试内容从短文本切换到长文本、从多行切换到单行时，ToolTip 窗口大小是否正确跟随变化。
