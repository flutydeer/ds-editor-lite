# docs/plans 索引

本目录存放**进行中**的方案与台账：`> 状态：` 为「未实施／待实施／进行中」的文档保留在本目录，标注为「已完成／已实施」的归档到 `docs/archive/`（规则与 [`../README.md`](../README.md) 的「文档规约」第 3 条相同）。下表**仅列本分支新增或改动的文档**。主线既有方案不在此重复列出，本目录中其余文件的存在不表示其已完成或已作废。本分支改动范围大，建议按下表顺序阅读。

> **2026-10-06 合并**：本分支的增量事项已收敛为**一份台账**——[`synthrt-main-migration.md`](./synthrt-main-migration.md)（含统一待办、已关闭记录、wolf 现状、推送就绪、未证实清单与复现命令）。原 `optimization-and-integration-survey.md`、`synthrt-main-migration-gaps.md`、`verified-defect-dossier.md`、`integration-pending-changes.md` 的独有信息已并入该文，四份文件**已删除**。`four-repo-integration-audit.md` 已归档到 `docs/archive/`（正文保留），下文表亦已按实际文件集更新。

| 文档 | 状态 | 摘要 |
| :-- | :-- | :-- |
| [`synthrt-main-migration.md`](./synthrt-main-migration.md) | 进行中 | **本分支增量的唯一台账**（§0 基线与快照口径 → §1 阅读顺序与文档规约 → §2 部署形态与唯一依赖树 → §3 端口 pin 与 overlay 遮蔽 → §4 模块映射与已知降级 → §5 顺带修复的缺陷 → §6 验证范围 → §7 转换脚本三件套与回归三层 → §8 统一待办台账（`MG-*` / `IP-*` / 审计 `F-*`，唯一编号）→ §9 已关闭与已推翻记录 → §10 wolf 现状 → §11 上游推送与分支就绪 → §12 未覆盖与未证实 → §13 复现命令与夹具 → §14 本地专属材料索引）。**易变值不复制**：依赖所固定的提交与分支名不在该文，以 `scripts/vcpkg-ports/*/portfile.cmake` 的 `REF` / `HEAD_REF` 为准。测试计数以 `ctest` 现场输出为准。**2026-10-08 迁移轮**：基线已换到 `synthrt/spec2.4-uptake`，改名的联合命名与 API 适配见文首更新块 |
| [`wolf-language-readiness-adversarial-review.md`](./wolf-language-readiness-adversarial-review.md) | 进行中 | wolf 语言就绪缺口的对抗复核报告：spec 2.4 合规性核实、两类失败分界、五组实验矩阵、G1~G5 分级建议、§8 逐条自查、§0.5 决策台账（轮次 A~H）。§10 为相对 spec 2.4 的加载期合规台账（F1~F5/V1~V11、V6 归属与"何时该改 synthrt"、修复优先级与代价、消费约束）。**§10.7~§10.9 为三次端到端实测**（F1 损坏 multig2p 模型、F2 无 ONNX 驱动、D5 覆盖率闭环）。**wolf 侧修复按用户裁定暂缓**，故本文留在 plans 续写（其现状摘要见台账 §10）。正文中的分支名 `linguistic-level-1-v2` 为该仓 2026-10-08 改名前的旧名，现名 `spec2.4-uptake` |
| [`dur-type-declaration-migration.md`](./dur-type-declaration-migration.md) | 进行中 | duration 的词级声明从两个布尔量改成文字选项 `dur_type`（`abs` / `rel`）的调研与实施记录：现状取证（synthrt 规范与实现 / 真实包 / 本仓 / wolf 与 OpenUtau 侧）、改动必要性、方案候选、setdir 实测与验证方案、风险与未证实项。§0 为编号决策台账（D-1~D-5），§5 为落地步骤。synthrt 侧改动为提交 `7ea424a`，现位于 `origin/spec2.4-uptake`（该分支 2026-10-08 由 `onnxruntime-builds-uptake` 改名而来）；lite 侧原先落成于 `synthrt/inferutil-binary-read`，2026-10-08 按"以新 main 为基座、只搬增量"的路线并入迁移分支 **`synthrt/spec2.4-uptake`**（`merge --squash` 取并集后按主题拆成 7 条提交，全部无 merge 记录，拆分前后树逐字节一致）。实测：ctest 79 用例 0 失败、转换器 38/38、真实模型 6/6、G2P 基线 0 差异、中文路径四格全绿、真实声库 `rel` 端到端跑通。**不分旧版兼容** |

工作树内本地专属材料：**2026-10-07 已全部从工作树移除**（五份：`four-repo-defect-scan.md`、`four-repo-increment-report.md`、`2026-10-03-recovered-audits-and-hardening.md`，以及同批复核发现的两份旧方案 `lang-package-24-rebuild.md` 与 `lyric-language-architecture.md`，后两份原先被 `.git/info/exclude` 排除）。
移除由用户裁定，执行前按「文档清理规约」第 3 条在**仓库外**打包并逐份哈希校验回读（不一致 0），
留档在仓库外（未入库，其路径不写入本文）。
`docs/archive/four-repo-integration-audit.md` 内对其中两份的既有引用按归档「正文保留不改」保留，
是本批之后**唯一**指向已移除文件的残留引用。该归档里原先指向本机临时产物的出处，已按「只指向
仓库内」的口径改为中性表述，结论与数字未动。

本分支已归档的文档（`docs/archive/`）：

| 文档 | 状态 | 摘要 |
| :-- | :-- | :-- |
| [`../archive/otter-extraction-packaging.md`](../archive/otter-extraction-packaging.md) | 已实施 | otter 抽参打包方案与裁定 |
| [`../archive/r68-voice-state-and-gpu-report-fix.md`](../archive/r68-voice-state-and-gpu-report-fix.md) | 已完成 | R68 的语音状态与 GPU 报告修复：根因、改动与验证表（含 R69 §5.1 补验） |
| [`../archive/four-repo-integration-audit.md`](../archive/four-repo-integration-audit.md) | 已归档（历史） | 2026-09-30 四仓（lite/synthrt/wolf/otter）联合增量审计：构建与依赖链现状、无头与 GUI 端到端实测、按严重性分级的发现清单（F-1~F-27）、对抗复核结论与未覆盖范围。**正文保留不改**，其发现项的处置状态与唯一编号已并入 `synthrt-main-migration.md` §8/§9，该文不再作为待办来源 |

各文档的状态以文内的 `> 状态：` 行为准：标注为「已完成／已实施」的归档，标注为「未实施／待实施／进行中」的属于活文档，保留在本目录。

## 文档清理规约

以下规约由用户于 2026-09-22 裁定，接手者须遵守。

1. **main 已有的文档一律原样保留**：不改、不移、不删。动手前先运行 `git diff --name-status origin/main..HEAD -- docs` 界定范围，只处理本分支**新增（A）**的部分。
2. 清理判据：文档自身的 `> 状态：` 行，以及仓内引用矩阵（按文件名检索，同名而内容不同的文件须带目录前缀区分）。
3. 本地专属文档（由 `.git/info/exclude` 排除）是**唯一副本**：须先在仓库外打包，再逐份以哈希校验回读，之后才可删除。
4. 用户文档中写明路径或指纹的资产，删除前须先征得用户同意。
5. `docs/` 的总索引页是 **`docs/README.md`**（本分支新增）。本文件是 `docs/plans/` 的入口。

## 路径迁移对照表（2026-10-03 核查）

历史文档（尤其 `docs/design/**` 里已完成的设计契约）按**写作时**的路径记录文件，此后的库抽取重构把文件搬走了。
原文不改（保留当时的记录），需要找文件时按下表换算。核查口径：把 `docs/**/*.md` 中形如 `<顶层目录>/…` 的路径
逐个判断是否存在——283 处引用中 **29 处已不存在**（另 26 处指向 wolf/otter/synthrt 的同名文件，属正常跨仓引用），
`X.h/.cpp` 这类简写按两个文件分别判定。成因与对应关系如下：

| 文档里写的 | 现在的位置 | 搬运/删除提交 |
| :-- | :-- | :-- |
| `src/app/Model/AppModel/**` | `src/libs/ProjectModel/AppModel/**` | `7e22c39d`（`2fc44ff3` 另搬 `AppModel.h`） |
| `src/app/Modules/PackageManager/**` | `src/libs/PackageManager/**` | `96a0403f` |
| `src/app/Modules/ProjectConverters/**` | `src/libs/ProjectConverters/**` | `d67bda14` |
| `src/app/Modules/Inference/InferenceApplyGate.{h,cpp}` | `src/app/Modules/Inference/Utils/InferenceApplyGate.{h,cpp}` | 同目录细分 |
| `tests/TestSyllabification/**` | `src/tests/TestSyllabification/**` | 测试目录并入 `src/` |
| `src/app/UI/Dialogs/Options/AppOptionsPanel.{h,cpp}` | 已删除（并入统一选项对话框） | `6840e37a` |
| `src/app/UI/Utils/WaveformPainter.{h,cpp}` | 已删除（波形几何改由采样共享） | `c06fb36e` |
| `src/libs/GUI/Animation/AnimationGlobal.h` | 已删除（动画档位改为开关） | `f6090d5b` |
| `docs/plans/document-workflow-and-multi-format-import-plan.md` | `docs/design/multi-format-import-design.md` | `91e3d21c` |
