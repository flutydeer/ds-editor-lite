# 抽参插件（RMVPE / GAME）按 spec 2.4 打包 + otter 在 lite 上实测 —— 方案（已实施）

> 状态：**已实施，此后形态已变更**。模型包由 otter 发布（release `models-v0.3.0.0`：`otter-rmvpe`、`otter-game`、`otter-hfa`，版本均为 0.2.0.0）。分析器此后改为 synthrt 内置 `inference` 类别下的模块（otter 设计 A26）：接口名为 `org.openvpi.otter.inference.{F0,Note,Align}`，声明位于 `inferences/<id>/inference.json`，插件位于 `plugins/otter/inferenceinterpreters`，宿主不再调用 `linkAnalysisCategory()`。lite 按接口从 inference 模块中筛选 F0 与 Note，引用串格式为 `<package>:inference/<contribution>`；旧设置中的 `:analysis/` 引用在读取时自动升级。
> **本文其余部分保留当时侦察与决策原貌**；模型与包声明的最终形态以 otter 仓为准，实测结论见 `synthrt-main-migration.md` §7。
>
> 目标来源：长期目标 ⑥⑦⑧（用户 2026-09-2x 指令）。本文是**只读侦察后的方案**，落地前需用户确认（**已确认并落地** ✓）；
> 台账 D1–D6 为需确认项。侦察快照：wolf `6c3fb5c`、lite `fa5f942c`、otter 检出见 §5。

## 1. 结论先行

两件事都不是"从零造"，而是"接线 + 按已有键表填清单 + 实测"：

- **otter 仓已内置三个分析提供者**：`src/plugins/analysisproviders/{rmvpe,game,stub}`，`plugin.json` 声明的就是
  `org.openvpi.otter.analysis.F0` `level 1` `variant rmvpe` 与 `…analysis.Note` `variant game` —— 与你给的两个模型目录**一一对应**（RMVPE→F0、GAME→Note）。
- **lite 侧的 otter 接入（otter 文档里的 M6）已经实现**：`src/app/Modules/Extractors/{AudioSlicer,AnalysisAudio,ExtractTask,ExtractPitchTask,ExtractMidiTask}` 存在，`SynthrtBootstrap` 调 `otter::linkAnalysisCategory()`，`src/app/CMakeLists.txt` 链接 `otter::otter`，自动化侧有 analyzer 校验（`ExtractionAutomationAdapter.cpp:454,480`）。
- 因此 ⑥ = **写两份包声明并接模型**，⑦ = **让 lite 用上本地 otter 构建 + 用真实模型跑通抽参**。

## 2. 打包（⑥）—— 按 spec 2.4 与 otter 的既有契约

### 2.1 两个包的目标形状（事实依据均在 otter 仓：otter README `:11-46`、`src/tests/auto/Analysis/packages/*` 12 个样例、`scripts/check-declarations.py`）

```
<包目录>/desc.json                      # $version 只写在这里（spec 2.4）；contributions.analysis[0].path 指向分析清单
<包目录>/analyzers/f0/analysis.json      # rmvpe：interface F0 / level 1 / variant rmvpe
<包目录>/assets/rmvpe.onnx               # 345 MB（或按 D2 改为外部引用）
```
```
<包目录>/desc.json
<包目录>/analyzers/note/analysis.json    # game：interface Note / level 1 / variant game
<包目录>/assets/{encoder,segmenter,estimator,bd2dur,dur2bd}.onnx
```

清单的两块按 spec 2.4 的划分：`exports` 属**契约**（采样率、声道、帧间隔、旋钮、语言），`configuration` 属**变体**（模型位置与接线，宿主不读）。

### 2.2 值必须与模型/provider 对齐（事实）

| 项 | rmvpe（F0） | game（Note） | 依据 |
| :-- | :-- | :-- | :-- |
| 必需 configuration 键 | `model` | `encoder`、`segmenter`、`estimator`、`boundaryToDuration`（+ `durationToBoundary` 可选） | `check-declarations.py:71-74`、`game/main.cpp:683-687` |
| exports 必需 | `sampleRate`、`interval` | `sampleRate` | `check-declarations.py:93,98` |
| 模型声明值 | 16000 Hz / 0.01 s / 单声道 | **44100 Hz** / `timestep 0.01` / 语言编号 `en=1,ja=2,yue=3,zh=4` | README `:63`、GAME 官方仓库的 `experiments/GAME-1.0.3-small-onnx/config.json` |
| 旋钮 | `voicingThreshold`（0–1，默认 0.03）、`interpolateUnvoiced`（默认 true） | 见 Note schema | README `:35-38` |
| 一致性闸口 | provider 载入时自检：声道≠1、语言无编号、`supportsKnownNotes` 无 `durationToBoundary` 一律**拒载** | | `game/main.cpp:734-751` |

### 2.3 可执行门禁（打包"完成"的判据）

1. `python scripts/check-declarations.py <包目录>…` ⇒ **errors 必须为 0**（warnings 仅记录）；
2. 载入验证：分析器必须出现在 `analyzers("org.openvpi.otter.analysis.F0"/"…Note")` 里；
3. 端到端：真实音频 ⇒ F0 曲线 / 音符产出（§4）。

## 3. 实测通道（⑦）—— 两个候选

| 方案 | 做法 | 成本 | 风险 |
| :-- | :-- | :-- | :-- |
| **A 隔离副本替换（推荐）** | 用现有 `lab-mine-dbg`（lite Debug 树整拷）换入**本地构建的 otter**（库 + `plugins/otter/analysisproviders/*`），把两个包放进副本的 packages 目录；用 MCP 自动化或 lite 测试 exe 驱动抽参 | 只需构建 otter（不重建 lite） | otter 与 lite **同源 ABI**：本分支之前 lite 侧并没有 otter 端口（该端口由本分支新增，现钉 `d8a8bf1a`）；若本地检出与该钉不同，需确认头文件兼容（otter README `:73-79` 明确 1.0 前无 ABI 承诺） |
| B vcpkg override | 把 `scripts/vcpkg-ports/otter/portfile.cmake` 的 `vcpkg_from_git` 改成指向本地检出，重装 vcpkg 依赖并重建 lite | 重钉 + 重建 lite（较大） | 改动落在 lite 仓的端口文件（需明确是否要入库） |

两者都可保留：先 A 跑通，再按需 B 固化。

## 4. 验证矩阵（要交付的证据）

| 用例 | 期望 |
| :-- | :-- |
| 包声明校验 | `check-declarations.py` errors=0（事实记录输出全文） |
| 分析器枚举 | lite 侧能看到 F0/rmvpe 与 Note/game 两个贡献 |
| F0 抽参 | 真实音频 ⇒ 帧间隔取自声明（0.01 s，非 lite 里写死的 10 ms）、`voicingThreshold` 旋钮生效路径、未 voiced 段的处理 |
| Note 抽参 | 产出音符区间；`supportsKnownNotes` 条件路径（给定已知音符）可用 |
| 拒载路径 | 故意把 exports 的采样率改成与模型不符 / 去掉 `durationToBoundary` 却声明 `supportsKnownNotes` ⇒ **必须拒载**且报文可读 |
| 隔离性 | 用户树零改动；副本可删；无残留进程 |

## 5. 待确认决策台账

| 编号 | 决策点 | 推荐 | 备选与取舍 |
| :-- | :-- | :-- | :-- |
| **D1** | ⑥ 产出形态 | **已安装 Package 目录**（够测试，快） | 同时产出 `.dspk` zip（走 Installer 路径，需额外打包步骤）；只出目录 |
| **D2** | 345 MB 模型的处置 | **模型留在原位/副本外，清单用相对或绝对引用**（spec 2.4 允许绝对路径与包外引用） | 复制进包（自包含，占空间）；裁剪模型（**有保真风险，不建议**） |
| **D3** | ⑦ 的接法 | **A 隔离副本替换** | B vcpkg override + 重建 lite；A→B 渐进 |
| **D4** | 是否把包声明纳入 otter 仓 | **纳入**（声明是资产，模型不入库） | 只放仓外临时区（不入库，不可复现） |
| **D5** | 是否允许改 otter 代码 | **允许**（用户已授权）；范围限于"打包与实测暴露的问题"，例如 `check-declarations.py` 键表缺口或 provider 诊断措辞 | 只打包不改码（若实测无缺口则不必改） |
| **D6** | 中间产物清理 | 我自行决定：保留 `lab-mine-dbg` 至实测收口；可再生的构建/日志按需删；**不删**用户文档引用过的资产 | — |

## 6. 下一步（确认后执行顺序）

1. 构建本地 otter（`--x-feature=onnx --x-feature=tests`）+ 跑其 ctest（fixtures 需要带 `onnx` 模块的 Python）；
2. 写两份包声明（先按 D1/D2/D4 的裁定），过 `check-declarations.py`；
3. 按 D3 的裁定接线，跑 §4 的验证矩阵；
4. 每步独立本地提交（otter 仓 / lite 台账），**绝不 push**。
