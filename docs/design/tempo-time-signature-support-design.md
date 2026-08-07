# 曲速与拍号变化支持 — 设计文档

> 状态：✅ 实施完成（阶段 1–9 与打磨全部完成；PR #65 已合并，2026-08-06 复核）
> 本文由原 `plans/` 实施计划整理而来；分阶段实施记录与提交清单已移除。

## 背景与目标

最初基线只支持**单一曲速 + 单一拍号**：`Timeline` 虽持有 `QList<Tempo>` / `QList<TimeSignature>`，换算仍只取 `.first()`，`AppModel` 也只暴露全局单值。"tempo 是全局常量"这一假设已渗透进 5 个子系统：21 处 `appModel->tempo()`、9 处 `appModel->timeSignature()`、25 处 `tempoChanged` 连接、14 个文件缓存了各自的 `m_tempo` 副本。

**目标**：支持工程内任意位置插入/编辑/删除曲速点与拍号点，并保证播放、导出、网格、吸附、推理全链路正确。

**参考实现**：`diffscope-project`（含 svscraft / scopicflow / talcs 子模块）。仅作**算法与交互设计的参照**——所有代码改动限定在 lite 内，不改 talcs / svscraft / opendspx；svscraft 是 LGPL-3.0，不复制其代码。

## 已确认的关键结论（源码调研）

| 事项 | 结论 |
|---|---|
| talcs 是否需要改 | **不需要**。`TimeConverter` 是 `std::function<qint64(int)>` 可捕获状态；talcs 内部全部是"绝对 tick → 绝对 sample 再取差"，无线性假设 |
| 拍号是否影响时间换算 | **完全不影响**。tick↔ms↔sample 只与曲速有关 → 拍号线与曲速线可并行 |
| 推理引擎是否感知 tempo | **不感知**。`GenericInferModel` 全是秒域，timeline 只出现在 `semanticObject()` 的缓存签名里 → 换算全在 lite 侧 |
| 索引空间 | 曲速按 **tick** 索引，拍号按 **小节号** 索引（对齐 `opendspx::TimeSignature::index`） |

## 已拍板的设计决策

1. **实施顺序**：拍号线优先（风险最低，且强制完成两条线共用的网格/吸附地基）
2. **吸附**：做对——以所在小节起点为基准（diffscope 的 `alignPosition` 是纯全局等分，此处无参照实现）
3. **音频 clip**：实时锚定，不随曲速拉伸；`AudioClip` 新增实时字段，tick 字段降为 UI 派生缓存
4. **改拍号不触碰音符**（保持 tick 不变，音符相对小节线漂移）——与 diffscope 一致
5. **位置 0 的曲速点/拍号点永不可删** —— 这是 Timeline 全部算法的不变量

## 退化等价性验证策略（跨阶段判据）

> 任何多点 timeline，若所有点的值相同（例如 tick 0 和 tick 9600 都是 120 BPM），其行为必须与单点 timeline **完全一致**——同样的 tick↔ms、同样的网格、同样的曲线采样点、逐比特相同的导出音频。

每个阶段都在**不引入真实变速**的前提下先验证重构正确性，把"重构错误"和"变速逻辑错误"两类问题分离。

## Timeline 核心算法

- `src/libs/MusicBase/Timeline.h` / `Timeline.cpp`
  - 曲速侧：`msecSumMap`（tick → 累计毫秒）+ 反向表，`upperBound` 二分 + 增量重算
  - 拍号侧：`measureMap`（tick → 小节号）+ `revMeasureMap`（小节号 → tick），独立维护
  - 新增 API：`tempoAt(tick)` / `timeSignatureAt(bar)` / `nearestTickWithTempoTo(tick)` / `nearestBarWithTimeSignatureTo(bar)` / `barToTick(bar)` / `timeToTick(bar,beat,tick)`
  - 保留 `tickToMs` / `msToTick` 的 **`double` 返回值**（推理链路依赖亚 tick 精度；svscraft 的 `msecToTick` 返回 `int`，不可照抄）
  - 反向查找**不用 `QMap<double,int>`**（浮点做 key 有精度隐患）——用有序 `QList` + `std::upper_bound`
- 新增 `MusicTime` 值类型（measure/beat/tick 三元组 + `fromString`/`toString`）：显示 1-based（"001:01:000"），内部 0-based；`fromString` 支持 `15` / `15:2` 简写与全角冒号
- `src/libs/MusicBase/TimeSignature.h`：`pos` 改名为 `barIndex` 并明确为小节号语义；JSON key 保持 `"pos"` 以兼容既有工程与推理缓存签名
- `src/libs/MusicBase/MusicTimeConverter.cpp` 降级为 Timeline 内部实现细节
- 转换公式保持与旧 `MusicTimeConverter` 相同的运算顺序，单点时**逐比特一致**（`TestMusicTimeline` 以 `==` 断言）；缓存采用整表重建而非增量重算（点数少，无性能差异）
- `timeToTick` 对负分量直接返回 -1（未沿用 svscraft 的负 beat 归一化循环）；beat 溢出按该小节拍长向后顺延
- 同位置多点由 Timeline 归一化（保留第一个并 warn）

## AppModel 收敛（纯重构，行为不变）

- `src/app/Model/AppModel/AppModel.h` / `AppModel_p.h`：`m_tempo` + `m_timeSignature` → 单个 `Timeline`（此时仍只有一个点）
- 信号 `tempoChanged(double)` + `timeSignatureChanged(int,int)` → **收敛为单个 `timelineChanged()`**；`ModelChangeHandler` 内部对曲速侧做 diff，改拍号不会误触推理重建。`setTempo` / `setTimeSignature` 是位置 0 锚点的兼容入口，标题栏按播放头当前区段编辑
- 21 处 `appModel->tempo()` / 9 处 `appModel->timeSignature()` 全部改走 Timeline API
- 14 个持有 `m_tempo` 副本的文件改为持有 Timeline 快照（`PlaybackController`、`WaveformPainter`、`AudioClipView`、`GetPhonemeNameTask`、`TempoComboBox`、`PlaybackView` 等）
- **两条音频刷新链路都要改接** `timelineChanged`：`AudioContext::handleTimeChanged`（音频 clip）与 `TrackSynthesizer`（演唱 clip 与 note）——漏一个就静默失效
- `ProjectModelData.h` 的 `tempo` + `timeSignature` → `Timeline`

## 序列化贯通（多点可读可存）

- `DspxProjectConverter` 读写改为完整序列（此前读取取 `front()` 后丢弃、写硬编码单点）；把 opendspx 的 `TimeSignature::index` 正确映射到 `barIndex`
- `AppModel::serialize()` 去掉硬编码的单 tempo 对象
- `MidiConverter`：`midiMediate.tempos()` / `timeSignatures()` 本来就是完整序列（此前只取 `front()`）；"denominator 必须是 2/4/8/16" 的校验**对每一项**做
- `ImportProjectActions` 的 `importTempo` / `importTimeSignature` 改为整序列替换

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

标尺顺带加拍号标签：仅当 `nearestBarWithTimeSignatureTo(bar) == bar` 时，在小节号右侧多画一个 "n/d"；复用 `TextPixmapCache`（`src/libs/GUI/Utils/`），加一种 key 即可。

## 吸附与 Bar:Beat 双向

- `src/libs/MusicBase/TimelineSnapUtils.h`：`snapNearest(tick, step)` 是从 tick 0 起的全局等分，新增以**所在小节起点为基准**的重载（diffscope 无参照实现，自行设计）
- 音符**移动**保留原 delta 吸附语义（保持子网格偏移，退化等价性要求）；绝对位置吸附（clip 拖动/缩放、画音符、切分、粘贴、loop 标记）全部走小节基准新重载 `snapNearest/snapDown(tick, step, timeline)`
- `PlaybackView` 的 Bar:Beat:Tick 显示改用 `MusicTime`；新增跳转输入（依赖 `timeToTick`）
- `AppModel` 的 `length = ticksPerWholeNote * numerator / denominator * bars` 改走 timeline

## 拍号编辑 UI 与撤销

> 交互方案按用户的 Lunacy 设计稿实现为轨道编辑器标尺下方的**拍号轨**，未采用"标尺右键菜单"方案。

- **拍号轨**：轨道编辑器标尺下方 28px 信息行（`src/app/UI/Views/TrackEditor/InfoLane/`）。`InfoLaneView` 为可复用基类（chip 绘制/命中/横向同步/与画布同款小节-拍-细分网格线/播放头实线+上次位置虚线，全走 qproperty 主题 token），曲速轨与标记轨将来复用；`TimeSignatureLaneView` 提供 chip 数据与交互；左侧面板对应 `InfoLaneHeaderView` 标题行。工具栏音符图标 toggle 控制显隐（占位图标，待重新设计）
- **交互**：双击空白 = 在鼠标所在小节插入；双击 chip = 编辑；chip 右键菜单 = 编辑/删除，**小节 0 删除置灰**。无 +/− 按钮（有意砍掉降复杂度）
- **编辑器**：模态对话框 `EditTimeSignatureDialog`（OK/取消，确认才提交 = 一条撤销记录）；分子 SpinBox + **分母下拉只给 2 的幂** + 常用拍号快捷（4/4、2/4、3/4、6/8）抽为 `TimeSignatureEditWidget`（`UI/Views/Common/`），与标题栏 popup 共用；无"位置输入"（位置由双击处/chip 决定）
- **撤销**：插入/删除/改值三类语义统一由 `EditTimeSignaturesAction` 整序列替换实现（`TimeSignatureActions::setTimeSignatureAt/removeTimeSignatureAt`，undo 栈名称区分三类；无变化时不产生记录）；单点 `EditTimeSignatureAction` 已删除。控制器入口 `AppController::onSetTimeSignatureAt/onRemoveTimeSignatureAt`
- **标题栏**：`TimeSignatureComboBox` 显示/编辑**播放头所在段**，编辑开始（popup 打开或行内编辑）时快照所在段小节号，播放中改动落在快照段上
- 已知小项：拍号轨行与轨道列表间分隔线与列表自带上边框叠加（视觉略粗，待打磨）

---

# 曲速线

## 音频引擎与波形（实时锚定）

### 四项决策

1. **持久化与 diffscope 对齐，dspx 只存 tick**：ms 真相仅存在于运行时内存；保存时落盘派生 tick（与落盘 timeline 天然自洽），载入时由 tick + 文件 timeline 反推 ms。不写 workspace 扩展。已知代价：每次"存盘→重开→再改曲速"循环 trim/长度重吸附当时 tick 网格（≤0.5 tick/循环），会话内零漂移，可见起点 P 永不漂移。**剪贴板例外**：`ClipsInfo` JSON 额外携带 ms（自有格式无兼容负担，覆盖"复制→改曲速→粘贴"）
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
- `AudioContext::feedCompensatedPosition` 喂三元组（含"勿用于演唱 clip"注释）；`handleTimeChanged` 改为"重算三元组 → set → 无条件 `updatePosition()`"（talcs setter 等值 no-op）
- 派生缓存重算顺序：`OverlappableSerialList` 以当前 `interval()` 为 key，必须 `removeClip → 改 → insertClip → notifyPropertyChanged`（setter 不发信号）
- **撤销 action 改存 ms 真相**（音频 clip）：`MidiConverter` AppendToProject 绕过 undo 栈直接 `setTimeline`，破坏"回放时 timeline 与录制时一致"前提，tick 快照回放不可靠。`ClipCommonProperties` 增加 ms 字段（-1 = 由 tick 派生），`build()` 时补齐，execute/undo 经 `applyRealTimeAnchorFromProperties` 重派生
- **删除旧重锚定逻辑**：`TempoActions::editTempo` 的 tick↔ms round-trip 重锚定（含 `static_cast<int>` 截断漂移）与新机制双重锚定，删除；`ImportProjectActions` 单点分支同理简化
- `AudioClip::preserveUnchangedTruth`：纯移动保留 trim/播放时长，只有对应 tick 分量变化时才重定义真相分量（否则跨段拖动会用落点 tick 几何改写真相，可能播出静音）
- 拖动预览用 **ms 真相预览**：按下时捕获真相窗口与按下点的 ms 偏移；Move 以"按下点内容始终跟随光标"为约束反解新可见起点（吸附仍作用于左缘）；提交时把手势的精确 ms 真相盖章进 `ClipCommonProperties`，避免从取整 tick 重派生的亚毫秒漂移

### 波形与播放

- 波形分段绘制：`AudioClipView` 与 `WaveformPainter` 的单一 `samplesPerTick` 改为按曲速段分段；未采用显式 section 列表，改为逐像素边界的绝对 tick→ms 映射（`tickToSamplePos`/`samplePosAtTick`），单曲速下与旧公式数学等价；PhonemeView 在 timelineChanged 时重对齐存活 piece 波形
- 循环范围、播放头位置在 timeline 变化后重算

## 参数曲线非均匀重采样（最易静默出错）

### 问题

参数曲线以 **5-tick 网格**存储（`DrawCurve.h` 的 `step = 5`），引擎工作在 **0.01s 帧网格**。两个方向各自"统一单位后做等间隔重采样"：入方向 `InferAcousticTask` 的 `resample(values, 5, timeline.secToTick(0.01))`，出方向 `InferPitchTask` 的 `resample(values, interval, timeline.tickToSec(5))`。两者都依赖 `secToTick(0.01)` / `tickToSec(5)` 是**常数**——变速下 tick 网格与秒网格是分段线性、整体非线性的映射，原 `resample` 的"等间隔源→等间隔目标"契约表达不了。

### 改动

1. `MathUtils` 新增显式**源位置数组 + 目标位置数组**重采样重载；目标数组长度就是输出长度，并以端点钳位明确定义边界行为
2. Duration / Pitch / Variance / Acoustic 的输入统一携带 clip 与 piece 的绝对 tick 范围；参数曲线快照同时携带自身 `localStartTick`
3. 推理输入按绝对 tick→秒构造引擎帧位置，输出按歌唱剪辑局部 5-tick 网格回写；所有动态参数、retake 和动态说话人混合以同一个 `frames` 为长度真相
4. 推理语义签名只包含 piece 有效范围内的曲速断点和曲线局部起点，避免无关断点误伤缓存，同时保证真实变速会使旧任务失效
5. RMVPE 按素材原点、裁剪可见区和提取器帧毫秒偏移映射到工程绝对时间，再落到歌唱剪辑局部 5-tick 网格；不再把帧偏移当作工程原点

**不用改**：`InferTaskHelper.cpp:94-139` 的 note 秒长度计算是逐 note 绝对换算再取差，变速下天然正确。

## 曲速编辑 UI 与撤销

- 新增独立曲速信息轨（结构与拍号轨同构，曲速点锚在 tick 而非小节）和显式开关；空白处双击按当前可见网格吸附插入，曲速 chip 可双击或右键编辑，tick 0 删除项置灰
- 编辑对话框与标题栏弹窗复用 `TempoEditWidget`（数值输入 + Tap Tempo）；标题栏显示播放头当前区段，并在开始编辑时锁定目标断点
- 撤销 Action 统一为整张曲速表的插入 / 编辑 / 删除，历史名称可翻译；冗余同值操作不写入历史
- **交互明确不支持拖拽**：曲速是纯阶梯断点，不提供渐变或斜坡语义；移动断点通过删除后重新插入完成
- **失效架构改为两层**：`Timeline::tempoChangeRanges` diff 新旧表的有效 BPM 区间，冗余同值点不失效；`AppModel` 只提高相交歌唱剪辑的修订号，`InferPipeline` 通过 `timelineChanged` 从所有活跃状态回到 duration，仅重启相交的存活 piece，新旧分段仍由 `reSegment` 最小替换

## 已知限制与遗留事项

- 2 条 i18n 未翻译：`"Import tempo"` 和 `"Import time signature"` 仍标记为 `type="unfinished"`
- Code Review 待讨论项最终状态：

| 编号 | 问题 | 状态 |
|------|------|------|
| 1 | `effectiveTempoArray` 区间过滤 `<=` vs `>` | 代码仍为 `tempo.pos <= startTick`；巧合正确，未改（改为 `>` 正向包含语义可消除维护陷阱） |
| 2 | `resampleFramesToCurve` 的 `qRound` vs `floor` | 代码仍为 `qRound`；可能丢失 ~0-5 tick 数据，影响极小，未改 |
| 3 | `handleTempoChanged` 中 `reSegment(timeline, false)` 信号完整性 | 注释已说明 AppModel 已 bump revision，逻辑正确 |
| 4 | 语义签名与 `fitToFrames` 的一致性 | 已确认为假警报，无需处理 |

- （可选）标尺抽稀层级按 4/4 硬算、拍线可见性假设 beat = 四分音符

## 关键文件（当前路径）

| 模块 | 文件 |
|---|---|
| Timeline 核心 | `src/libs/MusicBase/Timeline.h/.cpp`、`MusicTime.h`、`TimeSignature.h`、`TimelineSnapUtils.h` |
| 模型 | `src/libs/ProjectModel/AppModel/AppModel.h/.cpp`、`AudioClip.h`、`Clip.h`、`ProjectModelData.h` |
| 序列化 | `src/app/Modules/ProjectConverters/DspxProjectConverter.cpp`、`MidiConverter.cpp`、`src/app/Controller/Actions/AppModel/ImportProjectActions.cpp` |
| 网格/吸附 | `src/app/UI/Utils/ITimelinePainter.cpp`、`src/app/UI/Views/Common/TimelineView.cpp`、`TimeGridView.cpp`、`PlaybackView.cpp` |
| 拍号轨 | `src/app/UI/Views/TrackEditor/InfoLane/`（`InfoLaneView`、`TimeSignatureLaneView`、`InfoLaneHeaderView`）、`src/app/UI/Views/Common/EditTimeSignatureDialog`、`TimeSignatureEditWidget`、`TimeSignatureActions` |
| 曲速轨 | `TempoEditWidget`、`TempoActions`、`src/app/UI/Views/MainTitleBar/TempoComboBox` |
| 音频 | `src/app/Modules/Audio/AudioContext.cpp`、`TrackSynthesizer.cpp`、`src/app/UI/Utils/WaveformPainter.cpp`、`src/app/UI/Views/TrackEditor/GraphicsItem/AudioClipView.cpp` |
| 推理 | `src/app/Modules/Inference/Tasks/InferAcousticTask.cpp`、`InferPitchTask.cpp`、`src/libs/Support/MathUtils.h` |
| 测试 | `src/tests/TestMusicTimeline`、`TestAudioAnchor`、`TestParamResample` |
