# app 逻辑架构与静态库拆分地图

> 目标终态:像 LLVM 那样,**每个模块独立成静态库**,依赖构成一个 DAG。
> 本文是当前(基线 852 文件)的**实测**架构:物理结构 + 模块间真实依赖图 + 每个模块的"可抽库就绪度"。测量方法见 `scratchpad/depgraph.awk`(按模块粒度统计 `#include` 边)。

## 1. 物理结构(实测文件数)

| 顶层 | 文件 | 角色 |
|---|---:|---|
| `Global` | 8 | 常量/枚举。**不抽库**——是不该存在的 grab-bag,音乐域独立时各带自己的 Global |
| `Utils` | 18 | 通用工具(已抽走 ADT/Support/Audio 后的残余) |
| `Interface` | 13 | 抽象接口(依赖倒置的 seam) |
| `Model` | 89 | 工程数据模型(含 `Model/Utils`、`Model/InferenceData` 两个干净子层) |
| `Modules` | 240 | 11 个业务子系统 |
| `Controller` | 111 | 控制器(含 `Actions`/`DocumentWorkflow`/`Tasks`) |
| `UI` | 359 | 视图(app 主体,本质是可执行文件的前端) |
| `Bootstrap` | 11 | 启动胶水 |

`Modules/` 细分:Inference 86、FillLyric 51、Audio 36、ProjectConverters 13、PackageManager 13、Extractors 11、Language 9、History 6、SynthrtEngine 6、Task 6、SingingClipSlicer 3。

## 2. 逻辑分层(意图 DAG)

```
L0  Global(待解散)
L1  Utils   Interface
L2  Model  ├ Model.Utils  ├ Model.InferenceData
L3  Modules.*(业务子系统,内部还有子 DAG)
L4  Controller(Actions / DocWF / Tasks)
L5  UI
L6  App / Bootstrap
```

合法方向 = 只从上往下依赖。**违反此序的边(back-edge)= 环 = 阻止抽库的唯一障碍。**

## 3. 阻断抽库的 back-edge(实测,这些是全部工作量所在)

### 向上边(Modules/Controller 反向依赖更高层)

| 边 | 次数 | 具体 |
|---|---:|---|
| `Controller → UI` | 12 | Toast/Dialog/ThemeManager 等,控制器直接弹 UI |
| `Mod.Extractors → UI` | 6 | Toast/TaskDialog/AccentButton |
| `Mod.Audio → UI` | 5 | LevelMeterView/Manager |
| `Mod.ProjectConverters → UI` | 5 | MidiConverterDialog(逻辑持有对话框) |
| `Mod.FillLyric → UI` | 4 | Widgets 整套 UI 嵌在 Module 里 |
| `Mod.Inference → Controller` | 4 | PlaybackController/ModelChangeHandler/Actions |
| `Mod.Extractors → Controller` | 3 | 同上 |
| `Mod.Audio → Controller` | 2 | 同上 |
| `Mod.Extractors → Ctrl.Actions` | 2 | |
| `Utils → UI` | 1 | `WindowFrameUtils_{win,mac}` → ThemeManager(误放 Utils 的 UI 助手) |

### Model↔Modules 环(Model 反向依赖业务子系统)

| 边 | 次数 | 层级 | 破法难度 |
|---|---:|---|---|
| `Model → Mod.PackageManager` | 8 | **头文件**(Track/SingingClip/SpeakerMixData/EffectiveVoiceContext 持有 `SingerInfo`) | 硬——需设计(见 decoupling-plan) |
| `Model → Mod.SingingClipSlicer` | 2 | **.cpp**(SingingClip.cpp 调 slicer) | 较易——impl 层,可上提调用点 |

### Interface 残余

- `Interface → Model` (1):`ITrack.h → TrackControl.h`。确认 `TrackControl` 值类型即可接受,否则下沉。

> **Modules 内部子图无环**——Task/SynthrtEngine/Language/History/SingingClipSlicer 是叶子(只 →Model/Utils/Global);PackageManager→{Task,SynthrtEngine};Inference→{Task,SynthrtEngine,PackageManager,Language,SingingClipSlicer};Extractors→{Task,SynthrtEngine,History};FillLyric→Language;ProjectConverters→PackageManager。是一个干净 DAG。

## 4. 每个模块的"抽库就绪度"

### 已完成(foundation libs,已在 main)
`lite::ADT`(纯头)、`lite::Support`(STATIC)。

> 注:`Decibellinearizer`/`VolumeUtils`(音频信号)仍在 `Utils/`,尚未抽成 `lite::Audio`(见 §6)。

### 🟢 Tier 1 — 绿色可抽(只有向下依赖,无环无向上边)
抽库顺序里**风险最低**,是模块级静态库的样板。

| 模块 | 文件 | 依赖 | 备注 |
|---|---:|---|---|
| `Mod.Task` | 6 | →Utils,Global | **最干净,建议首抽** → `lite::Task` |
| `Mod.History` | 6 | →Model,Interface,Utils | 消费者(Actions 34/UI 7/Controller 5)全在其上 |
| `Mod.Language` | 9 | →Model | |
| `Mod.SynthrtEngine` | 6 | →Model | |

前提:它们依赖 `Utils`/`Model`,所以要么先把这些做成库,要么这些模块暂时 `target_include` app 源(过渡)。`Task` 只依赖 Utils/Global,可以最先走通。

### 🟡 Tier 2 — 被 Model↔Modules 环挡住
- `Mod.SingingClipSlicer`(3):环在 .cpp,**先破**(把 SingingClip.cpp 里对 slicer 的调用上提,或 slicer 反注入)。
- `Mod.PackageManager`(13):环在头文件(SingerInfo),**需先做设计议题**。
- 破掉这两个环后,`Model`(含 Model.Utils/InferenceData)才能成 `lite::Model`。

### 🔴 Tier 3 — 被向上边挡住(需 C 类接口倒置)
按 seam 归类,倒置后即可抽:
- **seam ① UI 弹窗**:Controller→UI、Audio→UI、Extractors→UI、ProjectConverters→UI(MidiConverterDialog)、FillLyric→UI(Widgets 应整体归 UI)、Utils→UI(WindowFrameUtils 归 UI/Utils)。
- **seam ② Playback 访问**:Inference/Audio/Extractors → PlaybackController。
- **seam ③ ModelChangeHandler/Actions**:Inference/Extractors → Controller。
- 涉及模块:`Inference`(86)、`Audio`(36)、`Extractors`(11)、`ProjectConverters`(13)、`FillLyric`(51)。

### 层级库
- `Interface`(13):近乎干净(仅 1 条 →Model),可早期成 `lite::Interface`(先定 ITrack/TrackControl)。
- `Controller`(111):被 `Controller→UI`(12,seam ①)挡住。
- `UI`(359):app 主体。LLVM 类比里它是"工具前端",最后再谈是否拆分为若干 UI 库,近期留在 app target。

## 5. 目标库 DAG(终态草图)

```
                 lite::ADT   lite::Support   (foundation, 纯/通用)
                     │            │
      lite::Interface│   lite::Audio(信号)   lite::Music(音乐域: Note/Time, 待建)
                     │            │            │
                  lite::Model ────┴────────────┘
                     │
   ┌─────────┬───────┼──────────┬──────────┬───────────┐
 Task  SynthrtEngine Language History  SingingClipSlicer PackageManager
   └─────────┴───────┴────┬─────┴──────────┴───────────┘
                          │
        Inference   Audio(重)  Extractors  ProjectConverters  FillLyric
                          │
                     Controller
                          │
                     app(UI + Bootstrap + main)
```

## 6. 建议推进顺序

1. **收尾 foundation**:`WindowFrameUtils`→`UI/Utils/`(消掉最后一条 Utils→UI);评估 Utils 残余是否再拆 / `Interface`→`lite::Interface`。
2. **首抽绿色模块** `lite::Task`(样板),再 History / Language / SynthrtEngine。
3. **破 Model↔Slicer 环**(易)→ 破 **Model↔PackageManager 环**(设计议题)→ 抽 `lite::Model`。
4. **C 类三 seam 倒置**,逐个解锁 Inference / Audio / Extractors / ProjectConverters / FillLyric。
5. **Controller**(seam① 后)→ 最后审视 UI 拆分。

## 7. 验证不变量(每步)
- 源文件计数守恒(基线 852);
- 全量重编 + `ctest`(当前 11/11);
- 重跑 depgraph,确认目标 back-edge 归零、无新增;
- 移动不改类名(避免 QSS/AUTOMOC 连锁)。
