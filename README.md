# DS Editor Lite

开发文档：[AGENTS.md](./AGENTS.md)

语言分析器/G2p开发文档：[language-manager](https://github.com/wolfgitpr/language-manager)

## 接手入口

- **文档总索引**：[docs/README.md](./docs/README.md)（先看这张）。
- **本分支目标**：将编辑器迁移到 synthrt 主线。引擎由 `synthrt` 提供，`wolf`（语言学）与 `otter`（分析模型）构建于其上，三者均通过本仓库自带的 overlay 端口引入。**依赖钉的权威来源是 `scripts/vcpkg-ports/{synthrt,wolf,otter}/portfile.cmake`**，提交 sha 与分支名不在此处复制。
- **构建与测试**：按 [AGENTS.md](./AGENTS.md) 走 vcpkg + CMake presets；测试经 `-DLITE_BUILD_TESTS=ON`（`debug` preset 默认开）。
- **端口位置**：三个端口位于本仓库的 `scripts/vcpkg-ports/` 下。manifest 将该目录排在共享 overlay（子模块 `scripts/vcpkg`）之前，因此本仓库的 `synthrt` 端口覆盖 overlay 中跟随 refactor 线的同名端口。`synthrt`、`wolf` 与 `otter` 均为公开仓库，安装时不需要凭据。端口不放在子模块中，因为子模块指向独立的 overlay 仓库：端口若位于该仓库，必须先推送该仓库，本仓库的依赖才能安装。
