# 触摸与触控笔输入设计

本文记录主编辑区（钢琴卷帘、参数编辑器、轨道编排区）的多点触控与触控笔支持。目标平台是 Windows 触摸屏二合一设备，实现本身不依赖任何 Windows API，在其他有触摸屏的平台上同样生效。

## 一、输入分流

四类输入按设备自动分流，没有模式开关。

| 输入设备 | 处理路径 |
| --- | --- |
| 触摸（手指） | `EditorTouchController` 手势层，见下文 |
| 触控笔 | 不做特殊处理。控件不接受 `QTabletEvent`，由 Qt 合成鼠标事件，走既有鼠标逻辑 |
| 精密触控板、滚轮 | Windows DirectManipulation（QWDMH），行为不变 |
| 鼠标 | 既有逻辑，完全不变 |

### DirectManipulation 作用域收缩

改动前 `MainWindow` 用 `registerWindow(window)` 注册，该重载等价于 `DeviceType::All`。DirectManipulation 会在窗口级吞掉全部指针原生消息，包括单指触摸和笔，Qt 因此永远收不到 `QTouchEvent`。

现在统一走 `MainWindow.cpp` 里的 `registerScopedDirectManipulation()`，设备掩码收缩为 `Touchpad | Wheel`，手势配置保持与原重载一致（`TranslationX | TranslationY | Scaling | TranslationInertia | ScalingInertia`）。触控板的平滑滚动和捏合缩放不受影响，触摸和笔则重新可见。

设置项文案同步改为触控板语义，见第五节。

## 二、分层结构

四个新文件都在 `src/app/UI/Views/Common/` 下。

```
EditorTouchGesture      纯逻辑手势状态机，不依赖控件，可单测
EditorTouchController   把手势翻译成合成鼠标事件或视口操作，绑定到具体控件
EditorTouchTarget       编辑器视图需要向手势层提供的接口
EditorPointerUtils      "这是手指还是鼠标"的共享判定
```

`EditorTouchTarget` 由两套后端各自实现，因此一层手势代码同时覆盖 Legacy（`TimeGraphicsView` 系）和 RHI（`EditorRhiWidget` 系）两条链路。

| 实现者 | 说明 |
| --- | --- |
| `TimeGraphicsView` | Legacy 后端的公共实现，导航部分对全部子类通用 |
| `PianoRollGraphicsView` | 覆盖命中判定与空白拖动策略 |
| `TracksGraphicsView` | 覆盖空白拖动策略与中止逻辑 |
| `ParamEditorGraphicsView` | 永远工具驱动 |
| `PianoRollRhiWidget` | RHI 钢琴卷帘 |
| `TracksRhiWidget` | RHI 轨道编排区 |

### 手势状态机

`EditorTouchGesture` 只接收触点 id、控件坐标和毫秒时间戳，返回一串意图。它识别：

- 点按与双击
- 超出 tap slop 的拖动
- 超时未移动的长按
- 双指导航（平移加两轴捏合）

关键的时序约定：**导航更新必须每个触摸事件 flush 一次**。`moved()` 在双指阶段只记录位置，真正的平移量和缩放系数由 `flushNavigation()` 产出。如果按触点逐个计算，两指同向平移时先动的那根手指会让跨距先缩后涨，表现为一次可见的缩放抖动。

### 合成鼠标事件

单指编辑不改动任何既有交互状态机，而是翻译成 `QMouseEvent` 发给控件。事件携带原始触摸设备指针，因此代码可以通过 `EditorPointer::isTouchDevice()` 反查来源。

`QGraphicsView` 的鼠标事件从 viewport 进入，所以 Legacy 后端的合成事件发给 `viewport()`，RHI 后端发给控件自身。

## 三、手势定义

### 单指

| 场景 | 钢琴卷帘 | 轨道编排区 | 参数编辑器 |
| --- | --- | --- | --- |
| 点按对象 | 选中 | 选中 | 工具驱动 |
| 点按空白 | 取消选择 | 取消选择 | 工具驱动 |
| 双击 | 与鼠标双击一致 | 与鼠标双击一致 | 工具驱动 |
| 拖动对象 | 移动 | 移动 | 工具驱动 |
| 拖动对象边缘 | 改长度 | 伸缩 | 工具驱动 |
| 拖动空白 | 画音符 | 平移视口 | 工具驱动 |
| 长按对象 | 上下文菜单 | 上下文菜单 | 无 |
| 长按空白后拖动 | 框选 | 框选 | 工具驱动 |

"拖动空白等于画音符"只在钢琴卷帘处于 `Select`（没有显式选中特殊工具）时成立，称为**直接操纵**。工具栏显式选中擦除、切割、区间选择、画音高、烘焙音高、调制音高、锚点编辑中的任何一个时，显式工具优先，单指拖动与鼠标拖动完全一致。

直接操纵的实现是在拖动开始前把编辑模式临时切到 `DrawNote`，抬手后恢复。这样交互状态机零改动。点按不会触发直接操纵，手势层用 `Event::tap` 标记把它挡在外面，否则轻点空白就会留下一个零长音符。

### 双指

双指在任何状态下都是导航，两轴独立：

- 平移跟随两指质心
- 横向捏合缩放时间轴，纵向捏合缩放键高或行高
- 缩放锚点是质心，不是鼠标光标位置

**主导轴锁定**：一次双指手势内只允许一个方向缩放，除非两个方向的幅度接近。判定用累计对数缩放量，`|ln fx|` 超过阈值且超过 `|ln fy|` 的 `axisLockRatio` 倍时锁定横向，反之锁定纵向，两者接近时两轴同时放开。锁定在手势结束前不再改变，避免后半程手指走偏就开始缩放另一个轴。

判定发生的那一帧会用累计对数量补齐（`exp(ln 累计)`），锁定前积累的位移不会被丢掉。

**第二指落下会中止进行中的单指操作**。中止走 `cancelTouchPointerInteraction()`，各后端复用自己已有的"放弃编辑"路径（`discardAction()` / `abortPointerInteractions()` / `discardDrag()`），不会提交半截编辑。

**平移抬手带惯性**。惯性只作用于平移，不作用于缩放。速度用指数平滑估计，衰减是帧率无关的 `exp(-decay * dt)`，低于阈值即停止。单指平移空白（轨道编排区）同样带惯性。

### 长按菜单必须队列化

上下文菜单通过 `exec()` 起嵌套事件循环。在触摸事件派发过程中同步弹出会卡死手势流，所以长按菜单用 `QCoreApplication::postEvent()` 投递 `QContextMenuEvent`，由事件循环稍后处理。

## 四、配套修复

触摸引入后有两处旧代码的隐含假设不再成立。

**`QCursor::pos()` 不跟手指走。** 边缘自动滚动的定时器回调原本每帧读光标位置。现在三个视图各自记录 `lastPointerPosition`（在 `mousePressEvent` / `mouseMoveEvent` / `mouseReleaseEvent` 里更新，因此合成事件自动覆盖），定时器改读它。

**`QGuiApplication::mouseButtons()` 在触摸拖动时恒为 `NoButton`。** 边缘自动滚动用它做"按键已松开"的安全网，触摸下会立刻自我解除。改用 `EditorPointer::isPointerPressed()`，它同时看真实鼠标键和活跃的合成触摸流。

**命中容差**。手指的命中精度远低于鼠标，`EditorPointer::resizeTolerance()` 在触摸流活跃期间把 `AppGlobal::resizeTolerance` 放大到 2.5 倍。两套后端的音符和剪辑边缘判定都改用了它。

## 五、设置项

外观设置页的 Touch 卡片：

| 选项 | 键名 | 默认 | 说明 |
| --- | --- | --- | --- |
| 多点触控手势 | `enableTouchGestures` | 开 | 关闭后控件不接受 `QTouchEvent`，退回 Qt 默认的触摸转鼠标合成 |
| 精密触控板与滚轮滚动 | `enableDirectManipulation` | 开 | 仅 Windows 构建可见，只影响触控板和滚轮 |

两项都热生效，不需要重启。

## 六、TouchProbe 输入探针

`src/tools/TouchProbe/` 是一个独立诊断窗口，用来在真机上确认平台到底送来什么事件。桌面应用本身回答不了这三个问题：

1. 触摸是以 `QTouchEvent` 到达 Qt，还是只有合成鼠标
2. 触控笔产生 `QTabletEvent`、鼠标事件，还是两份都有
3. DirectManipulation 注册后究竟吞掉了什么

轨迹按设备着色，鼠标蓝、触摸绿（每个触点 id 独立色相）、笔红、滚轮与原生手势黄。**合成鼠标画成灰色虚线**，意外的合成一眼可见。HUD 显示最近事件与当前触点数，全量日志写到 `AppDataLocation/touch-probe.log`。

按键：`C` 清屏，`D` 切换 DirectManipulation（用与应用完全相同的 `Touchpad | Wheel` 配置），`F` 全屏，`Esc` 退出。

构建目标 `TouchProbe`，产物在 `build/Debug/out/bin/`。

## 七、测试

`src/tests/TestTouchGestures/` 覆盖 `EditorTouchGesture` 的全部判定，不需要触摸硬件：分流规则、点按与双击、长按两种走向、第二指中止、双指平移不漏缩放、逐点更新不产生伪捏合、两轴锁定与锁定保持、惯性速度估计。

## 八、已知限制

- 歌词内联编辑依赖 Windows 触摸键盘，本方案不介入。
- `PhonemeView`、标尺、钢琴键盘没有开启 `WA_AcceptTouchEvents`，走 Qt 默认的触摸转鼠标合成。这些视图的悬停提示在触摸下不会出现，属于无 hover 的正常降级。
- DirectManipulation 的设备类型收缩改变了触控板路径的注册参数，触控板用户需要回归确认平滑滚动与捏合仍然正常。
- 双指以上（三指及更多）不识别，多余的手指会被忽略直到全部抬起。
