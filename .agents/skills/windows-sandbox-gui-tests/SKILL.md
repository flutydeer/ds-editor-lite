---
name: windows-sandbox-gui-tests
description: 在 Windows Sandbox 里运行 DS Editor Lite 的原生 GUI 测试用例，沙盒不可用时回退到当前桌面。用于需要真实窗口、真实焦点和窗口生命周期的 QtTest 用例，包括整套 NativeDesktopTests、单用例筛选和窗口弹出证据采集。
---

# 原生 GUI 测试（Windows Sandbox 优先）

## 目标

把 `TestNativeDesktop.exe` 优先放进一次性 Windows Sandbox 执行，宿主只负责启动沙盒和读取结果。测试期间宿主键鼠保持可用，测试窗口只出现在沙盒桌面里。沙盒不可用时可以静默回退到当前桌面执行，窗口会弹出并可能抢焦点。

只有同时满足以下条件才报告通过：

- 客体脚本 `runnerExitCode` 为 0，JUnit 可读
- 每个计划用例都实际执行，失败数为 0
- `windows/windows-summary.json` 记录到真实窗口（用例本身不建窗时除外）

## 为什么优先用沙盒

原生 GUI 测试会创建真实窗口、抢焦点、合成鼠标与触摸事件。在宿主直接运行会打断用户正在做的事，也可能把快捷键送进别的程序。Sandbox 有独立桌面和输入队列，宿主只看到沙盒窗口本身，所以默认先用沙盒。

沙盒不可用（功能未安装、hypervisor 未启动、没有 `wsb.exe`、构建路径在 C: 盘）时允许直接回退到当前桌面，不需要事先告知，不需要等待确认，也不等待用户空闲。回退模式会真的弹出窗口并可能抢焦点，全托管任务要接受这一点。

## 前置条件

1. Windows Sandbox 已启用。`Containers-DisposableClientVM` 的 `InstallState` 为 1，且 `HypervisorPresent` 为 True。
2. `wsb.exe` 存在，位置为 `%LOCALAPPDATA%\Microsoft\WindowsApps\wsb.exe`。
3. 构建产物存在：`build\<preset>\out\bin\TestNativeDesktop.exe`，同目录下有 `plugins` 和 `configs`。
4. 夹具声库存在：`build\<preset>\src\tests\resources\ci-fixture@1.0.0`。
5. 沙盒路径要求仓库位于非 C: 盘。测试二进制在编译时把构建目录的绝对路径写进了配置查找，客体需要用 `subst` 复刻该路径，C: 是系统盘无法替换。回退到当前桌面时该路径本来就存在，没有这条限制。

只读检查宿主是否就绪：

```powershell
Get-CimInstance Win32_OptionalFeature -Filter "Name='Containers-DisposableClientVM'" | Select-Object InstallState
(Get-CimInstance Win32_ComputerSystem).HypervisorPresent
Test-Path "$env:LOCALAPPDATA\Microsoft\WindowsApps\wsb.exe"
```

## 目录布局

默认试验根目录是 `build\SandboxGuiTrial`（位于 `build\` 下，已被 gitignore）。

```text
build\SandboxGuiTrial\
  payload\                 只读映射到客体 C:\SandboxTrialInput
    bin\                   TestNativeDesktop.exe 及其插件、配置、资源
    voicebank\             夹具声库
    plan.json              执行计划
    manifest.json          逐文件 SHA-256 清单
    Run-Guest.ps1          客体入口
    Watch-Windows.ps1      窗口观察器
  results\                 可写映射到客体 C:\SandboxTrialResults
    <UTC时间>-<随机ID>\     每轮结果
  host-results\            回退到当前桌面时的结果目录
  SandboxGuiTrial.wsb      由模板生成，含绝对路径
  current-sandbox.json     本轮沙盒 ID，供停止脚本使用
```

## 流程

### 1. 准备 payload

```powershell
.\.agents\skills\windows-sandbox-gui-tests\scripts\New-SandboxGuiPayload.ps1 -Preset debug
```

默认跑整套用例。要挑选用例时传 `-Selectors`：

```powershell
.\.agents\skills\windows-sandbox-gui-tests\scripts\New-SandboxGuiPayload.ps1 -Preset debug `
    -Selectors 'detachedPanelDiesWithItsMainWindow','rhiNoteDrawingCommitsAndUndoUpdatesInteraction:draw-tool'
```

参数说明：

- `-Selectors` 每项是 QtTest 选择器，`function` 或 `function:datatag` 两种写法。默认有一项空字符串，表示不带筛选跑整套。
- `-CaseTimeoutSeconds` 为 0 时使用默认值，整套 1800 秒，单用例 180 秒。
- `-UseHardLinks` 用硬链接代替复制，同一卷上省空间，适合反复准备。
- `-Force` 覆盖已有 payload。
- `-BinSource` 和 `-VoicebankSource` 用于非标准构建目录。

脚本只复制文件、写 `plan.json` 和 `manifest.json`，不执行任何程序。

### 2. 启动沙盒

```powershell
.\.agents\skills\windows-sandbox-gui-tests\scripts\Start-SandboxGuiTrial.ps1
```

不带 `-Launch` 时只做检查：功能状态、hypervisor、是否已有沙盒客户端、清单哈希、生成的 `.wsb` 映射与开关。确认无误后：

```powershell
.\.agents\skills\windows-sandbox-gui-tests\scripts\Start-SandboxGuiTrial.ps1 -Launch
```

沙盒窗口出现后，客体脚本先等待 20 秒。这时请最小化沙盒窗口并回到宿主工作，测试期间不要点击客体画面。启动脚本会把沙盒 ID 写到 `current-sandbox.json`。

### 3. 观察运行

结果直接出现在宿主 `results\<runId>\`。等待完成时可以看这几个文件：

```powershell
Get-Content "build\SandboxGuiTrial\results\<runId>\runner.log" -Tail 20
Get-Content "build\SandboxGuiTrial\results\<runId>\summary.json" -Raw
```

出现 `DONE.txt` 表示客体脚本正常收尾。

### 4. 关闭沙盒

```powershell
.\.agents\skills\windows-sandbox-gui-tests\scripts\Stop-SandboxGuiTrial.ps1
```

脚本读取记录的沙盒 ID，以 System 身份在客体执行关机，并等待 `WindowsSandboxRemoteSession.exe` 与 `vmmemWindowsSandbox` 退出。

**每轮结束都要关闭整个沙盒再重开。** 客体脚本发现 `C:\SandboxTrialWork` 已存在时会拒绝运行，这正是为了强制重建干净环境。只关测试程序或只在客体内部重启都不算干净。

## 回退：在当前离线桌面运行

只在沙盒不可用时使用。脚本默认只做检查，必须显式传 `-AllowHostGui` 才会启动 GUI。`-AllowHostGui` 是调用方的意图标记，不是交互确认，全托管任务可以直接带上。沙盒可用时脚本直接拒绝执行，传了 `-AllowHostGui` 也不会绕开沙盒优先。

```powershell
.\.agents\skills\windows-sandbox-gui-tests\scripts\Invoke-HostGuiTrial.ps1
.\.agents\skills\windows-sandbox-gui-tests\scripts\Invoke-HostGuiTrial.ps1 -AllowHostGui
```

与沙盒路径的区别：

- 不复制 payload、不校验清单、不改注册表，直接运行 `build\<preset>\out\bin` 里的二进制
- 测试窗口出现在当前桌面并可能抢焦点，运行期间不要操作键鼠
- 有真实音频设备与 MIDI 端口，不需要客体 MIDI 规避
- 结果写到 `build\SandboxGuiTrial\host-results\<runId>\`，文件结构与沙盒结果一致
- 窗口观察器加 `-AllowHostDesktop`，截图内容是当前桌面
- 超时只结束自己启动的进程树
- 实际执行前会再次确认沙盒仍不可用，可用则报错退出

检查模式只用于排查。正常自动化可以直接执行，不需要先征求用户同意。

## 结果判读

`summary.json` 的 `overall` 与用例 `status`：

| 值 | 含义 |
|---|---|
| `AllSelectedCasesPassed-IsolationStillRequiresObservation` | 沙盒模式全部通过，但宿主键鼠隔离仍需人工确认 |
| `AllSelectedCasesPassed` | 回退模式全部通过，窗口就在当前桌面 |
| `Incomplete-SkippedCoverage` | 有用例被跳过，退出码 3，不能算完整通过 |
| `Failed` | 有用例失败，退出码 1 |
| `PassedWithSkips` | 整套运行完成、无失败，但有 Skip |
| `Skipped-Unverified` | 单用例模式下的跳过，不算通过 |
| `TimedOut` | 超过外层超时，已结束该用例进程树 |
| `CrashOrLoaderError` | 退出码为负数或高位为 1，通常是崩溃或加载器错误 |
| `RequestedCaseNotExecutedExactlyOnce` | JUnit 里没有恰好出现一次指定用例 |
| `WindowEvidenceMissing` | 计划要求窗口证据，但观察器没有产出或没有记录到测试窗口，退出码 6 |

退出码约定：0 全通过，1 有失败，2 准备或脚本异常，3 有跳过，4 客体 MIDI 枚举仍不可用，6 缺少窗口证据。

整套运行默认要求窗口证据，观察器没有记录到测试窗口时整轮判为 `WindowEvidenceMissing`。单用例模式默认不要求，因为部分用例本身不渲染窗口，需要强制时在 `plan.json` 里把 `requireWindowEvidence` 改成 `true`。

QtTest 6.11 的 JUnit 名称规则：数据行写作 `function(tag)`，非数据用例可能是 `function` 或 `function()`。整套运行时 `summary.json` 里的 `executedTestcaseCount` 不含 `initTestCase` 与 `cleanupTestCase`。

## 证据文件

- `summary.json`：用例状态、退出码、耗时、MIDI 规避记录、窗口观察汇总。
- `<case>.stdout.txt`、`<case>.stderr.txt`：QtTest 控制台输出。用例控制台日志在 stdout，插件加载日志在 stderr。
- `<case>.junit.xml`：结构化结果，可直接看 `testsuite` 属性与 `testcase` 节点。
- `windows/windows.csv`：150 毫秒采样的窗口快照，含标题、类名、尺寸、最小化状态。
- `windows/windows-summary.json`：窗口句柄数、采样数、截图数、首次与末次出现时间。
- `windows/shots/*.png`：新窗口出现时抓取的客体桌面截图，只读观察，不注入输入。
- 回退模式的结果在 `host-results\<runId>\`，目录结构相同，`summary.json` 里 `mode` 为 `HostDesktop`。

## 已知坑

### 客体 MidiSrv 启动失败导致枚举永久阻塞

Windows Sandbox 里 `Midisrv` 每次启动即以 `0x80073B01` 退出。winmm 的 `wdmaud2` 转接在 `midiInGetNumDevs` 内无限等待，产品会停在 `Audio::MidiSystem` 初始化，表现为所有用例 180 秒超时。

`Run-Guest.ps1` 会在测试前移除 64 位 `Drivers32` 的 `midi1` 值并保留 `midi=wdmaud.drv`，再起一个 20 秒看门狗的独立进程确认枚举立即返回。改动只发生在一次性客体里，不碰宿主注册表，也不改产品源码。移除后产品日志是 `Audio::MidiSystem: no MIDI input devices available, skipping`。

### 严格模式读不存在的值会抛异常

客体脚本以 `Set-StrictMode -Version 2.0` 运行。`(Get-ItemProperty -Name 'midi1').midi1` 在值被删除后会抛 `PropertyNotFoundException`，必须经 `PSObject.Properties` 读取并把缺失当作空值。

### 一秒一次采样会漏掉短命窗口

编辑类用例的测试体常常不到一秒，窗口只存在几百毫秒。观察器默认 150 毫秒采样并按新句柄触发截图，调低采样间隔会直接漏证据。

### 关闭沙盒前截图可能没同步完

截图写到映射目录，关机瞬间可能还有几张没落到宿主。`windows-summary.json` 的 `screenshotCount` 可能大于宿主实际文件数，这是正常现象。

### 用例初始化路径不一致

测试二进制按编译时的绝对路径找配置。换 preset 或换盘符后要重新生成 payload，不要复用旧的 `plan.json`。

### 整套运行里个别时序用例可能失败

沙盒里的窗口尺寸与帧时序和开发机不同，自动滚动、拖拽到边缘选择一类用例最容易受影响。实测整套运行时 `rhiTrackMenuPasteAndSelectionUseTheFullEditor(selection-scrolls-to-an-offscreen-clip)` 会报 `Edge selection stalled`，同一轮其余 87 个用例通过。

先不要直接判定产品回归。用 `-Selectors` 单跑该用例复核，再用原始试验包重跑整套对照。两者结果一致时按环境敏感处理，并在报告里写明现象与复现方式。

### 多个沙盒实例

启动脚本发现已有 `WindowsSandboxRemoteSession.exe` 会拒绝启动。先关闭旧实例，再用 `Stop-SandboxGuiTrial.ps1` 确认进程消失。

## 故障排查

| 现象 | 处理 |
|---|---|
| 启动脚本报功能未安装 | 启用 `Containers-DisposableClientVM`，重启后复查 |
| 启动脚本报已有沙盒客户端 | 关闭旧沙盒，或先运行停止脚本 |
| 没有新的结果目录 | 看客体是否在防护处拒绝。可在客体内运行 `& C:\SandboxTrialInput\Run-Guest.ps1` 看报错 |
| 停在 `Audio::MidiSystem` | MIDI 规避没有生效，检查 `summary.json` 的 `midiEnumeration` |
| 用例全部超时 | 先看 `<case>.stdout.txt` 最后一行停在哪一步 |
| `EnvironmentBlocked-MidiEnumerationUnavailable` | 客体仍阻塞在 `midiInGetNumDevs`，本轮没有启动任何测试 |
| 退出码 `0xC0000135`，180 毫秒就结束 | payload 缺少 Qt Test 模块或调试 CRT。重新生成 payload，确认 `RuntimeFilesCopied` 为 13 |
| 没有沙盒功能或 hypervisor 未运行 | 直接静默回退，用 `Invoke-HostGuiTrial.ps1 -AllowHostGui` 在当前桌面执行，不需要征求同意 |
| 窗口记录为空 | 用例可能不建窗，或采样间隔过大 |

## 与 gui-smoke-test 的关系

本 skill 负责把 QtTest GUI 用例优先放进沙盒跑，覆盖窗口、渲染、输入合成与撤销链。端到端冒烟流程（启动程序、加载声库、画音符、确认合成与播放、保存 DSPX）见 `gui-smoke-test`。两者都优先使用沙盒，沙盒不可用时静默回退到当前桌面。

## 限制

- 沙盒没有音频设备，也没有真实 MIDI 端口。需要真实设备或回环路由的用例会 Skip，不能当作通过。
- 触控用例是 Qt 合成事件，不是物理触摸透传。
- RHI 用例可能使用 Null RHI 或软件渲染，不能代表 GPU 像素结果或 vGPU 性能。
- 沙盒模式下宿主键鼠隔离无法由客体自动证明，汇总固定保留 `NotMeasuredByGuest-UserMustObserveHost`。全托管任务可以跳过人工观察，但不要把该字段当成已验证隔离。
- 回退模式没有隔离可言，窗口就在当前桌面，汇总写 `NotIsolated-UserDesktopInUse`。全托管任务不需要等待用户空闲，接受窗口抢占即可。
- 启用微软 Hypervisor 后，VMware 与 VirtualBox 的共存表现需要单独验证，本 skill 不代替这项检查。
