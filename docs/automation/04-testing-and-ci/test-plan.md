# 四期测试执行计划

## 1. 执行入口

先准备环境并完整构建 Editor、Connector 和测试，再执行规定集合。定位问题时用 CTest 标签或程序名选择套件，用 Qt Test slot 选择具体行为。分类和程序职责见[测试大纲](test-outline.md)，能力到测试的对应见[覆盖矩阵](test-coverage-matrix.md)，验收事实见[测试报告](test-report.md)。

## 2. 环境与构建

### 2.1. 公共要求

使用 CMake 3.24 或以上、项目要求的 Qt 6、编译器及 vcpkg 依赖。Qt 包含 Core5Compat、ShaderTools、StateMachine、HttpServer、WebSockets 等组件。版本和系统包以 [workflow](../../../.github/workflows/tests.yml)、manifest 和 presets 为准，文档不维护平行版本表。

所有测试启用 LITE_BUILD_TESTS。tests preset 使用 build/Tests，coverage 使用 build/Coverage，避免与 IDE 的 build/Debug 共享生成文件。build preset 同时构建完整产品和 lite_tests，不能仅编译测试程序后宣称产品构建通过。

| 平台 | runner / 架构 | vcpkg triplet | CI 入口 |
|---|---|---|---|
| Linux | ubuntu-latest / x64 | x64-linux | coverage 构建，ctest --preset ci-coverage，随后独立 Coverage 步骤 |
| Windows | windows-latest / x64 | x64-windows | 普通 Debug tests 构建，ctest --preset ci |
| macOS | macos-latest / arm64 | arm64-osx | 普通 Debug tests 构建，ctest --preset ci |

### 2.2. Windows

通过项目 VS DevShell/preset wrapper 初始化 MSVC 和 Qt 环境：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Dependencies -Preset tests
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset tests
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Test -Preset local
```

Test 模式省略 preset 时默认 local，其他模式默认 debug。显式指定的 preset 保持原选择。依赖未变化时无需反复执行 Dependencies。

### 2.3. Linux 和 macOS

将 QT_ROOT_DIR 指向实际 Qt 安装目录，准备对应系统开发依赖。Linux 示例：

```bash
python3 scripts/ci/bootstrap-vcpkg.py --triplet x64-linux
cmake --preset tests -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" \
  -DQt6_DIR="$QT_ROOT_DIR/lib/cmake/Qt6" -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build --preset tests
ctest --preset local
```

macOS arm64 将 triplet 改为 arm64-osx，并显式选择架构：

```bash
python3 scripts/ci/bootstrap-vcpkg.py --triplet arm64-osx
cmake --preset tests -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" \
  -DQt6_DIR="$QT_ROOT_DIR/lib/cmake/Qt6" -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build --preset tests
ctest --preset local
```

无桌面 Linux 使用 xvfb-run -a 包装 CTest。bootstrap-linux.sh 仅委托跨平台 bootstrap-vcpkg.py，不另行维护依赖流程。

### 2.4. 定向执行与日志

- local 选择本机全部注册测试。ci 包含通用、资源和原生集合，ci-coverage 在覆盖构建中执行同一规定集合。
- CTest 用 -L 选择 unit、domain、workflow、protocol、process、gui 等职责标签，用 -R 选择程序。直接执行测试程序并传入 slot 名可定位具体行为，不另建函数清单。
- 两个 Provider 编译变体共用源码，均须构建和执行，其配置测试不要求 CUDA 设备。
- Windows wrapper 将结果写到 build/test-results。直接 CTest 可用 --output-junit 指定报告，逐例日志位于构建目录的 Testing/Temporary/LastTest.log。
- 产物记录源码、工具链、依赖版本和执行条件。阶段文档不复制每次执行的版本及运行 ID。

## 3. 运行条件

### 3.1. GUI 后端

| 程序或行为 | 运行条件 |
|---|---|
| GuiComponents、EditorRendering | offscreen |
| EditorInteraction | minimal:enable_fonts 与 Fusion，支持实际 Qt QDrag。offscreen 会忽略该拖放路径 |
| ApplicationGui | Windows/Linux offscreen，macOS 原生窗口，执行相同样例和断言 |
| NativeDesktop | 原生桌面，Linux CI 使用 Xvfb，与其他原生套件共用 desktop 资源锁 |
| RHI Null 场景 | 需要原生窗口，不要求物理 GPU，验证输入、CPU 准备及帧提交 |
| RHI 平台图形数据行 | Windows D3D11 软件渲染、macOS Metal 或 Linux OpenGL，后端可用后验证真实帧读回 |

本地原生测试需要可交互且空闲的桌面，期间避免移动鼠标、切换窗口或使用剪贴板。资源锁不能隔离其他桌面程序。fixture 同步合成输入与系统光标，结束后恢复自身改动。

没有可用图形后端时只跳过对应数据行。后端已可用后的窗口、绘制、读回或断言失败必须失败。完整窗口先完成初始化再准备工程，退出时正常销毁控件，不以遗留窗口规避生命周期问题。

### 3.2. 声库与模型

ModelResources、ApplicationWorkflows、ApplicationGui 及 NativeDesktop 的相关用例默认使用 [voicebank-fixture.zip](../../../src/tests/resources/voicebank-fixture.zip)。CMake 解压到当前构建树，不需在测试机安装训练框架。资源身份、中英文、声线及确定性模型说明见[资源 README](../../../src/tests/resources/README.md)。普通流程固定 CPU，不需播放设备。

外部真实声库必须同时配置以下四项，内容取自实际资源：

```powershell
$env:DSEL_TEST_VOICEBANK_ROOT = "C:/Voicebanks/Example"
$env:DSEL_TEST_SINGER_ID = "example-singer"
$env:DSEL_TEST_LANGUAGE = "cmn"
$env:DSEL_TEST_LYRIC = "la"
```

Linux/macOS 使用同名环境变量。显式配置优先于内置包，缺项、加载或执行失败报告 FAIL，不扫描个人目录、不自动回退内置包。

DirectML/CUDA 数据行使用内置包验证实际推理及声码器。未编译 Provider 或无设备时报告 QSKIP，已发现资源的初始化、加载或执行失败报告 FAIL。配置选择测试与真实 GPU 推理分别报告。GAME/RMVPE 实际模型不在本期执行范围。

### 3.3. 音频与 MIDI 设备

| 环境变量 | 选择与失败规则 |
|---|---|
| DSEL_TEST_AUDIO_DRIVER、DSEL_TEST_AUDIO_DEVICE | 均可省略，使用生产默认输出。无可用后端、没有设备或只有 dummy/disk 时 QSKIP。可只指定驱动或当前驱动的设备。指定名称不存在、已枚举设备初始化/打开失败、配置或播放失败均 FAIL |
| DSEL_TEST_MIDI_INPUT、DSEL_TEST_MIDI_OUTPUT | 必须指向已连接的专用回环端口，不由测试安装驱动。均缺少时 QSKIP，部分配置、端口不存在、打开/发送失败或未收到消息均 FAIL |

设备用例在 NativeDesktop 中，使用静音 WAV 验证真实回调、配置落盘、播放/暂停/停止，MIDI 检查 note-on/off、生产合成器 PCM 和释放后归零。只清理自己打开的端口和修改的配置，不要求扬声器发声。

文件删除能力按平台判断。unlinkingAudioSourcePreservesOpenDecodeUntilReload 在 Windows 后端不允许移除已打开文件时明确跳过，允许的平台继续验证旧句柄完成解码、重新打开后报告缺失。资源不足、平台不适用与测试通过分开记录，只跳过缺少条件的具体用例。

## 4. 隔离与时序

RuntimeResourcesFixture 从实际构建的 Editor 初始化规则和插件，macOS 同时使用 bundle 的 Frameworks。CTest 和直接执行使用相同装配，缺失部署或规则不能静默替换。内部数据根和插件路径覆盖仅对测试构建生效。

进程 fixture 使用独立配置、缓存、访问根、临时素材及单实例身份，只管理自己创建的进程。重启前将 stdout/stderr 指向沙箱文件，替代进程继承有效输出目标。成功清理，失败保留沙箱路径、参数、退出码和输出。

应用在套件内共享生命周期，每例复位文档、历史、任务、解析结果和通知。完整工程比较共用生产编码器，身份与非持久状态另行检查。外部回答或后端替身只控制依赖，保存状态机、领域规则、文件发布和输入接线使用真实实现。

竞态使用受控 worker、调度器或时钟。程序超时、Qt Test watchdog 和业务等待分别设置，不把总套件预算当作单个任务时限。失败先检查最后执行位置、具体状态和进度，不能靠无依据放宽等待或无限重跑取得通过。

## 5. CI 引导与失败复验

1. Draft PR 分别打通三平台依赖、配置、完整构建、通用工作流、协议/进程和 GUI。必须有实际运行，提交 workflow 不等于验证成功。
2. 从首个根因区分依赖、编译、加载、断言、超时或状态污染，保留原失败日志。
3. 可本地复现时先跑失败用例及所属组，再完成 Windows 完整构建、规定测试和覆盖分析，之后提交并正常推送。
4. 新代码由新运行验证，恢复三平台完整集合。旧 run 的重跑仍对应旧代码，一个平台通过不能替代其他平台。
5. 冷缓存、暖缓存及依赖更新分别验证。只操作本 PR 对应缓存，不清理其他 PR 缓存。缓存键包含 triplet、runner 镜像、Qt、manifest 及覆盖端口，每次仍执行 install，由 vcpkg ABI 决定复用或重建，不缓存整个 CMake 树。
6. 测试退出码正确传播，失败后仍收集已有日志、JUnit 和素材。macOS crash diagnostics 仅收集系统崩溃附件，不能代替测试结果。
7. 纯文档修改不重跑或重新等待已验证代码的 CI。缓存路径只在受影响时复验，稳定 PR body 不充当状态日志。

## 6. 覆盖率采集

### 6.1. Linux / GCC

Linux CI 在 Debug coverage 构建执行测试，独立 Coverage 步骤复用执行数据生成 GCC/gcovr 报告，不重复跑测试。本地示例：

```bash
cmake --preset coverage -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" \
  -DQt6_DIR="$QT_ROOT_DIR/lib/cmake/Qt6" -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build --preset coverage
ctest --preset coverage
python3 -m venv build/ci/coverage-env
build/ci/coverage-env/bin/pip install gcovr==8.6
mkdir -p build/test-results/coverage
build/ci/coverage-env/bin/gcovr --config scripts/ci/gcovr.cfg \
  --html-nested build/test-results/coverage/index.html \
  --json-summary build/test-results/coverage/summary.json \
  --csv build/test-results/coverage/files.csv --print-summary
```

独立采样先确保该覆盖构建没有旧 .gcda，防止累计历史命中。merge-lines 按文件和源码行合并模板实例，口径修正不算新增测试贡献。

### 6.2. Windows / MSVC

本地使用 Visual Studio 的 Microsoft.CodeCoverage.Console.exe 静态插桩。coverage 构建提供调试符号及 /PROFILE，并关闭 Edit and Continue。在可调用 CTest 的开发者终端执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset coverage
python scripts/tests/collect-msvc-coverage.py --output build/test-results/msvc-full -- --preset coverage
```

脚本自动发现采集器，亦可用 --collector 指定，CTest 不在 PATH 时用 --ctest。每次用新的输出目录，采集期间不改源码、不重新构建同一目录、不并行运行另一采集。静态插桩可能较慢，日志暂时安静不等于挂起，先核对实际进程和阶段。

### 6.3. macOS / LLVM

```bash
cmake --preset coverage -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" \
  -DQt6_DIR="$QT_ROOT_DIR/lib/cmake/Qt6" -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build --preset coverage
python3 scripts/tests/collect-llvm-coverage.py --output build/test-results/llvm-full -- --preset coverage
```

脚本通过 xcrun 选择配套工具，各进程/模块分别写 profile。Windows/macOS 的采集仅作为本地能力，普通 CI 不要求这些工具。公共源码过滤与行汇总复用 coverage_support.py。

### 6.4. 结果核对与缺口判断

核对构建、CTest 和采集器各自退出状态，保存原始数据、逐行/逐文件汇总、JUnit、完整日志及受测版本。发现与执行使用同一 CTest 筛选参数，不能把遗留程序或未选程序混入分母。按源码文件和行号合并同一次采样中程序/模块的重复命中，不拼接不同轮次或平台。

完整报告排除测试、第三方及生成代码，业务边界核算另保留分子、分母和逐文件区间。原始结果不能被边界统计覆盖。MSVC 不报告占位分支百分比，LLVM mapping/profile 警告保留并按函数核对，不静默缩小范围。

按[大纲的边界](test-outline.md#10-业务逻辑覆盖统计边界)及产品风险分析未命中行，优先真实编辑、失败恢复、持久化、文件发布和 GUI 接线。合并重复场景和生产实现，区分新增命中、源码重构及统计变化，不为 Schema、布局或不可达私有分支堆测试。

## 7. 验收与审查

Windows 完整环境业务逻辑覆盖达到约定的 90% 参考目标，三平台规定测试和 Linux 独立覆盖步骤通过，文档与实现一致后交付 ready PR。真实审查问题修复并本地验证后推送，回复并 resolve，由仓库智能触发复审，不逐次手工发送触发评论。受审代码的规定验证与 bot thumbs-up 或明确无问题结论同时成立才完成验收。
