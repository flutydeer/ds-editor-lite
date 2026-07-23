# app 内部依赖解耦方案

> 目标:在**单个 `DsEditorLite` target 内部**把错误的层间依赖梳理干净,为后续物理抽库(`src/libs/` + sync_include)扫清障碍。
> 依赖解耦与物理抽库是正交的两件事——本方案只做前者,全程不动 CMake target 结构,不改包含前缀风格(仍用 `"Model/Xxx.h"` 相对写法)。

## 层级模型

约定合法方向为**低层不得 include 高层**,秩越小越底层:

| 秩 | 层 | 说明 |
|---|---|---|
| 0 | `Global` | 常量/枚举,零依赖叶子 |
| 1 | `Utils` / `Interface` | 通用工具 / 抽象接口(依赖倒置的 seam) |
| 2 | `Model` | 工程数据模型 |
| 3 | `Modules` | 业务子系统(Inference/Audio/History/…) |
| 4 | `Controller` | 控制器 |
| 5 | `UI` | 视图(业务层顶点,可依赖以下一切) |
| 6 | `Bootstrap` / `AppContext` / `main` | 顶层胶水 |

## 依赖矩阵(行 include 列的出现次数)

```
SRC\DST    Global Utils Iface Model Mods  Ctrl  UI
Utils        3     1     0    8❌   0     0    2❌
Interface    1    12     0    1❌   1❌   0     0
Model       10    30     9    21   15❌   0     0
Modules     10    58     2   104   197   12❌  20❌
Controller   9    18     4   119    64    11   13❌
UI          48    63    13   141    76    64   351
```

❌ = 违规边(低层 include 高层)。共 **8 条违规边、约 72 处 include**。UI 无违规(它是业务层顶点)。

## 一条决定顺序的定理

> **向下搬(把类型移到更低层)永不制造新违规,只会消解已有违规**——前提是被搬文件自身的依赖在目标层能被满足。
> **向上搬(把文件移到更高层)有风险**:原本合法的同层/低层 includer 会被"甩在下面",变成新的低→高违规。

因此执行顺序必须是:**先向下搬(A 类)→ 再处理向上搬簇(B 类)→ 最后做真正的倒置(C 类)**。

---

## A 类 — 类型下沉(安全,优先做)

**决策(已定):只下沉轻量身份类型 + 真叶子;富类型簇留在 PackageManager,列为独立设计议题。**

理由(代码勘察结论):`SingerInfo`/`SpeakerInfo` **不是**无行为的值类型,而是 **PackageManager 域的富类型**——
- 生产者是 PackageManager 自身(`PackageManager.cpp`、`PackageInfo.cpp`、`SingerInfo.cpp`);
- 携带 `ResolutionState`(声库解析运行期状态,注释明说不持久化)与 `SingerCapabilitySummary`(对推理库 synthrt `SingerCapabilityReport` 的镜像)。

把它们下沉到 Model 会让 Model 混入声库/推理概念,并使**生产者 PackageManager 反向依赖 Model**,进而诱发在 PackageManager 侧重造本地表示(真·多份)。故**不搬**。

### 实际下沉清单(剔除富类型簇后)

| 待搬文件 | 自身依赖 | 波及面 | 落点 | 备注 |
|---|---|---|---|---|
| `Modules/Inference/Models/SingerIdentifier.h` | 纯 Qt/std(叶子) | 22 | `Model/AppModel/` | 轻量身份,Model/PackageManager 共享;PackageManager 反向向下依赖它,合法 |
| `Modules/Inference/Models/InferSpeakerMix.h` | 只依赖 `Model/AppModel/{SpeakerMixData,Timeline}` | 9 | `Model/AppModel/` | 已只依赖 Model,纯误放 |
| `Modules/History/HistoryFocus.h` | 纯 `<QList>`(叶子) | 7 | 待定(需 ≤ Interface 秩1) | 消解 `Interface→Modules` |

搬完后:`Model→Modules` 中由 `SingerIdentifier`/`InferSpeakerMix` 引起的边消解,`Interface→Modules` 清零。剩余 `Model→Modules`(`SingingClip→SingingClipSlicer`、`ClipsInfo→PackageManager`、以及 **Model 存储/序列化 `SingerInfo`/`SpeakerInfo`** 的边)归入下方设计议题。

### 独立设计议题(不在本轮解耦内):Model 持有 package 富类型

`SingingClip`/`Track`/`EffectiveVoiceContext` 用 `Property<SingerInfo>`/`SingerInfo m_singerInfo` **快照式持有并序列化**声库富信息。这是 Model 与 PackageManager 域的真实纠缠,**无法靠移动文件解决**,需专门设计:
- 方案 a:把 `SingerInfo` 拆为「可持久化值部分(可入 Model/共享层)」+「运行期能力/解析状态(留 PackageManager)」;
- 方案 b:Model 只存 `SingerIdentifier` + 轻量持久化快照,富字段运行期回查 PackageManager(改变离线/缺包行为,属产品决策)。

单列任务,先出设计再动手,不与本轮解耦混做。

---

## B 类 — 向上搬簇(有级联,需整簇处理)

这些"Utils"其实是 **Model 层辅助**,不是通用工具,应上移进 Model。但每个的 includer 里都**混着 1 个 Utils 同层文件**——单搬会制造 `Utils→Model` 新违规,说明这几个 Utils 文件是一个**互相引用的簇,必须一起上移**。

| 待搬文件 | 波及面 | includer 中的低层 | 落点 |
|---|---|---|---|
| `Utils/ParamUtils.{h,cpp}` | 5 | 1×Utils | `Model/` |
| `Utils/AppModelUtils.{h,cpp}` | 5 | 1×Utils | `Model/` |
| `Utils/DiffscopeAudioWorkspace.{h,cpp}` | 3 | 1×Utils | `Model/` |

> 动手前须先摸清这 3 个文件与"那 1 个 Utils 同层 includer"的引用闭包,确定整簇边界,再一次性上移。

`Utils/WindowFrameUtils.h`(UI 辅助误放 Utils,8 includer 含 2×Utils):同理,上移进 `UI/Utils/` 会甩下 2 个 Utils includer——需先确认那 2 个 Utils 文件是否也该跟着走,否则此项归入 C 类。

---

## C 类 — 真正的依赖倒置(硬骨头,三个 seam)

搬完 A/B 后剩下的才是真倒置,需接口/信号,**照抄项目现有 `Interface/IClipEditorView` 模式**。

### seam ① UI 弹窗 — 低层直接弹 Toast/Dialog

`Controller→UI`(13)+ `Modules→UI` 残余(~8):

| 源 | 弹的具体 UI |
|---|---|
| `Controller/{AppController,AudioDecodingController,ClipController,PlaybackController,TrackController}` | `Toast`、`LyricDialog`、`SearchDialog`、`AccentButton`、`AudioClipView`、`ThemeManager` |
| `Controller/DocumentWorkflow/DocumentWorkflowController` | `ProgressDialog` |
| `Modules/Extractors/{Midi,Pitch}ExtractController` | `Toast`、`TaskDialog`、`AccentButton` |
| `Modules/Audio/AudioContext` | `LevelMeterView/Manager` |
| `Modules/Audio/utils/SettingPagesSynthHelper` | `Svs*` 控件 |

**修法**:低层发信号或调注入的 `IXxxView` 接口;具体对话框/Toast 由 UI 层拥有。

### seam ② PlaybackController 访问

`Modules→Controller` 的 `PlaybackController.h`(4×:`Audio/AudioContext`、`Inference/InferController`、`Inference/States/{ProbeAcousticCache,UpdateVariance}`)。
**修法**:抽播放状态接口或信号回调,切断 Modules 对具体控制器直连。

### seam ③ ModelChangeHandler / Actions 访问

`Modules→Controller` 其余(`ModelChangeHandler.h`×3、`Actions/{TrackActions,ParamsActions}`、`AppController`、`DocumentWorkflowController`、`TrackController`)。
**修法**:模块不直接调控制器 Action;触发点上提到 Controller,或经接口下发。

### 伪装成搬迁、实为倒置的项

- `Modules/ProjectConverters/MidiConverterDialog.{h,cpp}`:对话框搬进 UI 只是把违规从"Dialog→Dialog.h"变成"MidiConverter.cpp→UI/…"。**本质是 `MidiConverter` 逻辑不该拥有并弹对话框** → seam ①。
- `Modules/FillLyric/Widgets/*`:整个是嵌在 Modules 里的一套 UI(依赖 `UI/Controls/ComboBox`、`ThemeManager`、`IconUtils`)。**FillLyric 的 Widgets 应整体归到 UI 层**,而非解耦。

### Interface 层残余

- `Interface/ITrack.h → Model/AppModel/TrackControl.h`:确认 `TrackControl` 是值类型→接口依赖 Model 值类型可接受;否则下沉。

---

## 执行顺序

```
1. A 类整簇下沉:{SingerIdentifier,SingerInfo,SpeakerInfo,LanguageInfo} → Model/AppModel
2. A 类单件下沉:InferSpeakerMix、HistoryFocus
   —— Model→Modules 15→~3、Interface→Modules 清零 ——
3. 上依赖护栏脚本(scripts/check-layering.py):当前违规记 baseline,只拦新增
4. B 类:摸清 Utils 模型辅助簇边界,整簇上移进 Model
5. C-seam①:UI 弹窗倒置(含 MidiConverterDialog、FillLyric→UI)
6. C-seam②:Playback 访问倒置
7. C-seam③:ModelChangeHandler/Actions 倒置
```

## 每步的验证不变量

- **源文件计数守恒**:`find src/app -name '*.h' -o -name '*.cpp' | wc -l` 搬前搬后一致(当前基线 861)。
- **每步后全量重编**(不信增量,见 stack-corruption 教训)+ 跑 `TestThemeColors`/`TestThemeIcons`/相关单测。
- **矩阵回归**:重跑本文件的矩阵脚本,确认目标违规边归零、无新增。
- 移动文件时**不同时改类名**(避免 QSS 选择器/AUTOMOC 连锁失效)。
