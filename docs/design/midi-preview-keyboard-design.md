# MIDI 设置页试听键盘设计

## 一、背景与目标

「选项 → MIDI」页里的合成器参数（生成器、音量、包络、A 频率）原本没有任何可弹奏的入口，没有 MIDI 键盘的用户要试听某个音高，只能关掉对话框回到歌声剪辑里用左侧键盘弹。目标是在该页底部放一条横向钢琴键盘，就地按键试听，听到的必须是当前界面上的值。

页面上原先的「预览」与「中断所有音符」两个按钮随键盘一起移除了：两者本来只是"放一段固定试听乐句"和"硬切合成器"，键盘提供真实弹奏后不再需要入口。

约束与取舍：

- 音域取 C2–C6（MIDI 36–84），共 49 键、29 个白键。用户确认五组足够，不铺满 88 键。
- 白键固定 20px，整条 580×64px，与真实键盘比例接近，不随窗口拉伸。
- 试听必须跟手：改了生成器立刻按一个键就要听到新音色，不能等关掉对话框。
- 至少支持单指触控。多点触控本期不做，理由见第六节。

## 二、组件结构

原有左栏竖排键盘 `PianoKeyboardView` 与新键盘共用一套输入与状态机，几何与绘制各写各的，因为两者根本不是同一个布局：

| 文件 | 职责 |
| --- | --- |
| `UI/Controls/PianoKeyboardBase.h/.cpp` | 共用核心：三个 QSS 颜色属性、键位范围、悬停/按下状态、鼠标按下/滑奏/释放/移出/隐藏、滚轮转发策略、触摸时钉住祖先 scroller |
| `UI/Views/ClipEditor/PianoRoll/PianoKeyboardView.h/.cpp` | 竖排键列，几何跟随钢琴卷帘的小数键位范围，绘制带圆角的键带和八度标签，音符投给 `MidiSystem` |
| `UI/Controls/HorizontalPianoKeyboardView.h/.cpp` | 横向试听键盘，C2–C6 真实钢琴布局，音符通过 `noteOn/noteOff` 信号交给持有者 |
| `UI/Controls/PianoPaintUtils.h` | `isWhiteKey` / `noteName`，两侧共用（原先在 PianoRoll 目录下） |

基类把「音符往哪去」留给子类，竖排键盘继续直接写 `MidiSystem` 的 integrator，横向键盘只发信号。这样控件本身不依赖音频模块，页面决定路由。

基类还定义了一个 `keyRangeChanged()` 钩子。横向键盘在范围变化时重建白键表与「白键边界 → 黑键」表，并 `setFixedSize` 出自然尺寸。

滚轮策略：基类用 `isSignalConnected()` 判断有没有人接 `wheelScroll`。钢琴卷帘接了，就转发并 accept。试听键盘没人接，事件 ignore 后上浮给外层滚动区，页面滚动不受影响。

顺带清掉了 `PianoKeyboardView` 里到达不了的 `Uniform` 样式（`m_style` 私有且无 setter，恒为 `Classic`）以及从未被调用的 `drawHoverOverlay`。

## 三、键盘几何

- 白键 `i` 占 `[i*20, (i+1)*20)`，整高 64。
- 黑键居中在相邻白键的边界上，宽 12（0.6 白键），长 40。
- 命中测试先按 `x/20` 定白键下标。若 `y < 40`，再判该白键左右两个边界上的黑键是否落在 ±6px 内，否则算白键。
- C 键在底部画 `C2`…`C6`。20px 的白键放不下 13px 的界面字体，所以标签字体会从界面字号开始逐像素缩小，直到 `单音名宽度 + 4 <= 20`，下限 8px。仍放不下时不画（见已知限制）。
- 悬停 alpha 128、按下 alpha 200。黑键横跨相邻两个白键，所以绘制顺序与竖排键盘一致：白键 → 白键高亮 → 黑键 → 标签 → 黑键高亮。白键高亮若画在黑键之上，会把压在它上面的半个黑键染蓝。
- 黑键矩形向上越界一个圆角半径再画，圆角只落在琴键前端，顶部两个角被控件上边缘裁掉，和竖排键盘的越界画法同一个套路。
- 键床最后描一圈 1px 外框（用分隔线颜色，不新增 token）。浅色主题下白键与卡片底色太接近，没有外框时左右两条边读不出来。

## 四、试听音频链

不能复用 `MidiSystem` 的合成器：页面上的改动要到关闭对话框时才经 `modifyOption()` 写进 `AudioSettings`，而 `MidiSystem` 的合成器只读已保存的值。改了生成器再按键会听到旧音色，正好毁掉这个功能的主要用途。

也不能复用 `SettingPageSynthHelper::m_testSynthesizer`：`MidiNoteSynthesizer` 的构造函数会 `setDetector()` 接管被包装合成器的检测器，而 helper 自己就是那个检测器（固定试听乐句仍保留在 helper 里，只是页面不再有入口），共用会把对方顶掉。

于是 helper 里加了一条独立的实时试听链：

```
MidiMessageIntegrator m_previewMidiIntegrator  ->  MidiNoteSynthesizer m_previewMidiSynthesizer
                                                              （自带 NoteSynthesizer）
```

- `initialize()` 里 `setStream(&m_previewMidiSynthesizer, false)` 后把 integrator 作为源加进 `m_testMixer`。这样它继承 `m_testMixer` 的音量增益与 premixer 挂接，试听音与页面控件调好的振幅走同一条增益与输出路径。
- `noteOn(key)` 先把 `m_testSynthesizer.config()` 整份拷给实时合成器，再补齐 `frequencyOfA`（A 频率为 0 表示跟随工程 cent shift，此时按 440 处理，与试听乐句的取法一致），然后投一条 `MidiMessage::noteOn`，力度 0.8，与左栏竖排键盘一致。
- 之所以在按下时同步而不是给每个控件加一路同步：`NoteSynthesizer::config()` 已经由页面的控件（含采样率变化回调）维护好，拷一份就够，控件那边一行都不用改。
- `flushPreviewNotes()` 清空队列并硬切正在响的音，供频率与 A 频率切换调用，避免按住的音留在旧调上。

声明顺序上 `m_previewMidiSynthesizer` 在前、integrator 在后：integrator 持的是非拥有指针，析构时先死的是它。

## 五、输入与触控

### 单指

沿用第十三节给 Fader/PanSlider 定的形态，不认领触摸流：

- 键区不设 `WA_AcceptTouchEvents`，Qt 把未接受的触摸合成成鼠标，既有的鼠标逻辑原样工作，点按与滑奏都能用。
- 按下时调 `TouchClaimFilter::stopAncestorScrollers(this)`，把祖先链上所有 `QScroller` 钉回 Inactive。否则手指按住琴键拖动时，选项页会同时跟着滚。真实鼠标不驱动 TouchGesture scroller，这个调用对鼠标只是顺手停掉滑行。
- 触摸被系统抢走（`TouchCancel`）时未认领，Qt 会补一条合成 release，状态自然复位。

### 外部 MIDI 输入

接着一个真实 MIDI 键盘弹奏时，页面上的同一条键盘会把按下的键点亮，这样不用抬头看硬件也能确认映射。

- `MidiInputNoteMonitor`（`Modules/Audio/utils/`）作为过滤器挂到 `MidiSystem` 的 integrator 上。设备 listener 把消息转给 integrator，integrator 再转发给自己的过滤器，所以监听器拿到的是**外部设备**的消息。页面自己的琴键走的是页面私有的 `m_previewMidiIntegrator`，不会绕回来重复点亮。
- 设备的回调跑在 MIDI 输入线程。监听器把 note on/off 转成信号，连接用默认的 `AutoConnection`：跨线程时排队投到 GUI 线程，同线程时直接调用。控件只在自己的线程改状态。
- note on 先判、note off 后判：力度 0 的 note on 是 running status 形式的 note off，两者都会命中 `isNoteOff()`。
- 收到 all-notes-off / all-sound-off 控制消息，或设备停止（`processDeviceStopped`）时发 `allNotesReleased()`，页面清空所有高亮，避免设备没发 note off 时按键一直亮着。
- 高亮画在与「按下」同一档（alpha 200），优先级：鼠标按下 > 外部按住 > 悬停。绘制顺序与按下高亮一致——白键那层在黑键之下，黑键那层在最后，白键高亮不会染到压在它上面的半个黑键。
- 监听器在 `MidiPage` 构造时创建并挂接一次，内容 widget（含键盘）在切换语言时重建，连接以键盘为上下文，随旧键盘一起失效。设备停止或没有设备时不会收到任何消息。

### 滚轮

键盘本身的滚轮事件在没有接收者时放行给外层滚动区。卡片内的横向滚动区只在窗口窄到放不下 580px 时才有范围，那时才出现横向滚动条并把卡片撑高一个条高。竖向滚轮在键盘上按常规滚动页面，不做「滚轮当横向滚动」的映射，避免鼠标刚好停在这块时页面滚不动。

### 横向滚动

默认窗口（900×600）下卡片内宽约 672px，键盘 580px 放得下，且有 `AlignHCenter` 居中。只有把窗口拉到最窄（对话框最小宽度约 776px，卡片内宽约 548px）时才会出现滚动条，作为兜底。滚动区用自己的 `padding: 0px` 规则抵消全局 `QScrollArea` 的 6px 内边距。

## 六、明确不做：多点触控

- Qt 只有一根合成鼠标光标。键区不 accept 触摸时，两指也只产出一条合成鼠标流，最多单音。
- 要真正多指，必须 accept 原始触摸流并按 touch point 维护各自的按键状态。但 `touch-and-pen-input-design.md` 第十三节已记录：认领（`TouchClaimFilter`）在纯拖动型控件上的真机投递不可靠，Fader/PanSlider 已经把认领拆掉。为一个试听键盘再造一层触摸层，风险与收益不成比例。
- 音频侧倒是现成的：`MidiMessageIntegrator` 是消息队列，多路 noteOn/noteOff 天然支持。瓶颈全在输入层。
- 将来的路径：键区 accept 触摸后维护 `touch point id -> key` 映射，直接向 `m_previewMidiIntegrator` 投消息。前置条件是有一条经过真机验证的触摸投递链路。

## 七、已知限制

- 一次只有一个音（单指 + 单光标），不做和弦。
- 键盘比窗口宽时只能横向滚动，触摸下无法拖动键区平移（单指拖动已经被滑奏占用）。曾考虑在键床顶部放一个拖动视口的手柄，本期不做：音域收到 C2–C6 后默认窗口下已能整条放下，溢出只出现在把窗口拉到最窄的极端情况。
- 八度标签依赖缩小的标签字体（下限 8px）。界面字体极大或 DPI 极高时可能整排消失，此时仍可通过黑白键的分组辨认八度。
- 试听链跟随界面值但不写配置，关掉对话框才由 `modifyOption()` 落盘，与页面上其它控件一致。
- 外部 MIDI 输入的高亮**只接在这一条试听键盘上**。钢琴卷帘左栏的竖排键盘还没接（状态接口在基类里已就绪：`setHeldKey` / `clearHeldKeys` / `isKeyHeld`，接上去只需在竖排键盘的绘制里加一层同样的 tint）。

## 八、测试

- `TestApplicationGui / tst_piano_keyboard`：横向键盘的固定尺寸、白键/黑键/黑键下方白键的命中、滑奏序列、按住时隐藏会补 note off、无接收者时滚轮不被消费、外框与黑键顶部裁剪、外部按住高亮的层级；`MidiInputNoteMonitor` 对 note on / note off / 力度 0 的 note on / all-notes-off / all-sound-off 的判定与解挂。
- `TestApplicationGui / tst_audio_midi_settings`：MIDI 页里能找回键盘与滚动区，点键经页面接线投到实时链（用 `MidiMessageListener` 挂过滤器断言 note on/off），按下后实时合成器的包络与 A 频率等于页面控件的当前值，外部输入经 MIDI 系统 integrator 能让键盘进入按住态并在 all-notes-off 后清空，同时配置与文档不受影响。
- `TestApplicationGui / tst_synth_preview`：打开 premixer 后 `noteOn` 能读到非零样本，`noteOff` 若干块后回到静音，证明实时链确实挂在 `m_testMixer` 上。
- `TestGuiComponents / tst_theme_qss_contract`：新选择器的 `qproperty-*` 能沿继承链解析到基类。
