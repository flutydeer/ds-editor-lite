# Tempo and Time Signature Change Support — Implementation Plan

## 进度总览

状态图例：⬜ 未开始 / 🔵 进行中 / ✅ 已完成 / ⏸️ 挂起

| 阶段 | 状态 | 难度 | 风险 |
|---|---|---|---|
| 1 — Timeline 核心算法（共享地基） | ⬜ | ★☆☆☆☆ | 低（有参照实现，但需 double 化改造） |
| 2 — AppModel 收敛（纯重构） | ⬜ | ★★☆☆☆ | 中（面广但机械；做扎实可大幅降低后续风险） |
| 3 — 序列化贯通 | ⬜ | ★★☆☆☆ | 低 |
| 4 — 网格重写〔拍号线〕 | ⬜ | ★★★☆☆ | 中（骨架有参照，视觉分层需自行嫁接） |
| 5 — 吸附与 Bar:Beat 双向〔拍号线〕 | ⬜ | ★★★☆☆ | 中（吸附**无参照实现**） |
| 6 — 拍号编辑 UI 与撤销〔拍号线〕 | ⬜ | ★★★☆☆ | 低（交互设计可照抄） |
| 7 — 音频引擎与波形〔曲速线〕 | ⬜ | ★★★☆☆ | 中（talcs 不需改，补偿方案已验证可行） |
| 8 — 参数曲线非均匀重采样〔曲速线〕 | ⬜ | ★★★★☆ | **高**（最易静默出错，必须靠退化等价性测试兜底） |
| 9 — 曲速编辑 UI 与撤销〔曲速线〕 | ⬜ | ★★★☆☆ | 低 |
| 10 — 打磨 | ⬜ | ★★☆☆☆ | 低 |

**风险重心在阶段 8**，其次是阶段 2 的收敛质量。阶段 1–3 是两条线共用的地基，务必先完成；之后拍号线优先。

---

## Context

当前工程只支持**单一曲速 + 单一拍号**。`src/app/Model/AppModel/Timeline.cpp` 虽然已经持有 `QList<Tempo>` / `QList<TimeSignature>`，但所有换算一律取 `.first()`（作者自己留了 `// TODO 支持多曲速`）；`src/app/Model/AppModel/AppModel.h` 实际存的是 `double m_tempo` + 单个 `TimeSignature`。

"tempo 是全局常量"这一假设已渗透进 5 个子系统：21 处 `appModel->tempo()`、9 处 `appModel->timeSignature()`、25 处 `tempoChanged` 连接、14 个文件缓存了各自的 `m_tempo` 副本。

**目标**：支持工程内任意位置插入/编辑/删除曲速点与拍号点，并保证播放、导出、网格、吸附、推理全链路正确。

**参考实现**：`E:\GitHub\diffscope-project`（含 svscraft / scopicflow / talcs 子模块）。仅作**算法与交互设计的参照**——所有代码改动限定在 lite 内，不改 talcs / svscraft / opendspx；svscraft 是 LGPL-3.0，不复制其代码。

### 已确认的关键结论（来自源码调研）

| 事项 | 结论 |
|---|---|
| talcs 是否需要改 | **不需要**。`TimeConverter` 是 `std::function<qint64(int)>` 可捕获状态；talcs 内部全部是"绝对 tick → 绝对 sample 再取差"，无线性假设 |
| 拍号是否影响时间换算 | **完全不影响**。tick↔ms↔sample 只与曲速有关 → 拍号线与曲速线可并行 |
| 推理引擎是否感知 tempo | **不感知**。`GenericInferModel` 全是秒域，timeline 只出现在 `semanticObject()` 的缓存签名里 → 换算全在 lite 侧 |
| 索引空间 | 曲速按 **tick** 索引，拍号按 **小节号** 索引（对齐 `opendspx::TimeSignature::index`） |

### 已拍板的设计决策

1. **实施顺序**：拍号线优先（风险最低，且强制完成两条线共用的网格/吸附地基）
2. **吸附**：做对——以所在小节起点为基准（diffscope 的 `alignPosition` 是纯全局等分，此处无参照实现）
3. **音频 clip**：实时锚定，不随曲速拉伸；`AudioClip` 新增实时字段，tick 字段降为 UI 派生缓存
4. **改拍号不触碰音符**（保持 tick 不变，音符相对小节线漂移）—— 与 diffscope 一致
5. **位置 0 的曲速点/拍号点永不可删** —— 这是 Timeline 全部算法的不变量

---

## 跨阶段验证策略：退化等价性测试

**每个阶段都用同一个廉价而强力的判据**：

> 任何多点 timeline，若所有点的值相同（例如 tick 0 和 tick 9600 都是 120 BPM），其行为必须与单点 timeline **完全一致**——同样的 tick↔ms、同样的网格、同样的曲线采样点、逐比特相同的导出音频。

这让每个阶段都能在**不引入真实变速**的前提下先验证重构正确性，把"重构错误"和"变速逻辑错误"两类问题分离开。每个阶段的验收都应先跑退化等价性，再跑真实变速用例。

---

## 阶段 1 — Timeline 核心算法（共享地基） ⬜

**目标**：`Timeline` 成为完整的双索引时间轴，纯计算、不接线，应用行为零变化。

### 改动

- `src/app/Model/AppModel/Timeline.h` / `Timeline.cpp`
  - 曲速侧：`msecSumMap`（tick → 累计毫秒）+ 反向表，`upperBound` 二分 + 增量重算
  - 拍号侧：`measureMap`（tick → 小节号）+ `revMeasureMap`（小节号 → tick），独立维护
  - 新增 API：`tempoAt(tick)` / `timeSignatureAt(bar)` / `nearestTickWithTempoTo(tick)` / `nearestBarWithTimeSignatureTo(bar)` / `barToTick(bar)` / `timeToTick(bar,beat,tick)`
  - 保留 `tickToMs` / `msToTick` 的 **`double` 返回值**（推理链路依赖亚 tick 精度；svscraft 的 `msecToTick` 返回 `int`，**不可照抄**）
  - 反向查找**不要用 `QMap<double,int>`**（svscraft 的做法，浮点做 key 有精度隐患）——用有序 `QList` + `std::upper_bound`
- 新增 `MusicTime` 值类型（measure/beat/tick 三元组 + `fromString`/`toString`），替代现在返回格式化字符串的 `getBarBeatTickTime`
- `src/app/Model/AppModel/TimeSignature.h`：`pos` 改名为 `barIndex` 并明确为小节号语义（现在只有一个拍号且 pos=0，改名成本极低；以后改是数据兼容性问题）
- `src/app/Utils/MusicTimeConverter.cpp` 降级为 Timeline 内部实现细节

### 参考

svscraft `src/libs/3rdparty/svscraft/src/core/time/MusicTimeline.cpp` 的 `updateMeasureMap` / `updateMsecSumMap` / `tickToTime` / `timeToTick`（约 150 行有效代码）。注意其 `timeToTick` 处理负 beat 的 while 循环较绕，是测试重点。**svscraft 自身没有 MusicTimeline 的单元测试**，不能把它当作正确性证据。

### 验收

新增测试目标 `src/tests/TestMusicTimeline`，照 `src/tests/TestSpeakerMixValidation/CMakeLists.txt` 的模式（它已经在编译 `Timeline.cpp` / `Tempo.cpp` / `TimeSignature.cpp`），在 `src/tests/CMakeLists.txt` 注册：

- [ ] 退化等价性：多个同值点 == 单点
- [ ] tick↔ms 往返一致性（含跨段、段边界上、段边界±1）
- [ ] tick↔MusicTime 往返，跨拍号段小节号累加正确
- [ ] `barToTick(timeToTick(...))` 恒等；负 beat / 越界输入返回 -1 而非崩溃
- [ ] 0 位置不可删的不变量（删除时应拒绝并保持 map 完整）
- [ ] 极端值：极慢/极快曲速、denominator = 1..128、连续同位置点

---

## 阶段 2 — AppModel 收敛（纯重构，行为不变） ⬜ ★关键安全垫

**目标**：把"tempo 是常量"的假设从 30+ 个文件收敛到 `Timeline` 一个类。**功能与改动前完全一致**。

### 改动

- `src/app/Model/AppModel/AppModel.h` / `AppModel_p.h`：`m_tempo` + `m_timeSignature` → 单个 `Timeline`（此时仍只有一个点）
- 信号 `tempoChanged(double)` + `timeSignatureChanged(int,int)` → `timelineChanged()`
- 21 处 `appModel->tempo()` / 9 处 `appModel->timeSignature()` 全部改走 Timeline API
- 5 处临时构造 `Timeline{{{0, appModel->tempo()}}}` 改为引用真实 timeline：`src/app/Model/AppModel/SingingClip.cpp:108` 与 `:160`、`src/app/Modules/Inference/InferController.cpp:92` 与 `:473`、`src/app/Model/AppModel/SingingClipPhonemeNormalizer.cpp:97`
- 14 个持有 `m_tempo` 副本的文件改为持有 Timeline 快照（`PlaybackController`、`WaveformPainter`、`AudioClipView`、`GetPhonemeNameTask`、`TempoComboBox`、`PlaybackView` 等）
- **两条音频刷新链路都要改接** `timelineChanged`：`src/app/Modules/Audio/AudioContext.cpp:133`（音频 clip）与 `src/app/Modules/Audio/TrackSynthesizer.cpp:41`（演唱 clip 与 note）——漏一个就静默失效
- `src/app/Model/AppModel/ProjectModelData.h` 的 `tempo` + `timeSignature` → `Timeline`

### 验收

- [ ] 现有测试全绿（`TestSpeakerMixValidation` / `TestVoiceContext` / `TestDocumentWorkflow` 已在编译 `Timeline.cpp`，构成天然回归网）
- [ ] 手工冒烟：打开工程 → 播放 → 改曲速 → 播放位置与音高正确 → 推理一次 → 导出音频
- [ ] **导出比对**：改动前后导出同一工程，音频文件逐比特相同

---

## 阶段 3 — 序列化贯通（多点可读可存，UI 只读） ⬜

**目标**：能正确打开、保存、往返带多曲速/多拍号的工程文件。

### 改动

- `src/app/Modules/ProjectConverters/DspxProjectConverter.cpp:1035-1050`（读，现在取 `front()` 后丢弃）与 `:1301-1303`（写，现在硬编码单点）改为完整序列；把 opendspx 的 `TimeSignature::index` 正确映射到 `barIndex`
- `src/app/Model/AppModel/AppModel.cpp:208-222` 的 `serialize()` 去掉硬编码的单 tempo 对象
- `src/app/Modules/ProjectConverters/MidiConverter.cpp:258-321`：`midiMediate.tempos()` / `timeSignatures()` 返回的本来就是完整序列，现在只取 `front()`；同时 `:278` 处 "denominator 必须是 2/4/8/16" 的校验要**对每一项**做
- `src/app/Controller/Actions/AppModel/ImportProjectActions.cpp:15-23` 的 `importTempo` / `importTimeSignature` 改为整序列替换

### 验收

- [ ] 手工构造一个含 3 个曲速点 + 2 个拍号点的 .dspx，打开 → 保存 → 再打开，序列完全一致
- [ ] 导入一个带曲速/拍号变化的 MIDI，序列完整进入模型
- [ ] 退化等价性：单点工程往返后仍是单点，字节级与旧版本兼容
- [ ] 非法 denominator（如 3）被正确拒绝并给出提示

---

# 拍号线（优先）

## 阶段 4 — 网格重写 ⬜

**目标**：打开多拍号工程，标尺与钢琴卷帘的小节线、拍线、细分线、小节编号全部正确。

### 改动

`src/app/UI/Utils/ITimelinePainter.cpp` 有三处**结构性**失效，需要换骨架：

| 现状 | 改法 |
|---|---|
| `for (tick += barTicks)` 假设小节等宽 | 外层按**小节号**迭代，`timeline.barToTick(bar)` 反查 |
| `tick % barTicks == 0` 判层级 | 用 `MusicTime::tick() == 0` / `beat() == 0` 判层级 |
| `logicalGridStepForScale(ticksPerPixel)` 返回单一全局 step | 签名改为 `(ticksPerPixel, atTick)`，**每小节各算一次** |

直接参考 scopicflow `src/libs/3rdparty/scopicflow/src/internal/PianoRollScaleQuickItem.cpp:139-172` 的骨架：

```
for (bar = startBar; bar <= endBar; bar++) {
    ts = timeline.timeSignatureAt(bar);            // 每小节取自己的拍号
    ticksPerBeat/ticksPerBar 由本小节拍号算出
    ratio 逐级降 2 的幂直到间距 ≥ 阈值           // 每小节自适应
    画小节线 at barToTick(bar)
    for (tick = step; tick < ticksPerBar; tick += step)   // 段内均匀步进，安全
        层级 = MusicTime(bar,0,tick).tick()==0 ? Beat : Subdivision
}
```

**保留 lite 现有的 opacity 淡入淡出**（`spacingVisibility` / `smoothStep` / `buildSubdivisionLevels`）——lite 的视觉比 scopicflow 精致，只换迭代骨架，不换视觉分层。

同步更新继承者：`src/app/UI/Views/Common/TimelineView.cpp`、`src/app/UI/Views/Common/TimeGridView.cpp`。

标尺上顺带加拍号标签（参考 scopicflow `src/internal/TimelineScaleQuickItem.cpp:253-260`）：仅当 `nearestBarWithTimeSignatureTo(bar) == bar` 时，在小节号右侧多画一个 "n/d"。lite 已有 `src/app/UI/Utils/TextPixmapCache.h`，加一种 key 即可。

### 验收

- [ ] 打开 4/4 → 3/4 → 6/8 的三段工程，标尺与钢琴卷帘小节线对齐、小节编号连续、拍号标签出现在正确位置
- [ ] 全缩放范围拉一遍：细分线的淡入淡出与抽稀层级不闪烁、不重叠
- [ ] 退化等价性：所有拍号相同时，渲染结果与改动前**像素级一致**（可截图比对）

---

## 阶段 5 — 吸附与 Bar:Beat 双向 ⬜

**目标**：变拍号下拖动音符/clip/loop 标记吸附到正确的小节线与拍线；支持"跳转到第 N 小节"。

### 改动

- `src/app/Utils/TimelineSnapUtils.h`：`snapNearest(tick, step)` 是从 tick 0 起的全局等分，新增以**所在小节起点为基准**的重载。此处 **diffscope 无参照实现**（其 `TimeManipulator::alignPosition` 也是纯全局等分），需自行设计。
- 所有拖动路径改用新重载：note / clip / loop 标记（`src/app/UI/Views/Common/TimelineView.cpp:272-274`）
- `src/app/UI/Views/MainTitleBar/PlaybackView.cpp` 的 Bar:Beat:Tick 显示改用 `MusicTime`；新增跳转输入（依赖阶段 1 的 `timeToTick`）
- `src/app/Model/AppModel/AppModel.cpp:169-172` 的 `length = ticksPerWholeNote * numerator / denominator * bars` 改走 timeline

### 验收

- [ ] 在 3/4 段内拖动音符，吸附点落在该段的小节线/拍线上，跨段拖动时基准正确切换
- [ ] Alt 关闭吸附仍然可用
- [ ] 输入 "015:2:000" 能跳转到正确位置，且跨拍号段计算正确
- [ ] 退化等价性：单拍号工程的吸附行为与改动前完全一致

---

## 阶段 6 — 拍号编辑 UI 与撤销 ⬜

**目标**：可在标尺上插入/编辑/删除拍号点。

### 改动

交互设计**照抄 diffscope**：

- **右键菜单双态**（参考 `src/plugins/visualeditor/qml/TimelineContextMenuHelper.qml`）：按 `nearestBarWithTimeSignatureTo(measure) == measure` 判断该小节是否已有拍号点，分别弹"插入"或"编辑/删除"菜单；菜单首项是禁用的当前 MusicTime 字符串作上下文标题；**删除项在 `measure == 0` 时置灰**
- **编辑对话框**（参考 `src/plugins/coreplugin/qml/dialogs/EditTimeSignatureDialog.qml`）：分子 SpinBox + **分母下拉只给 2 的幂**（从 UI 根除非法值）+ 常用拍号快捷按钮（4/4、2/4、3/4、6/8）+ Bar:Beat:Tick 位置输入 + "修改已有 / 插入新的" 单选
- **撤销 Action 只做三类**：插入 / 删除 / 改值（diffscope 也没有"移动"）。移动实现为"删除+插入"的复合 `ActionSequence`。扩展 `src/app/Controller/Actions/AppModel/TimeSignature/TimeSignatureActions.h`
- 拍号点**自动按小节吸附**——点击位置经 `MusicTime(tick).measure()` 转换即可，无需额外吸附逻辑
- `src/app/UI/Views/MainTitleBar/TimeSignaturePopupWidget.cpp` 从"改全局"变为"显示/编辑播放头所在段"（参考 `src/plugins/coreplugin/internal/addon/TimelineAddOn.cpp:114-117`）
- 同位置多点容错：保留第一个、删除其余并 warn

### 验收

- [ ] 插入 → 撤销 → 重做 → 删除 → 撤销，序列与网格每步都正确
- [ ] 尝试删除小节 0 的拍号点：菜单项置灰
- [ ] 输入非法分母：UI 上不可选
- [ ] 改拍号后音符 tick 不变（相对小节线漂移），符合既定语义
- [ ] 播放头移动时标题栏拍号显示跟随变化

---

# 曲速线

## 阶段 7 — 音频引擎与波形 ⬜

**目标**：多曲速下播放、循环、导出、波形全部正确。

### 改动

**核心是"补偿三元组"**。talcs 的 `src/dspx/DspxAudioClipContext.cpp:131-139` 中 `updatePosition()` 计算：

```
readOffset = convertTime(clipStartTick)                 // 素材内读取偏移
pos        = convertTime(startTick + clipStartTick)
len        = convertTime(startTick + clipStartTick + clipLenTick) - pos
```

其中 `readOffset` 把"素材内偏移"当成"工程时间轴上从 0 到该 tick 的时长"，变速下错。由于 `convertTime` 是 lite 自己装的单调可逆双射 *f*，且 `startTick` **从不单独参与换算**，lite 可以喂补偿值解决，不改 talcs：

```
clipStart' = f⁻¹(T · sr)              // T = 素材内修剪偏移（实时秒）
start'     = P − clipStart'           // P = clip 在时间轴上的位置（tick）
clipLen'   = f⁻¹( f(P) + L · sr ) − P // L = 播放时长（实时秒）
```

- `start'` 可能为负：已确认安全（talcs `setStart()` 无校验，且 `startTick` 单独从不进 `convertTime`）
- `src/app/Model/AppModel/AudioClip.h` 新增实时字段（`trimStartMs` / `lengthMs`）作为真相，`src/app/Model/AppModel/Clip.h:72-74` 的 tick 字段降为 UI 派生缓存，timeline 变化时重算

其余改动：

- `src/app/Modules/Audio/AudioContext.cpp:62-71` 的 `tickToSample` / `sampleToTick` 走 tempo map（后者需反查表）
- `src/app/Modules/Audio/AudioContext.cpp:494-502` 的 `handleTimeChanged()` 与 `src/app/Modules/Audio/TrackSynthesizer.cpp:41` 的同名函数，两条链路都重算补偿三元组
- 波形分段绘制：`src/app/UI/Views/TrackEditor/GraphicsItem/AudioClipView.cpp:166`、`:317`、`:435` 与 `src/app/UI/Utils/WaveformPainter.cpp:72` 共 5 处单一 `samplesPerTick` 改为按曲速段分段
- 循环范围、播放头位置在 timeline 变化后重算

### 验收

- [ ] **退化等价性（最强判据）**：把单曲速工程表达成两个同值曲速点，导出音频与单点版本**逐比特相同**
- [ ] 音频 clip 跨曲速变化点：素材不被拉伸，起点对齐正确，波形绘制无错位
- [ ] 播放中改曲速：位置连续、无爆音、循环范围正确
- [ ] 导出与实时播放结果一致

---

## 阶段 8 — 参数曲线非均匀重采样 ⬜ ★最易静默出错

**目标**：多曲速下推理参数曲线不漂移。

### 问题

参数曲线以 **5-tick 网格**存储（`src/app/Model/AppModel/DrawCurve.h:25` 的 `step = 5`），引擎工作在 **0.01s 帧网格**。两个方向各自"统一单位后做等间隔重采样"：

- **入方向** `src/app/Modules/Inference/Tasks/InferAcousticTask.cpp:357-399`：`resample(values, 5 /*tick*/, timeline.secToTick(0.01))` — 统一到 tick
- **出方向** `src/app/Modules/Inference/Tasks/InferPitchTask.cpp:291-292`：`resample(values, interval /*秒*/, timeline.tickToSec(5))` — 统一到秒

两者都依赖 `secToTick(0.01)` / `tickToSec(5)` 是**常数**。变速下 tick 网格与秒网格是分段线性、整体非线性的映射，`src/libs/Support/MathUtils.h:61-93` 的 `resample` 的"等间隔源→等间隔目标"契约表达不了。

### 改动

1. `MathUtils` 新增按**目标点位置数组**采样的重载，并改为**指定输出点数**而非指定间隔（见下）
2. 改造 17 处调用：`InferAcousticTask.cpp:367-399` 9 处、`InferVarianceTask.cpp:281-331` 7 处、`InferPitchTask.cpp:271` 2 处、`src/app/Modules/Extractors/ExtractPitchTask.cpp:222-227` 1 处
3. **传递绝对起始 tick**：现在这些函数只有局部序列，改造后需知道 piece 在工程时间轴上的绝对起点。`InferPitchInput` / `InferAcousticInput` / `InferVarianceInput` 与 `src/app/Modules/Extractors/ExtractTask.h:23` 的 `Input`（现在按值传 `double tempo`）都要带上 timeline 快照 + 绝对起点
4. **帧数对齐**：入方向 `frames = qRound(totalLength/interval)` 且 `retake.end = frames`，而 `resample` 的输出点数是算出来的（还带 `break` 提前退出）。变速下极易差 1–2 点导致引擎侧错位——改为让 `frames` 成为单一真相
5. **尾部截断**：`if (oldIndex >= numOldSamples - 1) break;` 会静默丢尾部；变速下截断位置会漂移，需明确定义边界外插值行为

**不用改**：`src/app/Modules/Inference/Utils/InferTaskHelper.cpp:94-139` 的 note 秒长度计算是逐 note 绝对换算再取差，变速下天然正确。

### 验收

新增测试目标 `src/tests/TestParamResample`（纯计算，无需引擎）：

- [ ] **退化等价性**：两个同值曲速点的 timeline，往返重采样结果与单点版本逐值相同
- [ ] 真实变速：曲线 → 引擎网格 → 曲线往返后，**首尾与中间关键点的时间位置不漂移**（这是本阶段的核心判据）
- [ ] 输出点数严格等于 `frames`
- [ ] 边界：空序列、单点序列、曲速点正好落在 piece 起点/终点上
- [ ] 集成验收：在变速工程上跑一次完整推理，音高曲线与音符位置对齐、无累积偏移

---

## 阶段 9 — 曲速编辑 UI 与撤销 ⬜

**目标**：可在标尺上插入/编辑/删除曲速点。

### 改动

结构与阶段 6 同构（曲速点锚在 tick 而非小节，可自由拖动）：

- 右键菜单双态（按 `nearestTickWithTempoTo(tick) == tick` 判定）
- 编辑对话框（参考 diffscope `src/plugins/coreplugin/qml/dialogs/EditTempoDialog.qml`）
- 撤销 Action 三类：扩展 `src/app/Controller/Actions/AppModel/Tempo/TempoActions.h` / `EditTempoAction.cpp`（现在只是 `setTempo(old/new)` 二元操作）
- `src/app/UI/Views/MainTitleBar/TempoComboBox.cpp` / `TempoPopupWidget.cpp` / `src/app/UI/Controls/TapTempoButton.cpp` 改为作用于播放头所在段
- 曲速变化时的推理失效：保留 `src/app/Model/AppModel/AppModel.cpp:46-56` 中 `setTempo` 里的 `bumpInferenceRevision()`，但优化为只失效受影响 tick 范围内的 piece

### 验收

- [ ] 插入 → 撤销 → 重做 → 删除 → 撤销，播放位置与音频每步正确
- [ ] 删除 tick 0 的曲速点：置灰
- [ ] 拖动曲速点：音频实时跟随，推理 piece 正确失效重推
- [ ] 与阶段 7/8 的联合验收：变速工程完整走一遍 打开→编辑→推理→播放→导出

---

## 阶段 10 — 打磨 ⬜

- [ ] 边界 case：同位置多点、极端曲速值、极端拍号、大量曲速点时的绘制性能（缓存命中率）
- [ ] i18n：`src/app/Resources/translate/translation_zh_CN.ts` 有 13 处相关条目，新增菜单/对话框文案需补齐
- [ ] 清理三处历史 TODO：`src/app/Model/AppModel/Timeline.cpp:9`、`src/app/Model/AppModel/SingingClip.cpp:107`、`src/app/UI/Views/Common/TimelineView.cpp:381`
- [ ] （可选）补齐 diffscope 也没做完的两处近似：标尺抽稀层级按 4/4 硬算、拍线可见性假设 beat = 四分音符

---

## 构建与测试

```powershell
# 进 VS 开发者环境与构建必须在同一条命令里（env 不跨调用持久）
$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community"
Import-Module (Join-Path $vs "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
Set-Location E:\GitHub\ds-editor-lite\build\Debug
ninja DsEditorLite; if ($?) { ctest --output-on-failure }
```

`debug` preset 已带 `LITE_BUILD_TESTS=ON`。新测试目标在 `src/tests/CMakeLists.txt` 用 `add_subdirectory` 注册，CMakeLists 照 `src/tests/TestSpeakerMixValidation/CMakeLists.txt` 的 `lite_add_test(...)` 模式直接编译 app 源码，main.cpp 用 `QCoreApplication` + 手写 `expect()` 断言。
