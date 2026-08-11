# 平滑滚动引擎重构（Plan B：三层拆分）——进行中方向

> 状态：**方向文档，暂不实施**。2026-08-11 已先用最小补丁修复「动画关闭后滚轮失效」：
> `SmoothScroller::eventFilter` 在 `getEffectiveAnimationTime()==0` 时直接 `bar->setValue()`，
> 不再启动 0 时长动画。本方案是收敛重复实现、根治该类问题的**远期重构方向**，
> 因核心改动涉及编辑区（TimeGraphicsView，含 scale 动画 / auto-page / edge-scroll 物理），
> 风险较高，当前阶段不宜实施；待补丁方案稳定后再评估推进。

## 现状问题清单（重构要解决的根子）

1. **动画关闭（duration==0）需显式瞬时路径** —— 7eb87b49 已把 eventFilter 在 duration==0 时直接
   `setValue`。注意：探针实测 Qt 6.11 的 0 时长动画会**同步落点并结束**，本身不卡死；真正造成
   「内嵌设置页无法滚动、必须重开对话框」的根因是 **`MainWindow` 的 Appearance `optionsChanged`
   挂钩在模态打开时重新注册 Direct Manipulation**（DM 劫持主窗 WM_MOUSEWHEEL，内嵌面板是主窗
   子级故滚轮永远到不了）——已于 2026-08-11 用 `m_modalHost->isOpen()` 守卫修复。此处仍保留
   instant-mode 作为设计目标：`duration==0` 应是执行器主动选择的「瞬时执行」，而非被动特例。
2. **滚轮来源判定两套实现** —— TimeGraphicsView::isMouseEventFromWheel()（120 倍数 + 400ms 单锁）
   vs SmoothScroller::m_stepWindow（6 项滑动窗口）。同一概念、两套语义，未来必然再次分叉出 bug。
3. **动画重定向逻辑（heat）重复** —— SmoothScroller::updateAnimationDuration() 与
   TimeGraphicsView::updateAnimationDuration() 结构几乎逐行相同（stop → setDuration →
   running? restart : return → duration==0? apply : restart），分散两处难维护。
4. **target 计算混在 eventFilter 里** —— perStep 三态（ScrollPerItem / ScrollPerPixel / QScrollArea）
   叠加、逻辑目标（logicalH/V）、边界夹取全部挤在一个 filter 里，职责不清晰。

## 目标架构：三层拆分

### 层 1：纯计算共享函数（新头文件 `src/libs/GUI/Controls/WheelScroll.h`，无状态）

```cpp
namespace WheelScroll {
// 120 整数倍 → 鼠标滚轮；否则可能触控板。纯函数，两处共用。
[[nodiscard]] bool isMouseWheel(const QPoint &angleDelta);

// 垂直于指定轴的每格位移（像素）。ScrollPerItem → 行数×行高；ScrollPerPixel → 行高×wheelScrollLines；
// QScrollArea → viewport×0.15 fallback。
[[nodiscard]] double perStep(QAbstractScrollArea *area, Qt::Orientation axis);

// 计算目标值：base（含逻辑目标叠加）− perStep × delta/120，并夹取到 bar 范围。
[[nodiscard]] int targetValue(QAbstractScrollArea *area, QScrollBar *bar,
                              Qt::Orientation axis, const QPoint &delta, int base);
}
```

状态化的滑窗/锁定（触控板检测的时序窗口）仍留在 SmoothScroller 负责，仅把「120 倍数判定」抽成纯函数共享。

### 层 2：共享「重定向滚动动画」helper（`src/libs/GUI/Controls/WheelScroll.h` 或独立 `.cpp`）

```cpp
// 收编 heat() + eventFilter 里的 stop/startValue/endValue/start 逻辑与 logical 维护。
// duration<=0（动画关闭）→ 直接 bar->setValue(target)、清 logical、返回 false；
// 否则按 running 状态衔接（currentValue 作 start 或启动新动画）、维护 logical、返回 true。
[[nodiscard]] bool retargetScrollAnimation(QPropertyAnimation &anim,
                                           QScrollBar *bar, int targetValue,
                                           int duration);
```

统一决策「瞬时 vs 动画」，调用方不再需要记得 duration==0 不能 start。

### 层 3：`SmoothScroller::eventFilter` 瘦身为三行骨架

```
拦截 wheel（无修饰键）→ WheelScroll 判定鼠标/触控板
  → 触控板直通不消费
  → 鼠标：WheelScroll::targetValue 算目标
      → retargetScrollAnimation(m_vAnim, bar, target, effectiveDuration)
      → 消费事件
```

`TimeGraphicsView` 的 `updateAnimationDuration()` / `onWheelHorScroll()` /
`onWheelVerScroll()` 同步换成层 2 helper（仅替换 heat/getEffective + target 计算，
不改变其 scale/auto-page 逻辑）。

## 实施顺序（分阶段）

- **阶段一**：抽出 `WheelScroll` 纯函数，先在 SmoothScroller 内使用；行为零变化，跑补丁期回归项。
- **阶段二**：抽 `retargetScrollAnimation`，替换 SmoothScroller 与 TimeGraphicsView 两处的 heat 逻辑。
  注意 TimeGraphicsView 还有 scale 动画（m_scaleX/YAnimation）→ helper 只服务于 scrollbar 动画；
  scale 的热更逻辑不强行合并（避免误伤缩放动画）。
- **阶段三（编辑区，风险高，延后）**：TimeGraphicsView 的 onWheel* 全部改走共享 target 计算，
  physical/perStep 口径统一。此阶段改动编辑器手感相关代码，必须有充分回归才做。

## 验收标准 / 回归项（补丁期与重构期共用）

- [ ] 动画关闭（None）时，设置各页 / 弹窗列表 / 独立设置窗口均可用滚轮滚动（本次补丁修复项）。
- [ ] 有动画/无动画的总滚动量一致（perStep 不因开启动画而「少滚」或「多滚」）。
- [ ] 触控板直通行为不变（高 DPI 像素滚动）。
- [ ] 编辑器（TimeGraphicsView）行为**零变化**——阶段三落地前任何重构不得改动它。
- [ ] 软件滚轮连滚不抽搐（逻辑目标叠加仍工作：连续滚动手感不变）。

## 参考

- `docs/plans/smooth-scrolling-plan.md`（原始方案）
- 代码：`src/libs/GUI/Controls/SmoothScroller.cpp`、`src/app/UI/Views/Common/TimeGraphicsView.cpp`
- 本次补丁 commit 说明 + skill 记录（smooth-scrolling-wheel-input / smooth-scroller-outcubic-scrolling）
