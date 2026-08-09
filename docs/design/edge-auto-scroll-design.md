# 边缘自动滚动设计（Edge Auto Scroll）

本文说明"拖动时指针到达视口边缘、视口自动跟随滚动"这套机制的设计。它服务于
钢琴卷帘、轨道视图和参数编辑器中的框选、音符/剪辑拖动、绘制等操作，让用户可以在
拖动过程中把选择范围或拖拽对象延伸到视口之外。

## 目标

- 拖动过程中，指针进入视口边缘热区时，视口以与进入深度相关的速度向该方向滚动；
- 指针离开热区（回到视口中部）时立即停止；
- 滚动行为是纯计算，独立于具体编辑器，可以脱离 GUI 做单元测试；
- 从边缘附近开始拖动时，不因微小位移产生与用户意图相反的反向滚动。

## 覆盖范围与接入点

| 编辑器 | 接入方式 | 启用轴 |
| --- | --- | --- |
| 钢琴卷帘（legacy 后端 `PianoRollGraphicsView`） | `TimeGraphicsView::armEdgeAutoScroll` | 矩形框选 H+V；框选模式（BeamSelect）H；音符/整体移动 H+V；绘制、擦除、锚点等 handler 拖动按需 |
| 轨道视图 `TracksGraphicsView` | 同上（继承 `TimeGraphicsView`） | 拖动/移动 clip H+V |
| 参数编辑器 `ParamEditorGraphicsView` | 同上 | 曲线编辑按需 |
| 轨道视图 RHI 后端 `TracksRhiWidget` | 独立实现（外部拖入文件），见下文 | 仅垂直 |
| 钢琴卷帘 RHI 后端 `PianoRollRhiWidget` | 暂无此机制 | — |

## 总体架构：计算与运行分离

### `EdgeAutoScroller`（纯计算层）

`src/app/UI/Views/Common/EdgeAutoScroller.h/.cpp` 中的 `EdgeAutoScroller` 不依赖任何
widget：速度曲线、亚像素累积、指针钳制都是静态纯函数或持有少量状态的成员函数，
帧时间由外部注入，因此可以脱离 Qt 事件循环做单元测试。

| 成员 | 职责 |
| --- | --- |
| `axisSpeed(pos, start, end, lowHotZone, highHotZone, maxSpeed, baseSpeed)` | 单轴有符号速度（px/s），两端热区宽度独立 |
| `velocity(pointerPos, viewportRect, axes, config)` | 指针位置 → 速度向量 |
| `velocity(pointerPos, pressPos, viewportRect, axes, config)` | 带按下点死区的速度向量（见"按下点死区"） |
| `computeStep(...)` | 一帧的整数滚动步长，携带亚像素余量 |
| `clampToRect(pos, rect)` | 把指针钳制回视口矩形内 |
| `start()/stop()/frame(dtMs)` | QTimer(16ms) + QElapsedTimer 驱动"真实帧时长" |

### `TimeGraphicsView`（编排层）

`src/app/UI/Views/Common/TimeGraphicsView.cpp` 负责：

1. 拖动开始时**武装**（`armEdgeAutoScroll`）并记录按下点；
2. 移动事件中**评估**（`updateEdgeAutoScrollState`）指针是否进入热区，决定定时器启停；
3. 定时器帧回调中**应用**滚动步长到滚动条，并把钳制后的指针位置回传给拖动逻辑
   （如橡皮筋框选）继续执行；
4. 释放或取消时**解除**（`disarmEdgeAutoScroll`）。

各编辑器不需要了解机制本身，只要在合适的时机调用 `armEdgeAutoScroll(axes)` 传入
允许滚动的轴即可。`armEdgeAutoScroll` 支持在拖动中反复调用：第二次调用只刷新轴与
热区状态，不改变按下点（覆盖子类拦截了 move 事件、未走基类 `mouseMoveEvent` 的情况）。

## 速度模型

### 热区与参数

```cpp
struct EdgeAutoScrollConfig {
    int hotZoneH = 72;       // 水平热区宽度（视口像素）
    int hotZoneV = 56;       // 垂直热区高度
    double maxSpeedH = 1200; // 满强度水平速度 px/s
    double maxSpeedV = 800;  // 满强度垂直速度 px/s
    double baseSpeed = 80;   // 热区内边界处的起步速度 px/s
    int intervalMs = 16;     // 定时器间隔
};
```

每条边（上/下/左/右）各有一个热区。指针在热区内时产生**朝向该边**的速度：
左/上边缘为负方向（滚动条值减小），右/下边缘为正方向。左右热区重叠的小视口中，
两侧贡献相互对称抵消。

### 强度曲线

对某条边，定义深度 `depth = 热区外边界到指针的距离`（指针越靠近边缘深度越大，
越过边缘后饱和）：

```text
strength = clamp(depth / hotZone, 0, 1)
speed    = baseSpeed * strength + (maxSpeed - baseSpeed) * strength²
```

曲线是二次的：刚进热区时以 `baseSpeed` 缓起步，靠近边缘时快速逼近 `maxSpeed`。
由于滚动使指针相对视口越来越靠近边缘，速度存在正反馈，指针停留边缘时会迅速
饱和到 `maxSpeed`。

### 亚像素累积

`computeStep` 把速度 × 帧时长累进 `m_accumulator`，每次只取整数值滚动，余量留到
下一帧。这样滚动条步长始终是整数，同时不受定时器抖动影响——1 秒内无论帧时长
如何分布，累积位移都严格等于 `speed × 1s`（误差小于 1 像素）。滚动会话开始时
应调用 `resetAccumulator()`。

## 生命周期

```text
mousePressEvent
  └─ armEdgeAutoScroll(axes)          记录按下点 pressPos
mouseMoveEvent
  └─ updateEdgeAutoScrollState(pos)
       ├─ 位移 < startDragDistance → 忽略（防止单击误触）
       └─ velocity(pos, pressPos, ...) 非零（在热区内）
            └─ 停止滚动条动画，start() 启动 16ms 定时器
定时器帧（每 16ms）
  ├─ 自检：按键已松开 / 不可见 → disarm 并退出（安全网）
  ├─ computeStep → setHorizontalBarValue / setVerticalBarValue
  ├─ clampToRect(指针) → onEdgeAutoScrollFrame 继续驱动拖动（如橡皮筋）
  └─ updateEdgeAutoScrollState(指针)  决定继续滚动或停止
mouseReleaseEvent
  └─ disarmEdgeAutoScroll             停止定时器
```

滚动期间直接写滚动条值，因此启动前要停掉滚动条的缓动动画
（`m_hBarAnimation` / `m_vBarAnimation`），避免两者互相打架。

## 按下点死区（press dead zone）

### 问题

旧实现只判断"指针当前是否在热区内"。当用户从视口左上角（已经处于 72/56px 热区
内）按下并向右下框选时，指针向右下移动一小段后仍在左上热区内，立即触发**向左、
向上**的滚动——方向与用户意图完全相反，且正反馈使速度很快飙到 `maxSpeed`，
表现为"按住没动，一动视口就朝反方向狂滚"。

### 方案

`velocity` 的 `pressPos` 重载把"按下点"纳入触发条件：对**每一条边**，若按下时
指针已经在该边热区内，该边的热区收窄为"按下时指针到该边的距离"：

```text
pressDist = press 到该边的距离（低边缘为 pos - edge，高边缘为 edge - pos）
effectiveHotZone = pressDist >= hotZone ? hotZone          // 按下在热区外，行为不变
                 : max(pressDist, 1.0)                     // 按下在热区内：收窄
```

收窄后，指针必须**比按下时更靠近该边**（深度超过按下深度）才会产生滚动：

- 从左上角按下向右下框选：指针与左/上边缘的距离只增不减，永不触发左/上滚动；
- 用户确实想继续向视口外选择、把指针拖出该边：深度超过按下深度，滚动开始，
  方向与意图一致；
- 横穿视口进入对侧热区：对侧热区未收窄（按下时不在热区内），正常滚动；
- 按下在热区外：两端口径维持配置值，与旧行为完全一致。

判定是无状态的：拖动中途指针离开热区再回到热区，比较的仍是最初的按下点深度，
不需要维护"是否曾经出区"的状态机。

### 边界情况

- 按下点精确落在边缘上：`pressDist = 0`，收窄下限 `1.0` 保证热区不为零、滚动仍
  可触发（再向外拖即满速 ramp）；
- 轴的两端分别处理：按下在左热区内只收窄左端，右端维持原热区；
- 仅在某个轴上收窄，另一个轴不受影响（如从左侧边缘按下后向下拖，垂直轴仍按
  原热区工作）。

## 轨道 RHI 后端的独立实现

`TracksRhiWidget` 的"外部文件拖入"滚动是独立场景：没有鼠标按下点，指针在拖放
过程中持续位于视口上/下边缘，期望行为就是"进入热区即垂直滚动"。它直接使用
`EdgeAutoScroller` 的无死区接口（`velocity` / `computeStep` 基本版本）驱动垂直
滚动条，复用同一套速度模型，但不受按下点死区约束。

## 测试

`src/tests/TestEdgeAutoScroll/main.cpp` 覆盖纯计算层：

- 热区外零速度、四条边方向符号正确；
- 向边缘单调加速、视口外饱和为 `maxSpeed`；
- 角点双轴速度、禁用的轴恒为零；
- 亚像素累积在抖动帧时长下 1 秒累积精确为 `speed × 1s`；
- 按下点死区：四个角按下后移向中心速度为零、向角内深入才向角滚动、贴边按下、
  热区外按下行为不变、`computeStep` 同步生效。

## 注意事项与已知限制

- 速度上限较高（水平 1200 px/s）且存在正反馈，指针停在边缘时视口会快速滚过；
  如果实际体验偏快，调整 `EdgeAutoScrollConfig` 即可，无需改逻辑；
- 滚动期间拖动逻辑（橡皮筋、音符跟随等）依赖 `onEdgeAutoScrollFrame` 收到的
  钳制后指针位置，接入新编辑器时需确保该回调被正确调用；
- 修改滚动条时先停缓动动画（`stopViewportAnimations` 相关路径），否则两者会互相
  覆盖；
- `PianoRollRhiWidget`（RHI 后端钢琴卷帘）目前没有该机制，若要补齐需在 RHI
  后端复用 `EdgeAutoScroller`，且同样需要考虑按下点死区。
