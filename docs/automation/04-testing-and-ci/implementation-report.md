# 四期实现报告

## 1. 当前状态

测试体系重构和 Linux CI 已落地。`50dc53a0` 使用 overlay `110ca6bb`，Windows 全量测试及 Linux 同一候选的完整冷/暖缓存运行均通过；行覆盖率 49.3%，分支覆盖率 40.4%。初始基线为 `a8fac646`，工作分支为 `test-ci`。实际运行、覆盖率与缺陷闭环见[测试报告](test-report.md)，后续审查状态见 [PR #187](https://github.com/flutydeer/ds-editor-lite/pull/187)。

## 2. 测试结构与生产复用

测试统一采用 Qt Test 的 slot 和 data row，CTest 负责六类标签、超时、运行环境及结果。`lite_tests` 聚合测试构建；`tests` build preset 同时构建 Editor、Connector 与全部测试，补上此前独立于 `src/tests` 的 ICU wrapper 测试。一个测试程序默认对应一条 CTest 注册；Headless 的跨 Host 场景因需要 GUI 平台单独注册。该程序用两个简单 Qt Test 类区分运行条件，CTest 只选择模式，不维护具体函数名单。

`EditorAutomationCore` 从原测试支持目标中提取生产的模型、编辑服务和 Facade 实现，供应用和测试共享。`AutomationTestSupport` 只承接测试 fixture 的使用入口。`EditorRuntime` 对象库承接主程序之外的应用实现、Qt 资源、翻译和界面依赖；Editor 与钢琴窗集成测试链接同一实现，避免在测试中重造编辑器。

生产源码按组件收敛：整片段剪贴板与共享曲线 JSON 编码、歌词配置及拆分/标记规则进入 `EditorAutomationCore`，测试直接链接该目标，消除各消费者重复编译。整片段与音符复制共用曲线编码；TrackController 与领域测试共用粘贴位置准备逻辑。

大入口按行为拆分，数据驱动用例每行重建状态。配置持久化、无效请求副作用、音频解码换代、任务取消与提交点等回归保留在各自所属层；代表性编辑序列比较直接 Facade、Registry、Native 和 MCP 的业务结果。接口测试不再维护工具总数、完整工具清单和 Schema 字段镜像。

产品行为复查补上真实 DSPX 编辑内容往返和丢弃保存分支后的 dirty 状态。后者确认了 History 保存点引用已销毁条目的缺陷：新条目复用地址可能被误认作保存点。修复在删除 redo 条目时同步使对应保存点失效，未引入新的身份系统。

真实界面测试覆盖钢琴窗鼠标输入、选择与拖动预览、一次性提交、撤销重做后的场景和模型；通过 ClipboardController 和实际 QClipboard 验证复制、剪切、按活动片段/播放位置粘贴及错误内容无副作用。控件层补上分隔条拖动、菜单选择、快捷键焦点以及生产动画收敛。数值/几何检查与真实控件输入分开描述，不将调用 Facade 后检查模型冒充 GUI 测试。

纯实验入口 AnchoredCurve、NewStyle、OpenGLWidget、ParamEdit、StateMachine、InsertTable、Cascader 已移除。ElasticAnimation 改为直接验证生产 ElasticAnimator，SingerMenuDisplay 合并入 TwoLevelComboBox；有效回归的具体去向见[覆盖矩阵](test-coverage-matrix.md)。

首次 Linux coverage 指出了整片段剪贴板、歌词拆分和推理完成门控未执行，以及已有音符交互覆盖不足。本轮新增应用工作流测试，将 `TestFillLyricTaggerOrder` 按共同职责改名为 `TestLyricRules`，并扩展现有 NoteTransfer 和 GUI 目标。应用工作流目标统一为 `TestApplicationWorkflows`，复用同一 Headless AppContext 验证全部推理门控及离线导出状态恢复。新增领域、GUI、应用工作流与真实进程用例均已纳入 Windows 和 Linux 完整通过集合。

后续覆盖复查补上无需声库的真实音频导出闭环：`755c4e77` 在现有 Headless 进程目标中生成短 WAV，经过 Editor 导入及导出任务后解码检查格式、时长和有效音频，验证导出不修改文档。该用例复用已有 libsndfile 依赖，不增加测试程序或模型资源要求。

## 3. 跨平台基础设施

`lite_register_test` 集中配置 category、offscreen、原生桌面条件、资源条件及 Windows Qt/vcpkg 动态库路径。原生桌面测试保留互斥锁；通用测试不以 CI 环境变量为前提。Qt Test 在 Windows 无控制台环境的日志统一写入可捕获输出，JUnit、失败详情和资源跳过都可见。

CMake 最低版本统一为 3.24，以支持测试 preset 和环境路径修改。覆盖率插桩仅在显式启用时要求 GCC，常规 Windows/MSVC 测试保持可用。

`ProcessFixture` 为每个场景创建临时数据根、配置、访问根及素材，只管理自身启动的进程。成功自动清理，失败保留进程参数、退出码、协议与 stdout/stderr。应用的配置、日志、缓存和单实例身份复用统一数据路径；`DSEL_TEST_DATA_ROOT` 仅在测试构建生效，不增加产品 CLI 或公开自动化工具。

Linux 首次完整执行暴露了 stdio 测试硬编码 `.exe`、公开播放失败弹出模态对话框和重启替代进程未就绪。改动通过 CMake target 提供可执行路径，只允许 TrustedGui 调用弹出设备错误提示，并为重启保留独立输出文件。复查同时补齐非 Windows 的替代进程存活/所有权/清理实现。播放及 GUI 补测提交为 `607fc7e9`，跨平台进程修复提交为 `3cb55146`；后续 Linux MCP、Headless 及 GUI 用例通过。stdio 阻塞接收端由 `bf72a3bb` 改为跨平台替身，已在 Linux 通过。

跨 Host 退出请求在响应返回前连接关闭的问题由受控大响应回归在 Windows 复现。`aa673b5a` 改为有序断开并在全部连接共享的 2 秒上限内排空，再释放服务器；`f73d6c9c` 补客户端不读取响应时仍按时停止的用例。正常读取、停止上限、HTTP、MCP 进程、Headless 及跨 Host 场景均通过 Windows 与 Linux 验证，原退出响应断言保留。

新增资源用例通过显式配置声库完成实际 CPU 推理和 WAV 导出，检查输出可解码、样本有限且有能量。未提供资源明确跳过；配置后失败按失败处理。当前还没有真实声库运行结果，不能据此宣称模型或音频设备通过。

## 4. Linux CI 与覆盖率

一个 Ubuntu 24.04 job 按环境、依赖、配置、完整构建、测试、覆盖率和产物收集执行。Qt 固定 6.11.2，上游 vcpkg 固定 revision，manifest/overlay 的版本来自仓库。Qt 和 vcpkg 包可缓存，不缓存 CMake 构建目录。依赖失败时用独立的 partial key 保留已成功构建的包；后续由 vcpkg ABI 判断哪些包可复用，成功后的完整 key 不受 partial key 占用。每次 PR 更新实际触发 Actions，失败后仍收集日志；调试根因与运行链接集中在[测试报告](test-report.md)。

vcpkg 缓存键包含 Qt 版本及引导脚本、manifest、overlay 端口/编译配置的内容哈希；回退缓存只提供候选包，每轮仍执行 install，由包 ABI 决定复用或重建。上游 overlay 更新后，项目临时覆盖端口已不再使用，其注册和 CI 哈希引用均已移除。依赖升级以实际 manifest 和端口声明为准，缓存不会替项目选择新版本。

编译并行度取 runner 实际 CPU 数，环境产物同时记录 CPU 和内存；依赖构建和测试程序并行度均为 2。Ninja 继续检查可独立编译的目标以一次收集编译错误，任何错误仍导致构建失败。

GCC/gcov 对 Debug 测试构建插桩，gcovr 汇总 `src/app`、`src/connector`、`src/libs`、`src/tools` 的行和分支覆盖。报告排除测试、第三方和生成文件，不设百分比门槛；按功能域查看未执行行为，决定必要补测。原生平台和设备未运行范围单独记录。

`b0d97a98` 增加 gcovr 原生逐文件 CSV 产物，并同步输出到 Actions 文本日志；新依赖运行已成功产出，逐文件汇总与 coverage 日志摘要一致。保留既有 HTML/JSON 产物，不增加依赖或长期文档生成机制。失败素材收集包含测试产物目录中的隐藏文件，用于保留导出的隐藏暂存文件。

行覆盖由首次采样的 46.9% 升至 `50dc53a0` 完整冷缓存运行的 49.3%，分支覆盖由 38.3% 升至 40.4%。整片段剪贴板、推理完成门控、歌词拆分、已有音符交互和纯音频导出均取得实际执行证据；AudioExporter 从 6 / 650 行提升到 243 / 654 行。准确分母、文件变化及未执行范围集中在[测试报告](test-report.md)。

上游依赖更新及导出修复后，`50dc53a0` 的 [冷缓存运行](https://github.com/flutydeer/ds-editor-lite/actions/runs/34319269165/attempts/1) 从 Qt/vcpkg 均未命中、0 包恢复开始，55 个依赖源码安装约 23 分钟完成，完整构建、68 项测试和 coverage 全部通过。同一 SHA 的 [暖缓存运行](https://github.com/flutydeer/ds-editor-lite/actions/runs/34319269165/attempts/2) 确认 Qt 和 vcpkg 精确命中，55 个包恢复后仍执行 install，完整构建、测试和 coverage 同样通过。历史轮次及缓存升级过程集中在测试报告中。

## 5. 设计取舍及偏差

- 专用 `build/Tests` 目录用于隔离 IDE 自动 configure，避免生成头文件与当前编译相互干扰。
- 初期锁定的 wolf-midi 和 synthrt 在 GCC 分别缺少 `<cmath>` 和 `<mutex>`，曾用项目端口补丁打通构建。`3a79cc51` 同步包含上游修复的 overlay `110ca6bb` 后，直接使用 wolf-midi 1.0.2#1（`2097d1d9`）和 synthrt 0.1.0.15#1（`0e3940dc`），同时带入 stdcorelib 端口更新；临时项目覆盖端口退出使用，新环境结果单独验证。
- 覆盖率统计是本期验证产物，不扩展为文档自动生成或长期同步机制。
- 测试目标按组件依赖和执行环境保留，不为减少目标数强行合并。第二轮拆分进一步分离 HTTP 准入/会话/取消、Connector 转发/超时/刷新/背压，以及进程启动/文档换代/协议兼容/退出；业务顺序有意义的一条端到端链仍保留。
- 推理完成门控通过真实任务构造和 Headless AppContext 验证，测试不调度这些任务，不依赖声库执行结果；实际模型输出仍由显式资源用例验证。
- 新目标链接完整生产实现时发现 SynthrtEngine 使用提取模块却未声明依赖，依赖补在生产库自身，避免给各测试单独加链接库；新目标定向验证已通过。
- 多次实际依赖失败发生在冷构建后段，因此增加失败包缓存以缩短调试；最终空缓存验证仍从 Qt 和 vcpkg 缓存未命中的状态完整执行。

## 6. 遗留与验收状态

Windows `50dc53a0` 完整构建通过，71 项 CTest 中 70 项通过、1 项声库资源明确跳过，59.34 秒、688 个行为/数据行通过。相同候选的 Linux 冷/暖缓存完整运行均 68 项通过，分别为 35.42 秒、33.62 秒，包含普通音频导出及 open/closed mixer 回归。未配置的真实声库模型输出、GPU 和实际听感保留为未执行范围。

本报告定稿时尚未进行首次 ready 审查。PR 后续审查、问题处理及最终 bot 认可保留在 [PR #187](https://github.com/flutydeer/ds-editor-lite/pull/187)；最终认可不为抄回文档另造提交。

Linux 发现的导出恢复缺陷已由 `50dc53a0` 修复：按原 isOpen 状态恢复 close 或原缓冲/采样率。Windows 的 closed-mixer 用例在修前复现相同断言，修后定向和 Windows/Linux 完整集合通过。过程证据集中在测试报告，保留原导出结果和跨进程完成断言。
