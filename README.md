# DS Editor Lite

开发文档：[AGENTS.md](./AGENTS.md)

语言层（G2P 与 linguist）：[wolf](https://github.com/diffscope/wolf)；分析模型：[otter](https://github.com/diffscope/otter)；推理引擎：[synthrt](https://github.com/diffscope/synthrt)

## 接手入口

- **文档总索引**：[docs/README.md](./docs/README.md)。
- **本分支目标**：将编辑器迁移到 synthrt 主线。引擎由 `synthrt` 提供，`wolf`（语言学）与 `otter`（分析模型）构建于其上，三者均通过本仓库自带的 overlay 端口引入。**依赖所固定的提交以 `scripts/vcpkg-ports/{synthrt,wolf,otter}/portfile.cmake` 为准**，提交 SHA 与分支名不在此处复制。
- **分析模型包**：otter 的模型包（`otter/rmvpe`、`otter/game`、`otter/hfa`）由 otter 的 release `models-v0.3.0.0` 提供，不随端口安装。
- **构建与测试**：按 [AGENTS.md](./AGENTS.md) 使用 vcpkg 与 CMake presets 构建；测试由 `-DLITE_BUILD_TESTS=ON` 开启（`debug` preset 默认开启）。
- **端口位置**：四个端口（`synthrt`、`wolf`、`otter` 与 wolf 语言包数据 `wolf-lang-packages`）位于本仓库的 `scripts/vcpkg-ports/` 下。manifest 将该目录排在共享 overlay（子模块 `scripts/vcpkg`）之前，因此本仓库的 `synthrt` 端口覆盖共享 overlay 中跟随 refactor 线的同名端口。`synthrt`、`wolf` 与 `otter` 均为公开仓库，安装时不需要凭据。端口不放在子模块中，因为子模块指向独立的 overlay 仓库：端口若位于该仓库，必须先推送该仓库，本仓库的依赖才能安装。
