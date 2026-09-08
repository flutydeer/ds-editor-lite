# 四期实现报告

## 1. 当前状态

实施中。基线为 `a8fac646`，工作分支为 `test-ci`。本报告在 ready 前按实际交付更新，执行结果由[test-report.md](test-report.md)记录。

## 2. 测试结构与生产复用

测试统一采用 Qt Test 的 slot 和 data row，CTest 负责六类标签、超时、运行环境及结果。`lite_tests` 聚合测试构建；`tests` build preset 同时构建 Editor、Connector 与全部测试，补上此前独立于 `src/tests` 的 ICU wrapper 测试。一个测试程序默认对应一条 CTest 注册；Headless 的跨 Host 场景因需要 GUI 平台单独注册。该程序用两个简单 Qt Test 类区分运行条件，CTest 只选择模式，不维护具体函数名单。

`EditorAutomationCore` 从原测试支持目标中提取生产的模型、编辑服务和 Facade 实现，供应用和测试共享。`AutomationTestSupport` 只承接测试 fixture 的使用入口。`EditorRuntime` 对象库承接主程序之外的应用实现、Qt 资源、翻译和界面依赖；Editor 与钢琴窗集成测试链接同一实现，避免在测试中重造编辑器。

大入口按行为拆分，数据驱动用例每行重建状态。配置持久化、无效请求副作用、音频解码换代、任务取消与提交点等回归保留在各自所属层；代表性编辑序列比较直接 Facade、Registry、Native 和 MCP 的业务结果。接口测试不再维护工具总数、完整工具清单和 Schema 字段镜像。

产品行为复查补上真实 DSPX 编辑内容往返和丢弃保存分支后的 dirty 状态。后者确认了 History 保存点引用已销毁条目的缺陷：新条目复用地址可能被误认作保存点。修复在删除 redo 条目时同步使对应保存点失效，未引入新的身份系统。

真实界面测试覆盖钢琴窗鼠标输入、选择与拖动预览、一次性提交、撤销重做后的场景和模型；通过 ClipboardController 和实际 QClipboard 验证复制、剪切、按活动片段/播放位置粘贴及错误内容无副作用。控件层补上分隔条拖动、菜单选择、快捷键焦点以及生产动画收敛。数值/几何检查与真实控件输入分开描述，不将调用 Facade 后检查模型冒充 GUI 测试。

纯实验入口 AnchoredCurve、NewStyle、OpenGLWidget、ParamEdit、StateMachine、InsertTable、Cascader 已移除。ElasticAnimation 改为直接验证生产 ElasticAnimator，SingerMenuDisplay 合并入 TwoLevelComboBox；有效回归的具体去向见[覆盖矩阵](test-coverage-matrix.md)。

## 3. 跨平台基础设施

`lite_register_test` 集中配置 category、offscreen、原生桌面条件、资源条件及 Windows Qt/vcpkg 动态库路径。原生桌面测试保留互斥锁；通用测试不以 CI 环境变量为前提。Qt Test 在 Windows 无控制台环境的日志统一写入可捕获输出，JUnit、失败详情和资源跳过都可见。

CMake 最低版本统一为 3.24，以支持测试 preset 和环境路径修改。覆盖率插桩仅在显式启用时要求 GCC，常规 Windows/MSVC 测试保持可用。

`ProcessFixture` 为每个场景创建临时数据根、配置、访问根及素材，只管理自身启动的进程。成功自动清理，失败保留进程参数、退出码、协议与 stdout/stderr。应用的配置、日志、缓存和单实例身份复用统一数据路径；`DSEL_TEST_DATA_ROOT` 仅在测试构建生效，不增加产品 CLI 或公开自动化工具。

新增资源用例通过显式配置声库完成实际 CPU 推理和 WAV 导出，检查输出可解码、样本有限且有能量。未提供资源明确跳过；配置后失败按失败处理。当前还没有真实声库运行结果，不能据此宣称模型或音频设备通过。

## 4. Linux CI 与覆盖率

一个 Ubuntu 24.04 job 按环境、依赖、配置、完整构建、测试、覆盖率和产物收集执行。Qt 固定 6.11.2，上游 vcpkg 固定 revision，manifest/overlay 的版本来自仓库。Qt 和 vcpkg 包可缓存，不缓存 CMake 构建目录。依赖失败时用独立的 partial key 保留已成功构建的包；后续由 vcpkg ABI 判断哪些包可复用，成功后的完整 key 不受 partial key 占用。每次 PR 更新实际触发 Actions，失败后仍收集日志；调试根因与运行链接集中在[测试报告](test-report.md)。

GCC/gcov 对 Debug 测试构建插桩，gcovr 汇总 `src/app`、`src/connector`、`src/libs`、`src/tools` 的行和分支覆盖。报告排除测试、第三方和生成文件，不设百分比门槛；按功能域查看未执行行为，决定必要补测。原生平台和设备未运行范围单独记录。

## 5. 设计取舍及偏差

- 专用 `build/Tests` 目录用于隔离 IDE 自动 configure，避免生成头文件与当前编译相互干扰。
- 当前上游 overlay 锁定的 wolf-midi 和 synthrt 在 GCC 分别缺少 `<cmath>` 和 `<mutex>`，采用项目端口补丁；不复制业务库或改动无写权限的子模块来源。
- 覆盖率统计是本期验证产物，不扩展为文档自动生成或长期同步机制。
- 测试目标按组件依赖和执行环境保留，不为减少目标数强行合并。第二轮拆分进一步分离 HTTP 准入/会话/取消、Connector 转发/超时/刷新/背压，以及进程启动/文档换代/协议兼容/退出；业务顺序有意义的一条端到端链仍保留。
- 多次实际依赖失败均发生在冷构建后段，因此增加失败包缓存以缩短调试，最终候选仍需独立空缓存和命中缓存的完整验证。

## 6. 遗留与验收状态

Windows 完整验证已经通过一轮候选；Linux 依赖引导、覆盖率采样及补测尚未完成。PR 保持 Draft，尚未进入最终验收及 bot 审查，不能据此报告声明完成。
