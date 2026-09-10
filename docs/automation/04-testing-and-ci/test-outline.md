# 四期测试大纲

## 1. 组织原则

用例按产品行为组织，不按历史 bug 或工具 ID 建立孤立目标。先看功能和风险，再复用旧用例；有效回归保留，重复断言合并，实验 demo 移出自动集合。

目标边界由组件、共享 fixture 和进程运行条件决定。新增测试默认采用已有 suite 的独立 slot 或数据行；同一目标可按职责分源文件，保留一个 Qt Test class 和一次 `qExec`。场景直接使用 Qt Test 断言，共享数值辅助保留原调用位置和容差，不以外层布尔包装代替用例迁移。CTest 管理程序运行条件，不镜像逐函数列表，也不约束测试目标数量。运行条件确需分开时保留独立注册。

用例文件为 `tst_<snake_case>.cpp`；多文件套件的入口为 `test_main.cpp`、声明为 `tst_<domain>.h`，单文件套件可在 `tst_` 文件内保留 `QTEST_MAIN` 或自定义入口。fixture 辅助保留语义名称。文件划分服务领域职责，不为命名强拆入口和头文件，也不保留合并前各个旧测试类及其执行入口。

## 2. 基础数据与算法（unit）

- 时间线、小节/tick/time、量化与边界。
- 曲线插值、重采样、裁剪、变换与音频锚点。
- Speaker Mix、声线继承、推理输入与缓存。输入转换和 Speaker Mix 输入校验复用生产 `EditorInferenceCore`；继承验证使用真实 ProjectModel 的轨道和片段，检查跟随轨道、独立声线、切换继承及通知。
- 歌词规则、音节、本地化文本、配置与 CLI。
- 歌词拆分保留混合文字与未匹配字符，内置/自定义规则的启用和优先级实际生效；空匹配不重复输入。
- 自有数据结构的正常及失败语义。

## 3. 编辑与应用状态（domain）

- 轨道、片段、音符、歌词、音素、参数及锚点的细粒度编辑。
- Speaker Mix、Tempo、拍号、总线和持久循环。
- 复制粘贴、批量原子性、no-op、撤销重做、savepoint、dirty 与 revision。
- 音符和整片段的剪贴板职责分开：整片段保留全部参数层、音符范围外曲线、发音与声线，跨轨道批量粘贴保持相对位置并可一次撤销。
- 显式幂等重放/冲突，以及设置和瞬时播放状态。
- 应用 Host 的能力、生命周期和状态转换，完整设置及稀疏更新、包版本/路径投影、歌词规则持久化和运行时拒绝。共用服务 fixture；规则输入快照由测试明确提供，不在替身中复制生产排序和投影逻辑。

## 4. 文档、文件与异步工作流（workflow）

- 新建、打开、保存、导入导出、DSPX/MIDI/音频往返。
- 普通音频片段以小型 WAV 经过真实导入及导出，验证音频格式、时长、有效内容和导出不修改文档；无需声库的导出与实际声库渲染分开验证。
- 离线导出结束后恢复混音器原先的打开/关闭状态及已打开时的缓冲、采样率，设备不可用时也能完成导出。
- 缺失音频、路径确认、重定位、解码及 source generation。
- AudioAssets 将解析与解码归为同一职责套件；解码控制使用真实 Headless AppContext 和生产服务接线，每例清理文档、任务和通知，移除 AudioContext 等生产方法的测试替写。
- 文件发布、覆盖策略、失败回滚和暂存清理。
- 任务接受、完成、失败、取消、提交点、重复完成及晚到回调。
- 文档换代、对象删除、输入变化、包刷新和外部工具不可用。
- 推理完成门控使用真实任务快照：输入变化或目标消失时丢弃，语义不变的 revision 变化仍可接纳，冲突编辑期间延后且结束后可继续应用。
- 运行中重新推理通过受控暂停真实任务验证：旧流水线销毁后队列仍正确清理并启动替换任务。实际声库流程验证异步分段准备、完整推理及输出，不能以任务被接受代替完成。
- 关闭自动推理且停止播放时，显式手动请求仍应完成声学阶段；本次请求完成或取消后恢复原自动推理策略，后台任务不能因此获得永久许可。

## 5. 接口与 Connector（protocol）

- Native/MCP/HTTP/stdio 请求和响应、错误传播、分帧与生命周期。
- 真实参数映射、Host/权限/文件限制、分页和准入释放。
- Connector exposure、连接、重连、超时、EOF、背压与 outcome unknown。
- 分页缓存查询不重复请求上游，Editor 快照变化触发刷新；离线调用产生明确状态和错误。验证实际请求与结果，不使用任意次数循环和无产品 SLA 的速度门槛。
- 停止服务时已生成的响应完整返回，客户端不读取时仍在共同截止时间内停止；退出响应契约同时保留跨进程验证。
- 代表性业务在直接调用及协议入口的结果和副作用一致。

不逐字段复述 Schema，不硬编码工具总数，不复制工具清单。

## 6. 真实应用进程（process）

- 默认和带工程启动、Headless 固定 Native、可选 MCP、Connector 接线。
- 一条编辑/撤销/保存/重开和一条文件任务闭环。
- 单实例转发、端口冲突、失败退出、重启、平台终止信号与资源释放。
- ProcessIntegration 共用进程 fixture，Headless、MCP 和跨 Host 场景按源文件、slot 及平台条件组织；需要 GUI Host 的代表性装配可以使用 offscreen。实际模型依赖单列为 ModelResources。

## 7. 界面组件与交互（gui）

- 真实控件的鼠标绘制、拖动、缩放、选择、取消和快捷键。
- 已有音符的拖动提交与 Escape 取消分别检查预览、模型、历史及场景恢复；通过实际剪贴板和控制器验证编辑入口接线。
- 焦点、活动对象、滚动、视口、布局及面板同步。
- 编辑失败和 Undo/Redo 后界面恢复；GUI 与模型的双向更新。
- 菜单、语言、主题资源、动画及绘制几何。
- 主题、菜单与组件动画归入 GuiComponents；编辑视图、Undo/Redo、视口、滚动和输入组件归入 EditorInteraction。原生窗口布局与动画设置归入 NativeDesktop，绘制几何、字形和波形归入 EditorRendering；保留各自可定位的行为用例。
- ApplicationGui 共用真实应用 GUI 环境，钢琴窗、轨道、参数曲线和音频导出交互分源文件。轨道片段拖动及参数绘制验证预览、提交、Escape 取消和撤销恢复；导出对话框通过键盘/鼠标改变格式、采样率、轨道和混音方式，检查文件预览、文件计划与取消后配置恢复，并经真实 Export 按钮完成导出，检查进度、关闭和任务清理。工作流/进程测试进一步验证输出内容及产品装配。
- 设置页通过侧栏与真实输入验证即时保存、配置落盘和重开；声线混合通过标签选择与权重分隔线拖动验证确认结果、取消后重开及预设/文档不被意外修改。
- 声线预设通过保存、选择、修改后标记、删除与取消验证界面接线；包管理检查搜索和所选包详情，缺失音频检查实际文件选择、取消、重定位、解码与撤销重做。
- GUI Host 的公开播放调用在无设备时返回失败，不弹出阻塞自动化调用的模态对话框；直接 GUI 操作仍保留必要提示。

普通组件通过 Qt Test 真实鼠标、键盘和控件事件进入三平台 CI；offscreen 承载填词、导入、设置和编辑视图，原生窗口布局在 Linux Xvfb、Windows/macOS 桌面执行。主观观感与听感为补充，不建立跨平台像素基线。

默认钢琴窗及轨道编辑器使用 Legacy/QGraphicsView；实验 RHI 后端需在开发者设置中显式启用并重启。`NativeDesktop` 使用真实 `PianoRollRhiWidget` 和 Qt 的 Null 后端，验证鼠标预览、提交、撤销重做、实际命中及帧提交。Null 执行 CPU 渲染准备和命令路径，不验证 GPU shader 的像素结果；不建立设备或截图矩阵。Qt offscreen 不支持 RHI，该用例使用原生窗口环境。

## 8. 资源与平台边界

普通测试用临时小素材及受控服务。仓库内置多语言、多声线的最小测试声库，默认用于完整填词和 CPU 推理工作流；通过真实包加载、语言处理、阶段执行及 WAV 解码验证装配，不要求播放设备。真实声库可以显式覆盖默认资源，用于模型兼容性等额外验证。GAME/RMVPE 模型暂不在本期覆盖；GPU 和实际设备播放另需相应资源。平台不适用、未执行和通过分开表达，显式资源执行失败不能记为跳过。执行方式见[测试计划](test-plan.md)，实现去向见[覆盖矩阵](test-coverage-matrix.md)。

覆盖率同时用于本地和三平台 CI。独立 `coverage` preset 隔离插桩构建；Linux 使用 GCC/gcovr，Windows 使用 MSVC 原生静态插桩，macOS 使用 LLVM 源码插桩。原生汇总按源码行跨模块 OR 去重；分别分析编译器、平台和资源条件下的功能缺口，不直接比较总百分比，不报告 MSVC 采集器不支持的分支覆盖率。

## 9. 套件职责与程序划分

下表是本轮全树整理后的职责划分。每个程序均以单个 Qt Test 类维护用例，编译宏变体共用源码；后续新增行为优先加入对应套件，不以维持程序数量为目标。

| 程序 | 包含的测试内容 | 主要类别 / 条件 |
|---|---|---|
| TestFoundation | Expected、自有基础数据语义、本地化文本选择 | unit；通用 |
| TestMusicTime | tempo/拍号与音乐时间换算、音频时间锚点 | unit；通用 |
| TestParameters | 重采样、插值支持、曲线轨迹与变换、锚点编辑、音高显示策略 | unit/domain；通用 |
| TestLyrics | 歌词拆分与规则、音节、文字/发音/音素属性级联 | unit/domain；通用 |
| TestVoiceAndInference | Speaker Mix、真实声线继承、推理输入转换与校验、会话及推理缓存 | unit/domain；无需模型输出 |
| TestPreferences | 自动化及通用推理配置、语言选择和配置读写行为 | unit；通用 |
| TestInferenceProviderDefault | 默认编译配置的推理 Provider 选择与约束 | unit；编译宏变体 |
| TestInferenceProviderCuda | CUDA 编译配置的 Provider 选择与约束 | unit；编译宏变体，不执行 GPU 推理 |
| IcuWrapperTests | 项目使用的 ICU 包装行为 | unit；适用平台 |
| TestAutomationRuntime | 调用上下文、准入、权限路径、任务状态及竞态、显式幂等 | domain/workflow/protocol；通用 |
| TestProjectEditing | 轨道、片段、音符、参数、时间线与历史，音符/整片段转移，钢琴窗提交桥接 | domain；通用 |
| TestApplicationServices | 应用 Host、播放、编辑状态、设置、歌词规则、包与预设；异步文件/导出/提取服务契约 | domain/workflow；受控服务 |
| TestDocumentIO | 文档生命周期、保存点与撤销分支、路径及确认、DSPX/MIDI 往返、原子发布与导入 | workflow；临时文件 |
| TestAudioAssets | 音频哈希、搜索/重定位、路径与来源换代、真实解码控制及通知 | workflow；小型或无效素材 |
| TestApplicationWorkflows | 真实应用运行时的推理门控、重启队列、手动声学许可、文件批量导入、规则及预设持久化、音频混音导出与受控播放 | workflow；隔离 Headless 环境 |
| TestAutomationProtocol | Wire/游标、公共注册及映射、Native/MCP 调度、HTTP 生命周期与响应停止边界 | protocol；通用 |
| TestConnector | Connector 连接、重连、stdio 分帧、背压、超时与结果不明确 | protocol/process；真实 Connector |
| TestBootstrap | 启动参数、Host 模式、单实例身份与传输 | protocol/process；当前平台 |
| TestProcessIntegration | 真实 Editor/Connector 启动、跨 Host、编辑/文件闭环、退出/重启和信号 | process；通用/offscreen/平台 |
| TestModelResources | 内置或显式声库的 CPU 推理、语言和声线接线、缓存/失效与 WAV 导出 | workflow；默认内置资源 |
| TestGuiComponents | 主题颜色/图标、二级菜单、组件动画 | gui；offscreen |
| TestNativeDesktop | 原生分隔布局、窗口动画设置、Null RHI 钢琴窗输入与帧提交 | gui；原生桌面 |
| TestEditorInteraction | 控制器、视口、边缘滚动、钢琴窗/轨道输入、滚动条及快捷键 | gui/domain；通用/offscreen |
| TestEditorRendering | 字形图集、RHI 几何、波形绘制计算 | gui/unit；offscreen；不代表完整 RHI 后端 |
| TestApplicationGui | 真实应用编辑、剪贴板、填词、导入导出配置及完成进度、设置、声线及预设、音素与搜索、包和音频资源工作流 | gui；offscreen；共用隔离应用环境 |

两个 Provider 程序在 `src/tests/TestInferenceProvider` 共用用例和构建定义。六类测试是行为职责标签，同一程序可承载关联职责，不要求程序划分与类别一一对应。当前三平台 CI 使用 Qt 6.11.2；Linux、Windows、macOS 的实际验证结论分别由验收报告和运行产物给出。
