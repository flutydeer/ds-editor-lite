# 四期测试执行计划

## 1. 执行顺序

先验证环境和完整构建，再运行 unit、domain、workflow、protocol、process、gui。具体程序由 CTest 标签选择；Qt Test 函数可直接定向运行。覆盖要求见[大纲](test-outline.md)，本期验收结论见[报告](test-report.md)，逐轮执行结果保留在测试产物和 PR 中。

## 2. 环境与构建

Windows 使用项目 VS DevShell/preset wrapper；Linux 和 macOS 使用同一 CMake 工程及固定依赖。所有测试启用 `LITE_BUILD_TESTS`，构建完整产品和测试聚合目标。CI 统一 Qt 6.11.2，平台配置如下；配置列表示执行要求，成功与否以实际运行结果为准。

| 平台 | runner / 架构 | triplet |
|---|---|---|
| Linux | `ubuntu-24.04` / x64 | `x64-linux` |
| Windows | `windows-2025-vs2026` / x64 | `x64-windows` |
| macOS | `macos-15` / arm64 | `arm64-osx` |

专用 `tests` configure/build preset 使用 `build/Tests`，避免与 IDE 的 `build/Debug` 自动配置共享生成文件。Windows 入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Dependencies -Preset tests
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset tests
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Test -Preset local
```

`-Mode Test` 省略 `-Preset` 时默认使用 `local`；显式指定的测试 preset（如 `ci`）保持用户选择。其他模式省略 `-Preset` 时仍默认使用 `debug`。

Linux 和 macOS 本地准备 Qt 与系统开发依赖后，将 `QT_ROOT_DIR` 指向实际 Qt 安装目录。Qt 需包含 Core5Compat、ShaderTools、StateMachine、HttpServer 和 WebSockets 等项目依赖；系统包与 Qt 安装细节以[workflow](../../../.github/workflows/tests.yml)为准。[bootstrap-vcpkg.py](../../../scripts/ci/bootstrap-vcpkg.py)支持上述三个 triplet，[bootstrap-linux.sh](../../../scripts/ci/bootstrap-linux.sh)仅为 Linux 的薄委托。Linux 示例：

```bash
python3 scripts/ci/bootstrap-vcpkg.py --triplet x64-linux
cmake --preset tests -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" \
  -DQt6_DIR="$QT_ROOT_DIR/lib/cmake/Qt6" -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build --preset tests
ctest --preset local
```

macOS arm64 示例：

```bash
python3 scripts/ci/bootstrap-vcpkg.py --triplet arm64-osx
cmake --preset tests -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" \
  -DQt6_DIR="$QT_ROOT_DIR/lib/cmake/Qt6" -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build --preset tests
ctest --preset local
```

Windows wrapper 将本地结果写到 `build/test-results`；直接 CTest 可加 `--output-junit` 指定报告路径，完整逐例输出位于 `build/Tests/Testing/Temporary/LastTest.log`。`local` 选择本机全部注册测试，`ci` 只选择通用与 offscreen 集合，适用于 CI 或没有桌面的本地执行；单组可用 CTest 的 `-L`/`-R`，具体函数可直接给 Qt Test 程序传函数名。

程序划分见[测试大纲的套件表](test-outline.md#9-套件职责与程序划分)。每个程序只有一个 Qt Test 类及一次执行入口，用例文件为 `tst_<snake_case>.cpp`。多文件套件由 `test_main.cpp` 启动、`tst_<domain>.h` 声明测试类；单文件套件可在 `tst_` 文件中保留 `QTEST_MAIN` 或自定义入口，fixture 辅助使用语义名称。定向执行选择程序或 slot，不新增与源码平行的用例清单。两个 Provider 编译变体均须构建和执行，其用例不要求 CUDA 设备。

执行产物记录源码版本、系统、编译器、Qt、CMake、Ninja、vcpkg 与子模块版本，用于重现问题。阶段文档不复制这些逐轮元数据，也不要求每次提交后更新报告。

## 3. 运行条件

| 条件 | CI | 本地 |
|---|---|---|
| 通用、无窗口 | 执行 | 所有支持平台 |
| offscreen 组件 | 执行 | 原生或 offscreen |
| 原生桌面 | 不执行 | 当前平台 |
| 模型、声库、设备 | 不执行 | 显式配置后 |
| 平台特有行为 | 各矩阵平台对应项 | 当前平台对应项 |

不以 CI 集合代替本地完整入口。资源不足和平台不适用必须明确，不计作通过。

原生桌面用例归入 `TestNativeDesktop`，由 local preset 在适用平台执行；具体集合以 CTest 的 `native` 标签为准。`TestGuiComponents`、`TestEditorInteraction`、`TestEditorRendering` 和 `TestApplicationGui` 中可用 offscreen 的组件不需要声库或音频设备。

真实声库用例为 `TestModelResources`：设置 `DSEL_TEST_VOICEBANK_ROOT`、`DSEL_TEST_LANGUAGE`、`DSEL_TEST_LYRIC`，多音源时再指定 `DSEL_TEST_SINGER_ID`。用例固定 CPU、关闭自动推理，创建带显式语言的短音符；通过可观察的模型目标就绪条件等待 G2P/分段，再手动启动推理并等待任务成功终态，随后导出 WAV，检查可解码、有限样本和非零能量。无需播放设备。未设置声库根时明确跳过；配置后的失败为失败，不自动扫描个人声库。

`TestApplicationWorkflows` 属于通用 workflow 集合，共用隔离的 Headless AppContext。结果门控用例构造未调度的实际任务快照；队列重启回归则受控暂停真实 duration worker，验证旧任务取消清理和替换任务终态，并通过生产状态组件检查手动声学许可及完成后恢复策略。离线导出用例验证混音器原先打开或关闭两种状态的恢复。fixture 关闭自己持有的音频设备，通用用例不依赖声库输出或物理设备；实际模型执行由上述资源用例负责。

`TestProcessIntegration` 的普通音频导入导出场景使用生成的小型 WAV，经过实际 Editor 导入和导出任务，再用 libsndfile 解码验证；归入通用进程集合，不需要配置声库或音频设备。Headless、MCP 与跨 Host 场景共用进程沙箱，平台专有断言在相应系统执行。

`TestAudioAssets` 中的解析用例使用独立文件与模型 fixture，解码控制复用真实 Headless AppContext 的生产接线。应用生命周期在套件内共享，每例仍清理文档、任务、路径解析结果和通知；不再通过替写 AudioContext 或 DocumentWorkflowController 的生产方法链接测试。

`TestApplicationGui` 共用应用与数据隔离环境，钢琴窗、轨道、参数曲线、导出配置、外观设置和声线混合分别在所属源文件中建立实际控件。设置输入等待窗口激活与编辑焦点，再通过真实事件提交并读取持久化结果；取消按对应对话框契约检查，不假定即时设置具有回滚行为。按测试程序或 slot 定向执行即可，不另设 GUI 通用 runner；组件级套件与应用共用 `EditorGuiCore` 的生产实现及资源，原生桌面条件和实验后端范围见[测试大纲](test-outline.md)。

## 4. 隔离与清理

每个进程 fixture 使用独立配置和数据根、访问根及临时素材；单实例服务名从实际数据根派生。只管理测试创建的进程。成功清理，失败保留沙箱位置、stdout/stderr 与退出事实。等待有截止时间，任务竞态优先受控触发，保留必要资源锁。

CTest 注册同时由程序超时派生 `QTEST_FUNCTION_TIMEOUT`，避免 Qt Test 默认的五分钟 watchdog 在较长资源工作流完成内部失败处理前直接终止进程。任务自身的截止时间仍然有效；不通过延长推理等待来处理卡住的队列。

重启场景的源进程将 stdout/stderr 写入沙箱内文件，使脱离原 QProcess 生命周期的替代进程继承有效输出目标；仍检查新进程身份、参数、服务就绪和退出。

## 5. CI 引导与失败复验

1. Draft PR 触发真实 Actions；在三个矩阵项中分别打通依赖、配置、完整 Editor/Connector/测试构建及各测试组。新增 Windows 和 macOS 路径须完成真实调试后才能记录通过。
2. 查首个根因：依赖、编译、动态库/插件、断言、超时或共享状态污染。
3. 修复对应实现，在可复现环境单跑失败用例和所属组。
4. 提交、推送新代码，恢复各平台完整规定集合；重跑旧执行只能验证原代码版本。某个平台通过不能替代另一个平台，失败项持续保留诊断产物和正确退出码。
5. 按 triplet 隔离 Qt/vcpkg 缓存路径；验证冷缓存时只清除本 PR 对应缓存，完整运行成功后用相同代码验证缓存实际命中和完整集合通过。只在变更影响相应路径时重复验证；不清除其他 PR 的缓存，不缓存整个 CMake 构建目录。
6. 仅修改不影响构建或测试的文档时，无需重新执行或等待 CI；运行证据继续使用对应代码版本的已有结果。

不能依靠无限重跑、删除有效断言、沉默跳过或无依据加大超时获得绿色状态。

## 6. 覆盖率与缺口分析

Linux CI 的 Debug 构建启用 `LITE_TEST_COVERAGE=ON`，通过 GCC/gcov 和 gcovr 8.6 统计应用、内部库和 Connector 的行覆盖与分支覆盖。测试代码、第三方代码及生成文件不进入分母；编译器生成的异常清理分支和无源码分支不作为补测目标。未在 Linux 编译的平台实现也不在本次数字内，必须结合功能矩阵检查，不能当作已经覆盖。

本地 GCC 环境可以在上述 configure 命令追加 `-DLITE_TEST_COVERAGE=ON`，运行测试后执行：

```bash
python3 -m venv build/ci/coverage-env
build/ci/coverage-env/bin/pip install gcovr==8.6
mkdir -p build/test-results/coverage
build/ci/coverage-env/bin/gcovr --config scripts/ci/gcovr.cfg \
  --html-nested build/test-results/coverage/index.html \
  --json-summary build/test-results/coverage/summary.json \
  --csv build/test-results/coverage/files.csv --print-summary
```

Windows MSVC 常规测试不需要 gcovr。GCC 每次独立采样应使用干净的覆盖构建目录，或先删除该目录内旧 `.gcda`，避免累计历史执行结果。CI 每次重新构建，不缓存 CMake 构建目录。macOS CI 本期验证构建和测试，不将其结果混入 Linux 覆盖率。

Windows 原生覆盖率使用 Visual Studio 的 `Microsoft.CodeCoverage.Console.exe`。先通过标准 wrapper 构建独立 `coverage` preset；该构建提供 `/PROFILE` 和调试符号，并关闭 Edit and Continue。采集时使用静态插桩，避免启动阶段的动态插桩干扰进程测试超时。在可调用 CTest 的开发者终端中执行，`$collector` 指向所安装 Visual Studio 的覆盖率工具：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset coverage
python scripts/tests/collect-msvc-coverage.py --collector $collector --output build/test-results/msvc-baseline -- -LE resources

$env:DSEL_TEST_VOICEBANK_ROOT = "C:/Voicebanks/Example"
$env:DSEL_TEST_LANGUAGE = "cmn"
$env:DSEL_TEST_LYRIC = "la"
python scripts/tests/collect-msvc-coverage.py --collector $collector --output build/test-results/msvc-full
```

声库路径、语言和歌词须与本机资源匹配，多音源时同样指定 `DSEL_TEST_SINGER_ID`。CTest 未在 PATH 时可用 `--ctest` 提供路径。每轮使用新的输出目录；脚本保留原始 `.coverage`、Cobertura、源码行去重 CSV、JUnit 和完整测试日志，并传播执行失败。比较无资源与全量集合时使用同一构建，按源码行并集合并各程序中的重复记录，检查分母和实际执行结果。Microsoft 原生报告不提供与 GCC 对等的分支覆盖，不采用其占位的分支百分比。

Linux CI 保存 HTML、JSON、文本及 gcovr 原生逐文件 CSV，并将 `files.csv` 输出到 workflow 文本日志；三个平台分别保存环境、依赖/构建日志、JUnit 和失败素材。大型 artifact 下载受阻时，可从该轮日志读取逐文件统计，仍以同一受测版本为准；这些是本期测试产物，不生成或自动改写阶段文档。

采样产物记录实际受测代码、执行集合和失败项，详细关联由 PR 和 Actions 保留。构建成功但测试失败时得到的报告可用于发现遗漏；最终候选另行完整运行，不能把失败轮次的覆盖率和后续未执行补测合并成通过结论。阶段文档保留统计口径和缺口处置，不维护每轮覆盖率数值。

结合 HTML 未执行行、目录汇总及功能矩阵判断缺口：优先未测的编辑结果、失败回滚、配置生效、文件发布和实际交互。数值低并不自动要求补测；设备/模型依赖、平台专属、主观观感及不可达的防御路径需说明范围。没有百分比门槛，不对 Schema、工具清单、无语义 getter 或历史 bug 数量设指标。覆盖率用于发现遗漏，不代替有意义的断言。[gcovr 统计口径](https://gcovr.com/en/stable/faq.html)。

## 7. 验收与审查

三个 CI 矩阵平台的规定集合，以及本地通用/进程/适用 GUI 和已配置资源验证完成，报告和实现一致后 ready；尚在调试的平台不能记为通过。文档更新不要求重复等待已验证代码的 CI。约五分钟后检查审查；真实问题修复、验证、回复并 resolve。ready 后代码或构建配置变化须 `@codex review`。最终受审代码的相应验证和 bot 明确认可同时成立才完成。
