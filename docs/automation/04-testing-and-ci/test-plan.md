# 四期测试执行计划

## 1. 执行顺序

先验证环境和完整构建，再运行 unit、domain、workflow、protocol、process、gui。具体程序由 CTest 标签选择；Qt Test 函数可直接定向运行。覆盖要求见[大纲](test-outline.md)，本期验收结论见[报告](test-report.md)，逐轮执行结果保留在测试产物和 PR 中。

## 2. 环境与构建

Windows 使用项目 VS DevShell/preset wrapper；Linux 和 macOS 使用同一 CMake 工程及固定依赖。所有测试启用 `LITE_BUILD_TESTS`，构建完整产品和测试聚合目标。CI 统一 Qt 6.11.2，平台配置如下；配置列表示执行要求，成功与否以实际运行结果为准。

| 平台 | runner / 架构 | triplet | CI 构建与测试入口 |
|---|---|---|---|
| Linux | `ubuntu-latest` / x64 | `x64-linux` | `coverage` / `build/Coverage`，`ctest --preset ci-coverage`，随后独立 Coverage 步骤 |
| Windows | `windows-latest` / x64 | `x64-windows` | 普通 Debug `tests` / `build/Tests`，`ctest --preset ci` |
| macOS | `macos-latest` / arm64 | `arm64-osx` | 普通 Debug `tests` / `build/Tests`，`ctest --preset ci` |

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

Windows wrapper 将本地结果写到 `build/test-results`；直接 CTest 可加 `--output-junit` 指定报告路径，完整逐例输出位于构建目录的 `Testing/Temporary/LastTest.log`。`local` 选择本机全部注册测试，`ci` 包含通用、GUI 与内置声库集合，`ci-coverage` 在独立覆盖构建中执行同一集合。CI 仅 Linux 使用覆盖构建；Windows/macOS 的完整测试不要求 Microsoft 覆盖率收集器或 LLVM 插桩工具。无桌面 Linux 使用 `xvfb-run -a` 包装 CTest；单组可用 CTest 的 `-L`/`-R`，具体函数可直接给 Qt Test 程序传函数名。

程序划分见[测试大纲的套件表](test-outline.md#9-套件职责与程序划分)。每个程序只有一个 Qt Test 类及一次执行入口，用例文件为 `tst_<snake_case>.cpp`。多文件套件由 `test_main.cpp` 启动、`tst_<domain>.h` 声明测试类；单文件套件可在 `tst_` 文件中保留 `QTEST_MAIN` 或自定义入口，fixture 辅助使用语义名称。定向执行选择程序或 slot，不新增与源码平行的用例清单。两个 Provider 编译变体均须构建和执行，其用例不要求 CUDA 设备。

执行产物记录源码版本、系统、编译器、Qt、CMake、Ninja、vcpkg 与子模块版本，用于重现问题。阶段文档不复制这些逐轮元数据，也不要求每次提交后更新报告。

## 3. 运行条件

| 条件 | CI | 本地 |
|---|---|---|
| 通用、无窗口 | 执行 | 所有支持平台 |
| offscreen 组件 | 执行 | 原生或 offscreen |
| 原生窗口布局 | 执行；Linux 使用 Xvfb | 当前平台桌面 |
| 内置测试声库 | 执行 | 默认执行 |
| 音频输出设备 | 有可用设备则执行，否则明确跳过 | 自动探测或按名称指定 |
| MIDI 回环 | 未配置则明确跳过 | 提供成对专用端口后执行 |
| 外部真实声库、GPU | 不作为默认资源 | 显式配置后；GPU 不等同于已有 CPU 用例 |
| 平台特有行为 | 各矩阵平台对应项 | 当前平台对应项 |

不以 CI 集合代替本地完整入口。资源不足和平台不适用必须明确，不计作通过。

原生桌面用例归入 `TestNativeDesktop`，由 `native` 标签标识，纳入三平台 CI。`TestGuiComponents`、`TestEditorInteraction`、`TestEditorRendering` 和 `TestApplicationGui` 使用 offscreen；普通组件无需播放设备，完整填词流程使用内置声库。

设备用例在 `TestNativeDesktop` 中按下列条件执行，环境变量均填写设备实际名称：

| 配置 | 行为与未执行规则 |
|---|---|
| `DSEL_TEST_AUDIO_DRIVER`、`DSEL_TEST_AUDIO_DEVICE` | 两者均可省略，使用生产默认输出；无可用后端、未枚举到设备或仅有 dummy/disk 后端时 QSKIP。可只指定驱动，或在当前驱动下指定设备；指定名称不存在、已枚举设备初始化/打开失败、配置或播放失败均 FAIL，不回退其他设备 |
| `DSEL_TEST_MIDI_INPUT`、`DSEL_TEST_MIDI_OUTPUT` | 填写已连接的专用 MIDI 回环输入/输出端口，测试不创建驱动。两者均缺少时 QSKIP；只配一项、端口不存在、打开/发送失败或收不到预期消息均 FAIL |

例如在 PowerShell 中设置 `$env:DSEL_TEST_AUDIO_DRIVER = "实际驱动名"`、`$env:DSEL_TEST_AUDIO_DEVICE = "实际设备名"`，或成对设置 `$env:DSEL_TEST_MIDI_INPUT` 与 `$env:DSEL_TEST_MIDI_OUTPUT`，随后执行 `ctest --preset local -R '^TestNativeDesktop$'`。Linux/macOS 使用同名环境变量。不要将示例名称当作有效设备配置。

音频场景使用静音 WAV，验证生产设备重开、缓冲配置及落盘、真实回调推进和公开播放/暂停/停止，完成后恢复自身配置。MIDI 场景发送专用通道的 note-on/off，验证实际输入及生产合成器输出后归零，并清理自己打开的端口；不要求扬声器发声。跳过的是缺少条件的具体 Qt Test 用例，不是整个原生桌面程序。Null RHI 钢琴窗和轨道场景不需要物理 GPU，但需要可用原生窗口环境；offscreen 不适用时明确跳过，实际后端或帧提交失败不能跳过。

`TestModelResources`、`TestApplicationGui` 和 `TestApplicationWorkflows` 默认使用同一 [voicebank-fixture.zip](../../../src/tests/resources/voicebank-fixture.zip)，CMake 解压到当前构建目录；无需安装 Python 或训练框架。测试包具有独立的 `ci-fixture` 身份、中英文资源和两条声线。资源程序检查完整推理、有效 WAV、缓存和重算，应用工作流检查真实读音/音素结果暂存与应用，GUI 使用语言及波形结果；均固定 CPU。普通 CI 不需真实训练权重或播放设备。

验证外部真实声库时设置 `DSEL_TEST_VOICEBANK_ROOT`、`DSEL_TEST_LANGUAGE`、`DSEL_TEST_LYRIC`，并按该资源配置 `DSEL_TEST_SINGER_ID`。显式资源优先于内置包；加载或执行失败报告失败，不自动扫描个人声库，也不回退内置资源。GAME/RMVPE 模型不在本期扩展范围。

`TestApplicationWorkflows` 属于 workflow 集合，共用隔离的 Headless AppContext 并加载上述声库。结果门控用例构造未调度的任务快照；读音/音素用例执行真实语言任务，在编辑事务中等待结果暂存，检查结束后应用以及文档换代/片段删除后丢弃。队列重启回归受控暂停真实 duration worker，验证取消清理和替换任务终态；生产状态组件检查手动声学许可及恢复策略。离线导出检查混音器原先打开/关闭状态恢复。fixture 关闭自身音频设备，完整声学和声码器输出仍由资源程序承担。

`TestProcessIntegration` 的普通音频导入导出场景使用生成的小型 WAV，经过实际 Editor 导入和导出任务，再用 libsndfile 解码验证；归入通用进程集合，不需要配置声库或音频设备。Headless、MCP 与跨 Host 场景共用进程沙箱，平台专有断言在相应系统执行。

`TestAudioAssets` 中的解析用例使用独立文件与模型 fixture，解码控制复用真实 Headless AppContext 的生产接线。应用生命周期在套件内共享，每例仍清理文档、任务、路径解析结果和通知；不再通过替写 AudioContext 或 DocumentWorkflowController 的生产方法链接测试。

`TestApplicationGui` 共用应用与数据隔离环境，钢琴窗、轨道、参数曲线、导出配置、外观设置和声线混合分别在所属源文件中建立实际控件。设置输入等待窗口激活与编辑焦点，再通过真实事件提交并读取持久化结果；取消按对应对话框契约检查，不假定即时设置具有回滚行为。按测试程序或 slot 定向执行即可，不另设 GUI 通用 runner；组件级套件与应用共用 `EditorGuiCore` 的生产实现及资源，原生桌面条件和实验后端范围见[测试大纲](test-outline.md)。

主窗口面板、嵌入设置、日志和 Tagger 用例同属 ApplicationGui，使用真实控件及通知总线，结束后恢复配置、规则和窗口接线。`newDocumentHonorsTheSaveDecision` 虽复用该程序的 AppContext，职责属于文件工作流：只替换保存提示及路径选择的外部回答，保留生产 DocumentWorkflowController、状态机和保存器；不据此声明真实保存对话框已被操作。

完整窗口用例先完成窗口初始化，再准备受测工程；通知连接绑定实际接收者，局部控件正常析构并清理其外部引用。RHI 场景同样执行私有子控件释放过程，不能通过遗留窗口、跳过析构或屏蔽事件规避生命周期失败。

EditorInteraction 由 CTest 设置 `QT_QPA_PLATFORM=minimal:enable_fonts` 和 Fusion，真实 QDrag 经鼠标移动与释放进入 Qt 拖放循环，Escape 走取消路径；不向私有状态注入拖放结果。该套件与 NativeDesktop 共用 GuiAppFixture 的真实应用接线和隔离目录。Qt offscreen 直接忽略 QDrag，因此 ApplicationGui 的后端选择不能代替完整拖放验证。

## 4. 隔离与清理

ApplicationWorkflows、AudioAssets、ApplicationGui、EditorInteraction 和 NativeDesktop 的实际运行时用例共用 `RuntimeResourcesFixture`，从所构建 Editor 目录初始化内置歌词规则；macOS 同时使用该 bundle 的插件及 Frameworks。CTest 和直接运行使用相同初始化，不依赖测试可执行文件恰好位于产品资源旁边。内部路径覆盖仅在测试构建中生效；目录缺失、规则为空或插件加载失败均按失败处理。

每个进程 fixture 使用独立配置和数据根、访问根及临时素材；单实例服务名从实际数据根派生。只管理测试创建的进程。成功清理，失败保留沙箱位置、stdout/stderr 与退出事实。等待有截止时间，任务竞态优先受控触发，保留必要资源锁。

CTest 注册同时由程序超时派生 `QTEST_FUNCTION_TIMEOUT`，避免 Qt Test 默认的五分钟 watchdog 在较长资源工作流完成内部失败处理前直接终止进程。任务自身的截止时间仍然有效；不通过延长推理等待来处理卡住的队列。

重启场景的源进程将 stdout/stderr 写入沙箱内文件，使脱离原 QProcess 生命周期的替代进程继承有效输出目标；仍检查新进程身份、参数、服务就绪和退出。

## 5. CI 引导与失败复验

1. Draft PR 触发真实 Actions；在三个矩阵项中分别打通依赖、配置、完整 Editor/Connector/测试构建及各测试组。新增 Windows 和 macOS 路径须完成真实调试后才能记录通过。
2. 查首个根因：依赖、编译、动态库/插件、断言、超时或共享状态污染。
3. 修复对应实现，在可复现环境单跑失败用例和所属组。本期先完成 Windows 本地完整构建、测试和覆盖率采样，确认逻辑缺口及实际失败均已处理后再推送。
4. 提交、推送新代码，恢复各平台完整规定集合；重跑旧执行只能验证原代码版本。某个平台通过不能替代另一个平台，失败项持续保留诊断产物和正确退出码。
5. 按 triplet 隔离 Qt/vcpkg 缓存路径；验证冷缓存时只清除本 PR 对应缓存，完整运行成功后用相同代码验证缓存实际命中和完整集合通过。只在变更影响相应路径时重复验证；不清除其他 PR 的缓存，不缓存整个 CMake 构建目录。
6. 仅修改不影响构建或测试的文档时，无需重新执行或等待 CI；运行证据继续使用对应代码版本的已有结果。

不能依靠无限重跑、删除有效断言、沉默跳过或无依据加大超时获得绿色状态。

vcpkg 精确缓存键包含 runner 镜像身份，避免 latest 镜像升级后重新构建的 ABI 包无法保存。回退缓存仅提供候选包，install 负责判断是否可用；依赖准备时间与实际复用包数以日志为准。三平台的 Tests 步骤直接执行 CTest；Linux 的 Coverage 步骤单独读取已有执行数据生成报告，构建或测试失败时仍保留可用诊断材料。

整体 job 超时覆盖完整构建、测试和适用的报告生成，不替代 CTest 程序超时及异步任务截止时间。按实际阶段区分构建、测试与报告故障；本地按需运行原生采集时，还须单独核对插桩阶段，不能把前置插桩耗时当作测试挂起。

## 6. 覆盖率与缺口分析

gcovr 启用 `merge-lines`，按文件和源码行号合并不同函数或模板实例的命中；重算既有原始数据时，数字变化只属于统计口径调整，不算新增测试贡献。以同一口径比较新增命中行及剩余行为，逻辑覆盖的 90% 方向目标不作为硬失败阈值。

Linux CI 的 Debug 构建启用 `LITE_TEST_COVERAGE=ON`，全部测试完成后，独立 Coverage 步骤通过 GCC/gcov 和 gcovr 8.6 生成应用、内部库和 Connector 的行覆盖与分支覆盖，不重复执行测试。Windows/macOS CI 使用普通 Debug 构建，覆盖率采集保留为本地按需能力。测试代码、第三方代码及生成文件不进入分母；编译器生成的异常清理分支和无源码分支不作为补测目标。未在 Linux 编译的平台实现也不在该数字内，必须结合功能矩阵检查，不能当作已经覆盖。

本地 GCC 环境使用 `coverage` configure/build preset，运行该构建中的测试后执行：

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

普通测试不要求覆盖率工具。GCC 每次独立采样应使用干净的覆盖构建目录，或先删除该目录内旧 `.gcda`，避免累计历史执行结果。CI 每次重新构建，不缓存 CMake 构建目录。本地跨平台专项采样分别统计，不将结果混合为一个比例。

Windows 本地按需采样使用 Visual Studio 的 `Microsoft.CodeCoverage.Console.exe`，不属于本期 CI 的必需步骤。先通过标准 wrapper 构建独立 `coverage` preset；该构建提供 `/PROFILE` 和调试符号，并关闭 Edit and Continue。采集时使用静态插桩；脚本自动查找已安装工具，也可用 `--collector` 指定路径。在可调用 CTest 的开发者终端中执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset coverage
python scripts/tests/collect-msvc-coverage.py --output build/test-results/msvc-full -- --preset coverage

$env:DSEL_TEST_VOICEBANK_ROOT = "C:/Voicebanks/Example"
$env:DSEL_TEST_SINGER_ID = "example-singer"
$env:DSEL_TEST_LANGUAGE = "cmn"
$env:DSEL_TEST_LYRIC = "la"
python scripts/tests/collect-msvc-coverage.py --output build/test-results/msvc-external -- --preset coverage
```

声库路径、歌手 ID、语言和歌词都须填写本机资源对应的实际值，显式资源的这四项配置缺一不可。CTest 未在 PATH 时可用 `--ctest` 提供路径。每轮使用新的输出目录；脚本保留原始 `.coverage`、Cobertura、源码行去重 CSV、JUnit 和完整测试日志，并传播执行失败。比较不同资源集合时使用同一构建，按源码行并集合并各程序中的重复记录，检查分母和实际执行结果。Microsoft 原生报告不提供与 GCC 对等的分支覆盖，不采用其占位的分支百分比。

macOS 本地按需采样也使用独立 `coverage` 构建，普通 CI 不执行该流程：

```bash
cmake --preset coverage -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" \
  -DQt6_DIR="$QT_ROOT_DIR/lib/cmake/Qt6" -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build --preset coverage
python3 scripts/tests/collect-llvm-coverage.py --output build/test-results/llvm-full -- --preset coverage
```

脚本通过 `xcrun` 选择与编译器配套的 LLVM 工具，为各进程/模块隔离 profile，再生成合并 profile、LCOV、HTML、源码行去重 CSV 和原生分支汇总。Windows 与 LLVM 的公共源码过滤及行汇总共用 `coverage_support.py`，不重复实现统计规则。

Linux CI 保存 HTML、JSON、文本及 gcovr 原生逐文件 CSV；三平台都保存环境、依赖/构建日志、JUnit 和失败素材。Windows/macOS 按需专项采样在各自输出目录保存覆盖率材料，建设中已取得的有效证据继续保留。数字以对应产物为准；这些是本期测试产物，不生成或自动改写阶段文档。

采样产物记录实际受测代码、执行集合和失败项，详细关联由 PR 和 Actions 保留。构建成功但测试失败时得到的报告可用于发现遗漏；最终候选另行完整运行，不能把失败轮次的覆盖率和后续未执行补测合并成通过结论。阶段文档保留统计口径和缺口处置，不维护每轮覆盖率数值。

结合 HTML 未执行行、目录汇总及功能矩阵判断缺口：优先未测的编辑结果、失败回滚、配置生效、文件发布和实际交互。数值低并不自动要求补测；设备/模型依赖、平台专属、主观观感及不可达的防御路径需说明范围。没有百分比门槛，不对 Schema、工具清单、无语义 getter 或历史 bug 数量设指标。覆盖率用于发现遗漏，不代替有意义的断言。[gcovr 统计口径](https://gcovr.com/en/stable/faq.html)。

## 7. 验收与审查

三个 CI 矩阵平台的全部适用测试、Linux 独立 Coverage 步骤，以及本地规定验证完成，报告和实现一致后 ready；Windows/macOS 普通 Debug CI 不以原生或 LLVM 采集作为验收条件，尚在调试的平台不能记为通过。文档更新不要求重复等待已验证代码的 CI。约五分钟后检查审查；真实问题修复、验证、回复并 resolve。审查由仓库配置智能判断和自动触发，不逐次发送人工触发评论。最终受审代码的相应验证和 bot 明确认可同时成立才完成。
