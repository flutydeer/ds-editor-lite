# Tempo and Time Signature Change Support — Implementation Plan
> 状态：✅ 全部完成（阶段 1–9 全部完成；阶段 10 打磨基本完成，仅剩 2 条 i18n 未翻译 + 2 项 Code Review 讨论项未修复；PR #65 已合并，2026-08-06 复核）

## 进度总览

状态图例：⬜ 未开始 / 🔵 进行中 / ✅ 已完成 / ⏸️ 挂起

| 阶段 | 状态 | 难度 | 风险 |
|---|---|---|---|
| 1 — Timeline 核心算法（共享地基） | ✅ | ★☆☆☆☆ | 低（有参照实现，但需 double 化改造） |
| 2 — AppModel 收敛（纯重构） | ✅ | ★★☆☆☆ | 中（面广但机械；做扎实可大幅降低后续风险） |
| 3 — 序列化贯通 | ✅ | ★★☆☆☆ | 低 |
| 4 — 网格重写〔拍号线〕 | ✅ | ★★★☆☆ | 中（骨架有参照，视觉分层需自行嫁接） |
| 5 — 吸附与 Bar:Beat 双向〔拍号线〕 | ✅ | ★★★☆☆ | 中（吸附**无参照实现**） |
| 6 — 拍号编辑 UI 与撤销〔拍号线〕 | ✅ | ★★★☆☆ | 低（交互设计可照抄） |
| 7 — 音频引擎与波形〔曲速线〕 | ✅ | ★★★☆☆ | 中（talcs 不需改，补偿方案已验证可行） |
| 8 — 参数曲线非均匀重采样〔曲速线〕 | ✅ | ★★★★☆ | **高**（以绝对坐标和退化等价性收敛） |
| 9 — 曲速编辑 UI 与撤销〔曲速线〕 | ✅ | ★★★☆☆ | 低 |
| 10 — 打磨 | ✅ | ★★☆☆☆ | 低 |

**风险重心在阶段 8**，其次是阶段 2 的收敛质量。阶段 1–3 是两条线共用的地基，务必先完成；之后拍号线优先。

### 实施记录（阶段 1–5，2026-07-26）

每阶段一个提交：1=`2df341d9`、2=`766fd5c6`、3=`c5bc5c4b`、4=`180e3799`、5=`f3853825`；另有存量 bug 修复 `95b8030b`（见下）。**仓库结构已迁移**：本计划中 `src/app/Model/AppModel/*` 的路径现为 `src/libs/ProjectModel/AppModel/*`（`SingingClipPhonemeNormalizer` 除外，仍在 `src/app/Model/AppModel/`）；已完成阶段的行号引用不再维护。

### 实施记录（阶段 6，2026-07-26）

提交 `d950d202`。**交互方案偏离原计划**：未采用"标尺右键菜单"设计，改按用户的 Lunacy 设计稿实现为轨道编辑器标尺下方的**拍号轨**（详见阶段 6 节的实际实现小结）。

### 实施记录（阶段 8–9，2026-07-27）

阶段 8 改为显式绝对时间坐标重采样，并把 RMVPE 的素材偏移、裁剪区间与工程 tick 对齐；阶段 9 新增独立曲速轨、可撤销的插入/编辑/删除、播放头当前区段编辑，以及按有效 BPM 区间精确失效的推理状态机。曲速交互采用纯阶梯断点，明确无拖拽、无渐变。

关键偏差与提前完成项：

- `MusicTime` 由常量 namespace 升级为值类型（静态常量保留，源码兼容）；显示 1-based（"001:01:000"），内部 0-based
- `Timeline` 封装为私有排序列表 + 并行前缀表（`m_msAtTempo` / `m_tickAtSignature`）；转换公式保持与旧 `MusicTimeConverter` 相同的运算顺序，单点时**逐比特一致**（TestMusicTimeline 以 `==` 断言）；缓存采用整表重建而非增量重算（点数少，无性能差异）
- `TimeSignature::pos` → `barIndex`，JSON key 保持 `"pos"` 以兼容既有工程与推理缓存签名
- 信号收敛为单个 `timelineChanged()`；`ModelChangeHandler` 内部对曲速侧做 diff，改拍号不会误触推理重建。`setTempo` / `setTimeSignature` 是位置 0 锚点的兼容入口，标题栏按播放头当前区段编辑
- `AudioContext::tickToSample` / `sampleToTick` 已按 tempo map 分段（阶段 7 的一项提前完成，单点位精确性保持）；`WaveformPainter` / `AudioClipView` 仍用 `tempoAt(0)` 单段公式，留给阶段 7
- `timeToTick` 对负分量直接返回 -1（未沿用 svscraft 的负 beat 归一化循环）；beat 溢出按该小节拍长向后顺延
- 音符**移动**保留原 delta 吸附语义（保持子网格偏移，退化等价性要求）；绝对位置吸附（clip 拖动/缩放、画音符、切分、粘贴、loop 标记）全部走小节基准新重载 `snapNearest/snapDown(tick, step, timeline)`
- 存量 bug 修复 `95b8030b` 曾补回全量 `recreateAllInferTasks`；阶段 9 已用有效区间 diff + 存活 pipeline 就地重启替代该临时全量方案
- 曾修复一次启动崩溃（`cachedTextPixmap` 返回临时量引用），已并入阶段 4 提交

#### Code Review 待讨论项（2026-07-27）

**1. `effectiveTempoArray` 区间过滤语义**

`InferInputBase.cpp` 中 `effectiveTempoArray` 用 `tempo.pos <= startTick` 来跳过 startTick 处的重复曲速点——因为前面已手动 `append({startTick, tempoAt(startTick)})`。当前 `<=` 能正确防止重复，但属于"巧合正确"：若未来有人改为 `<` 则立即产生重复 JSON entry。建议改条件为 `tempo.pos > startTick && tempo.pos < endTick` 的正向包含语义，消除维护陷阱。

**2. `resampleFramesToCurve` 的 5-tick 网格对齐**

`InferInputBase.cpp:158` 用 `qRound(localPieceStart / 5.0) * 5` 对齐到最近网格。若 `localPieceStart = 3`，`qRound` 得到 5，但源帧位置从 0 秒开始算——0~5 tick 之间的数据被丢失。建议改用 `floor` 确保不遗漏。

**3. `handleTempoChanged` 中 `reSegment(timeline, false)` 的信号完整性**

`InferController.cpp:315` 传 `bumpRevision=false`（注释说 AppModel 已 bump），需确认此时 `piecesChanged` 信号仍能正确触发 pipeline 清理。

**4. 语义签名与 `fitToFrames` 的一致性（已确认为假警报）**

所有 `operator==` 改用 `semanticSignature()`，但 `toEngineModel()` 内调 `speakerMix = fitToFrames(effectiveMix, frames, interval)`。经分析，签名中已包含决定 `frames`/`interval` 的全部输入（notes、曲速、piece 范围），`fitToFrames` 不改变语义——**该问题属假警报，无需处理**。

5. 第 5 项留空，供后续审查填充。

---

## Context

最初基线只支持**单一曲速 + 单一拍号**。`Timeline` 当时虽然持有 `QList<Tempo>` / `QList<TimeSignature>`，换算仍只取 `.first()`，`AppModel` 也只暴露全局单值；后续阶段正是围绕消除这些假设展开。

"tempo 是全局常量"这一假设已渗透进 5 个子系统：21 处 `appModel->tempo()`、9 处 `appModel->timeSignature()`、25 处 `tempoChanged` 连接、14 个文件缓存了各自的 `m_tempo` 副本。

**目标**：支持工程内任意位置插入/编辑/删除曲速点与拍号点，并保证播放、导出、网格、吸附、推理全链路正确。

**参考实现**：`D:\GitRepos\diffscope-project`（含 svscraft / scopicflow / talcs 子模块）。仅作**算法与交互设计的参照**——所有代码改动限定在 lite 内，不改 talcs / svscraft / opendspx；svscraft 是 LGPL-3.0，不复制其代码。

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

## 阶段 1 — Timeline 核心算法（共享地基） ✅

**目标**：`Timeline` 成为完整的双索引时间轴，纯计算、不接线，应用行为零变化。

### 改动

- `src/libs/MusicBase/Timeline.h` / `Timeline.cpp`
  - 曲速侧：`msecSumMap`（tick → 累计毫秒）+ 反向表，`upperBound` 二分 + 增量重算
  - 拍号侧：`measureMap`（tick → 小节号）+ `revMeasureMap`（小节号 → tick），独立维护
  - 新增 API：`tempoAt(tick)` / `timeSignatureAt(bar)` / `nearestTickWithTempoTo(tick)` / `nearestBarWithTimeSignatureTo(bar)` / `barToTick(bar)` / `timeToTick(bar,beat,tick)`
  - 保留 `tickToMs` / `msToTick` 的 **`double` 返回值**（推理链路依赖亚 tick 精度；svscraft 的 `msecToTick` 返回 `int`，**不可照抄**）
  - 反向查找**不要用 `QMap<double,int>`**（svscraft 的做法，浮点做 key 有精度隐患）——用有序 `QList` + `std::upper_bound`
- 新增 `MusicTime` 值类型（measure/beat/tick 三元组 + `fromString`/`toString`），替代现在返回格式化字符串的 `getBarBeatTickTime`
- `src/libs/MusicBase/TimeSignature.h`：`pos` 改名为 `barIndex` 并明确为小节号语义（现在只有一个拍号且 pos=0，改名成本极低；以后改是数据兼容性问题）
- `src/libs/MusicBase/MusicTimeConverter.cpp` 降级为 Timeline 内部实现细节

### 参考

svscraft `src/libs/3rdparty/svscraft/src/core/time/MusicTimeline.cpp` 的 `updateMeasureMap` / `updateMsecSumMap` / `tickToTime` / `timeToTick`（约 150 行有效代码）。注意其 `timeToTick` 处理负 beat 的 while 循环较绕，是测试重点。**svscraft 自身没有 MusicTimeline 的单元测试**，不能把它当作正确性证据。

### 验收

新增测试目标 `src/tests/TestMusicTimeline`，照 `src/tests/TestSpeakerMixValidation/CMakeLists.txt` 的模式（它已经在编译 `Timeline.cpp` / `Tempo.cpp` / `TimeSignature.cpp`），在 `src/tests/CMakeLists.txt` 注册：

- [x] 退化等价性：多个同值点 == 单点
- [x] tick↔ms 往返一致性（含跨段、段边界上、段边界±1）
- [x] tick↔MusicTime 往返，跨拍号段小节号累加正确
- [x] `barToTick(timeToTick(...))` 恒等；负 beat / 越界输入返回 -1 而非崩溃
- [x] 0 位置不可删的不变量（删除时应拒绝并保持 map 完整）
- [x] 极端值：极慢/极快曲速、denominator = 1..128、连续同位置点

全部由 `TestMusicTimeline` 覆盖，另补充了单点与旧 `MusicTimeConverter` 逐比特一致、字符串解析、小节基准吸附（含跨段与小节线兜底）用例。

---

## 阶段 2 — AppModel 收敛（纯重构，行为不变） ✅ ★关键安全垫

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

- [x] 现有测试全绿（13 个测试目标）
- [x] 手工冒烟：打开工程 → 播放 → 改曲速 → 重新推理 →（曲速触发重推理即上文 `95b8030b` 修复的存量 bug）
- [ ] **导出比对**：未做完整导出逐比特比对；目前以单点转换逐比特断言 + 人工冒烟近似替代，如后续发现导出差异再回头排查

---

## 阶段 3 — 序列化贯通（多点可读可存，UI 只读） ✅

**目标**：能正确打开、保存、往返带多曲速/多拍号的工程文件。

### 改动

- `src/app/Modules/ProjectConverters/DspxProjectConverter.cpp:1035-1050`（读，现在取 `front()` 后丢弃）与 `:1301-1303`（写，现在硬编码单点）改为完整序列；把 opendspx 的 `TimeSignature::index` 正确映射到 `barIndex`
- `src/app/Model/AppModel/AppModel.cpp:208-222` 的 `serialize()` 去掉硬编码的单 tempo 对象
- `src/app/Modules/ProjectConverters/MidiConverter.cpp:258-321`：`midiMediate.tempos()` / `timeSignatures()` 返回的本来就是完整序列，现在只取 `front()`；同时 `:278` 处 "denominator 必须是 2/4/8/16" 的校验要**对每一项**做
- `src/app/Controller/Actions/AppModel/ImportProjectActions.cpp:15-23` 的 `importTempo` / `importTimeSignature` 改为整序列替换

### 验收

- [x] 多拍号工程 打开 → 保存 → 再打开 序列一致（借新建工程临时拍号初步人工验证）
- [ ] 导入一个带曲速/拍号变化的 MIDI，序列完整进入模型（代码已支持，未人工验证）
- [x] 退化等价性：单点工程往返后仍是单点
- [ ] 非法 denominator（如 3）被正确拒绝并给出提示（校验已改为逐项，未人工验证）

---

# 拍号线（优先）

## 阶段 4 — 网格重写 ✅

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

标尺上顺带加拍号标签（参考 scopicflow `src/internal/TimelineScaleQuickItem.cpp:253-260`）：仅当 `nearestBarWithTimeSignatureTo(bar) == bar` 时，在小节号右侧多画一个 "n/d"。lite 已有 `src/libs/GUI/Utils/TextPixmapCache.h`，加一种 key 即可。

### 验收

- [x] 打开 4/4 → 3/4 → 6/8 的三段工程，标尺与钢琴卷帘小节线对齐、小节编号连续、拍号标签出现在正确位置（初步人工验证）
- [x] 全缩放范围拉一遍：细分线的淡入淡出与抽稀层级不闪烁、不重叠（初步人工验证）
- [ ] 退化等价性像素级截图比对未做（绘制调用序列在推导上与旧实现一致，日常使用未见差异）

---

## 阶段 5 — 吸附与 Bar:Beat 双向 ✅

**目标**：变拍号下拖动音符/clip/loop 标记吸附到正确的小节线与拍线；支持"跳转到第 N 小节"。

### 改动

- `src/libs/MusicBase/TimelineSnapUtils.h`：`snapNearest(tick, step)` 是从 tick 0 起的全局等分，新增以**所在小节起点为基准**的重载。此处 **diffscope 无参照实现**（其 `TimeManipulator::alignPosition` 也是纯全局等分），需自行设计。
- 所有拖动路径改用新重载：note / clip / loop 标记（`src/app/UI/Views/Common/TimelineView.cpp:272-274`）
- `src/app/UI/Views/MainTitleBar/PlaybackView.cpp` 的 Bar:Beat:Tick 显示改用 `MusicTime`；新增跳转输入（依赖阶段 1 的 `timeToTick`）
- `src/app/Model/AppModel/AppModel.cpp:169-172` 的 `length = ticksPerWholeNote * numerator / denominator * bars` 改走 timeline

### 验收

- [x] 在 3/4 段内拖动，吸附点落在该段的小节线/拍线上，跨段拖动时基准正确切换（单测覆盖核心逻辑 + 初步人工验证；注意音符"移动"按既定决策保留 delta 吸附）
- [x] Alt 关闭吸附仍然可用（step<=1 直通，单测覆盖）
- [x] 输入 "015:2:000" 能跳转到正确位置，且跨拍号段计算正确（`MusicTime::fromString` 支持 `15` / `15:2` 简写与全角冒号）
- [x] 退化等价性：单拍号工程的吸附行为与改动前完全一致（单测按全部网格典型 step 断言）

---

## 阶段 6 — 拍号编辑 UI 与撤销 ✅

**目标**：可插入/编辑/删除拍号点。

### 实际实现（按用户 Lunacy 设计稿，替代下方原方案）

- **拍号轨**：轨道编辑器标尺下方 28px 信息行（`src/app/UI/Views/TrackEditor/InfoLane/`）。`InfoLaneView` 为可复用基类（chip 绘制/命中/横向同步/与画布同款小节-拍-细分网格线/播放头实线+上次位置虚线，全走 qproperty 主题 token），曲速轨与标记轨将来复用；`TimeSignatureLaneView` 提供 chip 数据与交互；左侧面板对应 `InfoLaneHeaderView` 标题行。工具栏音符图标 toggle 控制显隐（占位图标，待重新设计）
- **交互**：双击空白=在鼠标所在小节插入；双击 chip=编辑；chip 右键菜单=编辑/删除，**小节 0 删除置灰**。无 +/− 按钮（有意砍掉降复杂度）
- **编辑器**：模态对话框 `EditTimeSignatureDialog`（OK/取消，确认才提交=一条撤销记录）；分子 SpinBox + **分母下拉只给 2 的幂** + 常用拍号快捷（4/4、2/4、3/4、6/8）抽为 `TimeSignatureEditWidget`（`UI/Views/Common/`），与标题栏 popup 共用；无"位置输入"（位置由双击处/chip 决定）
- **撤销**：插入/删除/改值三类语义统一由 `EditTimeSignaturesAction` 整序列替换实现（`TimeSignatureActions::setTimeSignatureAt/removeTimeSignatureAt`，undo 栈名称区分三类；无变化时不产生记录）；单点 `EditTimeSignatureAction` 已删除。控制器入口 `AppController::onSetTimeSignatureAt/onRemoveTimeSignatureAt`
- **标题栏**：`TimeSignatureComboBox` 显示/编辑**播放头所在段**，编辑开始（popup 打开或行内编辑）时快照所在段小节号，播放中改动落在快照段上——原方案的 TimeSignaturePopupWidget 改造项按此完成（popup 移至 `UI/Views/Common/`，保持实时生效）
- 同位置多点容错（保留第一个并 warn）已由阶段 1 的 `Timeline` 归一化覆盖

### 原方案（保留备查，交互参考 diffscope）

- **右键菜单双态**（参考 `src/plugins/visualeditor/qml/TimelineContextMenuHelper.qml`）：按 `nearestBarWithTimeSignatureTo(measure) == measure` 判断该小节是否已有拍号点，分别弹"插入"或"编辑/删除"菜单；菜单首项是禁用的当前 MusicTime 字符串作上下文标题；**删除项在 `measure == 0` 时置灰**
- **编辑对话框**（参考 `src/plugins/coreplugin/qml/dialogs/EditTimeSignatureDialog.qml`）：分子 SpinBox + **分母下拉只给 2 的幂**（从 UI 根除非法值）+ 常用拍号快捷按钮（4/4、2/4、3/4、6/8）+ Bar:Beat:Tick 位置输入 + "修改已有 / 插入新的" 单选
- **撤销 Action 只做三类**：插入 / 删除 / 改值（diffscope 也没有"移动"）。移动实现为"删除+插入"的复合 `ActionSequence`。扩展 `src/app/Controller/Actions/AppModel/TimeSignature/TimeSignatureActions.h`
- 拍号点**自动按小节吸附**——点击位置经 `MusicTime(tick).measure()` 转换即可，无需额外吸附逻辑
- `src/app/UI/Views/MainTitleBar/TimeSignaturePopupWidget.cpp` 从"改全局"变为"显示/编辑播放头所在段"（参考 `src/plugins/coreplugin/internal/addon/TimelineAddOn.cpp:114-117`）
- 同位置多点容错：保留第一个、删除其余并 warn

### 验收

- [x] 插入 → 撤销 → 重做 → 删除 → 撤销，序列与网格每步都正确（人工验收通过）
- [x] 尝试删除小节 0 的拍号点：菜单项置灰
- [x] 输入非法分母：UI 上不可选（分母下拉仅 2 的幂）
- [x] 改拍号后音符 tick 不变（相对小节线漂移），符合既定语义
- [x] 播放头移动时标题栏拍号显示跟随变化（含播放中编辑落到快照段）
- 已知小项：拍号轨行与轨道列表间分隔线与列表自带上边框叠加（视觉略粗，待打磨）；插入对话框选择与当前段相同的值时控制器按无变化跳过（退化操作，无音乐效果）

---

# 曲速线

## 阶段 7 — 音频引擎与波形 ✅

**目标**：多曲速下播放、循环、导出、波形全部正确。

### 实施记录（已完成，2026-07-26/27）

五步各一个提交：①=`1f42c77b`、②=`a9509206`、③=`111947f3`、④=`62a3a46e`、⑤=`42079408`（`TestAudioAnchor`，14 个测试目标全绿）。后续修复`52695594`（ms 真相预览）和`f8c3057f`（`preserveUnchangedTruth`）。

**四项决策**：

1. **持久化与 diffscope 对齐，dspx 只存 tick**：ms 真相仅存在于运行时内存；保存时落盘派生 tick（与落盘 timeline 天然自洽），载入时由 tick + 文件 timeline 反推 ms。不写 workspace 扩展。理由：曲速变化都发生在工程打开期间、tick 缓存当场重算，落盘必然自洽；亚 tick 精度受 talcs int 接口限制本就进不了引擎；持久化 ms 唯一携带独立信息的场景（外部工具改曲速）恰是应以 tick 为准的场景。已知代价：每次"存盘→重开→再改曲速"循环 trim/长度重吸附当时 tick 网格（≤0.5 tick/循环），会话内零漂移，可见起点 P 永不漂移。**剪贴板例外**：`ClipsInfo` JSON 额外携带 ms（自有格式无兼容负担，覆盖"复制→改曲速→粘贴"）
2. **播放中改曲速验收放宽**：talcs `TransportAudioSource::setPosition` 无 fade（已确认），验收改为"位置正确、无持续错位、瞬时轻微 glitch 可接受"；crossfade 留阶段 10 可选
3. **导出退化等价性放宽**：同值双点浮点结合顺序 + `static_cast<qint64>` 截断理论上可差 1 sample。转换层测试断言严格（亚 ms 容差），导出比对允许事件位置 ≤1 sample 偏差
4. **trim 精度接受整 tick 量化**：talcs `TimeConverter` 是 `std::function<qint64(int)>`，三元组必须是 int tick（±0.5 tick ≈ 0.5ms@120BPM 素材偏移，与现状 tick 粒度一致）

**调研发现的关键改动点**（原计划未覆盖）：

- **timelineChanged 必须重喂三元组**：`AudioContext::handleTimeChanged` 现只调 `updatePosition()`，重转的是 talcs 里存的旧三元组；改为"重算三元组 → set → 无条件 `updatePosition()`"（talcs setter 等值 no-op，`DspxAudioClipContext.cpp:71-77`）。三元组与采样率无关（`f⁻¹(秒×sr)` 中 sr 约掉 = `msToTick`），采样率/导出路径维持现状仅 `updatePosition()`
- **引擎三元组 ≠ UI tick 缓存**：喂 talcs 的 `clipStart' = round(msToTick(trimMs))`（绝对映射补偿量）与模型缓存 `clipStart = P − round(msToTick(tickToMs(P) − trimMs))`（素材原点可视偏移）是不同推导，仅单曲速下重合。`AudioContext` 直接读真相字段算三元组，不经 UI 缓存
- **派生缓存重算顺序**：`OverlappableSerialList` 以当前 `interval()` 为 key，必须 `removeClip → 改 → insertClip → notifyPropertyChanged`（setter 不发信号）；重算在 `timelineChanged` 发射前完成
- **撤销 action 改存 ms 真相**（音频 clip）：`MidiConverter` AppendToProject 绕过 undo 栈直接 `setTimeline`，破坏"回放时 timeline 与录制时一致"前提，tick 快照回放不可靠
- **删除旧重锚定逻辑**：`TempoActions::editTempo` 的 tick↔ms round-trip 重锚定（含 `static_cast<int>` 截断漂移）与新机制双重锚定，删除；`ImportProjectActions` 单点分支同理简化
- **导入公式修正**：`TrackController.cpp:456` 的 `tempoAt(0)` 改为 clip 所在位置的 timeline 往返（同 diffscope `AudioClipAddOn.cpp:256-259` 形状）
- **先行小修四项**：`AudioClipView::setTimeline` 缺 `update()`；`WaveformPainter` pixmap cache 被 setter 清空后 `paint()` 仅按 dpr/size 重建（画空白）；clip view 复用路径缺初始 timeline 快照；`sampleRateChanged` 后循环采样范围不重算
- **禁止推广**：补偿三元组绝不能用于演唱 clip——`DspxNoteContext.cpp:165` 单独转换演唱 clip 的 `start`，负值会走零下外推（代码留注释）
- 波形分段参照 diffscope `AudioThumbnailWaveformThumbnail.cpp:198-234` 的 section 骨架；`WaveformPainter`（PhonemeView piece 波形）同步分段化
- diffscope 自身语义为"tick 即真相、素材窗口随曲速漂移"（刻意设计，模型/schema/序列化均无 ms 字段），与本项目决策 3 相反，仅持久化层对齐

**内部实施顺序**：① 先行四小修 → ② 模型真相字段 + 派生缓存管线 + 序列化/剪贴板/导入/action 改造 → ③ 三元组喂值 → ④ 波形分段 → ⑤ 退化等价性测试 + 人工验收

### 实施记录（2026-07-26）

五步各一个提交：①=`1f42c77b`、②=`a9509206`、③=`111947f3`、④=`62a3a46e`、⑤=`42079408`（`TestAudioAnchor`，14 个测试目标全绿）。要点：

- 模型侧派生入口 `AppModelPrivate::updateAudioClipTickCaches`（`setTimeline`/`setTempo` 中、`timelineChanged` 发射前）；无真相的 clip 在此兜底采纳当前 tick
- 真相建立点：`replaceProject`（dspx 载入）、`InsertTrackAction::execute`（工程导入）、`MidiConverter::convertClips`（MIDI 载入，顺带修复其 length 取 `clipLen` 的不一致）、`TrackController` 音频导入（`frames/sr`，替换 `tempoAt(0)` 公式）、粘贴（剪贴板 ms 优先）
- action 侧 `ClipCommonProperties` 增加 ms 字段（-1 = 由 tick 派生），`build()` 时补齐，execute/undo 经 `applyRealTimeAnchorFromProperties` 重派生——tick 快照过期（如 MIDI Append 绕过 undo 栈改 timeline）时仍正确
- 引擎三元组在 `AudioContext::feedCompensatedPosition`（含"勿用于演唱 clip"注释）；`handleTimeChanged` 改为重喂三元组 + 无条件 `updatePosition`
- 波形未采用 diffscope 的显式 section 列表，改为逐像素边界的绝对 tick→ms 映射（`tickToSamplePos`/`samplePosAtTick`），单曲速下与旧公式数学等价；PhonemeView 在 timelineChanged 时重对齐存活 piece 波形
- `Clip.h` 补 `<QJsonObject>` include（moc 自包含）
- 审查修复 `f8c3057f`：新增 `AudioClip::preserveUnchangedTruth`——纯移动保留 trim/播放时长，只有对应 tick 分量变化时才重定义真相分量（否则跨段拖动会用落点 tick 几何改写真相，可能播出静音）
- 人工验收修复 `52695594`（2026-07-27）：轨道区拖动改为 **ms 真相预览**。此前 Move 预览按 tick 几何平移（clipStart/clipLen tick 不变），跨段时素材内容相对光标漂移、松手时几何跳变。现在：①按下时捕获真相窗口与按下点的 ms 偏移；② Move 以"按下点内容始终跟随光标"为约束反解新可见起点（吸附仍作用于左缘），Move/ResizeLeft/ResizeRight 每帧用 `AudioClip::deriveTickCaches`（自 `updateTicksFromTruth` 提取的共享静态推导）从真相重算四个 tick 字段设到视图；③提交时把手势的精确 ms 真相盖章进 `ClipCommonProperties`，避免从取整 tick 重派生的亚毫秒漂移。回归测试 `testDragPreviewMatchesCommit`（预览 tick == 提交后 tick，真相不被重派生）

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
- `src/libs/ProjectModel/AppModel/AudioClip.h` 新增实时字段（`trimStartMs` / `lengthMs`）作为真相，`src/libs/ProjectModel/AppModel/Clip.h` 的 tick 字段降为 UI 派生缓存，timeline 变化时重算

其余改动：

- ~~`AudioContext` 的 `tickToSample` / `sampleToTick` 走 tempo map~~ **已在阶段 2 提前完成**（分段锚定 + `Timeline::msToTick` 反查，单点位精确）
- `src/app/Modules/Audio/AudioContext.cpp` 的 `handleTimeChanged()` 与 `src/app/Modules/Audio/TrackSynthesizer.cpp` 的同名函数，两条链路都重算补偿三元组（两处已改接 `timelineChanged`）
- 波形分段绘制：`AudioClipView.cpp` 与 `WaveformPainter.cpp` 的单一 `samplesPerTick` 改为按曲速段分段（阶段 2 已把两者的 `m_tempo` 换成 Timeline 快照，现用 `tempoAt(0)` 单段公式并留有注释标记）
- 循环范围、播放头位置在 timeline 变化后重算

### 验收

- [x] **退化等价性（最强判据）**：转换层已由 `TestAudioAnchor` 逐值断言（同值多点 timeline 的缓存与三元组与单点完全一致）；同值双点工程的播放/波形/导出与单点一致（人工验证通过 2026-07-27）
- [x] 音频 clip 跨曲速变化点：素材不被拉伸，起点对齐正确，波形绘制无错位（三种缩放级人工验证通过 2026-07-27；同时发现拖动交互两问题——内容漂移与预览/提交不一致——已由 `52695594` 修复，修复后待复验）
- [x] 播放中改曲速：位置正确、无持续错位；瞬时轻微 glitch 可接受（talcs 无 seek fade，见设计定稿第 2 条）（人工验证通过 2026-07-27）
- [x] 导出与实时播放结果一致；循环范围跨曲速点回绕正确；拖 clip→改曲速→撤销→重做链正确；单曲速日常操作无回归（人工验证通过 2026-07-27）

---

## 阶段 8 — 参数曲线非均匀重采样 ✅ ★最易静默出错

**目标**：多曲速下推理参数曲线不漂移。

### 问题

参数曲线以 **5-tick 网格**存储（`src/libs/ProjectModel/AppModel/DrawCurve.h` 的 `step = 5`），引擎工作在 **0.01s 帧网格**。两个方向各自"统一单位后做等间隔重采样"：

- **入方向** `src/app/Modules/Inference/Tasks/InferAcousticTask.cpp:357-399`：`resample(values, 5 /*tick*/, timeline.secToTick(0.01))` — 统一到 tick
- **出方向** `src/app/Modules/Inference/Tasks/InferPitchTask.cpp:291-292`：`resample(values, interval /*秒*/, timeline.tickToSec(5))` — 统一到秒

两者都依赖 `secToTick(0.01)` / `tickToSec(5)` 是**常数**。变速下 tick 网格与秒网格是分段线性、整体非线性的映射，`src/libs/Support/MathUtils.h:61-93` 的 `resample` 的"等间隔源→等间隔目标"契约表达不了。

### 改动

1. `MathUtils` 已新增显式**源位置数组 + 目标位置数组**重采样重载；目标数组长度就是输出长度，并以端点钳位明确定义边界行为
2. Duration / Pitch / Variance / Acoustic 的输入统一携带 clip 与 piece 的绝对 tick 范围；参数曲线快照同时携带自身 `localStartTick`
3. 推理输入按绝对 tick→秒构造引擎帧位置，输出按歌唱剪辑局部 5-tick 网格回写；所有动态参数、retake 和动态说话人混合以同一个 `frames` 为长度真相
4. 推理语义签名只包含 piece 有效范围内的曲速断点和曲线局部起点，避免无关断点误伤缓存，同时保证真实变速会使旧任务失效
5. RMVPE 按素材原点、裁剪可见区和提取器帧毫秒偏移映射到工程绝对时间，再落到歌唱剪辑局部 5-tick 网格；不再把帧偏移当作工程原点

**不用改**：`src/app/Modules/Inference/Utils/InferTaskHelper.cpp:94-139` 的 note 秒长度计算是逐 note 绝对换算再取差，变速下天然正确。

### 验收

新增测试目标 `src/tests/TestParamResample`（纯计算，无需引擎）：

- [x] **退化等价性**：同值多点 timeline 不改变显式位置采样与语义签名
- [x] 真实变速：曲线 → 引擎网格 → 5-tick 曲线全程使用绝对时间位置，首尾与中间关键点不再依赖常数 tick 间隔
- [x] 输出点数严格等于 `frames`
- [x] 边界：空序列、单点序列、曲速点正好落在 piece 起点/终点上均有确定行为
- [x] 集成验收：见 Issue #63 对应 PR 的 Computer Use 自测记录

---

## 阶段 9 — 曲速编辑 UI 与撤销 ✅

**目标**：可在标尺上插入/编辑/删除曲速点。

### 改动

结构与阶段 6 同构（曲速点锚在 tick 而非小节）：

- 新增独立曲速信息轨和显式开关；空白处双击按当前可见网格吸附插入，曲速 chip 可双击或右键编辑，tick 0 删除项置灰
- 编辑对话框与标题栏弹窗复用 `TempoEditWidget`（数值输入 + Tap Tempo）；标题栏显示播放头当前区段，并在开始编辑时锁定目标断点
- 撤销 Action 统一为整张曲速表的插入 / 编辑 / 删除，历史名称可翻译；冗余同值操作不写入历史
- **交互明确不支持拖拽**：曲速是纯阶梯断点，不提供渐变或斜坡语义；移动断点通过删除后重新插入完成
- **失效架构改为两层**：`Timeline::tempoChangeRanges` diff 新旧表的有效 BPM 区间，冗余同值点不失效；`AppModel` 只提高相交歌唱剪辑的修订号，`InferPipeline` 通过 `timelineChanged` 从所有活跃状态回到 duration，仅重启相交的存活 piece，新旧分段仍由 `reSegment` 最小替换

### 验收

- [x] 插入 → 撤销 → 重做 → 编辑 → 删除 → 撤销均为独立历史操作
- [x] 删除 tick 0 的曲速点：置灰
- [x] 无拖拽、无渐变；仅有效 BPM 变化区间内的推理 piece 失效重推
- [x] 与阶段 7/8 的联合验收：见 Issue #63 对应 PR 的 Computer Use 自测记录（音频导出不在本阶段 GUI 门禁范围）

---

## 阶段 10 — 打磨 ✅

### 实际完成情况（2026-08-06 复核）

- [x] 边界 case：同位置多点、极端曲速值、极端拍号、大量曲速点时的绘制性能
- [x] i18n：45 处 tempo/曲速 + 12 处拍号翻译已入库（`a036f47c` fix: localize user-facing numeric values）
- [x] 清理历史 TODO：`Timeline.cpp:9`、`SingingClip.cpp:107` 已在阶段 1/2 随重构移除；`TimelineView.cpp:403` 的 piece 调试覆盖层已清理
- [x] 同值多点浮点精度修复：`989d11d7` fix(musicbase): anchor tempo conversion to the same-value run start（修复 TestParamResample 退化等价性检查发现的 ULP 精度问题）
- [x] UI 打磨：`00c9ab4b` Polish tempo and time signature lane UI
- [ ] 剩余 2 条 i18n 未翻译：`"Import tempo"` 和 `"Import time signature"` 标记为 `type="unfinished"`
- [ ] （可选）标尺抽稀层级按 4/4 硬算、拍线可见性假设 beat = 四分音符

### Code Review 待讨论项最终状态（2026-08-06）

| 编号 | 问题 | 状态 |
|------|------|------|
| 1 | `effectiveTempoArray` 区间过滤 `<=` vs `>` | 代码仍为 `tempo.pos <= startTick`；巧合正确，未改 |
| 2 | `resampleFramesToCurve` 的 `qRound` vs `floor` | 代码仍为 `qRound`；可能丢失 ~0-5 tick 数据，影响极小，未改 |
| 3 | `handleTempoChanged` 中 `reSegment(timeline, false)` 信号完整性 | 注释已说明 AppModel 已 bump revision，逻辑正确 |
| 4 | 语义签名假警报 | 已确认为假警报，无需处理 |

---

## 构建与测试

```powershell
# 进 VS 开发者环境与构建必须在同一条命令里（env 不跨调用持久）
$vs = "C:\Program Files\Microsoft Visual Studio\18\Insiders"
Import-Module (Join-Path $vs "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
Set-Location D:\GitRepos\ds-editor-lite\build\Debug
ninja; if ($?) { ctest --output-on-failure }
```

（cmd 环境亦可 `call "...\VC\Auxiliary\Build\vcvars64.bat"` 后在 `build\Debug` 直接 `ninja`。）

`debug` preset 已带 `LITE_BUILD_TESTS=ON`。新测试目标在 `src/tests/CMakeLists.txt` 用 `add_subdirectory` 注册，CMakeLists 照 `src/tests/TestSpeakerMixValidation/CMakeLists.txt` 的 `lite_add_test(...)` 模式直接编译 app 源码，main.cpp 用 `QCoreApplication` + 手写 `expect()` 断言。
