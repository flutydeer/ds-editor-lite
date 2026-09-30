# docs/plans 索引

本目录是**进行中**的方案与台账；已完成并归档的见 `docs/archive/`。下表**只列本分支新增或改动的文档** —— 主线既有方案不在此重复列出，本目录里其余文件名不代表它们已完成或作废。本带宽改动极大，接手者建议按下面的顺序读。

| 文档 | 角色 | 一句话 |
| :-- | :-- | :-- |
| `synthrt-main-migration.md` | 记录 | 迁移到 synthrt 主线的过程与叠加策略；**pin（提交 sha）与分支名不在此处复制**；权威来源是端口文件 `scripts/vcpkg-ports/synthrt/portfile.cmake` 的 `REF` / `HEAD_REF` |
| `otter-extraction-packaging.md` | 记录 | otter 抽参打包方案与裁定 |
| `../archive/r68-voice-state-and-gpu-report-fix.md` | 记录 | R68 的语音状态与 GPU 报告修复：根因、改动与验证表（含 R69 §5.1 补验） |

> 本分支的内部工作文档（过程台账与审计共 13 篇：`push-preparation-plan`、`handover`、`plugin-internals-hardening`、`plugin-chain-baseline-and-test-record`、`fullchain-redundancy-and-stability-audit`、`language-module-chain-audit`、`annex-language-module-test-record`、`annex-language-module-improvement-plan`、`annex-plugin-internals-audit`，以及 `docs/archive/` 下 `annex-plugin-internals-audit`、`audit-report-onnxruntime-builds`、`audit-report-ort-full-link`、`vcpkg-ort-synthrt-split`）**未随交付提交**，只保留在 git 历史中。

各文档的状态以文内的 `> 状态：` 行为准。**标注为"已完成/已实施"的才归档；"未实施/待实施/进行中"属活文档。**

## 文档清理口径（2026-09-22 用户裁定，接手者请遵守）

1. **main 已有的一律原样保留**：不改、不移、不删。动手前先跑 `git diff --name-status main..HEAD -- docs` 定界，只看本分支**新增（A）**的部分。
2. 清理判据：文档自己的 `> 状态：` 行 + 仓内引用矩阵（按名检索；同名不同物需带目录前缀）。
3. 本地专属文档（被 `.git/info/exclude` 排除）是**唯一副本**：先仓外打包 → 逐份哈希校验回读 → 才可删。
4. 被用户文档写明路径或指纹的资产，**先问再删**。
5. `docs/` 的总索引页是 **`docs/README.md`**（本分支新增 ✓）；本文件是 `docs/plans/` 的入口。不要再依赖随 `docs/audits/**` 移出仓外的那一份。
