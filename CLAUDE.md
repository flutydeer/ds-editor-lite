# ds-editor-lite

C++20 / Qt 6 Widgets desktop application (singing voice editor).

## Build prerequisites

- CMake 3.21+, Ninja, C++20 compiler
- Windows: Visual Studio 2026 MSVC x64 开发环境 + Qt 6.11.1+ + vcpkg 依赖
- 详细环境配置步骤 → `AGENTS.md`（Configure and build 章节）
- 初次搭建完整指南 → `docs/1 配置开发环境.md`

## Configure & Build

```bash
cmake --preset debug
cmake --build --preset debug
```

For release:
```bash
cmake --preset release && cmake --build --preset release
```

## Agent behavior

- **构建**：优先加载 `cmake-build` skill，使用 CLion MCP 构建（`clion_execute_run_configuration` with `configurationName="DsEditorLite"`）。如果 CLion 不可用则回退到 CMake presets 手动静默
- 代码风格：遵循 `.clang-format`（LLVM-based, 4-space indent, 100 columns, PointerAlignment: Right, SortIncludes: Never）
- 文档和注释主要为中文，项目结构见 AGENTS.md
