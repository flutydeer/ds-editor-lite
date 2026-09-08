# 四期测试报告

## 1. 当前结论

实施中，Windows 已通过完整本地验证；Linux CI 和覆盖率分析仍在进行。初始源码基线：`a8fac646`。本报告只填写实际运行事实。

## 2. 候选与执行摘要

Windows 受测提交 `8a525d39`，Qt 6.11.2、Visual Studio 2026 18.9 / MSVC 14.51，synthrt 项目补丁已同步。使用 `tests` preset 完整构建 Editor、Connector 和 `lite_tests`。`ctest --preset local` 共注册 70 项，69 项通过，`TestHeadlessResources` 因未设置声库而跳过，耗时 56.35 秒。日志与 JUnit 位于本地 `build/test-results/local.log`、`local.xml`。

| 类别 | 已执行程序/注册项 | 当前结果 |
|---|---:|---|
| 基础数据与算法（unit） | 19 | 全部通过 |
| 编辑与应用状态（domain） | 13 | 全部通过 |
| 文档、文件与异步（workflow） | 7 | 6 项通过，1 项资源测试跳过 |
| 自动化接口与 Connector（protocol） | 9 | 全部通过 |
| 真实应用进程（process） | 2 | 全部通过；另有 GUI 条件下的进程场景 |
| 界面组件与交互（gui） | 20 | 全部通过，包含真实钢琴窗输入事件及剪贴板 |

资源测试同时具有 workflow 标签，表中数量为 CTest 标签项，不将跳过项计作通过。该轮包含第二次长用例拆分及两处生产修复；覆盖率插桩在 Linux 执行。计划见[test-plan.md](test-plan.md)，覆盖去向见[test-coverage-matrix.md](test-coverage-matrix.md)。

## 3. CI 调试与缺陷修复

| 症状 | 根因 | 修复提交 | 验证结果与证据 |
|---|---|---|---|
| Qt 安装在测试前失败 | 安装器的 State Machines 包名是 `qtscxml`，并非 CMake 组件名 `qtstatemachine` | `c86bb0ed` | 首次运行 [34265782992](https://github.com/flutydeer/ds-editor-lite/actions/runs/34265782992)；后续 Qt 安装通过 |
| Linux 依赖构建失败 | 锁定的 wolf-midi 使用 `std::log2` 但未包含 `<cmath>`，GCC 拒绝编译 | `cc8f791f` 项目级单端口补丁 | [34266042711](https://github.com/flutydeer/ds-editor-lite/actions/runs/34266042711)，下一轮 wolf-midi 通过 |
| libxcrypt 的 autotools 配置失败 | runner 缺少 `autoconf-archive` 和 `libltdl-dev` | `1c8758c1` | [34267466480](https://github.com/flutydeer/ds-editor-lite/actions/runs/34267466480)，新运行复验中 |
| synthrt 在第 51/55 个依赖失败 | 锁定源码使用 `std::unique_lock` 却未包含 `<mutex>` | `5ad98ef3` | [34269205282](https://github.com/flutydeer/ds-editor-lite/actions/runs/34269205282)，前 50 项含 libxcrypt 已通过 |
| Qt HttpServer 配置失败 | Qt 6.11.2 HttpServer 依赖 WebSockets，独立 Qt 下载没有自动补齐该模块 | 显式安装 `qtwebsockets` | [34271852483](https://github.com/flutydeer/ds-editor-lite/actions/runs/34271852483)，全部依赖已通过并保存缓存，失败点进入应用 configure |
| Windows 首轮 CTest 缺测试程序且无 Qt Test 细节 | ICU wrapper 原有测试未进入聚合目标；无控制台时 Qt Test 文本日志进入 Windows debugger | 将 wrapper 纳入统一注册/构建；测试环境强制捕获日志 | 首轮 69 项有 2 项失败：wrapper 未运行、快捷键用例失败；随后定向复验 |
| 快捷键场景没有触发 | Qt 的 widget shortcut 要求 owner 可见，旧 fixture 只调用业务操作，未建立真实焦点环境 | `3344a8a5` | 定向 11 个 Qt Test 结果通过，随后 Windows 全量通过 |
| 新编辑分支可能误判为已保存 | 丢弃 redo 分支时保存点仍引用已销毁条目，后续分配复用地址即错误匹配 | `aa1bf5c8` | 新用例先复现，修后 DocumentWorkflow 17 项和 ProjectConverterAtomicWrite 8 项 Qt Test 结果全部通过 |
| 拆分后的 MCP 文档/退出 fixture 提前失败 | fixture 没有声明 `client_ref`，却依赖该映射取得新轨道 ID | `8ccd041f` | 补齐真实调用所需的标识，文档和退出断言恢复执行；定向 MCP 8 项通过 |
| 次实例报告转发发送失败 | `waitForBytesWritten` 在同步排空或管道关闭时可返回 false，无条件等待误判发送状态 | `8a525d39` | 依据 Qt 6.11.2 实现检查等待前后未发送字节，最终仍验证 ACK；定向 MCP 8 项、SingleInstance 8 项通过。未捕获瞬时 pending/available 数值，不将推断写成观测事实 |

仅记录有意义的失败及闭环，同一根因合并；原始日志留在 Actions artifacts 或本地产物目录。

## 4. 平台与资源执行范围

Linux CI、Windows 本地、原生桌面及资源依赖用例分别记录。未执行、不适用和通过分开说明。

另对 `LITE_BUILD_TESTS=OFF` 做过限定的依赖/属性静态检查：Core/Runtime、Connector、Qt 资源、翻译、平台定义和 ICU 生产目标不依赖测试目录；未发现确定遗漏。该检查不等同于实际 Release 构建，本期完整构建结果均来自 Debug 测试配置。

## 5. 覆盖缺口与补测

全树职责复查除整理存量测试，还识别并补齐以下行为：

- 实际 DSPX 保存/打开的音符、编辑发音、片段范围、draw/anchor 参数、声线混合和变拍内容保真；不比较整份 JSON。
- 真实 ClipboardController/QClipboard 的复制、按活动片段/播放位置粘贴、剪切的一次撤销，以及无效 MIME/内容不编辑。使用现有钢琴窗 GUI 目标，定向通过。
- 保存分支丢弃后的 dirty 状态及地址复用风险；补测确认生产缺陷并修复。

行和分支覆盖率由 Linux GCC/gcovr 的实际运行产出，当前仍待首次成功采样，不能用测试程序数量代替覆盖率。

## 6. 审查问题处理

实际审查后记录问题、判断、修复及回复链接。最终 bot 认可留在 PR 和交付答复，不为复制最终 reaction 再制造提交。

## 7. 最终通过条件

当前受审版本的规定测试与 Actions 通过，真实问题修复并 resolved，且 bot 对当前轮次给出 thumbs-up 或明确无问题。此前不声明目标完成。
