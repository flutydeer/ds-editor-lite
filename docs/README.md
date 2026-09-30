# docs/ 索引

本目录是 ds-editor-lite 的开发文档。下表是接手时的入口。

| 主题 | 文档 |
| :-- | :-- |
| 环境、配置、构建、测试、打包 | [`../AGENTS.md`](../AGENTS.md)（构建说明的唯一入口；`../CLAUDE.md` 是其精简版） |
| 本分支将引擎迁移到 synthrt 主线的过程、部署形态与待办 | [`plans/synthrt-main-migration.md`](./plans/synthrt-main-migration.md) |
| 本分支**新增或改动**的方案与记录（仅列这些） | [`plans/README.md`](./plans/README.md) |
| 对接层以外待改的问题（位置与建议） | [`plans/integration-pending-changes.md`](./plans/integration-pending-changes.md) |
| 已完成的设计契约与方案 | [`design/`](./design/) |
| 自动化、MCP 与无头模式的方案与报告 | [`automation/`](./automation/)（分 `01-automation-facade`、`02-mcp-server-and-connector`、`03-headless-mode` 三册） |
| 开发指南 | [`guides/`](./guides/)（本地化、偏好设置） |
| 主题与配色 | [`theme/`](./theme/) |
| 历史归档 | [`archive/`](./archive/) |
| 依赖版本更新脚本 | [`dev-scripts/`](./dev-scripts/) |

本目录根下另有 `iso-639-3.tab` 与 `Piece States.ts`，二者是数据与类型资产，不是文档。

## 文档规约

1. **主线既有的文档一律原样保留**：`design/`、`automation/`、`guides/`、`theme/`、`archive/` 下的既有文档不改、不移、不删。文档清理只针对**本分支新增**的部分，边界由 `git diff --name-status main..HEAD -- docs` 界定。
2. **不复制易变的值**：依赖所固定的提交与分支名以端口文件 `scripts/vcpkg-ports/*/portfile.cmake` 为准；版本号、测试数等以对应文件或脚本输出为准，不写入文档。
3. **归档规则**：`> 状态：` 标注为「已完成／已实施」的方案归档到 `archive/`；标注为「未实施／待实施／进行中」的属于活文档，保留在 `plans/`。本分支新增的文档均带 `> 状态：` 行，[`plans/README.md`](./plans/README.md) 按同一规则分两组列出保留在 `plans/` 与已归档的文档。
4. 清理判据、引用矩阵与本地专属文档的处理规则见 [`plans/README.md`](./plans/README.md) 的「文档清理规约」。
