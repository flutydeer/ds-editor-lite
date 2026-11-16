# 曲速与拍号变化支持 — 设计文档

工程支持在任意位置插入、编辑和删除曲速点与拍号点。播放、导出、网格、吸附与推理使用同一工程时间轴。

## 时间轴契约

设计参照为 `diffscope-project`，包含 svscraft、scopicflow 和 talcs。svscraft 使用 LGPL-3.0，仅作算法与交互参考。

| 事项 | 结论 |
|---|---|
| 曲速与音频 | talcs 的 `TimeConverter` 是 `std::function<qint64(int)>`。它从绝对 tick 转为绝对 sample，按两个位置求差，可使用分段曲速 |
| 曲速与拍号 | tick、毫秒和采样间的换算只受曲速影响。拍号定义音乐时间的拍、小节与网格 |
| 推理时间 | `GenericInferModel` 的输入输出使用秒域，由编辑器根据工程时间轴换算。推理语义签名保存相关曲速信息 |
| 索引空间 | 曲速按 **tick** 索引，拍号按 **小节号** 索引（对齐 `opendspx::TimeSignature::index`） |

- 位置 0 的曲速点和拍号点是时间轴的锚点，不可删除。
- 绝对位置吸附以所在小节起点为基准。改变拍号时保持音符的 tick 位置。
- 音频剪辑按实际时间锚定，改变曲速时保持素材的播放时长。
- 冗余的同值断点保持有效曲速不变。比较导出事件位置时，整数采样换算允许相差 1 个 sample。

## Timeline 核心算法

- `src/libs/MusicBase/Timeline.h` / `Timeline.cpp`
  - 曲速侧按断点缓存累计毫秒，拍号侧按断点缓存小节起始 tick。分别二分查找当前区间
  - 新增 API：`tempoAt(tick)` / `timeSignatureAt(bar)` / `nearestTickWithTempoTo(tick)` / `nearestBarWithTimeSignatureTo(bar)` / `barToTick(bar)` / `timeToTick(bar,beat,tick)`
  - 保留 `tickToMs` / `msToTick` 的 **`double` 返回值**（推理链路依赖亚 tick 精度。svscraft 的 `msecToTick` 返回 `int`，不可照抄）
  - 反向查找**不用 `QMap<double,int>`**（浮点做 key 有精度隐患）——用有序 `QList` + `std::upper_bound`
- 新增 `MusicTime` 值类型（measure/beat/tick 三元组 + `fromString`/`toString`）：显示 1-based（"001:01:000"），内部 0-based。`fromString` 支持 `15` / `15:2` 简写与全角冒号
- `src/libs/MusicBase/TimeSignature.h`：`pos` 改名为 `barIndex` 并明确为小节号语义。JSON key 保持 `"pos"` 以兼容既有工程与推理缓存签名
- `src/libs/MusicBase/MusicTimeConverter.cpp` 降级为 Timeline 内部实现细节
- 转换公式保持与旧 `MusicTimeConverter` 相同的运算顺序，单点时**逐比特一致**（`TestMusicTimeline` 以 `==` 断言）。缓存采用整表重建而非增量重算（点数少，无性能差异）
- `timeToTick` 对负分量直接返回 -1（未沿用 svscraft 的负 beat 归一化循环）。beat 溢出按该小节拍长向后顺延
- 同位置多点由 Timeline 归一化（保留第一个并 warn）

## 工程模型

- [`AppModel`](../../src/libs/ProjectModel/AppModel/AppModel.h) 持有工程的 `Timeline` 并发布 `timelineChanged()`。音频剪辑、歌声剪辑和音符的位置换算均使用更新后的时间轴。
- `setTempo` 和 `setTimeSignature` 作用于位置 0。标题栏按播放头当前区段编辑对应断点。

## 序列化贯通（多点可读可存）

- [工程格式转换器](../../src/libs/ProjectConverters)读写完整曲速和拍号序列。DSPX 的 `TimeSignature::index` 对应 `Timeline` 的 `barIndex`。
- 导入工程时可以选择采用来源工程的曲速或拍号序列。所选序列整体替换目标工程的对应序列。

---

# 拍号线

## 网格重写

`src/app/UI/Utils/ITimelinePainter.cpp` 有三处**结构性**失效，需换骨架：

| 现状 | 改法 |
|---|---|
| `for (tick += barTicks)` 假设小节等宽 | 外层按**小节号**迭代，`timeline.barToTick(bar)` 反查 |
| `tick % barTicks == 0` 判层级 | 用 `MusicTime::tick() == 0` / `beat() == 0` 判层级 |
| `logicalGridStepForScale(ticksPerPixel)` 返回单一全局 step | 签名改为 `(ticksPerPixel, atTick)`，**每小节各算一次** |

每小节迭代骨架（参照 scopicflow）：每小节取自己的拍号，`ticksPerBeat`/`ticksPerBar` 由本小节拍号算出，ratio 逐级降 2 的幂直到间距 ≥ 阈值，段内均匀步进。

**保留 lite 现有的 opacity 淡入淡出**（`spacingVisibility` / `smoothStep` / `buildSubdivisionLevels`）——只换迭代骨架，不换视觉分层。同步更新继承者：`TimelineView.cpp`、`TimeGridView.cpp`。

标尺顺带加拍号标签：仅当 `nearestBarWithTimeSignatureTo(bar) == bar` 时，在小节号右侧多画一个 "n/d"。复用 `TextPixmapCache`（`src/libs/GUI/Utils/`），加一种 key 即可。

## 吸附与 Bar:Beat 双向

- `src/libs/MusicBase/TimelineSnapUtils.h`：`snapNearest(tick, step)` 是从 tick 0 起的全局等分，新增以**所在小节起点为基准**的重载（diffscope 无参照实现，自行设计）
- 音符**移动**保留原 delta 吸附语义（保持子网格偏移，退化等价性要求）。绝对位置吸附（clip 拖动/缩放、画音符、切分、粘贴、loop 标记）全部走小节基准新重载 `snapNearest/snapDown(tick, step, timeline)`
- `PlaybackView` 的 Bar:Beat:Tick 显示改用 `MusicTime`。新增跳转输入（依赖 `timeToTick`）
- `AppModel` 的 `length = ticksPerWholeNote * numerator / denominator * bars` 改走 timeline

## 拍号编辑 UI 与撤销

> 交互方案按用户的 Lunacy 设计稿实现为轨道编辑器标尺下方的**拍号轨**，未采用"标尺右键菜单"方案。

- **拍号轨**：轨道编辑器标尺下方 28px 信息行（`src/app/UI/Views/TrackEditor/InfoLane/`）。`InfoLaneView` 为可复用基类（chip 绘制/命中/横向同步/与画布同款小节-拍-细分网格线/播放头实线+上次位置虚线，全走 qproperty 主题 token），曲速轨与标记轨将来复用。`TimeSignatureLaneView` 提供 chip 数据与交互。左侧面板对应 `InfoLaneHeaderView` 标题行。工具栏音符图标 toggle 控制显隐（占位图标，待重新设计）
- **交互**：双击空白 = 在鼠标所在小节插入。双击 chip = 编辑。chip 右键菜单 = 编辑/删除，**小节 0 删除置灰**。无 +/− 按钮（有意砍掉降复杂度）
- **编辑器**：模态对话框 `EditTimeSignatureDialog`（OK/取消，确认才提交 = 一条撤销记录）。分子 SpinBox + **分母下拉只给 2 的幂** + 常用拍号快捷（4/4、2/4、3/4、6/8）抽为 `TimeSignatureEditWidget`（`UI/Views/Common/`），与标题栏 popup 共用。无"位置输入"（位置由双击处/chip 决定）
- **撤销**：插入/删除/改值三类语义统一由 `EditTimeSignaturesAction` 整序列替换实现（`TimeSignatureActions::setTimeSignatureAt/removeTimeSignatureAt`，undo 栈名称区分三类。无变化时不产生记录）。单点 `EditTimeSignatureAction` 已删除。控制器入口 `AppController::onSetTimeSignatureAt/onRemoveTimeSignatureAt`
- **标题栏**：`TimeSignatureComboBox` 显示/编辑**播放头所在段**，编辑开始（popup 打开或行内编辑）时快照所在段小节号，播放中改动落在快照段上
- 已知小项：拍号轨行与轨道列表间分隔线与列表自带上边框叠加（视觉略粗，待打磨）
- 后续复用：第三类信息轨（时间（秒）轨，即区别于 tick 的秒坐标时间轴）已按同一基类落地，排在标尺正下方、曲速轨之前，见 [time-axis-and-timecode-design.md](time-axis-and-timecode-design.md)，`InfoLaneView` 为此新增了 `paintLaneBackground` / `paintLaneContent` 两个绘制钩子

---

# 曲速线

## 音频引擎与波形（实时锚定）

### 四项决策

1. **持久化与 diffscope 对齐，dspx 只存 tick**：ms 真相仅存在于运行时内存。保存时落盘派生 tick（与落盘 timeline 天然自洽），载入时由 tick + 文件 timeline 反推 ms。不写 workspace 扩展。已知代价：每次"存盘→重开→再改曲速"循环 trim/长度重吸附当时 tick 网格（≤0.5 tick/循环），会话内零漂移，可见起点 P 永不漂移。**剪贴板例外**：`ClipsInfo` JSON 额外携带 ms（自有格式无兼容负担，覆盖"复制→改曲速→粘贴"）
2. **播放中改曲速验收放宽**：talcs `TransportAudioSource::setPosition` 无 fade（已确认），验收改为"位置正确、无持续错位、瞬时轻微 glitch 可接受"
3. **导出退化等价性放宽**：同值双点浮点结合顺序 + `static_cast<qint64>` 截断理论上可差 1 sample。转换层测试断言严格（亚 ms 容差），导出比对允许事件位置 ≤1 sample 偏差
4. **trim 精度接受整 tick 量化**：talcs `TimeConverter` 是 `std::function<qint64(int)>`，三元组必须是 int tick（±0.5 tick ≈ 0.5ms@120BPM 素材偏移，与现状 tick 粒度一致）

### 补偿三元组（不改 talcs）

talcs 的 `updatePosition()` 把"素材内偏移"当成"工程时间轴上从 0 到该 tick 的时长"，变速下错。由于 `convertTime` 是 lite 自己装的单调可逆双射 *f*，且 `startTick` **从不单独参与换算**，lite 喂补偿值解决：

```
clipStart' = f⁻¹(T · sr)              // T = 素材内修剪偏移（实时秒）
start'     = P − clipStart'           // P = clip 在时间轴上的位置（tick）
clipLen'   = f⁻¹( f(P) + L · sr ) − P // L = 播放时长（实时秒）
```

- `start'` 可能为负：已确认安全（talcs `setStart()` 无校验，且 `startTick` 单独从不进 `convertTime`）
- **禁止推广**：补偿三元组绝不能用于演唱 clip——`DspxNoteContext.cpp:165` 单独转换演唱 clip 的 `start`，负值会走零下外推（代码留注释）

### 模型与管线

- `src/libs/ProjectModel/AppModel/AudioClip.h` 新增实时字段（`trimStartMs` / `lengthMs`）作为真相，`Clip.h` 的 tick 字段降为 UI 派生缓存，timeline 变化时重算（`AppModelPrivate::updateAudioClipTickCaches`，在 `setTimeline`/`setTempo` 中、`timelineChanged` 发射前完成）
- 真相建立点：`replaceProject`（dspx 载入）、`InsertTrackAction::execute`（工程导入）、`MidiConverter::convertClips`（MIDI 载入）、`TrackController` 音频导入（`frames/sr`，替换 `tempoAt(0)` 公式，改走 clip 所在位置的 timeline 往返）、粘贴（剪贴板 ms 优先）
- **引擎三元组 ≠ UI tick 缓存**：喂 talcs 的 `clipStart' = round(msToTick(trimMs))`（绝对映射补偿量）与模型缓存 `clipStart = P − round(msToTick(tickToMs(P) − trimMs))`（素材原点可视偏移）是不同推导，仅单曲速下重合。`AudioContext` 直接读真相字段算三元组，不经 UI 缓存
- `AudioContext::feedCompensatedPosition` 喂三元组（含"勿用于演唱 clip"注释）。`handleTimeChanged` 改为"重算三元组 → set → 无条件 `updatePosition()`"（talcs setter 等值 no-op）
- 派生缓存重算顺序：`OverlappableSerialList` 以当前 `interval()` 为 key，必须 `removeClip → 改 → insertClip → notifyPropertyChanged`（setter 不发信号）
- **撤销 action 改存 ms 真相**（音频 clip）：`MidiConverter` AppendToProject 绕过 undo 栈直接 `setTimeline`，破坏"回放时 timeline 与录制时一致"前提，tick 快照回放不可靠。`ClipCommonProperties` 增加 ms 字段（-1 = 由 tick 派生），`build()` 时补齐，execute/undo 经 `applyRealTimeAnchorFromProperties` 重派生
- **删除旧重锚定逻辑**：`TempoActions::editTempo` 的 tick↔ms round-trip 重锚定（含 `static_cast<int>` 截断漂移）与新机制双重锚定，删除。`ImportProjectActions` 单点分支同理简化
- `AudioClip::preserveUnchangedTruth`：纯移动保留 trim/播放时长，只有对应 tick 分量变化时才重定义真相分量（否则跨段拖动会用落点 tick 几何改写真相，可能播出静音）
- 拖动预览用 **ms 真相预览**：按下时捕获真相窗口与按下点的 ms 偏移。Move 以"按下点内容始终跟随光标"为约束反解新可见起点（吸附仍作用于左缘）。提交时把手势的精确 ms 真相盖章进 `ClipCommonProperties`，避免从取整 tick 重派生的亚毫秒漂移

### 波形与播放

- 波形分段绘制：`AudioClipView` 与 `WaveformPainter` 的单一 `samplesPerTick` 改为按曲速段分段。未采用显式 section 列表，改为逐像素边界的绝对 tick→ms 映射（`tickToSamplePos`/`samplePosAtTick`），单曲速下与旧公式数学等价。PhonemeView 在 timelineChanged 时重对齐存活 piece 波形
- 循环范围、播放头位置在 timeline 变化后重算

## 参数曲线非均匀重采样（最易静默出错）

### 问题

参数曲线以 **5-tick 网格**存储（`DrawCurve.h` 的 `step = 5`），引擎工作在 **0.01s 帧网格**。两个方向各自"统一单位后做等间隔重采样"：入方向 `InferAcousticTask` 的 `resample(values, 5, timeline.secToTick(0.01))`，出方向 `InferPitchTask` 的 `resample(values, interval, timeline.tickToSec(5))`。两者都依赖 `secToTick(0.01)` / `tickToSec(5)` 是**常数**——变速下 tick 网格与秒网格是分段线性、整体非线性的映射，原 `resample` 的"等间隔源→等间隔目标"契约表达不了。

### 改动

1. `MathUtils` 新增显式**源位置数组 + 目标位置数组**重采样重载。目标数组长度就是输出长度，并以端点钳位明确定义边界行为
2. Duration / Pitch / Variance / Acoustic 的输入统一携带 clip 与 piece 的绝对 tick 范围。参数曲线快照同时携带自身 `localStartTick`
3. 推理输入按绝对 tick→秒构造引擎帧位置，输出按歌唱剪辑局部 5-tick 网格回写。所有动态参数、retake 和动态说话人混合以同一个 `frames` 为长度真相
4. 推理语义签名只包含 piece 有效范围内的曲速断点和曲线局部起点，避免无关断点误伤缓存，同时保证真实变速会使旧任务失效
5. RMVPE 按素材原点、裁剪可见区和提取器帧毫秒偏移映射到工程绝对时间，再落到歌唱剪辑局部 5-tick 网格。不再把帧偏移当作工程原点

**不用改**：`InferTaskHelper.cpp:94-139` 的 note 秒长度计算是逐 note 绝对换算再取差，变速下天然正确。

## 曲速编辑 UI 与撤销

- 新增独立曲速信息轨（结构与拍号轨同构，曲速点锚在 tick 而非小节）和显式开关。空白处双击按当前可见网格吸附插入，曲速 chip 可双击或右键编辑，tick 0 删除项置灰
- 编辑对话框与标题栏弹窗复用 `TempoEditWidget`（数值输入 + Tap Tempo）。标题栏显示播放头当前区段，并在开始编辑时锁定目标断点
- 编辑对话框头部按标题栏时间码同款 `003:01:000`（小节:拍:tick）显示断点位置，小节长度取该 tick 实际生效的拍号。位置只读，移动断点仍靠删除后重新插入
- 撤销 Action 统一为整张曲速表的插入 / 编辑 / 删除，历史名称可翻译。冗余同值操作不写入历史
- **交互明确不支持拖拽**：曲速是纯阶梯断点，不提供渐变或斜坡语义。移动断点通过删除后重新插入完成
- **失效架构改为两层**：`Timeline::tempoChangeRanges` diff 新旧表的有效 BPM 区间，冗余同值点不失效。`AppModel` 只提高相交歌唱剪辑的修订号，`InferPipeline` 通过 `timelineChanged` 从所有活跃状态回到 duration，仅重启相交的存活 piece，新旧分段仍由 `reSegment` 最小替换

## 已知限制与遗留事项

DSPX 将音频剪辑的裁剪和时长写为整数 tick，因此重新打开工程后会有保存时曲速下的 tick 量化误差。曲速点描述阶梯变化，移动断点需在新位置重新插入。
