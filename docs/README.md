# docs/ 索引

本目录是 ds-editor-lite 的开发文档。**接手先从这张表进**。

| 你要什么 | 看哪份 |
| :-- | :-- |
| 环境、配置、构建、测试、打包 | [`../AGENTS.md`](../AGENTS.md)（构建说明的唯一入口；`../CLAUDE.md` 是精简版） |
| 本分支把引擎迁到 synthrt 主线的过程、叠加策略与待办 | [`plans/synthrt-main-migration.md`](./plans/synthrt-main-migration.md) |
| 本分支**新增或改动**的方案与记录（只列这些） | [`plans/README.md`](./plans/README.md) |
| 已完成的设计契约与方案 | [`design/`](./design/) |
| 自动化 / MCP / 无头模式的方案与报告 | [`automation/`](./automation/)（分 `01-automation-facade`、`02-mcp-server-and-connector`、`03-headless-mode` 三册） |
| 开发指南 | [`guides/`](./guides/)（本地化、偏好设置） |
| 主题与配色 | [`theme/`](./theme/) |
| 历史归档 | [`archive/`](./archive/) |
| 依赖版本更新脚本 | [`dev-scripts/`](./dev-scripts/) |

本目录根下另有 `iso-639-3.tab` 与 `Piece States.ts`，是数据与类型资产，不是文档。

## 文档口径（接手者请遵守）

1. **主线既有的文档一律原样保留**：`design/`、`automation/`、`guides/`、`theme/`、`archive/` 下的文档不改、不移、不删。文档清理只针对**本分支新增**的部分，边界用 `git diff --name-status main..HEAD -- docs` 界定。
2. **不复制会变的值**：依赖钉（提交 sha、分支名）的权威来源是端口文件 `scripts/vcpkg-ports/*/portfile.cmake`；版本号、测试数等以对应文件或脚本输出为准，不要写进文档。
3. **归档规则**：文内 `> 状态：` 标注为「已完成／已实施」的方案归档到 `archive/`；「未实施／待实施／进行中」属活文档，留在 `plans/`。
4. 清理判据、引用矩阵与本地专属文档的处理见 [`plans/README.md`](./plans/README.md) 的「文档清理口径」。
