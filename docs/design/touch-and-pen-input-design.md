# 触摸与触控笔输入设计

本文记录主编辑区（钢琴卷帘、参数编辑器、轨道编排区）的多点触控与触控笔支持。目标平台是 Windows 触摸屏二合一设备，实现本身不依赖任何 Windows API，在其他有触摸屏的平台上同样生效。

## 一、输入分流

四类输入按设备自动分流，没有模式开关。

| 输入设备 | 处理路径 |
| --- | --- |
| 触摸（手指） | `EditorTouchController` 手势层，见下文。平台自行合成的鼠标事件会被吞掉 |
| 触控笔 | 不做特殊处理。控件不接受 `QTabletEvent`，由 Qt 合成鼠标事件，走既有鼠标逻辑 |
| 精密触控板、滚轮 | Windows DirectManipulation（QWDMH），行为不变 |
| 鼠标 | 既有逻辑，完全不变 |

### DirectManipulation 作用域收缩

改动前 `MainWindow` 用 `registerWindow(window)` 注册，该重载等价于 `DeviceType::All`。DirectManipulation 会在窗口级吞掉全部指针原生消息，包括单指触摸和笔，Qt 因此永远收不到 `QTouchEvent`。

现在统一走 `MainWindow.cpp` 里的 `registerScopedDirectManipulation()`，设备掩码收缩为 `Touchpad | Wheel`，手势配置保持与原重载一致（`TranslationX | TranslationY | Scaling | TranslationInertia | ScalingInertia`）。触控板的平滑滚动和捏合缩放不受影响，触摸和笔则重新可见。

设置项文案同步改为触控板语义，见第五节。

### 必须吞掉平台合成的鼠标事件

接受 `QTouchEvent` 只能挡住 Qt 自己的触摸转鼠标合成，挡不住操作系统的。Windows 会把主触点提升为传统鼠标消息，`QWindowsPointerHandler::translateMouseEvent()` 原样转发，只打上 `Qt::MouseEventSynthesizedBySystem` 标记。真机日志证实了这一点：单指拖动同时产生完整的 `QTouchEvent` 流和一份 `mouse press/move/release dev=TouchScreen source=BySystem`。

后果有两层。单指编辑会跑两遍，一遍来自手势层的合成事件，一遍来自系统。更糟的是双指和三指导航时，被提升的主触点仍在驱动交互层，于是一边缩放一边拖动内容。

因此 `EditorTouchController::handleEvent()` 除了触摸事件，还要拦截鼠标事件并吞掉 `source() != Qt::MouseEventNotSynthesized` 的那些。判据成立是因为三类需要放行的事件都是 `NotSynthesized`：

- 手势层自己发的合成事件（`QMouseEvent` 构造时不传 source，默认 `NotSynthesized`）
- 真实鼠标
- 触控笔（`QGuiApplicationPrivate::processTabletEvent()` 从未被接受的 tablet 事件合成鼠标时显式用 `Qt::MouseEventNotSynthesized`，平台自己的笔转鼠标消息则被 `translateMouseEvent()` 无条件丢弃）

只在开启多点触控手势的编辑器控件内吞。应用其余部分照旧收系统合成的鼠标事件，因此按钮、菜单、轨道列表的触摸操作和长按右键都不受影响。

Qt 平台插件另有 `-platform windows:nomousefromtouch` 可以全局关掉系统合成，没有采用：它会连带干掉整个应用的触摸长按转右键，代价比收益大。

被吞掉的只有鼠标事件。Windows 长按转右键补的那一份 `QContextMenuEvent` 要放行，它正是长按菜单的来源，见下文。

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
| `PianoRollGraphicsView` | 覆盖命中三态判定与空白拖动策略 |
| `TracksGraphicsView` | 覆盖空白拖动策略与中止逻辑，命中三态沿用基类 |
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
| 拖动已选中的对象 | 移动 | 移动 | 工具驱动 |
| 拖动已选中对象的边缘 | 改长度 | 伸缩 | 工具驱动 |
| 拖动未选中的对象 | 平移视口 | 平移视口 | 工具驱动 |
| 拖动空白 | 平移视口 | 平移视口 | 工具驱动 |
| 长按对象 | 上下文菜单 | 上下文菜单 | 无 |
| 长按空白后拖动 | 框选 | 框选 | 工具驱动 |

空白拖动的归属由当前工具决定。钢琴卷帘处于 `Select` 时空白没有可操作的内容，单指拖动平移视口。工具栏显式选中画音符、擦除、切割、区间选择、画音高、烘焙音高、调制音高、锚点编辑中的任何一个时，显式工具优先，单指拖动与鼠标拖动完全一致——**创建音符和用鼠标一样需要先切到绘制音符模式**。

点按永远直通交互层，不会被当成拖动。手势层用 `Event::tap` 标记区分，因为按下和抬起是同一批事件发出的，只看"有没有命中内容"会把轻点空白误判成平移的起点。

### 先选中才能拖动

**手指只能拖动它已经选中的对象。** 落在未选中的音符或剪辑上时，拖动做的事情和落在空白上完全一样，也就是平移视口。

理由是命中面积。手指比鼠标粗得多，钢琴卷帘里音符又密，凡是想滚动视图的手势有很大概率压在某个音符上。如果按下即可拖动，浏览工程这件事就会不断误改音符长度和音高。规则改成"点一下选中，再拖才移动"之后，单指滚动配双指缩放可以放心地在音符堆里穿行，代价只是编辑某个对象要多点一下。这与 FL Studio Mobile 的处理一致。

判定统一由 `EditorTouchTarget::touchContentAt()` 给出三态：

| 返回值 | 含义 | 拖动结果 |
| --- | --- | --- |
| `None` | 空白画布 | 由 `touchBlankDragAction()` 决定 |
| `Unselected` | 有对象，但这根手指得先把它选中 | 同空白，即平移视口 |
| `Selected` | 手指可以直接拖动的对象 | 交给既有鼠标状态机 |

`Selected` 有两个来源：对象本身处于选中态，或者当前有显式工具占用了它。显式工具优先的规则在这里和空白拖动保持一致——绘制、擦除、切割等模式下音符照旧一按即拖，不需要先选中。

三态只影响拖动。点按、双击和长按仍然一律直通交互层，所以点按未选中的音符正常选中它，长按未选中的音符正常弹出上下文菜单。

各后端的实现：Legacy 侧 `TimeGraphicsView` 取最上层带 `ItemIsSelectable` 的图元读它的 `isSelected()`，覆盖轨道编排区和其余子类，钢琴卷帘另外把发音标签归到它所属的音符上。RHI 侧钢琴卷帘查 `appStatus->selectedNotes`，轨道编排区查 `ClipSnapshot::selected`。

### 双指

双指在任何状态下都是导航，两轴独立：

- 平移跟随两指质心
- 横向捏合缩放时间轴，纵向捏合缩放键高或行高
- 缩放锚点是质心，不是鼠标光标位置

**主导轴锁定**：一次双指手势内只允许一个方向缩放，除非两个方向的幅度接近。判定用累计对数缩放量，`|ln fx|` 超过阈值且超过 `|ln fy|` 的 `axisLockRatio` 倍时锁定横向，反之锁定纵向，两者接近时两轴同时放开。锁定在手势结束前不再改变，避免后半程手指走偏就开始缩放另一个轴。

判定发生的那一帧会用累计对数量补齐（`exp(ln 累计)`），锁定前积累的位移不会被丢掉。

**第二指落下会中止进行中的单指操作**。中止走 `cancelTouchPointerInteraction()`，各后端复用自己已有的"放弃编辑"路径（`discardAction()` / `abortPointerInteractions()` / `discardDrag()`），不会提交半截编辑。

**双指在任何状态下都能起手，包括手上还留着上一次手势的手指。** 手势结束后剩下的那根手指处于 `Settling`，它自己不能再发起编辑（否则抬手的过程会变成一次拖动），但只要第二根手指落下就立刻进入导航。少了这一条，一次被消费掉的长按或者上一次捏合残留的手指会把后续所有双指手势堵死，直到整只手离开屏幕。

**每个触摸事件都用平台上报的触点集合校准一次内部状态**（`syncActivePoints()`）。Qt 漏送一次 release（丢失的抬起、抓取转移）本来会让状态机永久卡住，校准让它在触点归零时无条件回到 `Idle`。

**平移抬手带惯性**。惯性只作用于平移，不作用于缩放。速度用指数平滑估计，衰减是帧率无关的 `exp(-decay * dt)`，低于阈值即停止。单指平移空白（轨道编排区）同样带惯性。

### 长按菜单交给平台

长按落在对象上时，手势层只做一件事：把这根手指标记为已消费，不让它拖动脚下的对象。菜单本身交给平台。

自己弹菜单在 Windows 上行不通。真机日志的时序是：手指按住约 900 毫秒抬起，Windows 在**抬起时**才补上右键按下、抬起和 `QContextMenuEvent`。手势层的长按 450 毫秒就触发，菜单在按住途中弹出，随后被 Windows 补的右键按下关掉——表现就是"按住半秒菜单出现，松手就消失"。让平台自己来，就顺带得到了按住时的白色方框反馈和松手才出现的原生时序，与 Windows 其他界面一致。

平台没有这类手势的地方（非 Windows）留了一条兜底：消费长按时记下位置，等所有手指离开屏幕后起一个 400 毫秒的定时器，期间收到任何 `QContextMenuEvent` 就取消。Windows 上平台菜单在触摸结束后一毫秒内就到，兜底不会触发。

兜底投递用 `QCoreApplication::postEvent()` 而不是 `sendEvent()`：上下文菜单通过 `exec()` 起嵌套事件循环，在触摸事件派发过程中同步弹出会卡死手势流。

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

轨迹按设备着色，鼠标蓝、触摸绿、笔红、滚轮与原生手势黄。触摸的每个触点 id 取绿色系里的一个固定色调，彼此可区分又不会被误认成别的设备。**合成鼠标画成灰色虚线**，意外的合成一眼可见。

轨迹按笔画分段：触点抬起、鼠标或笔松开都会结束当前笔画，另外超过 300 毫秒没有新点也会断开（悬停、滚轮和原生手势没有明确的结束事件）。否则抬手后在别处再落下会被一条直线连起来。

HUD 显示最近事件与当前触点数，全量日志写到 `AppDataLocation/touch-probe.log`。

按键：`C` 清屏，`D` 切换 DirectManipulation（用与应用完全相同的 `Touchpad | Wheel` 配置），`S` 切换吞掉合成鼠标（用与编辑器完全相同的判据），`F` 全屏，`Esc` 退出。开关 `S` 可以直接看出吞与不吞的差别，被吞的事件在日志里标 `SWALLOWED`，轨迹上不再出现灰色虚线。

探针也记录 `QContextMenuEvent` 及其 reason，用来确认长按是否引发了平台的右键模拟。

构建目标 `TouchProbe`，产物在 `build/Debug/out/bin/`。

## 七、测试

`src/tests/TestTouchGestures/` 覆盖 `EditorTouchGesture` 的全部判定，不需要触摸硬件：分流规则、点按与双击、长按两种走向、第二指中止、双指平移不漏缩放、逐点更新不产生伪捏合、两轴锁定与锁定保持、惯性速度估计。

## 八、已知限制

- 歌词内联编辑依赖 Windows 触摸键盘，本方案不介入。
- 触控笔在编辑器里同时产生 `QTabletEvent` 和鼠标事件。控件不处理前者，因此没有重复处理，但笔压和倾角目前没有被利用。
- `PhonemeView`、标尺、钢琴键盘没有开启 `WA_AcceptTouchEvents`，走 Qt 默认的触摸转鼠标合成。这些视图的悬停提示在触摸下不会出现，属于无 hover 的正常降级。
- DirectManipulation 的设备类型收缩改变了触控板路径的注册参数，触控板用户需要回归确认平滑滚动与捏合仍然正常。
- 双指以上（三指及更多）不识别，多余的手指会被忽略直到全部抬起。
