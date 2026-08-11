# 全局平滑滚动方案（SmoothScroller）
> 状态：📋 方案编写中（未改代码）。用户已确认：沿用现有二分手感——触摸板直通、鼠标滚轮 OutCubic 动画。2026-08-11

## TL;DR

将编辑区 `TimeGraphicsView` 的"滚轮 → OutCubic 动画 scrollbar value"模式抽成一份**独立的 `SmoothScroller`（QObject + eventFilter）**，挂载到所有原生滚动视图（QListWidget/QListView/QTreeWidget/QTableView/QScrollArea）的 viewport 上。不继承任何 widget 基类，因此对 `QScrollArea`、item view、自绘 view 通吃。鼠标滚轮走动画，触控板直通像素。

## 背景

当前 UI 中只有编辑区 `TimeGraphicsView` 拥有平滑滚动动画（内部 4 个 `QPropertyAnimation`，OutCubic，时长经 `IAnimatable::getEffectiveAnimationTime` 受全局动画等级控制）。其余滚动视图（轨道列表、说话人列表、G2p 列表、搜索弹窗、包管理器、导出对话框、Phoneme 命名弹窗、日志窗口、Options 各页）全部走 Qt 原生 `wheelEvent` → scrollbar 值直接跳变，滚动生硬"咔哒咔哒"。

**关键结论**：这些视图多数连 `OverlayScrollBar` 都没装（全项目 install 仅 4 处：编辑区、ComboBox 弹出层、IOptionPage、测试），所以不是"外观动画缺失"，而是**没有任何滚动值动画**。

## 当前代码观察

### 编辑区的成熟实现（复用基准）

`src/app/UI/Views/Common/TimeGraphicsView.cpp`：

- `m_hBarAnimation` / `m_vBarAnimation`：QPropertyAnimation 驱动 `horizontalScrollBarValue` / `verticalScrollBarValue` 属性，easing=OutCubic（构造器 62-76 行）。
- 滚轮分发 `wheelEvent`（500-512 行）→ `onWheelHorScroll/onWheelVerScroll/onWheelHorScale/onWheelVerScale`。
- 直接滚动路径 `onWheelVerScroll`（413-423 行）：

```cpp
if (isDirectManipulationEnabled() || !isMouseEventFromWheel(event)) {
    QGraphicsView::wheelEvent(event);   // 触控板直通
} else {
    auto scrollLength = -1 * viewport()->height() * 0.15 * deltaY / 120;
    auto startValue = verticalBarValue();
    auto endValue = static_cast<int>(startValue + scrollLength);
    verticalBarAnimateTo(endValue);     // 鼠标滚轮 → OutCubic 动画
}
```

- `horizontalBarAnimateTo / verticalBarAnimateTo`（动画实现）与 `m_logicalHorizontalBarValue / m_logicalVerticalBarValue`（动画中逻辑目标值跟踪，78-86 行 clearLogicalViewport、291-296 行赋值）。
- `updateAnimationDuration()`（623-652 行）在动画等级/时间缩放变化后重算 duration 并原地热更正在跑动画的目标。
- `isMouseEventFromWheel()`（598-621 行）——鼠标/触控板判定。

### 待挂载的滚动视图清单

| 视图 | 路径 | 类型 |
|------|------|------|
| TrackListView | src/app/UI/Views/TrackEditor/TrackListView.cpp | QListWidget |
| SpeakerMixList | src/app/UI/Dialogs/SpeakerMix/SpeakerMixList.cpp | QListWidget（含自定义 eventFilter） |
| G2pListWidget | src/app/UI/Controls/G2pListWidget.cpp | QListWidget |
| SearchDialog.resultListWidget | src/app/UI/Dialogs/Search/SearchDialog.cpp | QListWidget |
| PackageManagerDialog.listView | src/app/UI/Dialogs/PackageManager/PackageManagerDialog.cpp | QListView |
| AudioExportDialog.m_sourceListWidget | src/app/UI/Dialogs/Audio/AudioExportDialog.cpp | QListWidget |
| PhonemeNameListWidget | src/app/UI/Dialogs/Note/PhonemeNameListWidget.cpp | QListWidget |
| LogWindow.m_tableView | src/app/UI/Window/LogWindow.cpp | QTableView |
| IOptionPage（Options 各页基类，已有 OverlayScrollBar） | src/app/UI/Dialogs/Options/Pages/IOptionPage.cpp | QScrollArea |

逐个挂 `installEventFilter(smoothScroller)`。

## 已定的设计决策

### 1. Mouse/Touchpad 二分手感（用户已确认）

沿用现有二分，与编辑区一致：
- **鼠标滚轮**（angleDelta 为 120 整数倍）：拦截，做 OutCubic 动画驱动 scrollbar value。
- **触控板**（angleDelta 非 120 倍数 / pixelDelta 主导）：**直通**，不拦截，保持像素跟手。

### 2. Windows 上无 Qt API 区分来源（用户提出，确认）

Qt 在 Windows 由 `WM_MOUSEWHEEL` 产生 QWheelEvent，消息不携带来源信息；`QInputDevice::DeviceType` 在 Windows 恒为/不可靠地填 Mouse。`SUPPORTS_MOUSEWHEEL_DETECT_NATIVE` 仅在 Q_OS_MAC 定义（scheduler 也做同样判定）——项目里已有的 `isMouseEventFromWheel()` 就是 naive 方案。

### 3. 判定增强：滑动窗口替代单次锁

现有实现的 400ms 单次锁有两个弱点：
1. 快速重入（<400ms 又来事件）会误伤后续本应判为触控板的事件。
2. 判过的"鼠标"状态不衰减，长触控板滚动中途一旦出现一次 120 倍数 delta 就误切回鼠标。

**改进：滑动窗口 + 模式保持**。
- 记录最近 6 个事件的 delta，**全部**为 120 倍数 → 判定鼠标；
- 否则 → 判定触控板，并**保持** N 个事件或超时 T ms 后才允许切回鼠标。

（直接用 `event->pixelDelta().isNull()` 作为辅助信号，非决定性。）

### 4. 动画进行中叠加目标（复用编辑区思想）

动画未结束时再来一轮滚轮，`endValue` 应在**当前逻辑/实际动画目标**上叠加，而非 `startValue`，否则快速滚动会抽搐/回弹。同理动画等级变化时（`updateAnimationDuration` 等价物）需原地热更正在跑动画。

### 5. 组件形态：QObject + eventFilter，不进继承体系

不新增 widget 基类（避免 QtMultiple）也不改 `OverlayScrollBar`（那是外观层）。`SmoothScroller` 是独立 QObject，`event->type()==QWheelEvent` 时接管。放置路径：`src/app/UI/Controls/` 与 `TrackListView` 同级（或 `src/libs/GUI/Controls`）。倾向 app 层，因为它依赖的"手感/DOM"是应用语义。

## 实现要点（代码路径）

### SmoothScroller 类骨架（接口草图，未实现）

```cpp
class SmoothScroller : public QObject, public IAnimatable {
    Q_OBJECT
public:
    explicit SmoothScroller(QObject *parent = nullptr);
    void attachTo(QAbstractScrollArea *area);   // 或 eventFilter 直接拦截 viewport 事件

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override { restartAnimations(); }
    void afterSetTimeScale(double scale) override { restartAnimations(); }

private:
    struct Frame { int dx = 0, dy = 0; bool wheelStep; };
    QVector<Frame> m_recentFrames;        // 滚动窗口，cap 6
    bool m_touchpadLatched = false;
    int m_touchpadLatchRemaining = 0;
    static constexpr int kWindowSize = 6;
    static constexpr int kTouchpadKeepMs = 300;

    QPropertyAnimation m_hAnim;   // target: area->horizontalScrollBar()
    QPropertyAnimation m_vAnim;   // target scrollbar
    QScrollBar* m_activeBar = nullptr;
    // 动画中逻辑目标：避免快速滚抽搐
    std::optional<int> m_logicalH, m_logicalV;

    bool isTouchpad(QWheelEvent *event);
    void animateScrollTo(QScrollBar *bar, int endValue);
    void restartAnimations();      // 动画等级/时长变化时热更正在运行的动画
};
```

### eventFilter 拦截要点

- `watched == scrollArea->viewport()`，`event->type() == QWheelEvent`。
- 先调 `isTouchpad`：触控板 → `return QObject::eventFilter(...)` 不拦（Qt 原生滚）。
- 鼠标 → 计算 endValue，更新动画，`return true`（消费事件）。注意 `ScrollPerPixel` 语义不在此处处理——我们覆盖原生滚动，动画本质是平滑的像素滚动。

### 挂载（最小改动语义）

各处 `new QObject` 成员按住当前 widget 做 parent，`viewport->installEventFilter(scroller)`。

## 边界与副作用检查

- **TimeGraphicsView 挂不挂**：不挂（已有动画，且 eventFilter 在 viewport 上会跟内部滚动打架）。SmoothScroller 应跳过 `QGraphicsView`。
- **ComboBox 弹出层**：已有 OverlayScrollBar 做外观；弹出层的 QListView 是否动画需确认，倾向保持现状手感（后续再说）。
- **popup 内 QListView（自定义 Popup）**：先不强制，避免 popup 打开/关闭延迟感。
- **qApp 过滤器 vs per-viewport 过滤器**：方案 B 选 per-viewport（per-area），不装 qApp。原因：qApp 会影响全局并必须排除 TimeGraphicsView 等，耦合大。

## Pitfalls（写入本文件，防止后人踩）

1. **Windows 无 Qt API 区分鼠标/触控板滚轮**。`WM_MOUSEWHEEL` 不带来源。naive：angleDelta 120 倍数 = 鼠标（量化：240/360 也是 120 倍数）；非整倍数 = 触控板。不要依赖 `deviceType()`（Windows 恒为 Mouse）。
2. **勿用单一 400ms 锁**。改用 6 事件滑动窗口 + 保持窗，否则快速重入误伤。见设计决策 3。
3. **动画中叠加目标**，不是从打断点重新算。否则快速滚会反向回弹（编辑区 78-86 行已有此坑的解法）。
4. **动画等级变化热更**：全局关动画（None）时必须立即跳转，不能在跑动画。复用 / 参照 `TimeGraphicsView::updateAnimationDuration()` 的原地热更逻辑。
5. **不挂 TimeGraphicsView**：已有动画，重复拦截会导致双动画打架。

## 验收路径

1. 构建通过（`run-cmake-preset.ps1 ConfigureAndBuild debug`）。
2. 冒烟：拖一个含多轨道的工程，鼠标滚轮在轨道列表 / 说话人列表 / 搜索弹窗 / Options 页平滑滚动，无咔哒感；触控板跟手不变。
3. 关全局动画（AppearanceOption=无动画）后鼠标滚轮立即跳变，不残留动画。

## 待确认（后续迭代）

- ComboBox 弹出层 / 自定义 Popup 内列表是否纳入。
- SpeakerMixList 自定义 eventFilter 与 SmoothScroller 的组合顺序（它的自带 eventFilter 已拦截部分事件，需确认不冲突）。
