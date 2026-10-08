# 抽参插件（RMVPE / GAME）按 spec 2.4 打包与 otter 在 lite 上的实测方案（已实施）

> 状态：**已实施，此后形态已变更**。模型包由 otter 发布（release `models-v0.3.0.0`：`otter-rmvpe`、`otter-game`、`otter-hfa`，版本均为 0.2.0.0）。分析器此后改为 synthrt 内置 `inference` 类别下的模块（otter 设计 A26）：接口名为 `org.openvpi.otter.inference.{F0,Note,Align}`，声明位于 `inferences/<id>/inference.json`，插件位于 `plugins/otter/inferenceinterpreters`，宿主不再调用 `linkAnalysisCategory()`。lite 按接口从 inference 模块中筛选 F0 与 Note，引用串格式为 `<package>:inference/<contribution>`。旧设置中的 `:analysis/` 引用在读取时自动升级。
> **本文其余部分保留当时侦察与决策的原貌**。模型与包声明的最终形态以 otter 仓库为准，实测结论见 `synthrt-main-migration.md` §7。
>
> 目标来源：长期目标 ⑥⑦⑧（用户 2026-09 下旬的指令）。本文是**只读侦察后形成的方案**，落地前需用户确认（**已确认并落地**）。
> 台账 D1–D6 为需确认项。侦察快照取自 wolf、lite 与 otter 当时的集成分支。

## 1. 结论

两项工作都不是从零实现，而是接线、按已有键表填写清单并实测：

- **otter 仓库已内置三个分析提供者**：`src/plugins/analysisproviders/{rmvpe,game,stub}`，其 `plugin.json` 声明
  `org.openvpi.otter.analysis.F0` `level 1` `variant rmvpe` 与 `…analysis.Note` `variant game`，与用户提供的两个模型目录**一一对应**（RMVPE→F0、GAME→Note）。
- **lite 侧的 otter 接入（otter 文档中的 M6）已经实现**：`src/app/Modules/Extractors/{AudioSlicer,AnalysisAudio,ExtractTask,ExtractPitchTask,ExtractMidiTask}` 已存在，`SynthrtBootstrap` 调用 `otter::linkAnalysisCategory()`，`src/app/CMakeLists.txt` 链接 `otter::otter`，自动化侧有 analyzer 校验（`ExtractionAutomationAdapter.cpp:454,480`）。
- 因此 ⑥ 为**编写两份包声明并接入模型**，⑦ 为**使 lite 使用本地 otter 构建，并以真实模型完成抽参**。

## 2. 打包（⑥）：spec 2.4 与 otter 的既有契约

### 2.1 两个包的目标形状（依据均在 otter 仓库：otter README `:11-46`、`src/tests/auto/Analysis/packages/*` 的 12 个样例、`scripts/check-declarations.py`）

```
<包目录>/desc.json                      # $version 只写在此处（spec 2.4）；contributions.analysis[0].path 指向分析清单
<包目录>/analyzers/f0/analysis.json      # rmvpe：interface F0 / level 1 / variant rmvpe
<包目录>/assets/rmvpe.onnx               # 345 MB（或按 D2 改为外部引用）
```
```
<包目录>/desc.json
<包目录>/analyzers/note/analysis.json    # game：interface Note / level 1 / variant game
<包目录>/assets/{encoder,segmenter,estimator,bd2dur,dur2bd}.onnx
```

清单的两部分按 spec 2.4 划分：`exports` 属于**契约**（采样率、声道、帧间隔、旋钮、语言），`configuration` 属于**变体**（模型位置与接线，宿主不读取）。

### 2.2 与模型及 provider 对齐的取值

| 项 | rmvpe（F0） | game（Note） | 依据 |
| :-- | :-- | :-- | :-- |
| 必需 configuration 键 | `model` | `encoder`、`segmenter`、`estimator`、`boundaryToDuration`（`durationToBoundary` 可选） | `check-declarations.py:71-74`、`game/main.cpp:683-687` |
| 必需 exports 键 | `sampleRate`、`interval` | `sampleRate` | `check-declarations.py:93,98` |
| 模型声明值 | 16000 Hz / 0.01 s / 单声道 | **44100 Hz** / `timestep 0.01` / 语言编号 `en=1,ja=2,yue=3,zh=4` | README `:63`、GAME 官方仓库的 `experiments/GAME-1.0.3-small-onnx/config.json` |
| 旋钮 | `voicingThreshold`（0–1，默认 0.03）、`interpolateUnvoiced`（默认 true） | 见 Note schema | README `:35-38` |
| 一致性检查 | provider 载入时自检：声道数不为 1、语言无编号、声明 `supportsKnownNotes` 而无 `durationToBoundary` 时一律**拒绝加载** | | `game/main.cpp:734-751` |

### 2.3 完成判据

1. `python scripts/check-declarations.py <包目录>…` 的 **errors 必须为 0**（warnings 仅记录）。
2. 载入验证：分析器必须出现在 `analyzers("org.openvpi.otter.analysis.F0"/"…Note")` 的结果中。
3. 端到端：真实音频产出 F0 曲线与音符（§4）。

## 3. 实测通道（⑦）的候选方案

| 方案 | 做法 | 成本 | 风险 |
| :-- | :-- | :-- | :-- |
| **A 隔离副本替换（推荐）** | 在现有的 `lab-mine-dbg`（lite Debug 构建目录的完整副本）中换入**本地构建的 otter**（库与 `plugins/otter/analysisproviders/*`），将两个包放入副本的 packages 目录。通过 MCP 自动化或 lite 测试程序驱动抽参 | 只需构建 otter（不重建 lite） | otter 与 lite 须 **ABI 一致**：本分支之前 lite 侧没有 otter 端口（该端口由本分支新增，固定在 otter 集成分支的某个提交）。若本地检出与该提交不同，需确认头文件兼容（otter README `:73-79` 明确 1.0 之前不承诺 ABI） |
| B vcpkg override | 将 `scripts/vcpkg-ports/otter/portfile.cmake` 的 `vcpkg_from_git` 改为指向本地检出，重新安装 vcpkg 依赖并重建 lite | 更换端口提交并重建 lite（成本较大） | 改动落在 lite 仓库的端口文件中（需明确是否入库） |

两种方案可以并存：先以 A 完成验证，再按需以 B 固化。

## 4. 验证矩阵（需交付的证据）

| 用例 | 期望 |
| :-- | :-- |
| 包声明校验 | `check-declarations.py` errors=0（完整记录输出） |
| 分析器枚举 | lite 侧可枚举 F0/rmvpe 与 Note/game 两个贡献 |
| F0 抽参 | 真实音频的帧间隔取自声明（0.01 s，而非 lite 中硬编码的 10 ms）。`voicingThreshold` 旋钮生效。清音段按预期处理 |
| Note 抽参 | 产出音符区间。`supportsKnownNotes` 的条件路径（给定已知音符）可用 |
| 拒绝加载路径 | 将 exports 的采样率改为与模型不符，或去掉 `durationToBoundary` 而保留 `supportsKnownNotes` 声明时，**必须拒绝加载**且错误信息可读 |
| 隔离性 | 用户目录零改动，副本可删除，无残留进程 |

## 5. 待确认决策台账

| 编号 | 决策点 | 推荐 | 备选与取舍 |
| :-- | :-- | :-- | :-- |
| **D1** | ⑥ 的产出形态 | **已安装的 Package 目录**（满足测试需要，速度快） | 同时产出 `.dspk` zip（经 Installer 路径，需要额外打包步骤）。只产出目录 |
| **D2** | 345 MB 模型的处置 | **模型保留在原位或副本外，清单使用相对或绝对引用**（spec 2.4 允许绝对路径与包外引用） | 复制进包（自包含，占用空间）。裁剪模型（**有保真风险，不建议**） |
| **D3** | ⑦ 的接入方式 | **A 隔离副本替换** | B vcpkg override 并重建 lite。由 A 渐进到 B |
| **D4** | 是否将包声明纳入 otter 仓库 | **纳入**（声明是资产，模型不入库） | 只放在仓库外的临时目录（不入库，不可复现） |
| **D5** | 是否允许修改 otter 代码 | **允许**（用户已授权）。范围限于打包与实测暴露的问题，例如 `check-declarations.py` 的键表缺口或 provider 诊断措辞 | 只打包、不改代码（实测无缺口时无须修改） |
| **D6** | 中间产物清理 | 由执行者决定：`lab-mine-dbg` 保留至实测结束，可再生的构建与日志按需删除，用户文档引用过的资产**不删除** | — |

## 6. 执行顺序（确认后）

1. 构建本地 otter（`--x-feature=onnx --x-feature=tests`）并运行其 ctest（夹具需要带 `onnx` 模块的 Python）。
2. 按 D1/D2/D4 的裁定编写两份包声明，通过 `check-declarations.py`。
3. 按 D3 的裁定接线，执行 §4 的验证矩阵。
4. 每一步单独在本地提交（otter 仓库与 lite 台账），**不推送**。
