# 触控笔输入适配方案（反端橡皮 / 侧键语义 / 悬停）

> 状态：📝 方案待评审，尚未实施。2026-09-25
>
> **决策已逐项拍板（2026-09-25）**，记录见第二节。本方案的全部 Windows 平台论断来自同日 Surface Pen + Windows 平板的真机探测（`TouchProbe` 探针），并已用 Qt 6.11.2 源码逐条对照，不是推测；macOS/Linux 的结论只来自源码推断，标注为未验证。对照现有契约：`docs/design/touch-and-pen-input-design.md`（该文档第一节目前写的是"触控笔不做特殊处理"，完成后需要同步，见第八节）。

## Goal

触控笔在编辑器中目前的身份就是鼠标：控件不接受 `QTabletEvent`，由 Qt 合成鼠标事件走既有逻辑。这带来两个说不通的行为：

- **反端（笔尾）在画，而不是在擦。** 平台把反端也报成左键，Qt 合成的鼠标事件与笔尖完全一致。
- **笔杆侧键等于右键**，于是"侧键+拖动"一路把右键菜单拖出来，而按需求它应该擦除。

目标是把笔的三种物理输入映射到编辑器已经存在的语义上，**不引入新的编辑概念，也不改触摸路径**：

| 物理输入 | 期望行为 |
| --- | --- |
| 笔尖 | 与鼠标完全一致（今天的默认行为） |
| 反端 | 橡皮：可擦除的东西擦除，不适合擦除的工具不响应 |
| 侧键 + 落笔拖动 | 橡皮（同反端） |
| 侧键 + 原地点击、无位移 | 右键菜单 |
| 反端悬停（未接触） | 光标变橡皮，与 OneNote 一致 |
| 侧键悬停按下（未接触） | 光标提示擦除，与 OneNote 的套索提示同理 |
| 笔尖长按（无侧键） | 什么都不做；系统的圆环动画目前无法消除，见 §1.2-5 |

覆盖范围与触摸一致：钢琴卷帘、独立参数编辑器，Legacy 与 RHI 两套后端。**平台范围：Windows 先落地（只有它有真机数据），macOS/Linux 的代码分支同期写入但标注为未验证、不宣称支持**，见第七节。

## 一、真机实测基线

### 1.1 平台上报（Surface Pen，设备名 `wmpointer`）

| 手势 | 平台上报 |
| --- | --- |
| 笔尖悬停 | `QInputDevice` 才注册（**惰性**，启动枚举不到）；`tablet move ptr=Pen pressure=0`，同时派生鼠标 move |
| 笔尖落笔画线 | `tablet press ptr=Pen button=Left buttons=Left pressure>0`，同时派生鼠标 press |
| 反端画线 | 首次使用才注册**第二个 `QInputDevice`**（`ptr=Eraser`，同 `systemId`）；`press ptr=Eraser button=Left buttons=Left` |
| 侧键 + 拖动 | `press ptr=Pen button=Right buttons=Right pressure>0`（**不含 Left**） |
| 侧键 + 点击 | 与上行**完全一样**，平台不区分拖动与点击 |
| 侧键在笔画中途按/松 | 产生**自相矛盾的额外 press/release**：`press button=Left buttons=Right`、`release button=Left buttons=Left` |
| 悬停中按侧键 | **Qt 完全不上报**（1.8 秒窗口内 Qt 侧只有 `tablet move` 472 条、`mouse move` 6 条，零 press、零 menu）；原生过滤器读得到 `barrel=on inContact=off` |
| 反端悬停（未接触） | 同上，Qt 不上报；原生过滤器读得到 `inverted=on inContact=off` |
| 笔尖长按 3.4 / 3.5 / 6.9 秒 | 系统圆环动画出现，但**不产生任何 `QContextMenuEvent`**，tablet 流不中断 |
| 触摸长按 1.05–1.15 秒 | 系统补合成 `mouse press button=Right` + 真 `QContextMenuEvent`（既有行为，要保留） |

### 1.2 与实现直接相关的五条结论

**1. 反端只能靠 `pointerType()` 分辨，不能靠按键。** 反端 press 报的就是 `button=Left buttons=Left`，与笔尖一字不差。判据只能取 `QTabletEvent::pointerType() == QPointingDevice::PointerType::Eraser`（`EditorPointer::isPenDevice()` 已存在但全仓无人调用，将是本方案第一个消费者）。

**2. 侧键按下时 `buttons` 里没有左键，不能用它判断笔尖是否落下。** Qt 的判据写死在 `qwindowspointerhandler.cpp`：

```cpp
if (pointerInContact && penInfo->penFlags & PEN_FLAG_BARREL)
    mouseButtons = Qt::RightButton; // Either left or right, not both
```

侧键按住期间 `pressure` 仍然大于 0，所以**接触状态一律用 `pressure` 判断**，`buttons` 只用来读侧键。

**3. 不能用 `TabletPress`/`TabletRelease` 当笔画起止。** 上面的实测显示侧键中途切换会额外吐出 press/release，其中 `press button=Left buttons=Right` 出现 9 次、`release button=Left buttons=Left` 出现 11 次（后者自相矛盾：报"松开左键"而 `buttons` 仍带左键）。笔画边界必须由 `pressure` 的零与非零给出，press/release 只能当提示。

**4. 接受 `QTabletEvent` 就能独占该笔画的鼠标流。** 实测 A/B 双向可复现：`event->ignore()` 时笔的每个笔画都带一份派生鼠标事件，`event->accept()` 后该笔画**零 mouse 事件**，取而代之是密集 tablet 事件。机制在 Qt 源码里是闭合的——`qwindowsintegration.cpp:211` 把 `platformSynthesizesMouse` 设为 `false`，于是 `qguiapplication.cpp:3033` 在**未被接受**的 tablet 事件上合成鼠标事件，用笔设备、标 `Qt::MouseEventNotSynthesized`。这与触摸合成分工明确：触摸合成是 `source=BySystem` + `dev=TouchScreen`，笔合成是 `source=NotSynthesized` + `dev=` 笔设备。`EditorTouchController` 现在"只吞 `source() != NotSynthesized`"的判据因此对笔天然放行，无需改动。

**5. 笔的长按不需要处理，触摸的长按必须保留。** 笔尖按住 6.9 秒都不产生 `QContextMenuEvent`，只有视觉圆环（`DefWindowProc` 的 legacy 按压手势）；要消掉圆环，Qt 侧没有任何开关（`QWindowsWindowFunctions` 在 Qt 6 已删，`nomousefromtouch` 会连带干掉触摸长按菜单，本应用依赖它）。**因此本方案不处理笔的长按。** 触摸长按转右键是既有行为，见 `docs/design/touch-and-pen-input-design.md` 第三节，回归时必须确认未受影响。

### 1.3 悬停期侧键与反端：已确认可读（需原生过滤器）

侧键在悬停期的状态在系统层是存在的——OneNote 就是靠它把光标变成套索——但它过不了 Qt：结论 2 里那个 `pointerInContact &&` 让 `PEN_FLAG_BARREL` 在悬停期被直接丢弃，**任何纯 Qt 应用都读不到**（Krita 同样读不到，它的侧键判定也是 `buttons() & Qt::RightButton`）。

绕开方式是读原始消息。已在 `TouchProbe` 中实现并在真机验证（`PenRawStateFilter`：`QAbstractNativeEventFilter` 拦 `WM_POINTERUPDATE/DOWN/UP`，对 `PT_PEN` 指针调 `GetPointerPenInfo()` 读 `penFlags`），实测序列：

```
18:37:31.429 WM_POINTERUPDATE barrel=off inverted=off inContact=off   ← 基线，笔尖悬停
18:37:38.927 WM_POINTERUPDATE barrel=on  inverted=off inContact=off   ← 悬停中按下侧键
18:37:40.692 WM_POINTERUPDATE barrel=off inverted=off inContact=off   ← 松开侧键
18:37:58.199 WM_POINTERUPDATE barrel=on  inverted=off inContact=off   ← 再次按下，随后落笔
18:38:00.154 WM_POINTERDOWN   barrel=on  inverted=off inContact=on    ← 笔尖落下，状态连贯
18:38:03.626 WM_POINTERUP     barrel=on  inverted=off inContact=off
18:38:04.857 WM_POINTERUPDATE barrel=off inverted=off inContact=off   ← 抬笔后松开侧键
18:38:09.209 WM_POINTERUPDATE barrel=off inverted=on  inContact=off   ← 反端悬停，未接触即可读
```

三点结论：

1. **侧键在悬停期行为与修饰键一致**，按下即 `barrel=on`、松开即 `off`，`inContact` 始终为 `off`，逐状态边沿干净。
2. **反端在悬停期也能提前读到**（`inverted=on inContact=off`）。这比预期的多一条能力：反端此前只能靠 `QTabletEvent` 的 `pointerType()` 分辨，而那个设备要**首次接触才注册**；现在反端还没碰到屏幕就能知道，OneNote 那种"悬停即显示橡皮光标"因此可以做。
3. 同一时刻 Qt 侧确实一片空白（§1.1 表里的 1.8 秒窗口统计），所以这**不是** Qt 能不能转发的问题，必须自己读消息。

代价是这一段要依赖 Windows API。触摸层刻意保持的平台无关性（`docs/design/touch-and-pen-input-design.md` 开头就写明"实现本身不依赖任何 Windows API"）不适用于这一小块，因此实现上要把它隔离成一个 Windows 专属的小垫片，非 Windows 退化为空实现，钢笔层本身不出现任何 Windows 类型（见 §2 与步骤 7）。

## 二、设计决策

### 2.1 已拍板事项（2026-09-25）

| # | 决策项 | 结论 |
| --- | --- | --- |
| 1 | 接管范围 | **只接管接触中的事件**，悬停交回 Qt 合成鼠标 |
| 2 | 不支持擦除的工具下 | **整段吞掉，什么都不发生**（音符分割、锚点编辑、调制音高、区间选择、参数变换） |
| 3 | 笔画中途才按下侧键 | **忽略**，本笔画仍按笔尖处理（侧键落笔瞬间锁定） |
| 4 | 悬停期光标反馈 | **本期做**（反端显示橡皮光标、按住侧键显示擦除光标），为此引入悬停状态垫片，见第 8 条 |
| 5 | 反端"能擦"的范围 | **两类都擦**：音符选择下擦音符，画音高/擦除音高/描摹音高下擦参数 |
| 6 | 系统长按圆环 | **接受现状，不处理**（实测对笔只有视觉动画，无菜单事件） |
| 7 | 设置开关 | **不加**（笔没有"退回 Qt 默认合成"这个有意义的退回目标） |
| 8 | 跨平台范围 | **同时写入 Linux/macOS 分支**（按源码推断），标注未验证、不宣称支持 |

第 8 条决定了下面的结构：判据与垫片都按多平台组织，Windows 是唯一有真机验证的实现。

### 2.2 决策细则

| 项 | 决策 | 依据 |
| --- | --- | --- |
| 接管范围 | **只接管接触中的 tablet 事件**（`pressure > 0` 或已在笔画中）；悬停事件保持 `ignore()`，继续由 Qt 合成鼠标 move | 悬停提示、光标形状、边缘自动滚动的 `lastPointerPosition` 全部继续照旧工作，零改动 |
| 笔画边界 | 由 `pressure` 从 0 变正 / 变回 0 给出 | §1.2-3，press/release 不可靠 |
| 反端判据 | `pointerType() == Eraser` | §1.2-1 |
| 侧键判据 | **笔设备上的非左键即侧键**，**落笔瞬间锁定**，整个笔画内不再改 | §1.2-2；按键值各平台不同，写死 `RightButton` 会漏掉 X11（见 7.2）；笔画中途切换会产生假事件 |
| 侧键语义 | **延后合成按下**：落笔先只记位置，位移超过阈值才补发"擦除按下"，未超阈值就抬起则改为要一份右键菜单 | 与触摸层"空白长按：不动是菜单，动了是框选"（design 文档第三节同名小节）完全同构，同一套模式已在真机验证可用 |
| 擦除意图的表达 | 不切换工具、不改工具栏按钮状态；用 `EditorPointer` 的全局意图标记 + 逐工具接线 | 全局标记是 `EditorPointerUtils` 既有先例（`isTouchStreamActive` 等）；切工具会让工具栏高亮跳动，且"不响应的工具"没法表达 |
| 各工具是否响应 | 由各视图从当前工具算出，见步骤 6 的策略表 | 需求来自每类工具的实际用途，不是平台限制 |
| 悬停期侧键与反端 | 垫片按"接口 + 两份实现"：Windows 读原生 `WM_POINTER` + `GetPointerPenInfo()`，其他平台从悬停的 `QTabletEvent` 取；只读取、不消费消息 | Qt 在悬停期丢弃 `PEN_FLAG_BARREL`/`PEN_FLAG_INVERTED`（§1.2-2、§1.3），Windows 上没有别的入口 |
| 平台无关性 | 原生类型只出现在垫片的 Windows 实现里，`EditorPenController`、`EditorPenTarget` 与垫片接口不出现任何原生类型 | 触摸层刻意保持平台无关，这一小块做不到，只能隔离（第七节给出各平台差异与验证清单） |
| 合成事件目标 | 与触摸层一致：Legacy 发 `viewport()`，RHI 发控件自身 | `QGraphicsView` 的鼠标事件从 viewport 进入 |
| 触摸路径 | **一行不改** | 笔与触摸的设备、`source` 标记、接受策略三者都不相交 |

## 三、分层与接入点

新增三件，放在 `src/app/UI/Views/Common/`，与触摸三件套并列：

```
EditorPenController     接受 tablet 事件、跟踪笔画状态、合成鼠标事件并维护擦除意图
EditorPenTarget         视图向钢笔层回答"当前工具允许橡皮做什么"
EditorPenHoverWatcher   悬停期状态垫片，两份实现：Windows 读原生 WM_POINTER，
                        其他平台从悬停的 QTabletEvent 取（推断，未验证）
```

`EditorPenController` 不做手势识别（笔只有单点），也不需要 `EditorPenGesture` 之类的新状态机——它只是一层状态跟踪加事件翻译。

接入点与现有触摸控制器**一一对应**，共四处：

| 位置 | 文件 | 说明 |
| --- | --- | --- |
| Legacy 全部视图 | `TimeGraphicsView.cpp:135`（构造）、`:387`（`viewportEvent()`） | 触摸控制器就装在这里，因此钢琴卷帘、参数编辑器、轨道编排区一次覆盖 |
| RHI 钢琴卷帘 | `PianoRollRhiWidget.cpp:3017` | RHI 侧逐个控件构造 |
| RHI 轨道编排区 | `TracksRhiWidget.cpp:160`、`:460` | 同上 |
| RHI 基类入口 | `EditorRhiWidget.cpp:722`（`event()`） | 与触摸一致，在子类 `event()` 里分发 |

`EditorPenTarget` 的实现者与 `EditorTouchTarget` 相同（`TimeGraphicsView` 系与两个 RHI 控件），因此两个后端各只有一处策略表。

## 四、实现步骤

### 1. 骨架与接入

新建 `EditorPenController`/`EditorPenTarget`，在四处置入并接进事件链。此步只做"接受/忽略"的判定与日志，不改任何行为，便于先上真机验证接管本身没有副作用。

### 2. 悬停保持穿透

```cpp
// 只在接触中接管：悬停仍交回 Qt，悬停提示与光标因此完全不受影响。
if (event->type() == QEvent::TabletPress || penInContact(event))
    event->accept();
else
    event->ignore();
```

`penInContact(event)` = `event->pressure() > 0.0`，并叠加"当前笔画是否已开始"的内部标记，避免抬笔那一帧的 `pressure == 0` 被误判为悬停（`TabletRelease` 必须按已开始的笔画处理）。

### 3. 笔画状态与侧键锁定

`TabletPress` 时记录该笔画的既定属性：`pointerType`、侧键是否按下（**笔设备上的非左键即侧键**，见 7.2）、起点。整段笔画内这三项不再改变：

- 中途的 press/release 杂音一律忽略（§1.2-3）。
- **笔画中途才按下的侧键不改变语义**（决策 3）：这一段仍是笔尖笔画，不会中途变成擦除，也不会弹菜单。
- 接触状态由 `pressure` 独立跟踪，与上面三项无关。

### 4. 反端 → 擦除意图

`pointerType == Eraser` 时置位擦除意图，并把该笔画合成为**左键**鼠标笔画（与笔尖同一条路径），由各工具按步骤 6 的策略表决定响应与否。不合成右键——右键在参数编辑器里另有含义（`CommonParamEditorView.cpp:509-510` 右键即 Erase，但音符类 handler 会直接拒绝右键：`EraseNoteHandler.cpp:18`、`SplitNoteHandler.cpp:44` 都是 `if (event->button() != Qt::LeftButton) return;`）。

### 5. 侧键：拖动擦除 / 点击菜单

落笔时若锁定为侧键按下，**先不合成任何按下**，只记位置与时刻：

- 位移超过阈值 → 在该笔画**原始起点**补发一次左键按下并置位擦除意图，随后照常转发移动（起点不能取越过阈值的那一点，否则擦除会从偏后的位置开始）。整段不再触碰右键，因此平台与应用都不会弹菜单。
- 一直未超过阈值就抬起 → 确认是侧键点击，改为合成一次右键 press/release，走既有的上下文菜单路径。

阈值取与触摸长按同一个 slop 常量，避免两套手势手感不一致。

### 6. 各工具接线（两套后端各自一张策略表）

视图侧实现 `EditorPenTarget::penEraserAction()`，从当前工具算出结果：

| 当前工具 | 橡皮行为 | 触点 |
| --- | --- | --- |
| 音符选择 `Select` | 擦音符 | `EraseNoteHandler` 的擦除路径 |
| 擦除音符 `EraseNote` | 擦音符 | 同上（本工具已是擦除） |
| 画音高 `DrawPitch` | 擦参数 | `CommonParamEditorView::setEraseMode(true)` 的等价路径 |
| 手绘橡皮 `ErasePitch` | 擦参数 | 同上 |
| 描摹音高 `TracePitch` | 擦参数 | 同上 |
| 音符分割 `SplitNote` | **不响应** | 整段笔画吞掉 |
| 锚点编辑 `EditPitchAnchor` | **不响应** | 整段笔画吞掉 |
| 调制音高 `ModulatePitch` | **不响应** | 整段笔画吞掉 |
| 区间选择 `IntervalSelect` | **不响应** | 整段笔画吞掉 |
| 参数编辑器 `Draw`/`Erase`/`Trace` | 擦参数 | `ParamEditorGraphicsView.cpp:236-237` 的等价路径 |
| 参数编辑器 `Shape`/`Scale`/`Anchor` | **不响应** | 整段笔画吞掉 |

Legacy 侧接入点集中在 `PianoRollGraphicsView.cpp:1540` 的 `setPitchEditMode()`（它已经把工具翻译成 `setEraseMode`/`setTraceMode`/`setCurveTransformMode`），以及各 `PianoRollEditHandler` 子类的 `mousePressEvent`。RHI 侧在 `PianoRollRhiWidget` 的 `PitchEditType`（`PianoRollRhiWidget.cpp:2938`）与 `beginPitchEdit()`（`:1430`）附近。

"不响应"的实现在笔层完成（直接吞掉），不进入工具——这样"不响应"的工具不需要各自加判断。

### 7. 悬停期侧键与反端：光标反馈

§1.3 已确认这条路径可用，按下面实现，只驱动光标，不改任何笔画语义。

`EditorPenHoverWatcher` 分两份实现（第 8 条决定）。Windows 那份沿用仓库里已有的写法（`EditorTouchProbe.cpp:134` 的 `NativeFilter` 是同一种东西，安装点在它旁边的 `EditorTouchProbe::install()`），要点：

- 用 `QCoreApplication::installNativeEventFilter()` 装一个进程级过滤器，**只读取，永远返回 false**——Qt 的 `QWindowsPointerHandler` 还要照常处理同一条消息（`MainWindow::nativeEvent()` 是另一个先例，但那是窗口级的，这里要的是全局状态）。
- 只处理 `WM_POINTERUPDATE/DOWN/UP`，先 `GetPointerType()` 确认 `PT_PEN` 再 `GetPointerPenInfo()`，只取 `penFlags` 的 `PEN_FLAG_BARREL` 与 `PEN_FLAG_INVERTED | PEN_FLAG_ERASER`。悬停期消息频率约 260 条/秒（实测 1.8 秒 472 条 tablet 事件），所以按状态变化再做后续动作，不要在每条消息上改光标。
- 其他平台那份从悬停的 `QTabletEvent` 读 `buttons()` 与 `pointerType()`（7.5 的推断），先写着并按 7.7 标注未验证。
- 状态与 Qt 的 `QTabletEvent` 各自独立，只需在**落入编辑器控件范围时**生效：悬停光标属于视图职责，由 `EditorPenController` 在 `TabletEnterProximity`/`TabletLeaveProximity` 与悬停 move 之间维护，别让全局状态直接在任意控件上改光标。
- `EditorPenController` 只暴露平台无关的查询（例如 `isHoverEraseHint()`），原生类型不越过垫片边界。

若后续发现反端悬停的光标反馈与 `QTabletEvent` 的 `pointerType()` 有重复路径，以 `pointerType()` 为准、垫片只补它给不了的信息（悬停期、侧键），避免两套状态互相打架。

### 8. 回归

接受 tablet 事件之后，笔不再产生鼠标事件，因此以下几处必须逐项确认（它们此前都靠笔的鼠标流工作）：

- 悬停提示与光标形状（第二节"只接管接触中"这条决策就是为它们保留的）
- 边缘自动滚动读的 `lastPointerPosition`（`docs/design/touch-and-pen-input-design.md` 第四节）
- `EditorPointer::isPointerPressed()` 的安全网
- 触摸路径：`EditorTouchController` 的吞事件判据不受影响（§1.2-4），但需实测确认触摸长按菜单、双指导航、手势惯性均无变化

## 五、测试

`src/tests/` 下沿用触摸的思路，把可判定的部分做成不依赖硬件的单测：笔画边界由 `pressure` 决定（含抬笔那一帧 `pressure == 0` 不误判为悬停）、侧键锁定后中途的假 press/release 被忽略、反端合成为左键且带擦除意图、侧键位移阈值两侧的走向（补发按下 vs 补发右键）、"不响应"的工具整段被吞。策略表 `penEraserAction()` 的每条映射各一例。

真机验证仍需 `TouchProbe` 与平板上的人工手势序列，重点看：反端在音符选择下擦音符、在画音高下擦参数、在分割与锚点编辑下完全不动；侧键拖动擦除且不弹菜单；侧键点击弹菜单；笔尖行为与改动前一致；悬停期反端与侧键的光标反馈正确。

悬停期状态读取（步骤 7）走的是 Windows 原生消息，没有单测可写，只能真机验——本次已用探针在同一台设备上验证通过，实现后按同一套手势复验即可。

## 六、明确不做

- **不处理笔的长按与系统圆环**。笔尖按住 6.9 秒都不产生 `QContextMenuEvent`（§1.2-5），功能上无需处理；圆环动画在 Qt 侧无 API 可关，不做窗口过程级拦截。
- **不引入笔压与倾角**。平台已上报 `Pressure`/`XTilt`/`YTilt`，但编辑器目前没有任何按压敏感的表达方式，不在本方案范围（design 文档第九节已记为已知限制）。
- **不动触摸任何行为**，包括触摸长按转右键。
- **不为笔加设置开关**。触摸那套手势开关是为了"关掉退回 Qt 默认合成"，笔没有等价的退回目标——反端与侧键在退回后是错的，没有用户会想关掉。

## 七、可移植性（其他平台与其他笔）

### 7.1 天然可移植的部分

核心设计只用到 Qt 事件 API，没有任何 Windows 概念，因此换平台不需要重新设计：

| 设计元素 | 用到的 API | 可移植性 |
| --- | --- | --- |
| 笔画边界用接触状态 | `QTabletEvent::pressure()` | ✅ |
| 反端判据 | `QTabletEvent::pointerType() == Eraser` | ✅（反端来源各平台不同，见 7.3） |
| 侧键取得 | `QTabletEvent` 的 `buttons()` | ✅（按键值不跨平台，见 7.2） |
| 独占笔画 | `event->accept()` | ✅（源码依据见 7.4） |
| 拖动/点击区分 | 自行按位移判定 | ✅ |
| 逐工具策略表 | `EditorPenTarget` | ✅ |

### 7.2 需要改写的判据：侧键的按键值不跨平台

本方案在 Windows 上写的是 `buttons().testFlag(Qt::RightButton)`，这个值**不能跨平台照抄**：

| 平台 | 笔尖 | 侧键 | 来源 |
| --- | --- | --- | --- |
| Windows | `Left` | `Right`，**独占**（替换掉 Left） | 实测 + `qwindowspointerhandler.cpp` |
| X11 | `Left` | 笔杆两个键分别是 X 键 2/3，映射为 `Middle` / `Right`，且**叠加**（`buttons \|= b`） | `qxcbconnection_xi2.cpp:1497-1507`，注释写明 `the tip, plus two barrel buttons` |
| macOS | `Left` | 由 `NSEvent buttonMask` 跟踪 | `qnsview_tablet.mm:77` |

两点结论：

- 判据应写成**"笔设备上的非左键即侧键"**，而不是写死 `RightButton`。Wacom 双键笔杆在 X11 上还能分出上下两颗键（`Middle`/`Right`），Windows 上不行（7.6）。
- 这让"**接触状态必须用 `pressure` 判断**"这条从"Windows 上的坑"升级为跨平台必需：X11 的 `buttons` 可能同时含 `Left` 和侧键。

### 7.3 反端的来源各平台不同，但都能得到 `Eraser`

| 平台 | 反端如何产生 `PointerType::Eraser` |
| --- | --- |
| Windows | `PEN_FLAG_INVERTED \| PEN_FLAG_ERASER`，且是**惰性注册的第二个 `QInputDevice`**（实测） |
| X11 | Wacom 驱动上报的工具 id 列表（`Intuos* Airbrush Eraser`、`Art Pen Eraser` 等，`qxcbconnection_xi2.cpp:150-185`），并靠 `WacomSerialIDs` 上报 proximity |
| macOS | `NSPointingDeviceTypeEraser`（`qnsview_tablet.mm:165`） |

`pointerType()` 这层判据可以统一写；要留意 Windows 上反端设备首次接触才注册，所以**"反端设备存在"不能当能力探测**。

### 7.4 "接受 tablet 事件即接管鼠标流"是跨平台的

这条机制的依据是 Qt 自己的合成开关，各平台后端都会把它关掉：`windows`、`wayland`、`cocoa`、`ios`、`android`、`wasm` 无条件设 `setPlatformSynthesizesMouse(false)`，`xcb` 则在 XInput2 事件选择成功后设置（`qxcbconnection_xi2.cpp:94`，失败时不设）。所以"接受事件 → Qt 不再合成鼠标"在主流桌面平台都成立；X11 那个条件分支意味着**没有 XInput2 的异常环境**需要另测。

### 7.5 悬停期状态：接口一套，实现两份

按第 8 条决定，垫片不是 Windows 专用类，而是"接口 + 两份实现"：

| 实现 | 悬停期侧键与反端的来源 | 状态 |
| --- | --- | --- |
| Windows | 原生 `WM_POINTER` + `GetPointerPenInfo()` 的 `penFlags`（Qt 在这一层把它丢弃了，§1.3） | ✅ 真机验证 |
| 其他平台 | **大概率不需要原生代码**：X11 的 `tabletData->buttons` 由 XI 按键事件累积、与接触状态无关（`qxcbconnection_xi2.cpp:1497-1507`），macOS 的 `buttonMask` 同理（`qnsview_tablet.mm:77`），悬停期的 tablet 事件本身就该带着侧键状态，直接从 `QTabletEvent` 读即可 | ⚠️ 源码推断，未验证 |

这个分工让接口很薄：`EditorPenHoverWatcher` 只需回答"悬停期侧键是否按下、是否反端"。Windows 走原生消息，其他平台走 tablet 事件，都读不到的极端情况退化为没有光标反馈，笔画语义不受影响。

### 7.6 其他笔（Wacom 等）在 Windows 上

Windows 上所有笔统一走 WM_POINTER（Windows Ink），映射一致，所以方案不做任何设备特判。三个边界要知道：

- **Wacom 驱动里关掉"使用 Windows Ink"之后，笔会退化成普通鼠标**（不再产生 `QTabletEvent`），钢笔层整个不介入，行为回到今天：反端在画、侧键是右键。这是本方案覆盖不到的场景，也不是编辑器能修的，只能靠文档告知用户开启 Windows Ink。
- **双键笔杆在 Windows 上被 Qt 合并成一个 barrel 标志**（`PEN_FLAG_BARREL`），分不出上下键；X11 反而能分出两颗（7.2）。若要区分，垫片读的原始 `pointerFlags` 里有 `POINTER_FLAG_SECONDBUTTON` 可用，但手头没有这类笔可验，列为将来。
- 笔压、倾角、旋转各平台都上报（`QTabletEvent`），但本方案明确不用（§六）。

### 7.7 其他平台分支的落地方式与核对清单

按第 8 条决定，macOS/Linux 的判据与垫片实现同期写入，但**没有任何真机验证**，因此带三条约束：

- 代码里在推断得来的分支上写明依据（哪个文件哪一行）并注明未验证，便于首次上真机时快速定位。
- 文档（本方案与设计文档）**不宣称支持这些平台**，只写"已按各平台后端写法接入，待验证"。
- 首次在目标平台真机上跑时按下面四个问题逐条核对，很可能需要微调——最可能出问题的是 X11 侧键的**叠加**语义（`buttons` 同时含 `Left` 与侧键）与反端依赖的 Wacom 工具 id 列表：

1. 接受 `QTabletEvent` 后，该平台是否真的不再给出派生的鼠标事件（7.4 的条件分支是否成立）？
2. 侧键在接触期上报成哪个/哪些 Qt 按键，是否与 `Left` 叠加（7.2）？
3. 反端是否稳定给出 `PointerType::Eraser`（7.3）？
4. 悬停期侧键与反端能否从 `QTabletEvent` 直接读到（能则 7.5 的第二份实现成立）？

## 八、文档同步

本方案落地后需要更新 `docs/design/touch-and-pen-input-design.md`：

- 第一节输入分流表里"触控笔 | 不做特殊处理"这一行。
- 第二节分层结构（多出钢笔层三件套）。
- 第九节已知限制（与笔语义相关的条目；笔压与倾角仍属未利用，保留）。
- 第六节探针部分：补上 `penraw` 行与新的按键说明。
- **开头那句"实现本身不依赖任何 Windows API"需要加一句例外**：触摸部分仍然成立，钢笔层的悬停期状态读取是 Windows 专属垫片（步骤 7），非 Windows 平台退化为没有悬停光标反馈，笔画语义不受影响。
