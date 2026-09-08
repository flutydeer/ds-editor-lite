# 四期测试报告

## 1. 当前结论

实施中，Windows 已通过完整本地验证；Linux CI 和覆盖率分析仍在进行。初始源码基线：`a8fac646`。本报告只填写实际运行事实。

## 2. 候选与执行摘要

Windows 受测提交 `3344a8a5`，Qt 6.11.2、Visual Studio 2026 18.9 / MSVC 14.51，使用 `tests` preset 完整构建 Editor、Connector 和 `lite_tests`。`ctest --preset local` 共注册 69 项，68 项通过，`TestHeadlessResources` 因未设置声库而跳过，耗时 41.49 秒。日志与 JUnit 位于本地 `build/test-results/local.log`、`local.xml`。

| 类别 | 已执行程序/注册项 | 当前结果 |
|---|---:|---|
| 基础数据与算法（unit） | 19 | 全部通过 |
| 编辑与应用状态（domain） | 13 | 全部通过 |
| 文档、文件与异步（workflow） | 7 | 6 项通过，1 项资源测试跳过 |
| 自动化接口与 Connector（protocol） | 9 | 全部通过 |
| 真实应用进程（process） | 2 | 全部通过；另有 GUI 条件下的进程场景 |
| 界面组件与交互（gui） | 19 | 全部通过，包含真实钢琴窗输入事件 |

资源测试同时具有 workflow 标签，表中数量为 CTest 标签项，不将跳过项计作通过。当前这轮发生在追加覆盖率及第二次长用例拆分之前，后续候选需更新实测记录。计划见[test-plan.md](test-plan.md)，覆盖去向见[test-coverage-matrix.md](test-coverage-matrix.md)。

## 3. CI 调试与缺陷修复

| 症状 | 根因 | 修复提交 | 验证结果与证据 |
|---|---|---|---|
| Qt 安装在测试前失败 | 安装器的 State Machines 包名是 `qtscxml`，并非 CMake 组件名 `qtstatemachine` | `c86bb0ed` | 首次运行 [34265782992](https://github.com/flutydeer/ds-editor-lite/actions/runs/34265782992)；后续 Qt 安装通过 |
| Linux 依赖构建失败 | 锁定的 wolf-midi 使用 `std::log2` 但未包含 `<cmath>`，GCC 拒绝编译 | `cc8f791f` 项目级单端口补丁 | [34266042711](https://github.com/flutydeer/ds-editor-lite/actions/runs/34266042711)，下一轮 wolf-midi 通过 |
| libxcrypt 的 autotools 配置失败 | runner 缺少 `autoconf-archive` 和 `libltdl-dev` | `1c8758c1` | [34267466480](https://github.com/flutydeer/ds-editor-lite/actions/runs/34267466480)，新运行复验中 |
| Windows 首轮 CTest 缺测试程序且无 Qt Test 细节 | ICU wrapper 原有测试未进入聚合目标；无控制台时 Qt Test 文本日志进入 Windows debugger | 将 wrapper 纳入统一注册/构建；测试环境强制捕获日志 | 首轮 69 项有 2 项失败：wrapper 未运行、快捷键用例失败；随后定向复验 |
| 快捷键场景没有触发 | Qt 的 widget shortcut 要求 owner 可见，旧 fixture 只调用业务操作，未建立真实焦点环境 | `3344a8a5` | 定向 11 个 Qt Test 结果通过，随后 Windows 全量通过 |

仅记录有意义的失败及闭环，同一根因合并；原始日志留在 Actions artifacts 或本地产物目录。

## 4. 平台与资源执行范围

Linux CI、Windows 本地、原生桌面及资源依赖用例分别记录。未执行、不适用和通过分开说明。

## 5. 审查问题处理

实际审查后记录问题、判断、修复及回复链接。最终 bot 认可留在 PR 和交付答复，不为复制最终 reaction 再制造提交。

## 6. 最终通过条件

当前受审版本的规定测试与 Actions 通过，真实问题修复并 resolved，且 bot 对当前轮次给出 thumbs-up 或明确无问题。此前不声明目标完成。
