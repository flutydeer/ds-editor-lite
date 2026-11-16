# 测试指南

## 1. 执行入口

先准备环境并完整构建 Editor、Connector 和测试，再通过 CTest 执行。定位问题时用 CTest 标签或程序名选择套件，用 Qt Test slot 选择具体行为。测试注册以 [CMake 定义](../../../src/tests/CMakeLists.txt)为准。

## 2. 环境与构建

### 2.1. 公共要求

准备项目要求的 CMake、Qt、编译器和 vcpkg。CI 的 Qt 版本、组件、系统包和平台参数见
[测试工作流](../../../.github/workflows/tests.yml)，应用依赖见 [vcpkg manifest](../../../scripts/vcpkg-manifest/vcpkg.json)。

使用 [`tests` 或 `coverage` preset](../../../CMakePresets.json) 配置测试构建。这两个 preset
都启用 `LITE_BUILD_TESTS`，使用独立的构建目录。对应的 build preset 会构建产品和测试。

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

DirectML/CUDA 数据行使用内置包验证实际推理及声码器。未编译 Provider 或无设备时报告 QSKIP，已发现资源的初始化、加载或执行失败报告 FAIL。配置选择测试与真实 GPU 推理分别报告。

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

在 [测试工作流](../../../.github/workflows/tests.yml)中查看各平台的构建、筛选参数和缓存步骤。
失败时先查看退出状态、JUnit 及逐例日志。macOS 的崩溃附件可用于定位进程异常。能在本机
复现时，可先定向运行失败用例和所属套件。

## 6. 覆盖率采集

### 6.1. Linux / GCC

Linux CI 在 Debug coverage 构建执行测试，独立 Coverage 步骤复用执行数据生成 GCC/gcovr 报告，不重复跑测试。本地示例：

```bash
cmake --preset coverage -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" \
  -DQt6_DIR="$QT_ROOT_DIR/lib/cmake/Qt6" -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build --preset coverage
ctest --preset coverage
python3 -m venv build/ci/coverage-env
build/ci/coverage-env/bin/pip install gcovr
mkdir -p build/test-results/coverage
build/ci/coverage-env/bin/gcovr --config scripts/ci/gcovr.cfg \
  --html-nested build/test-results/coverage/index.html \
  --json-summary build/test-results/coverage/summary.json \
  --csv build/test-results/coverage/files.csv --print-summary
```

复现 CI 报告时使用工作流指定的 gcovr 版本。独立采样前清除对应构建树中的旧 `.gcda`，防止累计历史命中。`merge-lines` 按文件和源码行合并模板实例。

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

分析未命中行时，优先关注编辑、失败恢复、持久化、文件发布和 GUI 接线。归因时区分新增用例、源码变化和统计口径变化。新增用例遵循[测试原则](../../../AGENTS.md#testing-principles)。
