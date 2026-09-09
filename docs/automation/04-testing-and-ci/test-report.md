# 四期测试报告

## 1. 当前结论

实施中。候选 `3cb55146` 的 Windows 完整构建和本地验证通过：71 项中 70 项通过、1 项资源测试跳过。此前 Linux `59aa9cd0` 完整构建和首轮覆盖率采样完成，67 项测试中 64 项通过、3 项失败；补测和运行时修复的 Linux 验证继续执行。初始源码基线：`a8fac646`。

## 2. 候选与执行摘要

Windows 受测提交 `59aa9cd0`，Qt 6.11.2、Visual Studio 2026 18.9 / MSVC 14.51，synthrt 项目补丁已同步。使用 `tests` preset 完整构建 Editor、Connector 和 `lite_tests`。`ctest --preset local` 共注册 70 项，69 项通过，`TestHeadlessResources` 因未设置声库而跳过，耗时 56.64 秒。已核对 Qt Test 逐例输出，没有其他跳过项。构建日志、环境、运行日志和 JUnit 已归档到本地 `build/test-results/windows-59aa9cd0/`，避免后续运行覆盖证据。

Linux [Actions 34278449245](https://github.com/flutydeer/ds-editor-lite/actions/runs/34278449245) 对应 PR head `59aa9cd0`，实际 checkout 的 PR 合并提交为 `ecceffbcb669616b45a26a1ce5f442a61973704d`。环境为 Ubuntu 24.04 x64、GCC 13.3.0、Qt 6.11.2、CMake 3.31.6、Ninja 1.13.2，runner 4 CPU、约 15 GiB 内存。完整构建成功，CTest 耗时 70.44 秒，coverage 步骤成功；该轮整体失败。

| 类别 | Windows local | Linux ci |
|---|---|---|
| 基础数据与算法（unit） | 19 项通过 | 19 项通过 |
| 编辑与应用状态（domain） | 13 项通过 | 13 项通过 |
| 文档、文件与异步（workflow） | 6 项通过，1 项资源测试跳过 | 6 项通过；资源测试不在集合中 |
| 自动化接口与 Connector（protocol） | 9 项通过 | 8 项通过，DsConnectorLite 失败 |
| 真实应用进程（process） | 2 项通过 | MCP、Headless 两项失败 |
| 界面组件与交互（gui） | 20 项通过 | 18 项通过；2 项原生桌面不在集合中 |

资源测试同时具有 workflow 标签，表中数量为 CTest 标签项，不将跳过项计作通过。GUI 条件下的跨 Host 进程场景归在 gui 标签。Linux 原始产物已下载到 `build/actions-34278449245/`，其中 `build/test-results/` 保留环境、CTest、覆盖率和失败沙箱。计划见[test-plan.md](test-plan.md)，覆盖去向见[test-coverage-matrix.md](test-coverage-matrix.md)。

后续 Windows 工作树定向执行中，`TestNoteTransfer`、`TestLyricRules`、`TestInferenceWorkflow`、`TestDsConnectorLite` 和 `TestMcpProcessIntegration` 共 5 项 CTest 通过，耗时 36.10 秒。`TestPianoRollGuiIntegration` 完整目标随后通过，耗时 2.75 秒，包含拖动/取消、整片段剪贴板和公开播放失败。无设备场景通过关闭该测试进程持有的设备建立，避免无效设备名回退到默认设备。领域补测/生产共享提交为 `16d4ffa9`，GUI/播放修复为 `607fc7e9`，跨平台进程修复为 `3cb55146`。这些定向执行发生在包含待提交改动的工作树上，不作为某一干净 SHA 的完整验收；随后在 `3cb55146` 完成完整构建及 `local` 全量执行：71 项中 70 项通过、1 项声库资源测试跳过，耗时 56.63 秒。六类依次为 unit 19、domain 13、workflow 7 通过及 1 跳过、protocol 9、process 2、gui 20；CI 标签为 68 项。核对 Qt Test 输出只有资源测试跳过。原始构建、JUnit、执行日志和环境归档于 `build/test-results/windows-3cb55146/`。

## 3. CI 调试与缺陷修复

| 症状 | 根因 | 修复提交 | 验证结果与证据 |
|---|---|---|---|
| Qt 安装在测试前失败 | 安装器的 State Machines 包名是 `qtscxml`，并非 CMake 组件名 `qtstatemachine` | `c86bb0ed` | 首次运行 [34265782992](https://github.com/flutydeer/ds-editor-lite/actions/runs/34265782992)；后续 Qt 安装通过 |
| Linux 依赖构建失败 | 锁定的 wolf-midi 使用 `std::log2` 但未包含 `<cmath>`，GCC 拒绝编译 | `cc8f791f` 项目级单端口补丁 | [34266042711](https://github.com/flutydeer/ds-editor-lite/actions/runs/34266042711)，下一轮 wolf-midi 通过 |
| libxcrypt 的 autotools 配置失败 | runner 缺少 `autoconf-archive` 和 `libltdl-dev` | `1c8758c1` | [34267466480](https://github.com/flutydeer/ds-editor-lite/actions/runs/34267466480)，后续依赖完整构建通过 |
| synthrt 在第 51/55 个依赖失败 | 锁定源码使用 `std::unique_lock` 却未包含 `<mutex>` | `5ad98ef3` | [34269205282](https://github.com/flutydeer/ds-editor-lite/actions/runs/34269205282)，前 50 项含 libxcrypt 已通过 |
| Qt HttpServer 配置失败 | Qt 6.11.2 HttpServer 依赖 WebSockets，独立 Qt 下载没有自动补齐该模块 | `d3c80289` | [34271852483](https://github.com/flutydeer/ds-editor-lite/actions/runs/34271852483)，补齐后 configure 通过 |
| Connector 在 GCC 编译失败 | `QUrl = {}` 在 QUrl/QString 赋值重载间存在歧义 | `a82deb35` | [34274326884](https://github.com/flutydeer/ds-editor-lite/actions/runs/34274326884)，后续该源已编译通过；采用 Ninja `-k 0` 收集独立目标错误，仍保留失败退出码 |
| Speaker Mix 界面头文件无法编译 | `QLabel` 类型依赖了其他头文件的间接声明 | `6e02dc94` | [34275666363](https://github.com/flutydeer/ds-editor-lite/actions/runs/34275666363)，补前置声明；后续 [34278449245](https://github.com/flutydeer/ds-editor-lite/actions/runs/34278449245) Linux 完整构建通过 |
| 音频控制器测试在 Linux 链接失败 | Qt moc/vtable 引用的测试替身虚析构缺少定义 | `6e02dc94` | 同一 [run](https://github.com/flutydeer/ds-editor-lite/actions/runs/34275666363)，补默认析构；后续 Windows/Linux 用例通过 |
| 工程转换的消费者缺少 `ucsdet_*` 符号 | ProjectConverters 实现使用 ICU 检测 API，却只声明了 uc 依赖 | `6e02dc94` | 同一 [run](https://github.com/flutydeer/ds-editor-lite/actions/runs/34275666363)，在生产库补 i18n 私有依赖，未逐个测试程序加库；后续 Linux 完整构建通过 |
| Linux stdio 用例找不到 Connector | 测试路径硬编码 Windows 的 `.exe` 后缀 | `3cb55146` | [34278449245](https://github.com/flutydeer/ds-editor-lite/actions/runs/34278449245)，现改用 CMake `$<TARGET_FILE:DsConnectorLite>`；Windows 工作树定向通过，Linux 待验证 |
| GUI Host 的 MCP `playback.play` 超时 | 无音频设备时仅按 QApplication 判断交互性，弹出模态错误框阻塞公开调用 | `607fc7e9` | 同一 [run](https://github.com/flutydeer/ds-editor-lite/actions/runs/34278449245)；现限制为 TrustedGui 弹窗，并补失败后状态及查询断言；Windows MCP 与确定性无设备 GUI 用例通过，Linux 待验证 |
| Headless 重启后替代进程未就绪 | 替代进程继承原 QProcess 管道，其存活期不足是当前排查假设；尚无直接 SIGPIPE 证据。复查另发现非 Windows 存活/所有权/清理实现缺失 | `3cb55146` | 同一 [run](https://github.com/flutydeer/ds-editor-lite/actions/runs/34278449245)；现将输出写入沙箱文件并补跨平台进程管理，保留新进程身份、参数、就绪和退出断言；Linux 待验证 |
| Windows 首轮 CTest 缺测试程序且无 Qt Test 细节 | ICU wrapper 原有测试未进入聚合目标；无控制台时 Qt Test 文本日志进入 Windows debugger | 将 wrapper 纳入统一注册/构建；测试环境强制捕获日志 | 首轮 69 项有 2 项失败：wrapper 未运行、快捷键用例失败；随后定向复验 |
| 快捷键场景没有触发 | Qt 的 widget shortcut 要求 owner 可见，旧 fixture 只调用业务操作，未建立真实焦点环境 | `3344a8a5` | 定向 11 个 Qt Test 结果通过，随后 Windows 全量通过 |
| 新编辑分支可能误判为已保存 | 丢弃 redo 分支时保存点仍引用已销毁条目，后续分配复用地址即错误匹配 | `aa1bf5c8` | 新用例先复现，修后 DocumentWorkflow 17 项和 ProjectConverterAtomicWrite 8 项 Qt Test 结果全部通过 |
| 拆分后的 MCP 文档/退出 fixture 提前失败 | fixture 没有声明 `client_ref`，却依赖该映射取得新轨道 ID | `8ccd041f` | 补齐真实调用所需的标识，文档和退出断言恢复执行；定向 MCP 8 项通过 |
| 次实例报告转发发送失败 | `waitForBytesWritten` 在同步排空或管道关闭时可返回 false，无条件等待误判发送状态 | `8a525d39` | 依据 Qt 6.11.2 实现检查等待前后未发送字节，最终仍验证 ACK；定向 MCP 8 项、SingleInstance 8 项通过。未捕获瞬时 pending/available 数值，不将推断写成观测事实 |

仅记录有意义的失败及闭环，同一根因合并；原始日志留在 Actions artifacts 或本地产物目录。

## 4. 平台与资源执行范围

Linux CI、Windows 本地、原生桌面及资源依赖用例分别记录。未执行、不适用和通过分开说明。

目前没有提供实际声库资源，`TestHeadlessResources` 在 Windows 明确跳过，Linux CI 不选择该项；模型输出、GPU 和实际听感不据此记为通过。当前 Linux 通过集合包含 offscreen 组件及真实钢琴窗事件，原生桌面两项仅在 Windows 执行。最终候选的空缓存和正常缓存命中完整运行尚未完成。

另对 `LITE_BUILD_TESTS=OFF` 做过限定的依赖/属性静态检查：Core/Runtime、Connector、Qt 资源、翻译、平台定义和 ICU 生产目标不依赖测试目录；未发现确定遗漏。该检查不等同于实际 Release 构建，本期完整构建结果均来自 Debug 测试配置。

## 5. 覆盖缺口与补测

全树职责复查除整理存量测试，还识别并补齐以下行为：

- 实际 DSPX 保存/打开的音符、编辑发音、片段范围、draw/anchor 参数、声线混合和变拍内容保真；不比较整份 JSON。
- 真实 ClipboardController/QClipboard 的复制、按活动片段/播放位置粘贴、剪切的一次撤销，以及无效 MIME/内容不编辑。使用现有钢琴窗 GUI 目标，定向通过。
- 保存分支丢弃后的 dirty 状态及地址复用风险；补测确认生产缺陷并修复。

### 5.1. 首轮实测覆盖率

来源为上文失败候选的 GCC/gcovr 8.6 采样：**行 48,030 / 102,317（46.9%），分支 45,250 / 118,151（38.3%）**。完整 JSON、目录/文件 HTML 和汇总位于 `build/actions-34278449245/build/test-results/coverage/`，对应 Actions artifacts。统计覆盖 `src/app`、`src/connector`、`src/libs` 和已编译的 `src/tools` 源码，排除测试、第三方、生成代码及编译器异常清理等非目标分支；Linux 未编译的平台实现不在分母中。

以下为与本期判断相关的目录汇总，完整文件列表以产物为准：

| 生产目录 | 已覆盖/总行数 | 行覆盖 | 已覆盖/总分支数 | 分支覆盖 |
|---|---:|---:|---:|---:|
| app/Automation | 13,312 / 23,340 | 57.0% | 12,828 / 28,928 | 44.3% |
| app/Bootstrap | 1,055 / 1,271 | 83.0% | 969 / 1,501 | 64.6% |
| app/Controller | 2,772 / 5,019 | 55.2% | 2,079 / 5,025 | 41.4% |
| app/Model | 992 / 1,650 | 60.1% | 1,298 / 2,473 | 52.5% |
| app/Modules | 2,012 / 13,164 | 15.3% | 1,961 / 14,218 | 13.8% |
| app/UI | 11,163 / 32,406 | 34.4% | 8,288 / 33,148 | 25.0% |
| connector | 2,232 / 2,634 | 84.7% | 2,558 / 4,172 | 61.3% |
| libs/AutomationWire | 4,821 / 5,291 | 91.1% | 9,000 / 14,715 | 61.2% |
| libs/History | 140 / 144 | 97.2% | 101 / 121 | 83.5% |
| libs/ProjectModel | 2,319 / 3,148 | 73.7% | 1,632 / 2,607 | 62.6% |

该轮 3 项测试失败，数值表示这次实际运行触达的代码，不表示最终候选已通过。后续补测新增的源码、注册项和执行结果不混入本轮分母。

### 5.2. 从未执行行为选择补测

| 实测缺口 | 产品风险与本轮处置 | 当前验证状态 |
|---|---|---|
| ClipsInfo 0 / 250 行 | 整片段剪贴板遗漏参数；复用曲线编码，补 Original/Edited/Envelope、音符范围外及无音符曲线，跨轨粘贴保留发音/声线并一次撤销。GUI 控制器入口同时补测 | NoteTransfer 与 GUI 入口 Windows 工作树定向通过；Linux 待验证 |
| InferenceApplyGate 0 / 189 行 | 旧输入转换用例没有进入结果门控；新增真实任务快照的 Apply/Drop/Defer，检查四阶段输入变化、文档/片段消失、无关 revision 变化和冲突编辑 | 新增 InferenceWorkflow，Windows 工作树定向通过；Linux 待验证 |
| TextSplitter 0 / 178 行 | 规则生效和文字保真遗漏；补混合文字、启用/优先级及空匹配，修复 `(a*)` 对 `a中` 产生重复前缀的实现 | FillLyricTaggerOrder 改名为 LyricRules，Windows 工作树定向通过；Linux 待验证 |
| NoteInteractionController 8 / 112 行 | 绘制新音符不能覆盖已有音符拖动；扩展真实鼠标拖动提交及 Escape 取消，核对预览、场景、模型和撤销 | Windows GUI 目标通过；完整候选及 Linux 待验证 |

上述缺口由源码及覆盖产物确认；新增测试已完成上述 Windows 定向验证，但没有修复前的实际失败运行记录，不把源码分析写成已执行的回归复现。InferenceWorkflow 使用真实 Headless AppContext 和未调度的任务快照，验证完成门控，不依赖实际声库输出。当前重构还把歌词和剪贴板生产源码收敛到共享 target，消除重复编译；新目标构建发现的 SynthrtEngine 提取模块依赖已补在库自身。

较低的 UI/Modules 数字还包含原生渲染、设备/模型和交互对话框路径；按[覆盖矩阵](test-coverage-matrix.md)保留对应运行条件，不为追求覆盖率新增跨平台像素基线、设备组合或无语义 getter 测试。完成这批重要补测后重新采样，依据变化后的具体行为判断剩余缺口。

## 6. 审查问题处理

实际审查后记录问题、判断、修复及回复链接。最终 bot 认可留在 PR 和交付答复，不为复制最终 reaction 再制造提交。

## 7. 最终通过条件

当前受审版本的规定测试与 Actions 通过，真实问题修复并 resolved，且 bot 对当前轮次给出 thumbs-up 或明确无问题。此前不声明目标完成。
