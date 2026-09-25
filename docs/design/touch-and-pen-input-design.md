# 触摸与触控笔输入设计

本文记录主编辑区（钢琴卷帘、参数编辑器、轨道编排区）的多点触控与触控笔支持。目标平台是 Windows 触摸屏二合一设备。

> **状态**：两层均已实施，第一轮真机验证完成（2026-09-25）。**待优化项见第十一节**——其中"菜单开着时按侧键只关菜单、不擦除"是已知且已被接受的行为，不是回归。
>
> **为什么两层写在一篇里**：触摸与笔共用同一套分层（纯逻辑状态机 + 控制器 + 视图接口），也共用同一个根因——平台会把输入**提升成第二条流**（触摸是合成鼠标，笔是右键 + `WM_CONTEXTMENU`）。分两篇写会把同一件事描述两遍，也会让"谁负责吞掉什么"变得难查。
>
> **证据来源**：全部 Windows 论断来自真机探测（Surface Pen + Windows 平板上的 `TouchProbe`）并逐条对照过 Qt 6.11.2 源码，不是推测；macOS/Linux 的结论**只来自源码推断，未在真机验证**，见第六节。
>
> **平台无关性**：触摸部分不依赖任何 Windows API，在任何有触摸屏的平台上同样生效。**唯一的例外是钢笔层的悬停期状态读取**——Qt 在悬停期丢弃笔杆侧键与反端标志，只能靠自己读系统消息，因此那一小块是 Windows 专属垫片（5.8），非 Windows 平台退化为没有悬停光标反馈，笔画语义不受影响。

## 一、输入分流

四类输入按设备自动分流，没有模式开关。

| 输入设备 | 处理路径 |
| --- | --- |
| 触摸（手指） | `EditorTouchController` 手势层，见第三节。平台自行合成的鼠标事件会被吞掉 |
| 触控笔笔尖 | 不做特殊处理。控件不接受 `QTabletEvent`，由 Qt 合成鼠标事件，走既有鼠标逻辑（与改动前完全一致，双击、悬停提示、光标形状都在 Qt 手里） |
| 触控笔反端 / 笔杆侧键 | `EditorPenController` 钢笔层：反端与"侧键+拖动"合成为带擦除意图的左键笔画，侧键原地点击合成为右键点击（上下文菜单），不适合擦除的工具整段吞掉，平台给侧键补的那份右键菜单也一并吞掉，见第五节 |
| 精密触控板、滚轮 | Windows DirectManipulation（QWDMH），行为不变 |
| 鼠标 | 既有逻辑，完全不变 |

**触摸只吞平台合成的那一半。** Windows 会把主触点提升为传统鼠标消息，`QWindowsPointerHandler::translateMouseEvent()` 原样转发，只打上 `Qt::MouseEventSynthesizedBySystem` 标记。真机日志证实：单指拖动同时产生完整的 `QTouchEvent` 流和一份 `mouse press/move/release dev=TouchScreen source=BySystem`。不吞的后果有两层——单指编辑跑两遍，双指导航时被提升的主触点仍在驱动交互层，于是一边缩放一边拖动内容。

**笔只接管平台映射错的两个输入。** Qt 对未被接受的 tablet 事件会自己合成鼠标事件（`QGuiApplicationPrivate::processTabletEvent`），用的是笔设备、标 `Qt::MouseEventNotSynthesized`，这条路对笔尖来说与真鼠标完全等价，因此笔尖继续走 Qt。反端上报的按键与笔尖一字不差（`button=Left buttons=Left`），侧键在接触期被 Qt 替换成右键，这两者才是必须自己翻译的。一旦一个笔画被接管，就必须接管到抬笔——接受 tablet 事件会让 Qt 停止为这个笔画合成任何鼠标事件。

### DirectManipulation 作用域收缩

改动前 `MainWindow` 用 `registerWindow(window)` 注册，该重载等价于 `DeviceType::All`。DirectManipulation 会在窗口级吞掉全部指针原生消息，包括单指触摸和笔，Qt 因此永远收不到 `QTouchEvent`。

现在统一走 `MainWindow.cpp` 里的 `registerScopedDirectManipulation()`，设备掩码收缩为 `Touchpad | Wheel`，手势配置保持与原重载一致（`TranslationX | TranslationY | Scaling | TranslationInertia | ScalingInertia`）。触控板的平滑滚动和捏合缩放不受影响，触摸和笔则重新可见。

设置项文案同步改为触控板语义，见第十节。

### 必须吞掉平台合成的鼠标事件

接受 `QTouchEvent` 只能挡住 Qt 自己的触摸转鼠标合成，挡不住操作系统的。因此 `EditorTouchController::handleEvent()` 除了触摸事件，还要拦截鼠标事件并吞掉 `source() != Qt::MouseEventNotSynthesized` 的那些。判据成立是因为三类需要放行的事件都是 `NotSynthesized`：

- 手势层自己发的合成事件（`QMouseEvent` 构造时不传 source，默认 `NotSynthesized`）
- 真实鼠标
- 触控笔（`QGuiApplicationPrivate::processTabletEvent()` 从未被接受的 tablet 事件合成鼠标时显式用 `Qt::MouseEventNotSynthesized`，平台自己的笔转鼠标消息则被 `translateMouseEvent()` 无条件丢弃）

只在开启多点触控手势的编辑器控件内吞。应用其余部分照旧收系统合成的鼠标事件，因此按钮、菜单、轨道列表的触摸操作和长按右键都不受影响。

Qt 平台插件另有 `-platform windows:nomousefromtouch` 可以全局关掉系统合成，没有采用：它会连带干掉整个应用的触摸长按转右键，代价比收益大。

被吞掉的只有鼠标事件。Windows 长按转右键补的那一份 `QContextMenuEvent` 要放行，它正是长按菜单的来源，见第三节。

## 二、分层结构

触摸三件套与钢笔层四件套都在 `src/app/UI/Views/Common/` 下。

```
EditorTouchGesture      纯逻辑手势状态机，不依赖控件，可单测
EditorTouchController   把手势翻译成合成鼠标事件或视口操作，绑定到具体控件
EditorTouchTarget       编辑器视图需要向手势层提供的接口
EditorPointerUtils      "这是手指还是鼠标"的共享判定，兼两类指针流的全局标记

EditorPenStroke         纯逻辑单笔画状态跟踪（笔只有单点，不是手势识别器），可单测
EditorPenController     接受 tablet 事件、跟踪笔画、合成鼠标事件，绑定到具体控件
EditorPenTarget         视图向钢笔层回答"当前工具允许橡皮做什么"，并给出两张策略表
EditorPenHoverWatcher   悬停期状态垫片：接口一套，实现两份（Windows 读原生 WM_POINTER，
                        其他平台从悬停的 QTabletEvent 取）
EditorSystemGestureSuppressor
                        系统按压手势垫片：接口一套，实现两份（Windows 应答原生
                        WM_TABLET_QUERYSYSTEMGESTURESTATUS，其他平台空实现）
```

`EditorTouchTarget` 与 `EditorPenTarget` 由同一批视图实现，因此两套后端各只有一处策略表：

| 实现者 | 说明 |
| --- | --- |
| `TimeGraphicsView` | Legacy 后端的公共实现。触摸的导航部分对全部子类通用；钢笔层在基类回答"不响应" |
| `PianoRollGraphicsView` | 覆盖触摸的命中三态判定与空白拖动策略；覆盖笔的工具策略表 |
| `TracksGraphicsView` | 覆盖空白拖动策略与中止逻辑，命中三态沿用基类 |
| `ParamEditorGraphicsView` | 触摸永远工具驱动；笔的工具策略表 |
| `PianoRollRhiWidget` | RHI 钢琴卷帘，两类都覆盖 |
| `TracksRhiWidget` | RHI 轨道编排区，两类都是"不响应" |

接入点一一对应，共四处：

| 位置 | 文件 | 说明 |
| --- | --- | --- |
| Legacy 全部视图 | `TimeGraphicsView.cpp:141`（构造）、`:392`（`viewportEvent()`） | 两个控制器都装在这里，因此钢琴卷帘、参数编辑器、轨道编排区一次覆盖 |
| RHI 钢琴卷帘 | `PianoRollRhiWidget.cpp:3064` | RHI 侧逐个控件构造 |
| RHI 轨道编排区 | `TracksRhiWidget.cpp:162` | 同上 |
| RHI 基类入口 | `EditorRhiWidget.cpp:722`（`event()`） | 与触摸一致，在子类 `event()` 里分发 |

### 触摸：手势状态机

`EditorTouchGesture` 只接收触点 id、控件坐标和毫秒时间戳，返回一串意图。它识别：

- 点按与双击
- 超出 tap slop 的拖动
- 超时未移动的长按
- 双指导航（平移加两轴捏合）

关键的时序约定：**导航更新必须每个触摸事件 flush 一次**。`moved()` 在双指阶段只记录位置，真正的平移量和缩放系数由 `flushNavigation()` 产出。如果按触点逐个计算，两指同向平移时先动的那根手指会让跨距先缩后涨，表现为一次可见的缩放抖动。

### 触摸：合成鼠标事件

单指编辑不改动任何既有交互状态机，而是翻译成 `QMouseEvent` 发给控件。事件携带原始触摸设备指针，因此代码可以通过 `EditorPointer::isTouchDevice()` 反查来源。

`QGraphicsView` 的鼠标事件从 viewport 进入，所以 Legacy 后端的合成事件发给 `viewport()`，RHI 后端发给控件自身。钢笔层的合成事件目标与此完全一致。

## 三、触摸手势定义

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
| 长按空白不动 | 上下文菜单 | 上下文菜单 | 工具驱动 |
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

### 状态机不能有死角

双指导航有两处会彻底卡死，表现都是手指还在屏幕上但视图纹丝不动，要整只手抬起来重来。

**一、多余的手指落下之后，原来那两根抬起一根。** 第三根手指（休息的拇指、手掌边缘）不打断进行中的手势，但它会进入触点表。等原来的一对里有一根抬起时，导航结束，剩下两根手指让状态落到 `Settling`，而 `Settling` 只认新的按下，不认移动。于是屏幕上明明有两根手指，却什么都不会发生。

改法是导航结束时如果还剩至少两根手指，就地把手势交接给它们，重新 `beginNavigation()`。交接的那一次 `NavigationEnd` 不带速度，免得交接顺手甩出一段惯性。剩一根手指时照旧落到 `Settling`，单根手指本来就不是导航。

**二、手势中途丢状态，手指却没离开。** `TouchCancel`、指针捕获转移、送给别的控件的按下，都会让状态机清空触点表。此后这些手指报上来的是 `Updated` 而不是 `Pressed`，而 `moved()` 只认识自己见过的 id，于是这几根手指到抬起为止一直是死的。

改法是**收养**：控件收到任何一个不是 `Released` 而状态机又不认识的触点时，先按一次 `pressed(..., adopted = true)` 把它接管过来，再照常处理。`adopted` 标记只影响两件事——不触发长按，不参与双击——因为这根手指已经在屏幕上待了多久无从得知。双指的场景里，收养第二根手指会直接触发 `NavigationBegin`，导航在下一个事件就恢复了。

这两条都有单测（`liftingOneOfThreeFingersHandsNavigationToTheRest`、`anUnknownFingerIsAdoptedInsteadOfIgnored`）。

**平移抬手带惯性**。惯性只作用于平移，不作用于缩放。速度用指数平滑估计，衰减是帧率无关的 `exp(-decay * dt)`，低于阈值即停止。单指平移空白（轨道编排区）同样带惯性。

### 长按菜单由应用自己拥有

长按落在对象上时，手势层把这根手指标记为已消费，不让它拖动脚下的对象，同时记下"欠一份菜单"，等这根手指离开屏幕再弹。

**为什么要自己弹。** Windows 原本有一套自己的按压手势：按住出半透明方块，松开时补一份右键，Qt 随即合成 `QContextMenuEvent`。一度把这套留下来共用（理由是"自己弹会与系统那份打架"，而且抢过来也拿不到方块反馈），但代价是方块、约 1 秒的系统阈值、两套互不通气的判定与第二条输入通道。真机验证（11.4）确认系统侧有开关，而且只有一条有效：应答 `WM_TABLET_QUERYSYSTEMGESTURESTATUS` 返回 `TABLET_DISABLE_PRESSANDHOLD`。`EditorSystemGestureSuppressor` 就做这件事，于是长按回到应用手里：450 毫秒静止即判定（`EditorTouchGesture::Config::longPressMs`），方块与系统阈值一起消失。

**为什么在抬手时才弹。** 判定的时刻与弹出的时刻分开：判定在 450 毫秒（这时就把手指作废，它不再能拖动对象），弹出在所有手指离开屏幕的那一刻。按住途中弹出会把随后的抬手交给菜单——`QMenu` 带鼠标抓取，抬手落在菜单上就是一次选择，会误触手指底下的那一项。松手再弹也正是系统原来的时序，用户已经习惯。

**投递用 `QCoreApplication::postEvent()` 而不是 `sendEvent()`**：上下文菜单通过 `exec()` 起嵌套事件循环，在触摸事件派发过程中同步弹出会卡死手势流。

平台没有这类手势的地方（非 Windows）不需要任何兜底：判定与弹出都是我们自己的，`EditorSystemGestureSuppressor` 在那里的实现是空的。

### 没要过的平台菜单仍然必须吞掉

系统那份长按已经被关掉，这一节由"主要路径"退化成**安全网**：万一某个平台仍然把接触提升成右键（用户改过控制面板开关、换了一台机器、垫片没装上的窗口），规则照旧成立，不改。

Windows 的长按转右键只看它自己的判定，不看我们把这根手指用在了什么地方。空白长按在这里是框选，抬手时它照样补一份 `QContextMenuEvent`，于是框选完成的同时弹出菜单。

更糟的是它还会连带卡住框选矩形。菜单和触摸抬起的消息顺序没有保证，菜单先到时 `contextMenuEvent()` 里的 `exec()` 会起嵌套事件循环，弹窗抓走指针，随后的 `TouchEnd` 被投递给弹窗而不是编辑器控件。手势层收不到抬起，状态停在 `Single`，半透明的框选矩形留在场景里，要等下一次触摸被 `syncActivePoints()` 校准掉才消失。

规则因此收紧为：**触摸拥有上下文菜单期间，只有我们自己要过的那一份放行。**

- `m_contextMenuExpected` 在**我们自己投递**菜单的那一刻置位（`raiseContextMenu()`），被真正送达的菜单事件清掉。
- `touchOwnsContextMenu()` 判定这一份菜单能不能归因到手指：手势进行中，或者距最后一个触摸事件不足 600 毫秒。真实鼠标右键和笔的侧键不在这个窗口里，照旧放行。
- 归因到手指但没要过的，一律 `accept()` 吞掉。

放行的那一份如果撞上还在进行的合成流，先把流取消再放行，免得嵌套事件循环把释放事件吃掉。

> 笔那条流是同一个根因的另一个化身，但它的判据完全不同（侧键、`reason()`、时间戳），见 5.5 与 5.6。那一条**不是**本节的产物，与按压手势无关，垫片不影响它。

### 空白长按：不动是菜单，动了是框选

吞掉平台菜单会顺带吃掉空白处的上下文菜单（粘贴、新建轨道等），所以空白长按要能同时通向两种结果。

做法是**延后合成按下**。长按落在"平移领地"的空白上（`touchBlankDragAction()` 为 `Pan`）时，`SingleBegin` 不立刻发合成按下，只记住位置：

- 手指移动超过 `longPressSlopPx` 才补发按下，并且按在最初按住的位置上，框选矩形的锚点因此不会偏移到越过阈值的那一点。
- 手指一直没动就抬起，说明这是一次按住不放，从头到尾没有按下过，改为要一份上下文菜单。选中状态不受影响，不会被一个零尺寸的框选清空。

显式工具占用空白时（画音符等）不延后，按下照旧立即发出。

## 四、配套修复

触摸引入后有两处旧代码的隐含假设不再成立。

**`QCursor::pos()` 不跟手指走。** 边缘自动滚动的定时器回调原本每帧读光标位置。现在三个视图各自记录 `lastPointerPosition`（在 `mousePressEvent` / `mouseMoveEvent` / `mouseReleaseEvent` 里更新，因此合成事件自动覆盖），定时器改读它。

**`QGuiApplication::mouseButtons()` 在触摸拖动时恒为 `NoButton`。** 边缘自动滚动用它做"按键已松开"的安全网，触摸下会立刻自我解除。改用 `EditorPointer::isPointerPressed()`，它同时看真实鼠标键和活跃的合成触摸流。

**命中容差**。手指的命中精度远低于鼠标，`EditorPointer::resizeTolerance()` 在触摸流活跃期间把 `AppGlobal::resizeTolerance` 放大到 2.5 倍。两套后端的音符和剪辑边缘判定都改用了它。

## 五、触控笔

笔在编辑器里原本的身份就是鼠标：控件不接受 `QTabletEvent`，由 Qt 合成鼠标事件走既有逻辑。这带来两个说不通的行为：

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
| 笔尖长按（无侧键） | 什么都不做；系统的圆环动画目前无法消除，见 5.1 第 5 条 |

覆盖范围与触摸一致（见第一节分流表与第二节接入点），Legacy 与 RHI 两套后端。**平台范围：Windows 先落地（只有它有真机数据），macOS/Linux 的代码分支同期写入但标注为未验证、不宣称支持**，见第六节。

### 5.1 真机实测基线

#### 平台上报（Surface Pen，设备名 `wmpointer`）

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
| 笔尖长按 3.4 / 3.5 / 6.9 秒 | 系统圆环动画出现，但**不产生任何 `QContextMenuEvent`**，tablet 流不中断（圆环与触摸方块是同一个系统开关，见 11.4） |
| 触摸长按 1.05–1.15 秒 | 系统补合成 `mouse press button=Right` + 真 `QContextMenuEvent`（既有行为，要保留） |
| 侧键点击/抬起 | 原生层额外产生一整套 `RBUTTONDOWN`→`RBUTTONUP`→`CONTEXTMENU`，与 tablet 事件无关，见 5.5 |

#### 与实现直接相关的五条结论

**1. 反端只能靠 `pointerType()` 分辨，不能靠按键。** 反端 press 报的就是 `button=Left buttons=Left`，与笔尖一字不差。判据只能取 `QTabletEvent::pointerType() == QPointingDevice::PointerType::Eraser`（该接口继承自 `QPointerEvent`，等价于 `pointingDevice()->pointerType()`）。

**2. 侧键按下时 `buttons` 里没有左键，不能用它判断笔尖是否落下。** Qt 的判据写死在 `qwindowspointerhandler.cpp`：

```cpp
if (pointerInContact && penInfo->penFlags & PEN_FLAG_BARREL)
    mouseButtons = Qt::RightButton; // Either left or right, not both
```

侧键按住期间 `pressure` 仍然大于 0，所以**接触状态一律用 `pressure` 判断**，`buttons` 只用来读侧键。

**3. 不能用 `TabletPress`/`TabletRelease` 当笔画起止。** 上面的实测显示侧键中途切换会额外吐出 press/release，其中 `press button=Left buttons=Right` 出现 9 次、`release button=Left buttons=Left` 出现 11 次（后者自相矛盾：报"松开左键"而 `buttons` 仍带左键）。笔画边界必须由 `pressure` 的零与非零给出，press/release 只能当提示。

**4. 接受 `QTabletEvent` 就能独占该笔画的鼠标流。** 实测 A/B 双向可复现：`event->ignore()` 时笔的每个笔画都带一份派生鼠标事件，`event->accept()` 后该笔画**零 mouse 事件**，取而代之是密集 tablet 事件。机制在 Qt 源码里是闭合的——`qwindowsintegration.cpp:211` 把 `platformSynthesizesMouse` 设为 `false`，于是 `qguiapplication.cpp:3033` 在**未被接受**的 tablet 事件上合成鼠标事件，用笔设备、标 `Qt::MouseEventNotSynthesized`。这与触摸合成分工明确：触摸合成是 `source=BySystem` + `dev=TouchScreen`，笔合成是 `source=NotSynthesized` + `dev=` 笔设备。`EditorTouchController` 现在"只吞 `source() != NotSynthesized`"的判据因此对笔天然放行，无需改动。

**5. 笔的长按与触摸的长按是同一个系统开关。** 笔尖按住 6.9 秒都不产生 `QContextMenuEvent`，只有视觉圆环（`DefWindowProc` 的 legacy 按压手势）。当初据此写成"笔的长按不处理"，理由是"Qt 侧没有任何开关"——**这句话本身对，结论已在 2026-09-25 真机探针中推翻**：开关在系统侧（应答 `WM_TABLET_QUERYSYSTEMGESTURESTATUS` 返回 `TABLET_DISABLE_PRESSANDHOLD`），一次同时关掉触摸的方块、触摸长按补的右键与菜单、以及笔的圆环，见 11.4。触摸长按转右键仍是既有行为，落地之前回归时必须确认未受影响。

#### 悬停期侧键与反端：已确认可读（需原生过滤器）

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

1. **侧键在悬停期行为与修饰键一致**，按下即 `barrel=on`、松开即 `off`，`inContact` 始终为 `off`。
2. **反端在悬停期也能提前读到**（`inverted=on inContact=off`）。这比预期的多一条能力：反端此前只能靠 `QTabletEvent` 的 `pointerType()` 分辨，而那个设备要**首次接触才注册**；现在反端还没碰到屏幕就能知道，OneNote 那种"悬停即显示橡皮光标"因此可以做。
3. 同一时刻 Qt 侧确实一片空白（上表的 1.8 秒窗口统计），所以这**不是** Qt 能不能转发的问题，必须自己读消息。

代价是这一段要依赖 Windows API。开头那个"触摸不依赖任何 Windows API"的约定不适用于这一小块，因此实现上把它隔离成一个 Windows 专属的小垫片，非 Windows 退化为空实现，钢笔层本身不出现任何 Windows 类型（见 5.8 与第六节）。

### 5.2 设计决策

#### 已拍板事项（2026-09-25）

| # | 决策项 | 结论 |
| --- | --- | --- |
| 1 | 接管范围 | **只接管接触中的事件**，悬停交回 Qt 合成鼠标 |
| 2 | 不支持擦除的工具下 | **整段吞掉，什么都不发生**（音符分割、锚点编辑、调制音高、区间选择、参数变换） |
| 3 | 笔画中途才按下侧键 | **忽略**，本笔画仍按笔尖处理（侧键落笔瞬间锁定） |
| 4 | 悬停期光标反馈 | **本期做**（反端显示橡皮光标、按住侧键显示擦除光标），为此引入悬停状态垫片，见 5.8 |
| 5 | 反端"能擦"的范围 | **两类都擦**：音符选择下擦音符，画音高/擦除音高/描摹音高下擦参数 |
| 6 | 系统长按圆环 | **接受现状，不处理**（实测对笔只有视觉动画，无菜单事件） |
| 7 | 设置开关 | **不加**（笔没有"退回 Qt 默认合成"这个有意义的退回目标） |
| 8 | 跨平台范围 | **同时写入 Linux/macOS 分支**（按源码推断），标注未验证、不宣称支持 |

第 8 条决定了下面的结构：判据与垫片都按多平台组织，Windows 是唯一有真机验证的实现。

#### 决策细则

| 项 | 决策 | 依据 |
| --- | --- | --- |
| 接管范围 | **只接管平台映射错的那两个输入**（反端、侧键）的接触期 tablet 事件，一次接管管到抬笔；笔尖与全部悬停事件保持 `ignore()`，继续由 Qt 合成鼠标事件 | 悬停提示、光标形状、边缘自动滚动的 `lastPointerPosition`、双击、`isPointerPressed()` 的安全网全部继续照旧工作。初版写的是"接管全部接触中事件"，落地时收窄了，理由见第十一节第 2 条 |
| 笔画边界 | 由 `pressure` 从 0 变正 / 变回 0 给出 | 5.1 结论 3，press/release 不可靠 |
| 反端判据 | `pointerType() == Eraser` | 5.1 结论 1 |
| 侧键判据 | **笔设备上的非左键即侧键**，**落笔瞬间锁定**，整个笔画内不再改 | 5.1 结论 2；按键值各平台不同，写死 `RightButton` 会漏掉 X11（见 6.2）；笔画中途切换会产生假事件 |
| 侧键语义 | **延后合成按下**：落笔先只记位置，位移超过阈值才补发"擦除按下"，未超阈值就抬起则改为要一份右键菜单 | 与触摸层"空白长按：不动是菜单，动了是框选"完全同构，同一套模式已在真机验证可用 |
| 擦除意图的表达 | 不切换工具、不改工具栏按钮状态；用 `EditorPointer` 的全局意图标记 + 逐工具接线 | 全局标记是 `EditorPointerUtils` 既有先例（`isTouchStreamActive` 等）；切工具会让工具栏高亮跳动，且"不响应的工具"没法表达 |
| 各工具是否响应 | 由各视图从当前工具算出，见 5.4 第 6 条的策略表 | 需求来自每类工具的实际用途，不是平台限制 |
| 悬停期侧键与反端 | 垫片按"接口 + 两份实现"：Windows 读原生 `WM_POINTER` + `GetPointerPenInfo()`，其他平台从悬停的 `QTabletEvent` 取；只读取、不消费消息 | Qt 在悬停期丢弃 `PEN_FLAG_BARREL`/`PEN_FLAG_INVERTED`（5.1 结论 2 与上一小节），Windows 上没有别的入口 |
| 平台无关性 | 原生类型只出现在垫片的 Windows 实现里，`EditorPenController`、`EditorPenTarget` 与垫片接口不出现任何原生类型 | 触摸层刻意保持平台无关，这一小块做不到，只能隔离（第六节给出各平台差异与验证清单） |
| 合成事件目标 | 与触摸层一致：Legacy 发 `viewport()`，RHI 发控件自身 | `QGraphicsView` 的鼠标事件从 viewport 进入 |
| 触摸路径 | **一行不改** | 笔与触摸的设备、`source` 标记、接受策略三者都不相交 |

### 5.3 分层与接入点

`EditorPenController` 不做手势识别（笔只有单点），也不需要 `EditorPenGesture` 之类的新状态机——它只是一层状态跟踪加事件翻译。四个件的职责与四处接入点见第二节。`EditorPenTarget` 的实现者与 `EditorTouchTarget` 相同，因此两个后端各只有一处策略表。

### 5.4 设计细则

按输入分成的八条，前七条是行为契约，第八条是它们对既有行为的约束。

#### 1. 骨架与接入

`EditorPenController` 与 `EditorPenTarget` 两个件，在四个接入点置入并接进事件链（见第二节）。控制器只做两件事：判定"这次输入要不要接管"，以及把接管后的笔画翻译成合成鼠标事件；工具语义一概不在这里。

#### 2. 悬停保持穿透

```cpp
// 只有被接管的那两个输入才进钢笔层：笔尖与悬停都交回 Qt，悬停提示、
// 光标形状与双击因此完全不受影响。判定一次，管到抬笔。
const bool claimed = m_stroke.phase() != EditorPenStroke::Phase::Idle;
if (!claimed && !EditorPenStroke::needsTranslation(sample))
    return false;   // Qt 合成，钢笔层只看不动
```

`needsTranslation(sample)` = 反端（`pointingDevice()->pointerType() == Eraser`）或笔尖以外的按键（侧键）。抬笔那一帧的 `pressure == 0` 不会被误判为悬停：边界由 `EditorPenStroke::feed()` 按 `pressure` 判定，`TabletRelease` 即使压力为 0 也按已开始的笔画处理。

#### 3. 笔画状态与侧键锁定

`TabletPress` 时记录该笔画的既定属性：`pointerType`、侧键是否按下（**笔设备上的非左键即侧键**）、起点。整段笔画内这三项不再改变：

- 中途的 press/release 杂音一律忽略（5.1 结论 3）。
- **笔画中途才按下的侧键不改变语义**（决策 3）：这一段仍是笔尖笔画，不会中途变成擦除，也不会弹菜单。
- 接触状态由 `pressure` 独立跟踪，与上面三项无关。

#### 4. 反端 → 擦除意图

`pointerType == Eraser` 时置位擦除意图，并把该笔画合成为**左键**鼠标笔画（与笔尖同一条路径），由各工具按第 6 条的策略表决定响应与否。不合成右键——右键在参数编辑器里另有含义（`CommonParamEditorView.cpp:509-510` 右键即 Erase，但音符类 handler 会直接拒绝右键：`EraseNoteHandler.cpp:18`、`SplitNoteHandler.cpp:44` 都是 `if (event->button() != Qt::LeftButton) return;`）。

#### 5. 侧键：拖动擦除 / 点击菜单

落笔时若锁定为侧键按下，**先不合成任何按下**，只记位置与时刻：

- 位移超过阈值 → 在该笔画**原始起点**补发一次左键按下并置位擦除意图，随后照常转发移动（起点不能取越过阈值的那一点，否则擦除会从偏后的位置开始）。整段不再触碰右键，因此应用自己不会弹菜单。
- 一直未超过阈值就抬起 → 确认是侧键点击，改为合成一次右键 press/release，走既有的上下文菜单路径。

阈值取与触摸长按同一个 slop 常量，避免两套手势手感不一致。

**平台自己还有一条右键流**，接受 tablet 事件挡不住它，见 5.5。

#### 6. 各工具接线（两套后端各自一张策略表）

视图侧实现 `EditorPenTarget::penEraserAction()`，从当前工具算出结果：

| 当前工具 | 橡皮行为 | 触点 |
| --- | --- | --- |
| 音符选择 `Select` | 擦音符 | `EraseNoteHandler` 的擦除路径 |
| 擦除音符 `EraseNote` | 擦音符 | 同上（本工具已是擦除） |
| 画音符 `DrawNote` | 擦音符 | 同上（初版方案漏了这一项，见第十一节第 4 条） |
| 画音高 `DrawPitch` | 擦参数 | `CommonParamEditorView::setEraseMode(true)` 的等价路径 |
| 手绘橡皮 `ErasePitch` | 擦参数 | 同上 |
| 描摹音高 `TracePitch` | 擦参数 | 同上 |
| 音符分割 `SplitNote` | **不响应** | 整段笔画吞掉 |
| 锚点编辑 `EditPitchAnchor` | **不响应** | 整段笔画吞掉 |
| 调制音高 `ModulatePitch` | **不响应** | 整段笔画吞掉 |
| 区间选择 `IntervalSelect` | **不响应** | 整段笔画吞掉 |
| 参数编辑器 `Draw`/`Erase`/`Trace` | 擦参数 | `ParamEditorGraphicsView.cpp:236-237` 的等价路径 |
| 参数编辑器 `Shape`/`Scale`/`Anchor` | **不响应** | 整段笔画吞掉 |
| 轨道编排区（两套后端） | **不响应** | 整段笔画吞掉 |

Legacy 侧接入点集中在 `PianoRollGraphicsView` 的 `applyToolPitchEditMode()`（它已经把工具翻译成 `setEraseMode`/`setTraceMode`/`setCurveTransformMode`），以及各 `PianoRollEditHandler` 子类的 `mousePressEvent`。RHI 侧在 `PianoRollRhiWidget` 的 `PitchEditType` 与 `beginPitchEdit()` 附近。

**"不响应"的实现在笔层完成（直接吞掉），不进入工具**——这样"不响应"的工具不需要各自加判断。"不响应"时的悬停表现见 5.7。

#### 7. 悬停期侧键与反端：光标反馈

5.1 末节已确认这条路径可用。这一条只驱动光标，不改任何笔画语义。

`EditorPenHoverWatcher` 分两份实现（决策 8）。Windows 那份沿用仓库里已有的写法（`EditorTouchProbe.cpp:134` 的 `NativeFilter` 是同一种东西，安装点在它旁边的 `EditorTouchProbe::install()`），要点：

- 用 `QCoreApplication::installNativeEventFilter()` 装一个进程级过滤器，**只读取，永远返回 false**——Qt 的 `QWindowsPointerHandler` 还要照常处理同一条消息（`MainWindow::nativeEvent()` 是另一个先例，但那是窗口级的，这里要的是全局状态）。
- 只处理 `WM_POINTERUPDATE/DOWN/UP`，先 `GetPointerType()` 确认 `PT_PEN` 再 `GetPointerPenInfo()`，只取 `penFlags` 的 `PEN_FLAG_BARREL` 与 `PEN_FLAG_INVERTED | PEN_FLAG_ERASER`。悬停期消息频率约 260 条/秒，所以按状态变化再做后续动作，不要在每条消息上改光标。
- 其他平台那份从悬停的 `QTabletEvent` 读 `buttons()` 与 `pointerType()`（6.5 的推断），按 6.7 标注为未验证。
- 状态与 Qt 的 `QTabletEvent` 各自独立，只需在**落入编辑器控件范围时**生效：悬停光标属于视图职责，由 `EditorPenController` 在悬停 move 之间维护，别让全局状态直接在任意控件上改光标。
- `EditorPenController` 只暴露平台无关的查询（`eraseHintFor()` / `eraseHintActive()` / `eraseHintRefused()`），原生类型不越过垫片边界。
- **只在这条状态上做两件事**：换光标、记下侧键最近按下的时刻。不要拿它的**边沿**去判断"用户开始了一个新动作"——菜单弹窗的 SetCapture 会让它来回抖，试错记录见第十一节 11.3-A。

#### 8. 接管之后必须仍然成立的既有行为

接受 tablet 事件之后，被接管的那两类笔画不再产生 Qt 合成的鼠标事件，因此以下几处必须逐项确认（它们此前都靠笔的鼠标流工作）：

- 悬停提示与光标形状（决策 1 就是为它们保留的）
- 边缘自动滚动读的 `lastPointerPosition`（第四节）
- `EditorPointer::isPointerPressed()` 的安全网
- 触摸路径：`EditorTouchController` 的吞事件判据不受影响（5.1 结论 4），但需实测确认触摸长按菜单、双指导航、手势惯性均无变化

### 5.5 侧键的第二条流：平台自己的右键

侧键在 Windows 上不只是 tablet 事件里的一个按键，它同时是**一条独立的传统鼠标流**。真机日志（`TouchProbe`）里，侧键每一次按下/抬起都会在原生消息层产生完整的一套：

```
win RBUTTONDOWN synthesized from touch
win RBUTTONUP   synthesized from touch
win CONTEXTMENU synthesized from touch
```

`QWindowsContext::handleContextMenuEvent()` 把这条 `WM_CONTEXTMENU` 原样转成 `QContextMenuEvent`，位置取 `msg.pt`，也就是**光标当前所在处**。钢笔层接受 tablet 事件挡不住它——它是另一条消息，跟 tablet 事件没有关系。于是：

- 侧键原地点击：平台的这条消息落到**我们自己的菜单弹窗**上，不会变成给编辑器的 `QContextMenuEvent`，因此看不出差别（真机日志里一次点击只出现一条 `context menu passed through`，就是我们自己发的那条）；
- 侧键拖动擦除：平台只在"点击"形态下发这套消息，拖动时没有，因此没事；
- **菜单被一次按下关掉之后**（见 5.6）：消息落到编辑器窗口上，变成一份新的菜单弹在抬笔处，说不通——这一条是判断二存在的理由。

因此 `EditorPenController` 吞掉 `reason() == QContextMenuEvent::Mouse` 的那条菜单事件（自己的菜单 `reason() == Other`，不会误伤）。判据有两条：

| 判据 | 覆盖的情形 |
| --- | --- |
| 被接管的侧键笔画已开始（`m_platformMenuPending`） | 正常路径。**必须在按下那一刻置位，不能在笔画结束时**——平台的消息是在**抬键**时产生的，结束再置位就等于在要吞的事件之后才武装（第一版就是这么写的，实测一条都没吞到） |
| 侧键刚按下过（`penBarrelActive()`，400 ms 内） | 那次按下被菜单弹窗吃掉了，本层根本没看到笔画，只剩"笔杆侧键刚按下过"这一条痕迹 |

两条都不会误伤手指长按转右键的那一份：前者要一条被接管的笔画，后者要求侧键在 400 ms 内按下过，而长按本身就要按住远超 400 ms。

### 5.6 菜单不能吃掉笔的笔画

侧键原地点击弹菜单之后，**菜单开着时再按住侧键拖动，擦不掉东西**：`QMenu` 是带鼠标抓取的弹出窗口，笔的按下被它拿去（按下本身是用来关掉菜单的），钢笔层一行事件都收不到，于是既没有擦除，也不会有任何提示；而平台那条右键流照旧在抬键时发一份，屏幕上就变成"什么都没发生，松手又在原地冒出一个菜单"。

真机日志长这样（钢笔层完全缺席，只有平台和自己两条菜单）：

```
21:42:06.420 win RBUTTONDOWN / RBUTTONUP / CONTEXTMENU     ← 平台自己的
21:42:06.436 EditorTouchController  context menu passed through
```

**现状按"真鼠标语义"接受：这一次按下属于菜单**（它把菜单关掉），因此擦除要从下一次开始；平台上那份多余菜单已由 5.5 的判据二吞掉，所以不会出现"松手又冒出一个菜单"。

**待优化**（"同一次拖动就擦掉"）：曾经试过让悬停垫片在"侧键由松变按"的边沿关菜单，真机上误关了用户刚打开的菜单（抬笔后在悬停里移动就会触发），**已回退**，机制与四步方案见第十一节 11.3-A。

### 5.7 不响应时的悬停表现

工具不响应时，笔画被整段吞掉，工具根本收不到按下，但**悬停反馈仍然会画出来**：分割工具的红叉、锚点编辑的虚线插入预览，都只由 hover/move 事件驱动，跟"这个笔画能不能做这件事"无关。结果是屏幕上画着一个不可能发生的动作。

判据与笔画一致，只是把"这个笔画要不要吞"换成"要不要显示"：

| 钢笔层状态 | 悬停表现 |
| --- | --- |
| 反端/侧键在范围内，工具**能擦** | 橡皮光标（`EditorPenController::eraseCursor()`） |
| 反端/侧键在范围内，工具**不能擦** | `Qt::ForbiddenCursor`，并撤回本工具的悬停反馈 |
| 没有反端/侧键提示 | 各视图自己的光标与提示，照旧 |

撤回走 `PianoRollEditHandler::suppressHoverFeedback()`（分割工具清掉指示器，锚点工具清掉虚线预览且保留 `cursorInView`，笔一离开就能恢复）；两套后端的悬停/移动入口都先问 `EditorPenController::eraseHintRefused(tool)`，问到了就直接返回，不再把这次移动喂给工具。参数编辑器的锚点前景同样处理——它只服务锚点工具，因此任何擦除提示都意味着拒绝。

### 5.8 悬停期状态垫片

悬停期（笔在范围内、未接触）的侧键与反端状态在 Windows 上过不了 Qt（5.1 末节）。因此 `EditorPenHoverWatcher` 分两份实现：

| 实现 | 来源 | 状态 |
| --- | --- | --- |
| Windows | 进程级原生过滤器读 `WM_POINTERUPDATE/DOWN/UP`，`GetPointerType()` 确认 `PT_PEN` 后 `GetPointerPenInfo()` 取 `penFlags`。只读取，永远返回 false，Qt 的指针处理器照常处理同一条消息 | 真机验证 |
| 其他平台 | 悬停的 `QTabletEvent` 自带的 `buttons()` 与 `pointerType()`（X11 的 `tabletData->buttons` 由 XI 按键事件累积，macOS 的 `buttonMask` 同理） | 源码推断，未验证 |

垫片是进程级单例，因为悬停期按侧键**不产生任何 Qt 事件**，只能靠原生消息主动推送（`changed()` 信号）。它只在"悬停期 + 当前工具能擦"时把编辑器的光标换成橡皮（`EditorPenController::eraseHintFor()`），笔画语义不受影响；两处后端的光标逻辑都先问这一句，避免把橡皮光标改回工具光标。悬停提示的另外两态见 5.7。

`TabletLeaveProximity` 与 `WM_POINTERLEAVE` 都会把状态整体清零，`setState()` 又只在三元组真的变化时才发信号——**"清零再填回"因此会合成一次假的"侧键刚按下"**。垫片只负责如实发布状态，判断留给使用者，别在这条状态上做边沿触发。

## 六、可移植性（其他平台与其他笔）

### 6.1 天然可移植的部分

核心设计只用到 Qt 事件 API，没有任何 Windows 概念，因此换平台不需要重新设计：

| 设计元素 | 用到的 API | 可移植性 |
| --- | --- | --- |
| 笔画边界用接触状态 | `QTabletEvent::pressure()` | ✅ |
| 反端判据 | `QTabletEvent::pointerType() == Eraser` | ✅（反端来源各平台不同，见 6.3） |
| 侧键取得 | `QTabletEvent` 的 `buttons()` | ✅（按键值不跨平台，见 6.2） |
| 独占笔画 | `event->accept()` | ✅（源码依据见 6.4） |
| 拖动/点击区分 | 自行按位移判定 | ✅ |
| 逐工具策略表 | `EditorPenTarget` | ✅ |

### 6.2 需要改写的判据：侧键的按键值不跨平台

初版在 Windows 上写的是 `buttons().testFlag(Qt::RightButton)`，这个值**不能跨平台照抄**：

| 平台 | 笔尖 | 侧键 | 来源 |
| --- | --- | --- | --- |
| Windows | `Left` | `Right`，**独占**（替换掉 Left） | 实测 + `qwindowspointerhandler.cpp` |
| X11 | `Left` | 笔杆两个键分别是 X 键 2/3，映射为 `Middle` / `Right`，且**叠加**（`buttons \|= b`） | `qxcbconnection_xi2.cpp:1497-1507`，注释写明 `the tip, plus two barrel buttons` |
| macOS | `Left` | 由 `NSEvent buttonMask` 跟踪 | `qnsview_tablet.mm:77` |

两点结论：

- 判据应写成**"笔设备上的非左键即侧键"**，而不是写死 `RightButton`。Wacom 双键笔杆在 X11 上还能分出上下两颗键（`Middle`/`Right`），Windows 上不行（6.6）。
- 这让"**接触状态必须用 `pressure` 判断**"这条从"Windows 上的坑"升级为跨平台必需：X11 的 `buttons` 可能同时含 `Left` 和侧键。

### 6.3 反端的来源各平台不同，但都能得到 `Eraser`

| 平台 | 反端如何产生 `PointerType::Eraser` |
| --- | --- |
| Windows | `PEN_FLAG_INVERTED \| PEN_FLAG_ERASER`，且是**惰性注册的第二个 `QInputDevice`**（实测） |
| X11 | Wacom 驱动上报的工具 id 列表（`Intuos* Airbrush Eraser`、`Art Pen Eraser` 等，`qxcbconnection_xi2.cpp:150-185`），并靠 `WacomSerialIDs` 上报 proximity |
| macOS | `NSPointingDeviceTypeEraser`（`qnsview_tablet.mm:165`） |

`pointerType()` 这层判据可以统一写；要留意 Windows 上反端设备首次接触才注册，所以**"反端设备存在"不能当能力探测**。

### 6.4 "接受 tablet 事件即接管鼠标流"是跨平台的

这条机制的依据是 Qt 自己的合成开关，各平台后端都会把它关掉：`windows`、`wayland`、`cocoa`、`ios`、`android`、`wasm` 无条件设 `setPlatformSynthesizesMouse(false)`，`xcb` 则在 XInput2 事件选择成功后设置（`qxcbconnection_xi2.cpp:94`，失败时不设）。所以"接受事件 → Qt 不再合成鼠标"在主流桌面平台都成立；X11 那个条件分支意味着**没有 XInput2 的异常环境**需要另测。

### 6.5 悬停期状态：接口一套，实现两份

按决策 8，垫片不是 Windows 专用类，而是"接口 + 两份实现"（见 5.8）：

| 实现 | 悬停期侧键与反端的来源 | 状态 |
| --- | --- | --- |
| Windows | 原生 `WM_POINTER` + `GetPointerPenInfo()` 的 `penFlags`（Qt 在这一层把它丢弃了） | ✅ 真机验证 |
| 其他平台 | **大概率不需要原生代码**：X11 的 `tabletData->buttons` 由 XI 按键事件累积、与接触状态无关（`qxcbconnection_xi2.cpp:1497-1507`），macOS 的 `buttonMask` 同理（`qnsview_tablet.mm:77`），悬停期的 tablet 事件本身就该带着侧键状态，直接从 `QTabletEvent` 读即可 | ⚠️ 源码推断，未验证 |

这个分工让接口很薄：`EditorPenHoverWatcher` 只需回答"悬停期侧键是否按下、是否反端"。Windows 走原生消息，其他平台走 tablet 事件，都读不到的极端情况退化为没有光标反馈，笔画语义不受影响。

### 6.6 其他笔（Wacom 等）在 Windows 上

Windows 上所有笔统一走 WM_POINTER（Windows Ink），映射一致，所以不做任何设备特判。三个边界要知道：

- **Wacom 驱动里关掉"使用 Windows Ink"之后，笔会退化成普通鼠标**（不再产生 `QTabletEvent`），钢笔层整个不介入，行为回到今天：反端在画、侧键是右键。这是本设计覆盖不到的场景，也不是编辑器能修的，只能靠文档告知用户开启 Windows Ink。
- **双键笔杆在 Windows 上被 Qt 合并成一个 barrel 标志**（`PEN_FLAG_BARREL`），分不出上下键；X11 反而能分出两颗（6.2）。若要区分，垫片读的原始 `pointerFlags` 里有 `POINTER_FLAG_SECONDBUTTON` 可用，但手头没有这类笔可验，列为将来。
- 笔压、倾角、旋转各平台都上报（`QTabletEvent`），但本设计明确不用（第七节）。

### 6.7 其他平台分支的落地方式与核对清单

按决策 8，macOS/Linux 的判据与垫片实现同期写入，但**没有任何真机验证**，因此带三条约束：

- 代码里在推断得来的分支上写明依据（哪个文件哪一行）并注明未验证，便于首次上真机时快速定位。
- 文档**不宣称支持这些平台**，只写"已按各平台后端写法接入，待验证"。
- 首次在目标平台真机上跑时按下面四个问题逐条核对，很可能需要微调——最可能出问题的是 X11 侧键的**叠加**语义（`buttons` 同时含 `Left` 与侧键）与反端依赖的 Wacom 工具 id 列表：

1. 接受 `QTabletEvent` 后，该平台是否真的不再给出派生的鼠标事件（6.4 的条件分支是否成立）？
2. 侧键在接触期上报成哪个/哪些 Qt 按键，是否与 `Left` 叠加（6.2）？
3. 反端是否稳定给出 `PointerType::Eraser`（6.3）？
4. 悬停期侧键与反端能否从 `QTabletEvent` 直接读到（能则 6.5 的第二份实现成立）？

## 七、明确不做

- **不处理笔的长按与系统圆环**。笔尖按住 6.9 秒都不产生 `QContextMenuEvent`（5.1 结论 5），功能上无需处理；圆环动画在 Qt 侧无 API 可关，不做窗口过程级拦截。
- **不引入笔压与倾角**。平台已上报 `Pressure`/`XTilt`/`YTilt`，但编辑器目前没有任何按压敏感的表达方式，不在本次范围（第十二节已记为已知限制）。
- **不动触摸任何行为**，包括触摸长按转右键。
- **不为笔加设置开关**。触摸那套手势开关是为了"关掉退回 Qt 默认合成"，笔没有等价的退回目标——反端与侧键在退回后是错的，没有用户会想关掉。

## 八、测试

两个测试目标都不需要硬件，把可判定的部分做成单测：

- `src/tests/TestTouchGestures/` 覆盖 `EditorTouchGesture` 的全部判定：分流规则、点按与双击、长按两种走向、第二指中止、双指平移不漏缩放、逐点更新不产生伪捏合、两轴锁定与锁定保持、惯性速度估计、三指抬一指的导航交接、丢状态后的触点收养。
- `src/tests/TestPenInput/` 覆盖 `EditorPenStroke` 与两张策略表：分流规则（哪类输入才接管）、笔画边界由接触状态给出（含抬笔那一帧压力为 0 不误判为悬停、以及抬笔被报成 move 的情形）、中途的侧键假 press/release 既不改语义也不结束笔画、侧键锁定后中途松键不改变本笔画、反端与"侧键+拖动"合成为带擦除意图的左键笔画、侧键 slop 两侧的走向（补发按下 vs 要菜单）、不支持擦除的工具整段吞掉且不会退化成菜单、取消只释放不弹菜单，以及两张策略表的每一条映射。

指令：`ctest -R "TestTouchGestures|TestPenInput"`，或直接跑 `build/Debug/out/bin/` 下的同名可执行文件。

悬停垫片走 Windows 原生消息，没有单测可写，只能真机验。真机验证走 `TouchProbe`（第九节）与平板上的人工手势序列，清单见 11.5。

## 九、探针

两层各有一个探针，回答两个不同的问题：**平台送来了什么**（`TouchProbe`，独立窗口）与**我们怎么处理的**（应用内，开发者开关）。

### 9.1 TouchProbe（平台侧）

`src/tools/TouchProbe/` 是一个独立诊断窗口，桌面应用本身回答不了这三个问题：

1. 触摸是以 `QTouchEvent` 到达 Qt，还是只有合成鼠标
2. 触控笔产生 `QTabletEvent`、鼠标事件，还是两份都有
3. DirectManipulation 注册后究竟吞掉了什么

轨迹按设备着色，鼠标蓝、触摸绿、笔红、滚轮与原生手势黄。触摸的每个触点 id 取绿色系里的一个固定色调，彼此可区分又不会被误认成别的设备。**合成鼠标画成灰色虚线**，意外的合成一眼可见。

轨迹按笔画分段：触点抬起、鼠标或笔松开都会结束当前笔画，另外超过 300 毫秒没有新点也会断开（悬停、滚轮和原生手势没有明确的结束事件）。否则抬手后在别处再落下会被一条直线连起来。

HUD 显示最近事件与当前触点数，全量日志写到 `AppDataLocation/touch-probe.log`。

按键：`C` 清屏，`D` 切换 DirectManipulation（用与应用完全相同的 `Touchpad | Wheel` 配置），`S` 切换吞掉合成鼠标（用与编辑器完全相同的判据），`A` 切换接受 tablet 事件（笔的接管开关，与 `EditorPenController` 的接受策略同源），`G` 切换 `SetGestureConfig`（11.4 里被证伪的那条路），`Q` 切换按压手势查询的应答（11.4 里被证实的那条路），`F` 全屏，`Esc` 退出。开关 `S` 可以直接看出吞与不吞的差别，被吞的事件在日志里标 `SWALLOWED`，轨迹上不再出现灰色虚线；开关 `A` 是它的笔版本——接受时该笔画不再出现派生鼠标事件，取而代之是密集的 tablet 事件。

探针也记录 `QContextMenuEvent` 及其 reason，用来确认长按是否引发了平台的右键模拟。

#### raw 行与两个系统手势开关

11.4 的长按实验靠三样东西：两个独立开关、一条带臂标记的原始消息流、以及每次接触结束的时长。

`G` 对探针窗口的 HWND 调 `SetGestureConfig(hwnd, 0, 1, {dwID=0, dwWant=0, dwBlock=GC_ALLGESTURES}, sizeof)`，`Q` 让原生过滤器应答 `WM_TABLET_QUERYSYSTEMGESTURESTATUS` 返回 `TABLET_DISABLE_PRESSANDHOLD`（不应答时留给 `DefWindowProc`）。两个开关可以任意组合，四组手臂各自对应 11.4 表格里的一行。

每条原始消息都带当时的开关状态，所以日志自己说明它属于哪一组实验，开关中途被翻转也不会读错：

```
[G=on Q=off] raw      WM_TABLET_QUERYSYSTEMGESTURESTATUS -> 0, left to DefWindowProc
[G=on Q=off] raw      WM_RBUTTONDOWN extra=0xff515789 from=touch lastPointer=PT_TOUCH
[G=on Q=off] api      SetGestureConfig(hwnd=0x130130, dwBlock=GC_ALLGESTURES) touchWindow=1 touchFlags=0x0 -> TRUE
hold     contact lasted 1169 ms
```

- `raw` 行覆盖 `WM_POINTERDOWN/UP`（带指针类型，也就是 Qt 会缓存到它自己 `m_pointerType` 里的那个值）、`WM_GESTURE`、按压手势查询、以及 `WM_LBUTTON*/WM_RBUTTON*/WM_CONTEXTMENU`，后三类附带 `GetMessageExtraInfo()` 的签名来源（`touch` / `pen` / `none`）。
- `api` 行记 `SetGestureConfig` 的返回值、`GetLastError`、`IsTouchWindow` 与 `touchFlags`，随后读一次 `GetGestureConfig`。把"调用被接受"和"调用没起作用"分开。
- `hold` 行在每次触摸结束时给出这次接触的时长，用来看住按是否够到系统约 1 秒的阈值。

产物按部署目标选配置：本地调试用 `build/Debug/out/bin/`，上平板要走设备同一套发行版 Qt（`build/PortableDmlRelease/out/bin/`，RelWithDebInfo），再把 exe 覆盖到设备 `C:/Data/Lite/bin/TouchProbe.exe` 并用 `editor-session.ps1 -Action Start -EditorExe TouchProbe.exe` 在交互会话里启动，日志在设备 `%APPDATA%\TouchProbe\touch-probe.log`。

#### penraw 行

探针里另装了一个原生过滤器（`PenRawStateFilter`），在 Qt 之前读 `WM_POINTERUPDATE/DOWN/UP`，对 `PT_PEN` 指针调 `GetPointerPenInfo()`，把 Qt 拿不到的悬停期状态直接打出来：

```
penraw   WM_POINTERUPDATE barrel=on inverted=off inContact=off pos=(1813,1206)
```

只报状态变化（悬停消息约 260 条/秒，逐条打会把日志冲垮），首次观察必定报一次，因此笔尖悬停就能证明过滤器活着。这一行是 5.8 那个垫片的唯一验证手段：`barrel=on inContact=off`（悬停期按住侧键）与 `inverted=on inContact=off`（反端悬停）都无法从任何 `QTabletEvent`、`QMouseEvent` 或 `QInputDevice` 上读到。

构建目标 `TouchProbe`，产物在 `build/Debug/out/bin/`。

### 9.2 应用内探针

开发者设置页的 **Log touch events** 打开后，两个控制器都会写日志；行首 tag 取自源文件名，所以日志窗口按 `EditorTouchController` / `EditorPenController` 过滤即可。开关默认关闭，写的是 Debug 级别。

#### EditorTouchController

每个触摸事件一行：

```
touch update [0:hold(412,233) 1:move(688,240)] nav->nav tracked=2 out=[NavUpdate]
touch update [0:move(400,233) 1:move(700,240)] nav->settling tracked=2 out=[NavEnd,NavBegin,NavUpdate]
```

字段依次是事件类型、每个触点的 id 与状态与坐标、手势阶段的前后变化、状态机当前跟踪的触点数、这一轮吐出的意图。触点被收养时行尾会多一段 `adopted=[1]`。另外几种不在触摸事件里发生、但足以解释卡死的时刻也各占一行：`TouchCancel`、被吞掉或被放行的上下文菜单、被吞掉的合成鼠标按下与抬起、控件主动 `cancel()`。

#### EditorPenController

回答每个笔画走了哪条路：

```
pen tip press Pen pressure=0.17 -> Qt                  笔尖交回 Qt
pen sidebutton press buttons=2 pressure=0.48 -> taken over   侧键被接管
pen eraser press buttons=1 pressure=0.46 -> swallowed        反端被吞（工具不响应）
pen Begin at (467,143) erase                            派生笔画开始
pen Move at (479,143) erase                             派生笔画移动
pen End at (888,140) erase                              派生笔画结束
pen ContextMenu at (638,82)                             原地点击要菜单
pen ignored mid-stroke release (buttons=2)              笔画中途的 press/release 噪声
pen swallowed platform context menu at (712,190)        吞掉平台给侧键补的那份菜单
```

三处读法要留意：

- `ignored mid-stroke` 的 `release` 是**印错的标签**：这一支同时覆盖"笔画中途的假 press/release"和"侧键还没越过 slop 的移动"（两者都不产出意图），而标签只看 `TabletPress` 之外一律写 `release`。因此侧键刚落下时那一串 `ignored … (buttons=2)` 实际是**移动**，越不越过 slop 要看后面有没有 `pen Begin`。
- `pen Move` 是逐点写的，一笔下来能有几百行，排查时按 `Begin/End/ContextMenu/press` 过滤。
- **Idle 阶段的 tablet move/release 一行都不打**（`phase() == Idle` 时既不产出意图也不进噪声分支）。所以"日志里钢笔层一行都没有"本身就是"事件根本没到我们这里"的证据，而不是"到了但没处理"。

## 十、设置项

外观设置页的 Touch 卡片：

| 选项 | 键名 | 默认 | 说明 |
| --- | --- | --- | --- |
| 多点触控手势 | `enableTouchGestures` | 开 | 关闭后控件不接受 `QTouchEvent`，退回 Qt 默认的触摸转鼠标合成。同时不再应答系统的按压手势查询，长按转右键与方块一并交回平台（见 11.4） |
| 精密触控板与滚轮滚动 | `enableDirectManipulation` | 开 | 仅 Windows 构建可见，只影响触控板和滚轮 |

两项都热生效，不需要重启。笔没有开关（决策 7）。

## 十一、真机验证记录与待优化项

### 11.1 与初版方案不一致的六处取舍（已实施）

落地时有六处原文没有覆盖或自相矛盾的地方做了取舍，逐条记录理由与真机状态。

**1. 多了一个纯逻辑件 `EditorPenStroke`。** 状态跟踪与"不依赖硬件的单测"合起来，只能落在一个不依赖控件的类上——与 `EditorTouchGesture` 从 `EditorTouchController` 拆出来同一套路。它不是手势识别器（笔只有单点），只做笔画状态跟踪，并把"报告 → 意图"的翻译表放在那里可测。因此分层从三件套变成四件。

**2. 笔尖不接管，接管范围比初版收窄了。** 初版写的是"只接管接触中的 tablet 事件"，落地改成"只接管平台映射错的那两个输入"。理由是 Qt 那条合成路还负责**双击**：`QGuiApplicationPrivate::processMouseEvent()` 只在自己派发的 press 上判双击并产生 `MouseButtonDblClick`，自己合成会把它丢掉，而钢琴卷帘的双击进歌词编辑、双击空白建音符都依赖它；顺带 `isPointerPressed()` 的安全网、hover 提示、光标形状也全都不需要重做。代价是钢笔层不再统一处理接触期事件。**真机确认项：笔尖的绘制、拖动、双击、悬停提示与改动前逐项一致。**（另外仍保留了"接管即接管到抬笔"的规则，以及 `EditorPointer::isPenStreamActive()`，因为被接管的那两类笔画确实不再经过 Qt 的鼠标合成。）

**3. 笔尖笔画中途的侧键噪声也吞掉了。** 5.1 结论 3 的假 press/release 在"那个笔画没被接管"时依旧会进 Qt（笔尖就是这种情形），于是控制器加了一个观察位：看到笔尖接触中，就把中途的 press/release 吞掉、move 照旧放行。这是决策 3「忽略」的字面实现，也是初版没写的一小步。**真机确认项：笔尖拖动过程中按/松侧键，拖动手感不变、不弹菜单、不出现多余按下。**

**4. `DrawNote` 补进策略表。** 初版的工具表列了 9 个，漏了画音符。落地取"擦音符"：与 `Select`/`EraseNote` 同域，橡皮在这一层的含义唯一。**真机确认项：画音符模式下翻笔擦音符；若不希望如此，改一行即可。**

**5. 侧键原地点击不受工具限制。** 决策 2（"不支持擦除的工具下整段吞掉"）与侧键语义、以及目标表里无条件的"侧键原地点击 → 右键菜单"互相冲突。落地取**菜单与工具无关**：要菜单不是擦除请求，只有擦除受策略表约束；一旦越过 slop 就成了擦除尝试，此时不支持的工具整段吞掉、不回头补菜单。这样改之前侧键在锚点编辑等模式下的菜单不会丢。**真机确认项：锚点编辑/音符分割等模式下，侧键原地点击仍然弹菜单。**

**6. 悬停橡皮光标只在"当前工具能擦"时出现。** 决策 4 只说"反端显示橡皮光标"，没说工具条件；落地加了这个条件，因为光标不该承诺一个笔画做不到的事。**真机确认项：可擦工具下反端悬停与按住侧键悬停都是橡皮光标，不可擦工具下是禁止光标。**

### 11.2 第一轮真机验证后补齐的两处（已实施）

第一轮分件验证发现两处问题，都在同一个方向上：**钢笔层管住了 tablet 那条流，没管住与之并行的第二条流（平台自己的鼠标/菜单）和悬停期的显示**。

**一、平台自己会给侧键发一套右键 + `WM_CONTEXTMENU`。** 真机日志里侧键每次按下/抬起都会在原生消息层产生 `RBUTTONDOWN`→`RBUTTONUP`→`CONTEXTMENU`，`QWindowsContext::handleContextMenuEvent()` 把它转成 `QContextMenuEvent`，位置是**光标当前所在处**。修正见 5.5：按两条判据吞掉 `reason() == Mouse` 的那条菜单事件。

第一版把标记设在**笔画结束**，实测一条都没吞到：平台的 `WM_CONTEXTMENU` 是在**抬键**那一刻产生的，而抬键比最后一次 tablet 事件还早几毫秒——日志里平台的三连在 `21:42:03.701`、我们自己的菜单在 `.707`，标记是在 `.707` 之后才置位的，恰好在要吞的事件之后。第二版改成**在按下那一刻置位**（平台的消息只会更晚，不会更早）。第二条判据来自 11.3-A：那次按下可能被菜单弹窗吃掉，本层根本没看到笔画，只剩"侧键刚按下过"这一条痕迹（`m_barrelDownMs` 由悬停垫片或 tablet 采样记录）。

**二、不响应的工具仍在画悬停反馈。** 分割工具的红叉、锚点编辑的虚线插入预览都只由 hover/move 驱动，与"这个笔画能不能做"无关，于是屏幕上画着一个不可能发生的动作。修正见 5.7：悬停判据从"工具能不能擦"扩成三态，不能擦时撤回本工具的悬停反馈。

### 11.3 待优化项（触控笔）

#### A. 菜单开着时按侧键只关菜单、不擦除（**现状如此，已知且已被接受**）

现象：侧键点击弹出菜单之后，不关菜单，直接在菜单外按住侧键拖动 —— 不擦除，松手时又弹出一份菜单。

机制（真机日志已证实）：`QMenu` 是**带鼠标抓取的弹出窗口**，笔的按下被它拿去关自己了，因此那一次交互**钢笔层一行事件都收不到**（`21:42:06` 只有平台的右键三连 + 一条 `context menu passed through`）——既没有擦除，也不会有任何提示。它属于"用户用一次按下关掉了菜单"的正常菜单语义，与真鼠标在菜单外按一下一致；平台上额外那份菜单已由 5.5 的判据二吞掉，所以现在是"只关菜单，不再冒第二份菜单"。

曾经试过、**已回退**的做法，以及它为什么不能这么做：让悬停垫片在"侧键由松变按"这个边沿把当前 `QMenu` 关掉。它在真机上误关了用户刚打开的菜单——抬笔后只要在悬停里移动就会触发。原因是这个状态量在悬停期不成立：菜单打开时 `QMenu` 会 SetCapture，捕获切换让平台发出 `WM_POINTERLEAVE` → 垫片 `clear()` 归零 → 下一条 `WM_POINTERUPDATE` 又把 `sideButton` 填回 → `setState()` 的"值变了才发信号"把这趟来回合成一个假的"侧键刚按下"。**教训：不要用一个持续状态量的瞬时变化去判断"用户开始了一个新动作"，这个量会被捕获切换和进出范围搅动。**

要彻底解决，"这一次按下"必须活下来。四步，第 0 步先做：

0. 给垫片加 `inContact`（Windows：`POINTER_PEN_INFO.pointerFlags & POINTER_FLAG_INCONTACT`，配 `WM_POINTERDOWN/UP` 定界），并把**每次状态变化**与**每次"准备关菜单"的判定理由**各写一行探针。不改行为，只让下一轮的判断有证据。
1. 换触发条件：只在"**新接触开始**"时关菜单，并要求此前连续 ≥120 ms 不是接触中、笔在范围内已 ≥60 ms（抖动是毫秒级，人的动作不是）。只对侧键与反端生效——笔尖接触无需插手，菜单在外部按下时自己关掉本来就是正常菜单行为。
2. 让这一次按下活下来：在原生过滤器里**同步**解除弹窗的鼠标捕获（`menu->releaseMouse()`，只是平台侧 `ReleaseCapture()`，**不会像 `close()` 那样展开 `exec()` 的嵌套循环**，因此在过滤器里是安全的），随后用排队的 `close()` 关掉菜单。捕获一解除，Qt 处理同一条 pointer 消息时就会把 tablet 按下交给编辑器。**这一步需要真机确认**：Windows 可能在更早阶段就定好了消息的目标窗口。
3. 第 2 步不成立时的兜底：允许钢笔层接受一个它没看到按下的笔画——垫片已经报告"笔刚接触"，就用它的位置与状态起一笔，后续 tablet 移动接着喂（菜单关掉之后消息确实会重新落到编辑器上，日志已证）。代价是反转"按下必须来自平台"这个依赖，改动面比第 2 步大。

退路：维持现状。它与真鼠标的行为一致，只是不满足"同一次拖动就擦掉"这条期望。

#### B. 反端受同一个机制影响

菜单开着时用反端划一下，同样擦不掉（原因同 A：按下被弹窗吃掉）。第 1 步的设计里已经把它带上——反端的悬停期"在范围内"是常态，所以**必须**用接触而不是"在范围内"触发，否则就是 A 里那个误关的翻版。

#### C. 轨道编排区没有禁止光标

`EditorPenPolicy::arrangement()` 恒为"不响应"，但 `TracksRhiWidget` 仍显示它自己的悬停光标。是否改成"橡皮在这里什么都不能做"的禁止光标，等钢琴卷帘的效果定下来再一起拍。

#### D. 非 Windows 分支未验证

垫片的第二份实现、侧键的按键值、反端的 `Eraser` 判定都只来自源码推断，见第六节，首次上机时按 6.7 的四个问题逐条核对。

### 11.4 系统的长按转右键：已真机验证（2026-09-25 22:48–22:58）

#### 曾经的现状

长按菜单一度**交给平台**（见第三节）：手势层的长按只把手指标记为已消费、不让它拖动脚下的对象，真正的菜单来自 Windows 在**抬起时**补的那份右键合成。当初的理由是"共存比抢过来便宜"——自己弹菜单会与系统那份打架（按住半途弹出、松手又被系统补的右键关掉），而且抢过来也拿不到按住时的方块反馈。**这套已经在 2026-09-25 拆掉，应用现在自己拥有长按**，下面是"能不能抢过来"的真机答案。

#### 查证到的三个事实（当初的依据）

| 事实 | 出处 |
| --- | --- |
| Qt 的 windows 插件**完全不处理 `WM_GESTURE`**（只在消息名表里出现过）；触摸提升出来的鼠标事件按"上一条指针消息的类型"标注来源 | `qwindowspointerhandler.cpp:819-835` |
| 应用**可以**关掉它：`SetGestureConfig(hwnd, 0, 1, &{0, 0, GC_ALLGESTURES}, sizeof)`（Win7+），或应答 `WM_TABLET_QUERYSYSTEMGESTURESTATUS` 返回 `TABLET_DISABLE_PRESSANDHOLD` | Raymond Chen《How do I disable the press-and-hold gesture for my window?》 |
| **MFC 默认就返回 `TABLET_DISABLE_PRESSANDHOLD`**，官方理由是长按判定会给左键引入延迟、应用显得不跟手 | 同上 |

也就是说"关掉系统长按、应用自己拥有长按"是微软给 Win32 应用准备的**默认姿势**，不是 UWP 独有。代价——方块、约 1 秒的系统阈值、两套互不通气的判定、第二条输入通道——都是在没关掉系统机制的前提下才由我们承担的，关掉之后全部消失。

#### 真机结论：两条路里只有一条有效

| 机制 | 传说是怎么关的 | 真机结果 |
| --- | --- | --- |
| legacy 手势栈 | `SetGestureConfig(hwnd, 0, 1, {dwID=0, dwWant=0, dwBlock=GC_ALLGESTURES}, sizeof)` | **无效**。API 返回 `TRUE`、窗口确实是 touch window，长按照样补右键与菜单，全日志 `WM_GESTURE` 零条 |
| 按压手势 | 应答 `WM_TABLET_QUERYSYSTEMGESTURESTATUS` 返回 `TABLET_DISABLE_PRESSANDHOLD` | **有效**。触摸按住时的方块、触摸长按补的右键与菜单、笔尖长按的圆环，三样同时消失 |

对照数据（`TouchProbe` 本轮新增的 `G` / `Q` 两个开关，每条日志行都带 `[G=… Q=…]` 臂标记，人工只要说"有没有方块"）：

| 臂 | 按住时长 | 平台补的右键 | Qt 菜单事件 | 方块/圆环（人工） |
| --- | --- | --- | --- | --- |
| 基线 | 1172 / 914 ms | `WM_RBUTTONDOWN`+`UP` 两次都有 | `reason=Mouse` 两次都有 | 有 |
| 只开 G | 1030 / 1169 ms | 仍有 | 仍有 | 有 |
| 只开 Q | 2504 / 1243 ms | 一次都没有 | 一次都没有 | 没有 |
| G + Q | 1858 / 1266 ms | 一次都没有 | 一次都没有 | 没有 |
| 笔尖，Q 开 | 2623 ms | 没有，只有 `WM_LBUTTONDOWN` | 没有 | 无圆环 |
| 笔尖，Q 关 | 1423 / 1034 ms | 第二次有 `WM_RBUTTONDOWN`+`WM_CONTEXTMENU` | 探针控件没收到 | 有圆环 |

**为什么 G 那条没用**：按压手势根本不在 legacy 手势栈上，`WM_GESTURE` 在这套 WM_POINTER 窗口上从未出现过（Qt 认了这个消息类型然后什么都不做，`qwindowscontext.cpp` 里 `case QtWindows::GestureEvent: break`）。栈既没启用，关不关它都一样。

**为什么 Q 那条有用**：`WM_TABLET_QUERYSYSTEMGESTURESTATUS`（`tpcshrd.h`，`WM_TABLET_DEFBASE + 12`）在每次 `WM_POINTERDOWN` 之后 0~4 ms 到达，**每次接触都问一遍**，触摸与笔都问（本轮 21 次以上，`PT_TOUCH` 与 `PT_PEN` 都有）。应答之后长按退化成普通左键流：`WM_LBUTTONDOWN` 在按下那一刻就来，一直按到松手。**一条判据同时覆盖触摸与笔**，不需要两套。

**能答到它的层**（Qt 6.11.2 源码核对）：`QWindowsContext::windowsProc()` 里这条消息不算 input message，会先走 `filterNativeEvent(&msg, result)`（应用级原生过滤器链），之后 `filterNativeEvent(platformWindow->window(), ...)` 才到 `QWidgetWindow::nativeEvent` → `QWidget::nativeEvent`。两条路都够得着，探针走的是应用级过滤器，与 `EditorPenHoverWatcher`、`EditorTouchProbe` 同一层，因此落地不需要再去子类化或挂钩窗口过程。

#### 两处机制更正

**一、触摸长按不产生 `WM_CONTEXTMENU`。** 全日志零条。`translateMouseEvent()` 对鼠标消息返回 `true`（`qwindowspointerhandler.cpp:890`），`DefWindowProc` 因此拿不到 `WM_RBUTTONUP`，那份 `QContextMenuEvent` 是 **Qt 自己合成的**（`QWindowPrivate::maybeSynthesizeContextMenuEvent`，`qwindow.cpp:2825`，由右键 release 触发）。所以触摸与笔是两条来源，第三节与 5.5 原来写成一条：

- 触摸长按：平台补右键 → Qt 处理鼠标消息并返回 true → Qt 合成 `QContextMenuEvent(reason=Mouse)`；
- 笔：平台补右键 → 因为 `m_pointerType == PT_PEN`，Qt 在 `qwindowspointerhandler.cpp:828-833` 特意 `return false` → `DefWindowProc` 生成 `WM_CONTEXTMENU` → `QWindowsContext::handleContextMenuEvent()` 转成 `QContextMenuEvent`。

两条最终都在控件收到之前变成 `QContextMenuEvent`，所以第三节那条吞掉判据对两条都成立，不用改。附带记一笔：`windowsEventType()` 给 `WM_CONTEXTMENU` 的标记里**没有** `MouseEventFlag`，`windowsProc` 于是用 `GetCursorPos()` 填 `msg.pt`，`handleContextMenuEvent()` 拿它判断是否落在客户区内，出界就 `return false` 交回 `DefWindowProc`——本轮笔尖那次就是这样（`raw WM_CONTEXTMENU` 到了，控件没收到菜单事件）。

**二、顺带看到的左键延迟。** 基线里"按压手势"会把提升出来的左键压后：触摸短按 26~41 ms（只开 Q 时 1~6 ms），笔的一次接触 474 ms（只开 Q 时 2 ms）。样本很少，只当旁证，但方向与 MFC 那条官方理由一致。

#### 落地做法

分两步，第一步是垫片本身，第二步才拆代管。

**第一步（已实施）**：`EditorSystemGestureSuppressor` 是一个无状态的进程级垫片，形状照 `EditorPenHoverWatcher`：接口平台无关，Windows 实现是一份 `QAbstractNativeEventFilter`，看到 `WM_TABLET_QUERYSYSTEMGESTURESTATUS` 就写回 `TABLET_DISABLE_PRESSANDHOLD` 并返回 true；其他平台是空实现。`EditorTouchController` 构造时把宿主控件交给它（`addWindow()`），过滤器在首次调用时装上，永不卸载。一次覆盖触摸与笔，因为两边问的是同一条消息。

两处范围收窄，都是有意为之：

- **只应答被认领的窗口**。查询送到触点所在的那个窗口，垫片用 `GetAncestor(hwnd, GA_ROOT)` 取根窗口再与已认领控件当时的 `window()` 比对，因此编辑器窗口自己的原生子窗口也覆盖得到，而对话框、服务窗不在其中。理由是这个应答同时也会拿走"长按弹右键菜单"这条**在别处唯一的**长按入口（文本框里的复制粘贴就靠它），所以它只该覆盖触摸层真正接管的那些窗口。"认领"记的是控件而不是 HWND，`window()` 每次查询时再解析，把面板拖出成浮动窗口也跟着走。
- **只在手势层开着时应答**。`enableTouchGestures` 是热开关，关掉之后没有任何东西接管长按，所以这时把长按整个交回平台（连方块一起还回去）。读选项放在每次查询里，因此不需要跟着开关装卸过滤器。

装上之后的现状：方块与系统那份右键消失，长按菜单暂时仍由第三步要拆掉的 400 毫秒兜底定时器投递（`reason() == Other`），因此菜单会晚 400 毫秒出现，这是过渡状态，不是终态。

**第一步真机验证（2026-09-25 23:09–23:14，平板 + `touch-debug.dspx`）**：人工六项全过（音符长按、空白长按、空白拖动框选、双指平移缩放、笔尖长按无圆环、笔侧键原地点击出菜单）。日志侧的硬证据是把设备上历次运行的日志横向比出来的：

| 运行 | `navBegin` | 导航中的 `touch cancel` | `WM_POINTERCAPTURECHANGED` | 平台补的触摸右键 |
| --- | --- | --- | --- | --- |
| 09-25 20:59（改前） | 43 | 25 | 27 | 30 |
| 09-25 21:49（改前） | 5 | 3 | 4 | 19 |
| 09-25 22:07（改前） | 5 | 3 | 4 | 5 |
| **09-25 23:09（装垫片）** | 9 | 7 | 8 | **0** |

`平台补的触摸右键` 统计的是 `EditorTouchProbe` 记的 `win RBUTTONDOWN synthesized from touch`，改前每次有触摸交互的运行都有 5~30 条，装垫片这次是 0 条。那一次菜单仍由 400 毫秒兜底定时器投递，实测六次"触摸结束 → 菜单放行"的间隔正好是 398 / 405 / 407 / 409 / 411 / 412 毫秒，与 `contextMenuFallbackMs` 吻合，也印证了这一步只换了开关、没换时序。日志副本与逐项统计在 `build/_pencheck/`（`editorlog.log`、`editor-log-survey.txt`）。

**第二步（已实施，真机待验）**：

1. **拆掉"交给平台"那段代管**：`EditorTouchController` 里的 `m_contextMenuFallbackTimer`、`contextMenuFallbackMs`、`armContextMenuFallback()` 与 `cancelContextMenuFallback()` 全部删除，换成 `raiseContextMenu()` 与 `dropPendingContextMenu()`。第三节整节重写成"长按菜单由应用自己拥有"。
2. **长按走自己的机器**：450 毫秒静止 → `confirmLongPress(false)` 把手指标记为已消费并记下"欠一份菜单"，随后**在最后一根手指离开屏幕的那一刻**弹出（`m_menuPending` + `raiseContextMenu()`，内容与空白两条路都走这里）。空白那条的"延后合成按下"不变：动了是框选，没动才是菜单。判定阈值是 `EditorTouchGesture::Config::longPressMs`，可调。
3. **吞掉平台菜单的判据作为安全网留着**（`m_contextMenuExpected` + `touchOwnsContextMenu()`），挡住的是"垫片没覆盖到的窗口"与笔侧键那条流。
4. **过渡态的 400 毫秒延迟随第一步的定时器一起消失**，菜单现在与系统原先的时序一致：抬手即出。
5. **笔那边没有动**：`EditorPenController::m_platformMenuPending` 的两条吞菜单判据只针对侧键那条传统右键流，与按压手势无关（11.3-A 另行处理）。

**第二步真机验证（2026-09-25 23:20–23:22，同机同工程）**：人工六项全过，其中"抬手即出菜单"由日志量化——五次长按的"触摸结束 → 菜单放行"间隔是 **0 / 1 / 1 / 1 / 3 毫秒**（第一步那轮是 398~412 毫秒），过渡态的 400 毫秒延迟确实随定时器一起消失。整个运行里 `win RBUTTONDOWN synthesized from touch` **零条**，编辑器内的每一次接触都走到控制器（16 次接触里 5 次落在菜单弹窗上、控制器看不到，其余 11 次都有 `touch begin`），说明手势层全程在线。笔侧键那条流照旧：`pen sidebutton press -> taken over`、`pen ContextMenu`、以及 `pen swallowed platform context menu` 都还在。日志副本 `build/_pencheck/editorlog-step2.log`。

**尚未验到的一项**：第 4 条那个范围收窄（把"多点触控手势"关掉后长按应回到系统方块与系统菜单）本轮没有留下证据，日志里所有接触都被控制器处理了，也就是手势层全程是开的。要补的话在设置里关掉开关再长按一次即可。

**第二步待真机确认**：① 空白长按后拖动仍是框选、且不弹菜单（本轮已过）② 双指导航、惯性、点选/框选不受影响（本轮已过）③ 笔尖长按无圆环且笔画照旧（本轮已过）④ **"多点触控手势"关掉后长按应回到系统方块与系统菜单**（本轮未验到）。

探针本轮加的两个开关与原始消息行已经写进 9.1，复现实验照那一节做即可。

#### 未来规划：阈值即弹与长按的视觉反馈（尚未实施）

现在长按只有"450 毫秒把手指标记为已消费、抬手出菜单"这一条路径，按住期间屏幕上什么都不会发生。窗口的 XAML 界面（任务栏等）不这样：它在阈值处**立刻**弹出菜单，而且抬手不收。两者要分开评估。

**视觉反馈（建议先做这一半）。** 判定仍在 450 毫秒，但那一刻给出一个可见信号，菜单仍在抬手时弹出。候选形式：

| 形式 | 做法 | 代价 |
| --- | --- | --- |
| 被长按对象进入按压态 | 视图在长按判定时把命中的对象（音符、剪辑）画成按下或"菜单将作用于它"的样子，抬手时恢复并出菜单 | 每套后端各一处绘制，命中判定已经有了（`EditorTouchTarget::touchHitsContent()`） |
| 触点位置的圆环或涟漪 | 在触点位置画一个短暂的圆环，位置与系统原来的方块一致 | 一处绘制，与命中什么无关，最接近被替掉的那份系统反馈 |
| 悬停式菜单预览 | 提前高亮"菜单将要作用于的对象" | 与第一种相近，语义更明确 |

共同点是不碰 popup 的输入路径，回归面只在绘制。

**阈值即弹（任务栏同款，成本高）。** 只把 `raiseContextMenu()` 从"抬手时"挪到"判定时"是**不行的**，Qt 的 popup 模型会立刻与那根还按着的手指打架：

1. 触摸事件先被转发给活动 popup（`QWindowPrivate::forwardToPopup`，`qwindow.cpp:2495`），popup 不接手才继续往下走。
2. 编辑器窗口这时会忽略触摸（`QWidgetWindow::handleTouchEvent` 里 `if (QApplication::activePopupWidget()) event->ignore()`，`qwidgetwindow.cpp:710`），Qt 于是合成鼠标事件（`qguiapplication.cpp:3303-3352`，`source = MouseEventSynthesizedByQt`）。**`EditorTouchController` 从这一刻起收不到那根手指的任何事件**，它已经聋了。
3. 这些合成鼠标事件，以及 Windows 那条 `BySystem` 的提升流，在活动 popup 存在时被整体改投给 popup（`qwidgetwindow.cpp:515-580`），绕过我们"只吞外来合成鼠标"的判据。
4. `QMenu` 收到之后有两种结局：press 落在菜单内而 `hasMouseMoved()` 为假时直接 `hideUpToMenuBar()`（`qmenu.cpp:2913-2932`），屏幕上是"菜单闪一下"；release 落在 `currentAction` 上且已过漂移阈值时 `activateAction()`（`qmenu.cpp:2943-2989`），**手指底下那一项被误触发**。`hasMouseMoved()` 的判据是"距弹出位置超过 `startDragDistance`，或收到过 6 次以上移动"（`qmenu.cpp:1579`），手指按住时触摸更新每秒上百条，后者转眼就成立。

所以要做到任务栏那样，必须让菜单在那根手指抬起之前**完全看不见它**：一个输入守卫，在投递菜单时武装，吞掉 `MouseEventSynthesizedByQt` 与 `MouseEventSynthesizedBySystem` 两条流里属于该触点的事件（合成事件带 `eventPointId`，`qwindowsysteminterface_p.h:240`，可以用来对上触点 id），吞掉它的 release 之后解除。菜单还要向上偏移一点再弹，否则第一项被手指盖住。守卫在武装期间是全局的，而右键菜单是本应用最常用的交互，回归面覆盖每一类菜单，且只能真机验证。**先做视觉反馈，这一条留到有明确需求时再单独开一轮**，动手之前先加临时探针确认到底是哪条流在实际触发。

#### 与它无关、但同样表现为"双指卡住"的四个候选

**必须先分类再动手**：四个候选的成因不同，其中三个与长按无关。

| 候选 | 机制 | 判别方法（都是现成的日志行） |
| --- | --- | --- |
| A. 假鼠标穿透 | `translateMouseEvent()` 里那个 `switch (m_pointerType)` **没有 `default:` 分支**，而 `m_pointerType` 是粘的（保留上一条指针消息的类型）。既不是 TOUCH 也不是 PEN 时，提升出来的鼠标事件会以 `NotSynthesized` 身份进入应用，恰好绕过我们"只吞 `BySystem`"的判据 | 卡住瞬间看 `swallowed synthesized mouse press/release` 有没有断档，同时看 `QGuiApplication::mouseButtons()` 是否非空 |
| B. 导航中途被判长按 | 系统的识别器不认我们的"这是导航"，只看单根触点的位移；有一根手指按住不动超过它的阈值，就会在导航中途补一份右键 + `WM_CONTEXTMENU` | `context menu swallowed (phase Navigation, …)` 是否出现 |
| C. 掌拒逻辑吃掉触点 | Qt 用 `RegisterTouchWindow` 注册窗口，触摸类型默认是 `NormalTouch`（不是 `WantPalmTouch`），Windows 的掌拒逻辑可能把掌触点合并进主触点或直接丢掉 | 卡住时对比日志里的 `tracked=N` 与实际手指数 |
| D. 提升出的左键触发 `SetCapture` | 平台把某个触点提升成左键按下，Qt 在窗口过程里照旧 `SetCapture`（那段发生在事件到达控件之前，我们"吞掉合成鼠标"拦不住），捕获变更随即变成 `WM_POINTERCAPTURECHANGED`，Qt 于是取消整个触摸序列。手法与长按无关，但表现一样：手指还在屏幕上，状态机已经归零 | `win WM_POINTERCAPTURECHANGED` 紧跟 `win LBUTTONUP/DOWN synthesized from touch`，再紧跟 `qt touch CANCEL` 与 `touch cancel (phase was nav, tracked=2)` |

一个便宜的对照实验：控制面板 → 笔和触控里的"按住以右键单击"（`HKCU\Software\Microsoft\Wisp\Touch\TouchMode_hold`，触控与笔分开）与"触摸时显示视觉反馈"是两个独立开关，分别关掉可以直接看出方块与右键各自的归属。

本轮探针与首次编辑器验证给这几个候选各留了一条观察：

- **候选 A 未复现**：每一次被提升的鼠标消息都带 `extra=0xff5157xx from=touch`，且 `lastPointer=PT_TOUCH`，Qt 会把它标成 `BySystem`，我们现有判据接得住。它的前提（`translateMouseEvent()` 那个 `switch` 没有 `default:`，粘住的 `m_pointerType` 若是 `PT_MOUSE` 就会让事件以 `NotSynthesized` 身份进入）仍然成立，但触发它需要在两条指针消息之间夹一条非 TOUCH/PEN 的消息，两次真机序列里都没有出现。
- **候选 C 的前提被证实**：探针窗口 `IsTouchWindow=1` 但 `touchFlags=0x0`，即 Qt 用 `RegisterTouchWindow(hwnd, 0)` 注册，既没有 `TWF_FINETOUCH` 也没有 `TWF_WANTPALM`，掌拒逻辑确实按 `NormalTouch` 对待这些窗口。
- **候选 B 没有独立线索**：只做了单指长按与正常双指导航，没有做"一根手指按住不动 + 另一根划"的场景。
- **候选 D 是眼下最强的一个**。设备上历次日志里，导航中的 `touch cancel` 与 `WM_POINTERCAPTURECHANGED` 一直跟着 `LBUTTONDOWN synthesized from touch` 出现（见 11.4 的横向对比表，改前 25/27、3/4、3/4），实测序列是 `WM_POINTERCAPTURECHANGED` → `LBUTTONUP` → `LBUTTONDOWN` → `qt touch CANCEL` → `touch cancel (phase was nav, tracked=2)` → 紧接着一份新的 `qt touch begin [0:down 1:down]`。也就是说每次有新的触点被提升成左键，导航就被取消一次，然后靠新的 `TouchBegin` 重建。多数时候用户只觉得"顿一下"，但只要重建时触点集合不完整，状态机就会停在错误的相位上，这正是"手指还在、视图不动"的样子。
  它有一个现成的候选开关：`-platform windows:nomousefromtouch`（`QT_QPA_PLATFORM`，必须在构造 `QApplication` 之前设）会让 `translateMouseEvent()` 在触摸合成的鼠标事件上**提前返回**（`qwindowspointerhandler.cpp:822`），于是 `handleCaptureRelease()` 不再执行、不会 `SetCapture`、也就没有 `WM_POINTERCAPTURECHANGED`。5.1 结论 5 当初否掉它的理由是"会连带干掉触摸长按菜单"——**这个理由在垫片落地之后不成立了**，因为触摸长按菜单已经不再依赖平台那份右键合成。要确认的是：关掉这条流之后，触摸的短按/双击（我们自己合成）与"手势层关掉时退回 Qt 合成"两条路都不受影响。**尚未实施，也未在真机验证。**

**下一个工作项就是它**：先只加环境变量、不改代码，用设备日志看导航中的 `touch cancel` 与 `WM_POINTERCAPTURECHANGED` 是否消失，再决定要不要把这条设置固化进产品。

### 11.5 测试与回归入口

- 单测见第八节。
- **第一轮真机已确认**（日志可查）：反端擦音符与擦参数都产生完整的 `Begin…End erase`；侧键拖动擦除；侧键原地点击弹菜单；笔尖绘制/拖动/双击走 Qt 原路（`pen tip press … -> Qt`）；分割与锚点编辑下反端整段吞掉（`-> swallowed`）。
- **尚未在真机逐项确认**：11.1 各条末尾的"真机确认项"、5.7 的禁止光标与悬停反馈撤回。
- **2026-09-25 探针验证与落地（触摸与笔的系统长按）**：见 11.4。结论是"按压手势查询有效、`SetGestureConfig` 无效"，**垫片与拆代管都已实施并真机验证**（`EditorSystemGestureSuppressor` + `EditorTouchController` 的 450 毫秒自有判定，菜单抬手即出）。唯一未验到的是"关掉手势开关后长按回到系统菜单"这条范围收窄。

## 十二、已知限制

- 歌词内联编辑依赖 Windows 触摸键盘，本次不介入。
- 触控笔笔尖在编辑器里仍是"平台合成鼠标"的老路径，压感与倾角没有被利用（反端与侧键走钢笔层，会自己合成鼠标事件）。
- 侧键原地点击在"不支持擦除"的工具下仍然要一份上下文菜单：要菜单与这个工具能不能擦无关，只有擦除受策略表约束。若实测发现这与工具语义冲突，需要重新拍板。
- 悬停期的橡皮光标在"当前工具不能擦"时不出现，改成禁止光标，并且该工具的悬停反馈（分割红叉、锚点虚线）同时撤回。光标与提示都不该承诺一个笔画做不到的事。
- 轨道编排区（`EditorPenPolicy::arrangement()` 恒为不响应）目前**没有**接禁止光标，见 11.3-C。
- 侧键那条平台右键流只在"点击"形态下出现（拖动态实测没有）；如果将来遇到平台的右键在拖动中途就冒出来的机型，`penBarrelActive()` 那条判据仍然覆盖得到，不需要改。
- **菜单开着时按侧键只关菜单、不擦除**，反端同理，见 11.3-A。这是被接受的现状，不是回归。
- 悬停垫片的非 Windows 实现来自源码推断，未在 X11/macOS 真机上验证，见 11.3-D。
- 笔尖的长按圆环与触摸的长按方块是同一个系统开关，垫片只在"多点触控手势"开着、且窗口属于编辑器时才应答。**只用手写笔、从不开手势层的用户**（或对话框里的长按）仍然会看到系统圆环与系统菜单，见 11.4。这是有意的范围收窄，不是遗漏。
- `PhonemeView`、标尺、钢琴键盘没有开启 `WA_AcceptTouchEvents`，走 Qt 默认的触摸转鼠标合成。这些视图的悬停提示在触摸下不会出现，属于无 hover 的正常降级。
- DirectManipulation 的设备类型收缩改变了触控板路径的注册参数，触控板用户需要回归确认平滑滚动与捏合仍然正常。
- 双指以上（三指及更多）不识别，多余的手指会被忽略直到全部抬起。
- 触摸长按菜单现在由应用自己拥有（判定 450 毫秒、抬手时弹出），系统那套方块与约 1 秒阈值已关闭，成因、真机验证与落地做法见 11.4。
