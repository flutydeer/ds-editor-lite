# spec 2.4 接入状态

编辑器通过 synthrt 加载 spec 2.4 包，使用 dsinfer 执行合成，使用 wolf 处理语言，
使用 otter 执行音高和音符提取。声库需直接提供符合新版契约的声明。

## 1. 端口与依赖

依赖入口是 [vcpkg manifest](../../scripts/vcpkg-manifest/vcpkg.json)。
本仓的 `scripts/vcpkg-ports` 优先于项目补丁端口和共享 overlay，
确保编辑器使用同一套 synthrt、wolf 与 otter。

| 端口 | 职责 |
| --- | --- |
| synthrt | 包加载与贡献系统，ONNX feature 同时提供 dsinfer 和驱动 |
| wolf | 语言解释器与 linguist provider |
| otter | 音高、音符等分析器解释器 |
| cpp-pinyin2 | wolf 拼音插件使用的 2.x 实现 |
| wolf-lang-packages | 下载并安装所选语言资源及其依赖 |

具体提交、features 和资源摘要分别以各端口的 `portfile.cmake`、`vcpkg.json`、
`assets.cmake` 为准。依赖安装目录是 `vcpkg/installed`，与 presets 保持一致。
otter 的 macOS 管道补丁仍由端口应用，固定版本尚需该兼容适配。

## 2. 模型包与资源

wolf 二进制插件和语言资源是两种交付物。语言包由 vcpkg 安装，构建时部署到
插件根下的 `wolf/packages`。来源依次为 `LITE_WOLF_LANG_PACKAGES`、
`WOLF_LANG_PACKAGES_SOURCE` 环境变量及已安装的数据端口。
同级 wolf 开发目录仅在 `LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK=ON` 时作为兜底。

otter 端口不包含分析器模型权重。用户需安装 otter 提供的模型包，然后在设置中选择
音符或音高分析器。分析器使用包贡献引用，模型路径由包声明提供。

部署路径由 [DeployLayout.h](../../src/libs/SynthrtEngine/DeployLayout.h) 统一定义。
构建和安装负责携带插件、ONNX Runtime 与语言资源，不能以“可执行文件已生成”代替部署检查。

## 3. 运行时契约

- 歌手由包 ID、包版本和贡献 ID 共同标识。扫描结果保留包信息、语言与声线能力。
- 合成流水线持有包句柄，任务通过对应阶段的 executive 执行。
- wolf 负责歌词、发音、音素与起音转换。宿主先检查语言可达深度，再申请所需结果。
- 音高和音符提取通过 otter 分析器执行，音频解码、切片与工程时间换算由宿主处理。
- 刷新声库时建立新的运行单元，并使旧模型缓存失效。已经开始的任务保有其引用的资源。
- 加载失败的声库和不可用语言向用户显示原因，不能被当作可合成资源。

模块分工与行为约束见 [接入指南](../guides/synthrt-wolf-otter-integration.md)。

## 4. 验证入口

CI 使用 [voicebank-fixture.zip](../../src/tests/resources/voicebank-fixture.zip)。
这是符合 spec 2.4 的确定性假声库，包含可执行 ONNX 图和中英文语言资源。
测试直接解压并加载它，不经过声库转换器。

[资源测试](../../src/tests/TestModelResources/CMakeLists.txt)覆盖加载、G2P、合成、故障恢复与导出。
真实声库审计通过 `LITE_AUDIT_VOICEBANK` 和 `LITE_AUDIT_LANGUAGE_PACKAGES` 配置，
真实提取模型通过 `LITE_OTTER_FIXTURES` 配置。未提供外部资源时仅跳过对应可选用例。
具体步骤见 [验证指南](../synthrt/developer-repro.md)。

## 5. 当前边界

- 声库加载成功只证明声明和依赖可用，模型是否可执行仍需推理验证。
- 声库的 `defaultLyric` 声明已读取为元数据。实际默认歌词仍按用户设置、内置语言默认值、
  `la` 的顺序选择，尚未消费声库声明。
- SP、AP 与声库声明的保留音素参与宿主处理。宿主没有为“额外注入保留标记”提供用户设置。
- CoreML 名称可被识别，但当前编辑器构建不提供对应运行时或设置入口。
