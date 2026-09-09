# 四期测试报告

## 1. 当前结论

`b0d97a98` 的 Windows 完整构建和本地验证通过：71 项中 70 项通过、1 项资源测试跳过，686 个行为/数据行通过。此前 Linux `153a359d` 的完整空缓存及同 SHA 缓存命中运行均 68 项全部通过，coverage 成功。包含普通音频导出补测和 CSV 输出的新候选继续执行完整 Linux CI；PR 审查尚未开始。初始源码基线：`a8fac646`。

## 2. 候选与执行摘要

### 2.1. 已验证候选

Windows 受测代码提交 `b0d97a98`，Qt 6.11.2、Visual Studio 2026 18.9 / MSVC 14.51。使用 `tests` preset 完整构建 Editor、Connector 和 `lite_tests`，`ctest --preset local` 71 项中 70 项通过，`TestHeadlessResources` 因未设置声库而跳过，耗时 58.92 秒。完整 LastTest 记录 686 个行为/数据行通过及 1 项资源跳过，不计 init/cleanup；构建、JUnit、环境和完整日志归档于 `build/test-results/windows-b0d97a98/`。

Linux [Actions 34310458032 attempt 1](https://github.com/flutydeer/ds-editor-lite/actions/runs/34310458032/attempts/1) 对应 PR head `153a359d778b4a6fdc6ef33e54bf5be53069bc49`，该提交仅在上述受测代码上追加文档；实际 checkout 的 PR 合并提交为 `366714225902c404c69da6c82fc2f70b9bb8d2c7`。Ubuntu 24.04 x64、GCC 13.3.0、Qt 6.11.2、CMake 3.31.6、Ninja 1.13.2，runner 4 CPU、约 15 GiB 内存。Qt 和 vcpkg 均未命中缓存，依赖源码构建、完整产品/测试构建、68 项测试和 coverage 全部通过；CTest 耗时 29.90 秒。同一 head/checkout 的 [attempt 2](https://github.com/flutydeer/ds-editor-lite/actions/runs/34310458032/attempts/2) 正常命中缓存，完整构建、68 项测试和 coverage 全部通过，CTest 耗时 35.72 秒。

| 类别 | Windows local | Linux ci |
|---|---|---|
| 基础数据与算法（unit） | 19 项通过 | 19 项通过 |
| 编辑与应用状态（domain） | 13 项通过 | 13 项通过 |
| 文档、文件与异步（workflow） | 7 项通过，1 项资源测试跳过 | 7 项通过；资源测试不在集合中 |
| 自动化接口与 Connector（protocol） | 9 项通过 | 9 项通过 |
| 真实应用进程（process） | 2 项通过 | 2 项通过 |
| 界面组件与交互（gui） | 20 项通过 | 18 项通过；2 项原生桌面不在集合中 |

资源测试同时具有 workflow 标签，表中数量为 CTest 标签项，不将跳过项计作通过；行为/数据行数量仅记录本轮执行，不作为后续数量约束。GUI 条件下的跨 Host 进程场景归在 gui 标签。CI 的完整逐例日志由 Actions artifacts 保存，JUnit 对通过用例的输出可能截断。计划见[test-plan.md](test-plan.md)，覆盖去向见[test-coverage-matrix.md](test-coverage-matrix.md)。

### 2.2. 阶段验证记录

首轮 `59aa9cd0` 的 Windows 全量 69 项通过、1 项资源跳过，56.64 秒，产物归档于 `build/test-results/windows-59aa9cd0/`。同候选 [Actions 34278449245](https://github.com/flutydeer/ds-editor-lite/actions/runs/34278449245) 的实际 checkout 为 `ecceffbcb669616b45a26a1ce5f442a61973704d`，完整构建及 coverage 成功，64/67 项通过，70.44 秒；该轮失败，原始产物在 `build/actions-34278449245/`。

后续 Linux [Actions 34304622902](https://github.com/flutydeer/ds-editor-lite/actions/runs/34304622902) 的 PR head 为 `6407433f61435df38c52e9aea1eba4168bd4c50f`，实际 checkout 为 `b50c0bfe6776df062c25541793226e987ecf1186`；系统、编译器、Qt 和工具版本与上一轮相同。完整构建和 coverage 成功，CTest 耗时 37.18 秒；unit 19、domain 13、workflow 7、process 2、gui 18 全部通过，protocol 9 项中 8 项通过，唯一失败为 `TestDsConnectorLite::stdioFraming` 的阻塞接收端启动。该轮仍整体失败，完整产物归档于 `build/actions-34304622902/`。

空缓存 [Actions 34306312498](https://github.com/flutydeer/ds-editor-lite/actions/runs/34306312498) 的 PR head 为 `bf72a3bb`，实际 checkout 为 `5e6572f6412c711c5d03a626aad97a95d5649857`，环境版本与前两轮相同。Qt、全部 vcpkg 依赖源码构建、完整应用/测试构建及 coverage 通过，CTest 耗时 36.57 秒。unit 19、domain 13、workflow 7、protocol 9、process 2 全部通过；gui 18 项中 17 项通过，仅 `TestHeadlessCrossHostIntegration::crossHost` 的退出请求收到 `Connection closed`。该轮整体失败；完整产物已校验并归档于 `build/actions-34306312498/`。

领域补测/生产共享提交为 `16d4ffa9`，GUI/播放修复为 `607fc7e9`，跨平台进程修复为 `3cb55146`。定向验证后，在 `3cb55146` 完成 Windows 完整构建及 local 全量执行：70 项通过、1 项声库资源跳过，56.63 秒，产物归档于 `build/test-results/windows-3cb55146/`。

`3cb55146` 的完整 `LastTest.log` 另记录 Qt Test 行为/数据行：unit 156、domain 145、workflow 89、protocol 131、process 11、gui 151，共 683 项通过，资源用例 1 项跳过；不计 `initTestCase`/`cleanupTestCase`。这是本轮执行统计，不作为后续必须保持的数量约束。CTest JUnit 对通过用例的输出默认截断，逐例核对使用已归档的 `build/test-results/windows-3cb55146/LastTest.log`；Linux 对应完整日志由 Actions artifacts 保存。

修复 stdio 替身后的 `bf72a3bb` 再次完成 Windows 全量构建与 local 测试：70 项通过、1 项资源测试跳过，耗时 55.86 秒。完整 LastTest 同样记录 683 个行为/数据行通过及 1 项资源跳过。该提交的 `local.log`、`local.xml`、`LastTest.log`、`build.log` 和 `environment.txt` 归档于 `build/test-results/windows-bf72a3bb/`。

退出响应受控大响应回归在旧逻辑上失败，`aa673b5a` 修复后正常读取场景通过，`f73d6c9c` 再补不读取客户端仍可停止的场景；这一过程没有放宽跨进程退出响应断言。完整 Windows 结果见上方已验证候选。

新增普通音频导出用例 `755c4e77` 在 Windows Headless 目标通过，整组耗时 33.06 秒，随后纳入上述完整 Windows 验证。`b0d97a98` 增加原生 CSV 覆盖率输出，等待新候选 Linux 运行确认；依赖仍为此前已验证的 wolf-midi 项目补丁和原 overlay 版本，缓存路径不变。

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
| Linux stdio 用例找不到 Connector | 测试路径硬编码 Windows 的 `.exe` 后缀 | `3cb55146` | [34278449245](https://github.com/flutydeer/ds-editor-lite/actions/runs/34278449245)，改用 CMake `$<TARGET_FILE:DsConnectorLite>`；后续 Linux 已进入 stdio 场景 |
| stdio 阻塞接收端无法启动 | 接收端替身依赖 `powershell.exe`，Linux 不存在该程序 | `bf72a3bb` | [34304622902](https://github.com/flutydeer/ds-editor-lite/actions/runs/34304622902)，改为跨平台替身；Windows Connector 定向通过，耗时 35.21 秒，后续 [34306312498](https://github.com/flutydeer/ds-editor-lite/actions/runs/34306312498) Linux Connector 通过 |
| GUI Host 的 MCP `playback.play` 超时 | 无音频设备时仅按 QApplication 判断交互性，弹出模态错误框阻塞公开调用 | `607fc7e9` | 首次 [34278449245](https://github.com/flutydeer/ds-editor-lite/actions/runs/34278449245) 失败；限制为 TrustedGui 弹窗并补失败后状态及查询断言，后续 [34304622902](https://github.com/flutydeer/ds-editor-lite/actions/runs/34304622902) MCP 与无设备 GUI 场景通过 |
| Headless 重启后替代进程未就绪 | 替代进程继承原 QProcess 管道，其存活期不足是排查推断，尚无直接 SIGPIPE 证据；复查另发现非 Windows 存活/所有权/清理实现缺失 | `3cb55146` | 首次 [34278449245](https://github.com/flutydeer/ds-editor-lite/actions/runs/34278449245) 失败；输出改为沙箱文件并补跨平台进程管理，新进程身份、参数、就绪及退出断言在后续 [34304622902](https://github.com/flutydeer/ds-editor-lite/actions/runs/34304622902) 通过 |
| 跨 Host 退出请求响应前连接关闭 | HTTP worker 停止时直接销毁仍有待发送响应的 socket；退出协议要求响应先返回 | `aa673b5a`；`f73d6c9c` 补停止上限场景 | [34306312498](https://github.com/flutydeer/ds-editor-lite/actions/runs/34306312498) 首次出现；6 MiB 受控响应在 Windows 修前复现 RemoteHostClosedError，修后四目标定向通过（35.69 秒），完整读取及不读取客户端场景均通过。连接有序断开并共用 2 秒排空上限，保留原跨进程断言；[最终空缓存运行](https://github.com/flutydeer/ds-editor-lite/actions/runs/34310458032/attempts/1) HTTP 和全部进程场景通过。修前/修后日志在 `build/test-results/shutdown-response/` |
| Windows 首轮 CTest 缺测试程序且无 Qt Test 细节 | ICU wrapper 原有测试未进入聚合目标；无控制台时 Qt Test 文本日志进入 Windows debugger | 将 wrapper 纳入统一注册/构建；测试环境强制捕获日志 | 首轮 69 项有 2 项失败：wrapper 未运行、快捷键用例失败；随后定向复验 |
| 快捷键场景没有触发 | Qt 的 widget shortcut 要求 owner 可见，旧 fixture 只调用业务操作，未建立真实焦点环境 | `3344a8a5` | 定向 11 个 Qt Test 结果通过，随后 Windows 全量通过 |
| 新编辑分支可能误判为已保存 | 丢弃 redo 分支时保存点仍引用已销毁条目，后续分配复用地址即错误匹配 | `aa1bf5c8` | 新用例先复现，修后 DocumentWorkflow 17 项和 ProjectConverterAtomicWrite 8 项 Qt Test 结果全部通过 |
| 拆分后的 MCP 文档/退出 fixture 提前失败 | fixture 没有声明 `client_ref`，却依赖该映射取得新轨道 ID | `8ccd041f` | 补齐真实调用所需的标识，文档和退出断言恢复执行；定向 MCP 8 项通过 |
| 次实例报告转发发送失败 | `waitForBytesWritten` 在同步排空或管道关闭时可返回 false，无条件等待误判发送状态 | `8a525d39` | 依据 Qt 6.11.2 实现检查等待前后未发送字节，最终仍验证 ACK；定向 MCP 8 项、SingleInstance 8 项通过。未捕获瞬时 pending/available 数值，不将推断写成观测事实 |

仅记录有意义的失败及闭环，同一根因合并；原始日志留在 Actions artifacts 或本地产物目录。

## 4. 平台与资源执行范围

Linux CI、Windows 本地、原生桌面及资源依赖用例分别记录。未执行、不适用和通过分开说明。

没有提供实际声库资源，`TestHeadlessResources` 在 Windows 明确跳过，Linux CI 不选择该项；模型输出、GPU 和实际听感不据此记为通过。Linux 通过集合包含 offscreen 组件及真实钢琴窗事件，原生桌面两项在 Windows 执行。macOS 本轮未实际运行，测试实现仍使用当前平台的路径、Qt 和进程接口。

已验证候选 `153a359d` 的[空缓存运行](https://github.com/flutydeer/ds-editor-lite/actions/runs/34310458032/attempts/1)已完整通过。完整 workflow 日志明确记录 Qt `Automatic cache miss`、vcpkg 精确键及回退键均未命中；全部 55 个依赖从源码安装约 19 分钟完成，随后完整构建、68 项测试及 coverage 通过，并生成新的 Qt/vcpkg 缓存。证据归档于 `build/actions-34310458032-1-workflow.log`；这一结果依据实际日志，未以删除缓存动作代替冷路径验证。

同一 SHA 的 [attempt 2](https://github.com/flutydeer/ds-editor-lite/actions/runs/34310458032/attempts/2) 明确记录 Qt 自动命中、vcpkg 精确键恢复成功，55 个包在 4.6 秒内恢复；install 仍执行并成功，完整构建、68 项测试和 coverage 全部通过。证据归档于 `build/actions-34310458032-2-workflow.log`。两轮完整成功已经验证当前依赖的冷/暖路径；后续仅补音频工作流用例，依赖声明与缓存路径不变，仍需在新提交上完整执行测试。

此前 `bf72a3bb` 的 [34306312498](https://github.com/flutydeer/ds-editor-lite/actions/runs/34306312498) 也完成冷依赖及构建，但因退出响应问题失败，仅作为调试记录，不作为空缓存验收通过依据。

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
| ClipsInfo 0 / 250 行 | 整片段剪贴板遗漏参数；复用曲线编码，补 Original/Edited/Envelope、音符范围外及无音符曲线，跨轨粘贴保留发音/声线并一次撤销。GUI 控制器入口同时补测 | NoteTransfer 与 GUI 入口在 Windows、Linux 通过 |
| InferenceApplyGate 0 / 189 行 | 旧输入转换用例没有进入结果门控；新增真实任务快照的 Apply/Drop/Defer，检查四阶段输入变化、文档/片段消失、无关 revision 变化和冲突编辑 | 新增 InferenceWorkflow，Windows、Linux 通过 |
| TextSplitter 0 / 178 行 | 规则生效和文字保真遗漏；补混合文字、启用/优先级及空匹配，修复 `(a*)` 对 `a中` 产生重复前缀的实现 | FillLyricTaggerOrder 改名为 LyricRules，Windows、Linux 通过 |
| NoteInteractionController 8 / 112 行 | 绘制新音符不能覆盖已有音符拖动；扩展真实鼠标拖动提交及 Escape 取消，核对预览、场景、模型和撤销 | GUI 目标在 Windows、Linux 通过 |

上述缺口由源码及覆盖产物确认；新增测试已完成 Windows 和 Linux 验证，但没有修复前的实际失败运行记录，不把源码分析写成已执行的回归复现。InferenceWorkflow 使用真实 Headless AppContext 和未调度的任务快照，验证完成门控，不依赖实际声库输出。当前重构还把歌词和剪贴板生产源码收敛到共享 target，消除重复编译；新目标构建发现的 SynthrtEngine 提取模块依赖已补在库自身。

### 5.3. 补测后的实测变化

`6407433f` 对应的第二轮采样为 **行 49,920 / 102,350（48.8%），分支 47,283 / 118,191（40.0%）**。完整产物位于 `build/actions-34304622902/build/test-results/coverage/`。与首轮相比，多执行了 1,890 行和 2,033 个分支；同时生产共享和修复改变了源码分母，因此以各轮实际分母分别统计。

| 重点生产实现 | 首轮行覆盖 | 补测后行覆盖 | 首轮分支覆盖 | 补测后分支覆盖 |
|---|---:|---:|---:|---:|
| ClipsInfo | 0 / 250（0%） | 239 / 281（85.1%） | 0 / 580（0%） | 460 / 620（74.2%） |
| InferenceApplyGate | 0 / 189（0%） | 149 / 189（78.8%） | 0 / 337（0%） | 203 / 337（60.2%） |
| TextSplitter | 0 / 178（0%） | 140 / 177（79.1%） | 0 / 201（0%） | 137 / 205（66.8%） |
| NoteInteractionController | 8 / 112（7.1%） | 57 / 112（50.9%） | 0 / 68（0%） | 46 / 68（67.6%） |
| 新提取的 ParameterCurvesJson | 原属其他文件 | 84 / 91（92.3%） | 原属其他文件 | 100 / 122（82.0%） |

新增用例确实进入原先缺失的产品行为：整片段参数往返、推理完成门控、歌词拆分及已有音符拖动/取消均在 Linux 通过。目录层面，app/Model 行覆盖由 60.1% 升至 74.0%，app/Modules 由 15.3% 升至 21.3%；具体已执行行仍以原始 HTML/JSON 为准。

后续失败的空缓存 `bf72a3bb` 采样为行 49,936 / 102,350（48.8%），分支 47,299 / 118,191（40.0%），以上五个重点文件的覆盖数与第二轮一致。产物位于 `build/actions-34306312498/build/test-results/coverage/`。

退出响应修复后的全绿冷候选 `153a359d` 采样为 **行 49,961 / 102,375（48.8%），分支 47,336 / 118,228（40.0%）**，来自 [34310458032 attempt 1](https://github.com/flutydeer/ds-editor-lite/actions/runs/34310458032/attempts/1) 的成功 coverage 步骤。测试集合完整通过后重新采样，未将历史失败轮次数据累加进本轮结果。

同一 SHA 的成功暖缓存采样为 **行 49,967 / 102,375（48.8%），分支 47,341 / 118,228（40.0%）**，来源为 attempt 2；两轮分别统计，不将微小的运行路径差异解释为新增功能覆盖。

48.8% 是已编译生产源码的行覆盖，不是已完成产品功能的比例。未覆盖部分既有原生渲染、设备/模型和交互对话框，也有普通产品工作流：复查发现 AudioExporter 仅触达少量初始化代码，因此补充了使用现场生成小 WAV 的纯音频导出闭环，无需声库。Windows 已通过，后续 Linux 重新采样确认覆盖变化。

`TestHeadlessProcessIntegration::audioImportAndWaveExport` 生成短音频，执行真实导入与 WAV 导出任务、等待成功终态，并解码检查格式、时长、有限非零样本及文档版本不变。首轮时长差异来自 fixture 用 `SFM_RDWR` 打开已有素材后追加样本，实际导出正确；补 `sf_seek` 定位首帧后整组通过，保留原 2 ms 容差，没有修改生产导出逻辑。该用例不计入前述 `153a359d` 的结果。

为获取新候选的逐文件统计，workflow 已在 `b0d97a98` 增加 gcovr `files.csv` 并输出到文本日志。下一轮按原生 CSV 核对 AudioExporter 和其他重要目录的实际覆盖；历史 HTML/JSON 仍保留。该改动仅增加测试诊断输出，不建立文档生成机制。

其余低覆盖路径按[覆盖矩阵](test-coverage-matrix.md)标明职责和运行条件；不把它们一概视为不需测试，也不为追求百分比扩展像素基线、设备组合或无语义 getter 测试。实际 CI 发现的退出响应竞态已归入协议生命周期并完成修复及 Linux 验证。

## 6. 审查问题处理

本报告定稿阶段尚未触发首次 ready 审查。当前实现的 CI 故障已修复，后续真实审查问题在本节记录判断、修复及回复链接。最终 bot 认可留在 [PR #187](https://github.com/flutydeer/ds-editor-lite/pull/187) 和交付答复，不为复制最终 reaction 再制造提交。

## 7. 最终通过条件

当前受审版本的规定测试与 Actions 通过，真实问题修复并 resolved，且 bot 对当前轮次给出 thumbs-up 或明确无问题。此前不声明目标完成。
