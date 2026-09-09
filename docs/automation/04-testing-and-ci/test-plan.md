# 四期测试执行计划

## 1. 执行顺序

先验证环境和完整构建，再运行 unit、domain、workflow、protocol、process、gui。具体程序由 CTest 标签选择；Qt Test 函数可直接定向运行。覆盖要求见[大纲](test-outline.md)，实际结果仅写入[报告](test-report.md)。

## 2. 环境与构建

Windows 使用项目 VS DevShell/preset wrapper。Linux 使用同一 CMake 工程和固定依赖，不依赖开发机 PATH。所有测试启用 `LITE_BUILD_TESTS`，构建完整产品和测试聚合目标。

专用 `tests` configure/build preset 使用 `build/Tests`，避免与 IDE 的 `build/Debug` 自动配置共享生成文件。Windows 入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Dependencies -Preset tests
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset tests
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Test -Preset local
```

Linux 本地已准备 Qt 与系统开发依赖后，将 `QT_ROOT_DIR` 指向实际 Qt 安装目录。当前验证版本为 Qt 6.11.2，模块包括 Core5Compat、ShaderTools、StateMachine、HttpServer 和 WebSockets；Ubuntu 开发包与固定 vcpkg 引导见[workflow](../../../.github/workflows/tests.yml)和[bootstrap-linux.sh](../../../scripts/ci/bootstrap-linux.sh)。本地有桌面环境时：

```bash
bash scripts/ci/bootstrap-linux.sh
cmake --preset tests -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" \
  -DQt6_DIR="$QT_ROOT_DIR/lib/cmake/Qt6" -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build --preset tests
ctest --preset local
```

Windows wrapper 将本地结果写到 `build/test-results`；直接 CTest 可加 `--output-junit` 指定报告路径，完整逐例输出位于 `build/Tests/Testing/Temporary/LastTest.log`。`local` 选择本机全部注册测试，`ci` 只选择通用与 offscreen 集合，适用于 CI 或没有桌面的本地执行；单组可用 CTest 的 `-L`/`-R`，具体函数可直接给 Qt Test 程序传函数名。

执行时记录 commit、系统、编译器、Qt、CMake、Ninja、vcpkg 与子模块版本。正式候选的命令及结果在测试报告中记录。

## 3. 运行条件

| 条件 | CI | 本地 |
|---|---|---|
| 通用、无窗口 | 执行 | 所有支持平台 |
| offscreen 组件 | 执行 | 原生或 offscreen |
| 原生桌面 | 不执行 | 当前平台 |
| 模型、声库、设备 | 不执行 | 显式配置后 |
| 平台特有行为 | Linux 对应项 | 当前平台对应项 |

不以 Linux CI 集合代替本地完整入口。资源不足和平台不适用必须明确，不计作通过。

当前需要原生桌面的两个目标是 `TestAnimationSettings` 和 `TestOverlaySplitter`；它们仍由 local preset 自动执行。其余 GUI 组件使用 offscreen，不需要声库或音频设备。

真实声库用例为 `TestHeadlessResources`：设置 `DSEL_TEST_VOICEBANK_ROOT`、`DSEL_TEST_LANGUAGE`、`DSEL_TEST_LYRIC`，多音源时再指定 `DSEL_TEST_SINGER_ID`。用例固定 CPU，创建短音符，完成推理及 WAV 导出并检查可解码、有限样本和非零能量。未设置声库根时明确跳过；配置后的失败为失败，不自动扫描个人声库。

`TestApplicationWorkflows` 属于通用 workflow 集合，共用隔离的 Headless AppContext。推理用例构造实际任务快照并调用完成门控，关闭自动推理且不调度这些任务；离线导出用例验证混音器原先打开或关闭两种状态的恢复。fixture 关闭自己持有的音频设备，通用用例不依赖声库输出或物理设备；实际模型执行由上述资源用例负责。

`TestHeadlessProcessIntegration::audioImportAndWaveExport` 使用生成的小型 WAV，经过实际 Editor 导入和导出任务，再用 libsndfile 解码验证；归入通用进程集合，不需要配置声库或音频设备。

## 4. 隔离与清理

每个进程 fixture 使用独立配置和数据根、访问根及临时素材；单实例服务名从实际数据根派生。只管理测试创建的进程。成功清理，失败保留沙箱位置、stdout/stderr 与退出事实。等待有截止时间，任务竞态优先受控触发，保留必要资源锁。

重启场景的源进程将 stdout/stderr 写入沙箱内文件，使脱离原 QProcess 生命周期的替代进程继承有效输出目标；仍检查新进程身份、参数、服务就绪和退出。

## 5. CI 引导与失败复验

1. Draft PR 触发真实 Actions；逐步打通依赖、配置、全构建及各测试组。
2. 查首个根因：依赖、编译、动态库/插件、断言、超时或共享状态污染。
3. 修复对应实现，在可复现环境单跑失败用例和所属组。
4. 提交、推送新代码，恢复完整规定集合；旧 run 重跑仅诊断旧 SHA。
5. 最终候选先清除本 PR 对应的 Qt/vcpkg 缓存，完整运行成功后在同一 SHA 重跑，核对缓存实际命中和完整集合通过。只在变更影响相应路径时重复验证；不清除其他 PR 的缓存。

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

Windows MSVC 常规测试不需要 gcovr。每次独立采样应使用干净的覆盖构建目录，或先删除该目录内旧 `.gcda`，避免累计历史执行结果。CI 每次重新构建，不缓存 CMake 构建目录。

CI 同时保存 HTML、JSON、文本及 gcovr 原生逐文件 CSV，并将 `files.csv` 输出到 workflow 文本日志。大型 artifact 下载受阻时，可直接从该轮日志读取逐文件行/分支统计，仍以同一受测版本为准；这些是本期测试产物，不生成或自动改写阶段文档。

采样记录 PR head、Actions 实际 checkout SHA、执行集合和失败项。构建成功但测试失败时得到的报告可用于发现遗漏；最终候选另行完整运行，不能把失败轮次的覆盖率和后续未执行补测合并成通过结论。

结合 HTML 未执行行、目录汇总及功能矩阵判断缺口：优先未测的编辑结果、失败回滚、配置生效、文件发布和实际交互。数值低并不自动要求补测；设备/模型依赖、平台专属、主观观感及不可达的防御路径需说明范围。没有百分比门槛，不对 Schema、工具清单、无语义 getter 或历史 bug 数量设指标。覆盖率用于发现遗漏，不代替有意义的断言。[gcovr 统计口径](https://gcovr.com/en/stable/faq.html)。

## 7. 验收与审查

Linux 全集合、Windows 本地通用/进程/适用 GUI 验证完成，报告和实现一致后 ready。约五分钟后检查审查；真实问题修复、验证、回复并 resolve。ready 后代码或构建配置变化须 `@codex review`。最终当前版本 CI 和 bot 明确认可同时成立才完成。
