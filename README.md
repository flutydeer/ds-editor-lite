# DS Editor Lite

开发文档：[AGENTS.md](./AGENTS.md)

语言层（G2P 与 linguist）：[wolf](https://github.com/diffscope/wolf)；分析模型：[otter](https://github.com/diffscope/otter)；推理引擎：[synthrt](https://github.com/diffscope/synthrt)

## 接手入口

- **文档总索引**：[docs/README.md](./docs/README.md)。
- **本分支目标**：将编辑器迁移到 synthrt 主线。引擎由 `synthrt` 提供，`wolf`（语言学）与 `otter`（分析模型）构建于其上，三者均通过本仓库自带的 overlay 端口引入。**依赖所固定的提交以 `scripts/vcpkg-ports/{synthrt,wolf,otter}/portfile.cmake` 为准**，提交 SHA 与分支名不在此处复制。
- **构建与测试**：按 [AGENTS.md](./AGENTS.md) 使用 vcpkg 与 CMake presets 构建；测试由 `-DLITE_BUILD_TESTS=ON` 开启（`debug` preset 默认开启）。
- **端口与模型包**：四个端口（`synthrt`、`wolf`、`otter` 与 wolf 语言包数据 `wolf-lang-packages`）位于本仓库 `scripts/vcpkg-ports/` 下；端口为何放在本仓库、manifest 的覆盖顺序，以及 otter 模型包（由 otter 的 release `models-v0.3.0.0` 提供）的来源，见 [synthrt-main-migration.md](./docs/plans/synthrt-main-migration.md) §1–§2。
