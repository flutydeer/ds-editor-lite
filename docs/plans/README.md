# docs/plans 索引

本目录存放**进行中**的方案与台账：`> 状态：` 为「未实施／待实施／进行中」的文档保留在本目录，标注为「已完成／已实施」的归档到 `docs/archive/`（规则与 [`../README.md`](../README.md) 的「文档规约」第 3 条相同）。下表**仅列本分支新增或改动的文档**；主线既有方案不在此重复列出，本目录中其余文件的存在不表示其已完成或已作废。本分支改动范围大，建议按下表顺序阅读。

| 文档 | 状态 | 摘要 |
| :-- | :-- | :-- |
| `synthrt-main-migration.md` | 进行中 | 迁移到 synthrt 主线的过程、部署形态、验证与剩余事项。依赖所固定的提交与分支名不在该文复制，以端口文件 `scripts/vcpkg-ports/synthrt/portfile.cmake` 的 `REF` / `HEAD_REF` 为准 |
| `integration-pending-changes.md` | 待实施 | 联合审计与联测中落在对接层以外的改动：保留音素、零长音符与区间树、语言回写、5 tick 常量、遗留 G2P 标识等，每项注明位置与建议 |

本分支已归档的文档（`docs/archive/`）：

| 文档 | 状态 | 摘要 |
| :-- | :-- | :-- |
| `../archive/otter-extraction-packaging.md` | 已实施 | otter 抽参打包方案与裁定 |
| `../archive/r68-voice-state-and-gpu-report-fix.md` | 已完成 | R68 的语音状态与 GPU 报告修复：根因、改动与验证表（含 R69 §5.1 补验） |

> 本分支的内部工作文档（过程台账与审计共 13 篇：`push-preparation-plan`、`handover`、`plugin-internals-hardening`、`plugin-chain-baseline-and-test-record`、`fullchain-redundancy-and-stability-audit`、`language-module-chain-audit`、`annex-language-module-test-record`、`annex-language-module-improvement-plan`、`annex-plugin-internals-audit`，以及 `docs/archive/` 下的 `annex-plugin-internals-audit`、`audit-report-onnxruntime-builds`、`audit-report-ort-full-link`、`vcpkg-ort-synthrt-split`）**未随交付提交**，仅保留在 git 历史中。

各文档的状态以文内的 `> 状态：` 行为准：标注为「已完成／已实施」的归档，标注为「未实施／待实施／进行中」的属于活文档，保留在本目录。

## 文档清理规约

以下规约由用户于 2026-09-22 裁定，接手者须遵守。

1. **main 已有的文档一律原样保留**：不改、不移、不删。动手前先运行 `git diff --name-status main..HEAD -- docs` 界定范围，只处理本分支**新增（A）**的部分。
2. 清理判据：文档自身的 `> 状态：` 行，以及仓内引用矩阵（按文件名检索；同名而内容不同的文件须带目录前缀区分）。
3. 本地专属文档（由 `.git/info/exclude` 排除）是**唯一副本**：须先在仓库外打包，再逐份以哈希校验回读，之后才可删除。
4. 用户文档中写明路径或指纹的资产，删除前须先征得用户同意。
5. `docs/` 的总索引页是 **`docs/README.md`**（本分支新增）；本文件是 `docs/plans/` 的入口。随 `docs/audits/**` 移出仓库的旧索引不再使用。
