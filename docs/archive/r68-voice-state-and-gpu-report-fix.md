# R68 缺陷修复方案：清声后派生数据残留与 MCP 面 GPU 漏报

- 状态：**已实施并验证**；补验见 §5
- 来源轮次：R68 测试轮（本地一致性台账的 P-110 / P-104）
- 仓库范围：**仅 ds-editor-lite**（两处缺陷均已定位在 lite 侧；按 `lite-conventions` 无需改 synthrt）
- 本方案不推送到远程，不修改 GUI，不修改 synthrt、wolf 与 otter。

## 1. 需人工确认的关键点

| 编号 | 决策 | 推荐 | 备选 |
| --- | --- | --- | --- |
| **D1** | 清声后音符的**发音/音素**应变成什么 | **恢复"原词 + 空音素"**（= 新插入的无声符音符状态，与两个语言任务既有的"无歌者回退"语义一致） | (a) 保留原派生值，只改 UI 展示；(b) 保留发音，仅清除音素 |
| **D2** | 缺陷①的落地位置 | **收窄 `canStartClipInference` 对两个语言任务的拦截**（无歌者时仍执行，从而进入既有回退分支），声学管线守卫不变 | 在清声动作中显式清除派生数据（增加一处写入路径） |
| **D3** | 缺陷②的 GPU 候选数据源 | **进程内惰性枚举 + 缓存**（`DmlGpuUtils::getGpuList()` / `CudaGpuUtils::getGpuList()`），两个 MCP 面读取同一份结果 | (a) 只复用引擎初始化时的那次枚举（CPU 配置下仍为空）；(b) 两个面各自枚举（两处实现） |
| **D4** | `gpus[]` 是否带 provider 归属 | 本期**不加字段**（schema 仍为 `{index,id,name}`），仅在文档中说明"候选与当前 provider 相关" | 加 `provider` 字段（改 wire schema，影响面更大） |
| **D5** | 候选列表的 provider 语义（D3 的连带决策，**实施前必须确定**） | **与 GUI/引擎一致的 provider 专用列表**（CUDA ⇒ `CudaGpuUtils::getGpuList()`，DirectML ⇒ `DmlGpuUtils::getGpuList()`，CPU ⇒ 空）。理由：id 空间因 provider 而异（CUDA 为 `GPU-…` UUID，DirectML 为 PCI 串），只有同源的 id 才能同时通过校验与引擎解析 | 合并 CUDA∪DML（CPU 配置下也能列出显卡，但 id 空间混用，校验需要区分 provider） |

### 1.1 已确认（R68 实施轮，用户逐条选定）

| 编号 | 结论 |
| --- | --- |
| D1 | **A**：清声后发音/音素恢复为「原词 + 空音素」 |
| D2 | **A**：收窄守卫（实施时进一步收紧，见 §1.2 与 §3.1） |
| D3 | **A**：进程内惰性枚举 + 缓存，两个 MCP 面同源 |
| D4 | 采纳推荐：不加 `provider` 字段 |
| D5 | **A**：候选列表按当前 provider 专用（与 GUI 下拉框、InferEngine 同源） |
| 构建 | 用户已授权；已按 `cmake-build-local` 在 `cmake-build-release-cuda` 中构建通过（exit 0），测试门禁 76/76 |

### 1.2 已实施的改动

| 文件 | 改动 |
| --- | --- |
| `src/app/Modules/Inference/Utils/GpuCatalog.{h,cpp}` | 新增：按 provider 惰性枚举 + 进程内缓存（`CudaGpuUtils`/`DmlGpuUtils`）；CPU 或未知 provider 返回空；探测在锁外执行，以免阻塞其他线程 |
| `src/app/Automation/AppOptionsAutomationAdapter.cpp` | `gpuCandidates` 改为真实枚举（原实现以 `selectedGpuId` 回填） |
| `src/app/Automation/SettingsAutomationFacade.cpp` | **仅添加注释，行为不变**：该处是 `fallbackPublicSnapshot`（服务不可用路径），不执行 GPU 探测 |
| `src/app/Automation/Public/PublicAutomationHostAdapter.cpp` | `inference.get_capabilities.devices` 改为真实枚举 |
| `src/app/Modules/Inference/InferController_p.h`、`InferController.cpp` | 两个语言任务增加 `allowUnvoicedFallback`（默认 `false`）；新增 `restartLanguageTasksAfterVoiceCleared`；`handleVoiceContextChanged` 在歌者变为空时改为调用该函数 |

**D2 的实施比方案更严格**：没有放宽共用守卫 `canStartClipInference`，而是为两个任务增加默认关闭的强制回退参数，只在清声路径传入 `true`。原因是放宽共用守卫会使**任何**无歌者音符的普通编辑路径也执行语言任务，从而覆盖用户在无声库音符上手动填写的发音与音素（数据丢失）。

## 2. 现象与证据（均可复现）

### 缺陷① 清声后派生数据残留

- 现象：`tracks.clear_voice` 之后 `clips.get.voice_context.available=false`，但音符仍保留清声前的 `发音=w er l d / 音素=[w er l d]`。
- 证据：台账 P-110 + `instance\r-main11.txt` + `instance\stdout-main-r68-voice.log`。
- **事实**：清声动作是 SpeakerMix 空选择提交（`src/app/Automation/ParameterAutomationFacade.cpp:1789-1838` 的 `clearTrackVoice` / `clearClipVoice` → `selectTrackSingleSpeaker({},{},track)` / `selectClipSingleSpeaker({},{},clip)`）。
- **事实**：歌者变化时会发 `InvalidateClip` 并调 `ensureClipInferenceStarted`（`src/app/Modules/Inference/InferController.cpp:728-748`）。
- **推断（强）**：任务被守卫拦截，因此派生数据从未更新。守卫链：`ensureClipInferenceStarted`（`:840`）与 `createAndRunGetPronTask` / `createAndRunGetPhoneTask`（`:1143` / `:1165`）都以 `canStartClipInference` 开头，而它要求 `!clip.singerInfo().isEmpty() && !clip.singerIdentifier().isEmpty()`（`:826-828`），因此无歌者时直接返回。
- **事实（支持该推断）**：两个语言任务本身带正确的"无歌者回退"语义——`GetPronunciationTask::getPronunciationResults` 预填 `pronunciation = 原词`（`Tasks/GetPronunciationTask.cpp:108-130`）；`GetPhonemeNameTask::getPhonemeNames` 在 `resolutionState != Resolved` 时返回**等长空结果**（`Tasks/GetPhonemeNameTask.cpp:92-97`）。若任务已执行，发音应已变为原词，而实测仍为 `w er l d`，因此任务未执行。

### 缺陷② MCP 面 GPU 漏报

- 现象：`settings.query{domains:["compute_device"]}.gpus = []`、`inference.get_capabilities.devices = []`，而引擎日志已枚举并选定显卡（台账 P-104 / P-108）。
- **事实（根因）**：这两处都不是枚举，而是"**用当前已选 GPU 回填候选**"：
  - `src/app/Automation/SettingsAutomationFacade.cpp:488-495`（`gpuCandidates`，仅 `!selectedGpuId.isEmpty()` 时 append 一项）
  - `src/app/Automation/AppOptionsAutomationAdapter.cpp:781-793`（同构第二处）
  - `src/app/Automation/Public/PublicAutomationHostAdapter.cpp:2065-2072`（`inference.get_capabilities.devices` 同样只回填 selected）
  因此未选择显卡（`gpu_id:null, gpu_index:-1`）时列表必然为空，与硬件能力无关。
- **事实（可用数据源）**：真实枚举已存在——`src/app/Modules/Inference/Utils/DmlGpuUtils.cpp:56` `getGpuList()`、`CudaGpuUtils.cpp:120` `getGpuList()`（含显存下限过滤）、`InferEngine.cpp:139-146` 初始化时按 provider 选用其一。GUI 另有自己的异步检测（`UI/Dialogs/Options/Pages/InferencePage.cpp:76,125,139` 的 `QFutureWatcher<QList<GpuInfo>>`），无头模式下不运行，不适合作为数据源。
- **约束（来自 `docs/design/async-project-loading-design.md:41`）**：包元数据扫描**不得**耦合 GPU 选择/驱动初始化，因此修复**不得**将新枚举接入启动或包扫描路径。
- **事实（耦合，决定实施细节）**：`gpuCandidates` 同时参与 `settings.compute_device.update` 的校验——`SettingsAutomationFacade.cpp:1019-1036` 在 `provider != CPU && !gpuCandidates.isEmpty() && !selectedGpuId.isEmpty()` 时要求所选 `gpu_id` 命中候选（`available` + id + index）。当前候选只含"已选项"，因此该校验**恒为真（死代码）**；填入真实枚举后该校验**将被激活**，因此候选 id/index 必须与 `InferEngine` 解析所选显卡时使用的函数同源（CUDA 走 `getGpuList()`/UUID，DirectML 走 `getGpuList()`/PCI 串，见 `InferEngine.cpp:139-146,183-193`），否则会拒绝合法设置（回归风险）。实施后必须对两个 provider 各实测一次 `settings.compute_device.update`。
- **事实（额外汇总点）**：枚举**已在启动时发生过一次并被丢弃**——`src/app/Bootstrap/LoggingBootstrap.cpp:14` 调 `DmlGpuUtils::getGpuList()`；`InferEngine.cpp:139-146` 与 `UI/Dialogs/Options/Pages/InferencePage.cpp:120-123` 各有一份"按 provider 选择枚举器"的重复逻辑，因此惰性缓存可同时消除该重复。

## 3. 修复设计（最小改动）

### 缺陷①（D1/D2 确认后实施）

1. 把"歌者为空"从 `canStartClipInference` 的**粗守卫**中分出来：为两个语言任务引入"可执行但走回退"的入口——即 `ensureClipInferenceStarted` 在无歌者时仍调度 `GetPronunciationTask` / `GetPhonemeNameTask`（这两个任务已能产出"原词 + 空音素"的结果）。
2. 声学管线（`createPipeline` / 时长·音高·方差·声学任务）**保持**原守卫不变。
3. 结果需经既有 apply 通道写回（History 一条、可撤销）；实现时须核对 `InferenceApplyGate` / `m_pendingPhonemeNameApplies` 对 `success=false` 结果是否放行——**这是本项最大的实现风险**（`GetPhonemeNameTask` 在回退时会把 `m_success` 置 false，`:86/:95`）。若不放行，则改为在清声路径上直接提交一次"重置派生数据"的文档变更（D2 备选）。
4. 撤销清声后应能恢复派生值：歌者恢复后再次执行 `InvalidateClip` 并重新运行任务（现有路径，`InferController.cpp:736-747`）。

### 缺陷②（D3/D4 确认后实施）

1. 增加一处**进程内惰性缓存**的 GPU 枚举（首次由自动化面查询时执行一次，后续复用），供 `AppOptionsAutomationAdapter`、`PublicAutomationHostAdapter` 两处消费（`SettingsAutomationFacade` 的 fallback 路径按 §1.2 不做探测）；`available` 按条目真实可用性填写。
2. `gpuCandidates` / `devices` 不再由 `selectedGpuId` 回填；`configured`/`effective` 语义保持不变。
3. 不新增启动期工作；不动 GUI 页（其异步检测保留，作为后续可选统一项）。

## 4. 验证方案（无头 + MCP，含对照臂）

- 复现脚本：扩展现有的 lab 脚本 `r-main11.ps1`（缺陷①）与 `r-main*.ps1`（缺陷②），沿用**前置断言**（`clips.get.voice_context.available` / `settings.query` 读数）与 `lease.ps1` 租约、`instance\` 日志留档。
- 缺陷① 判定：清声前后逐音符打印 `发音/音素/偏移`；**对照臂**＝同一工程里"新插入的无声符音符"（应为原词 + 空音素），两者须一致；再验撤销清声后派生值恢复。
- 缺陷② 判定：无头下 `gpus` / `devices` 非空且与引擎日志枚举一致；CUDA 与 DirectML 两种 provider 配置各测一次；同时确认 `settings.query` 首次调用的额外耗时（缓存边界）。
- 单测：`src/tests/TestPublicAutomationRegistry`（已含 `gpuCandidates` 用例，`:2041-2053`）需同步更新为新语义；缺陷① 视改动面补 `src/tests/` 覆盖。
- 门禁：改动后运行既有测试目标；行为类结论必须同时报告内置门禁与真实无头实例两侧的结果。
- 台账：结果按 R 轮追加到 `TEST-RECORD.md`（P-113…），作废项入 §5。

## 5. 验证结果（R68 实施轮 + 补验，无头 + MCP）

| 判定项 | 结果 | 证据 |
| --- | --- | --- |
| 内置门禁 | ✅ `100% tests passed, 0 tests failed out of 76`（另 5 个 voicebank/synthrt 用例本就 Skipped，与改动无关） | `.tmp\r68-ctest2.log` |
| 缺陷② `settings.query{compute_device}.gpus` | ✅ 真实枚举：`{"id":"GPU-52180708-446f-0603-f1a8-10800d707294","index":0,"name":"NVIDIA CMP 40HX"}` | `instance\r-fix1.txt`、`r-fix2.txt` |
| 缺陷② `inference.get_capabilities` | ✅ 真实枚举（**载荷位于 `capabilities` 下**，而非顶层 `devices`）：`capabilities.devices[0] = {id: GPU-…, display_name: NVIDIA CMP 40HX, available: true}` | `instance\r-fix2.txt` |
| 缺陷① 清轨声后 | ✅ `发音=world(original)`、`音素=[]`，与对照臂（从未设过声库的音符）**同态** | `instance\r-fix1.txt` |
| 缺陷① 清片段声后 | ✅ 同上（`available=false, inherits=false`） | `instance\r-fix1.txt` |
| 缺陷① 恢复路径 | ✅ 重设轨声后 `available=true`、`发音=w er l d`、音素回来（清声→恢复往返成立） | `instance\r-fix2.txt` |
| 缺陷① 落盘通道 | ✅ 日志证实执行了回退分支且结果被放行：`SingerInfo not resolved, skip pronunciation fetch` + `InferenceApplyGate decision: "Apply"`（方案中最大的风险，即结果被 Defer/Drop，未出现） | `instance\stdout-fix1.log` |
| ② `gpu_id` 校验被激活（实测） | ✅ 真实 id+index+`validate_only` 通过；伪造 id 被拒 `code=invalid_argument, field_path=gpu_id, "Inference GPU is unavailable"`；`provider=CPU` 不做该项校验；全程 `validate_only`，appConfig 哈希前后一致 | `instance\r-fix3.txt` |
| ② **DirectML 分支（补验）** | ✅ 将 `configured` 切换为 DirectML（`restart_required_fields=[execution_provider]`、`effective` 仍 CUDA）后：`gpus={"id":"1F0B10DE","index":1,"name":"NVIDIA CMP 40HX (RainCandy Technology)"}`（PCI 形态，与 `DmlGpuUtils` 同源）；真实 DML id 校验通过、伪造 `0000FFFF` 被拒 | `instance\r-fix4.txt` |
| 缓存边界（补验） | ✅ 同实例内 `settings.query` 第 1 次 134ms、第 2/3 次各 34ms，即首次枚举约 **100ms**，其后命中进程内缓存；三次结果一致 | `instance\r-fix4.txt` |
| 测试限制 | ⚠️ 均在**本机 NVIDIA CMP 40HX** 上实测（CUDA 与 DirectML 两条枚举路径都已覆盖）；未在多卡/无卡机器上验证候选排序与 index 稳定性；`provider=CPU` 时按设计返回空候选 | — |

## 5.1 R69 补验：clip 语言告警、`field_path` 修正、三档转换探针

| 判定项 | 结果 | 证据 |
| --- | --- | --- |
| F6 `field_path` 与 wire 属性一致 | ✅ 纯空白 `language_id` 被拒时 `field_path=language_id`（`clips` 与 `tracks` 两个工具均实测）；改前为 `language`（代码事实，未实测） | `instance\r-fix5.txt`、`instance\r-fix5b.txt` |
| F2 声明过的语言 | ✅ `clips.set_default_language{eng}` ⇒ `ok=true, changed=true, warnings=[]` | `instance\r-fix5.txt` |
| F2 未声明的语言 | ✅ `{kor}` ⇒ `warnings=[The clip's default language "kor" is not one this singer declares: cmn, eng, jpn, yue]`，且**已实际写入**（`clips.get` 返回 `kor`），即告警而非拒绝 | `instance\r-fix5.txt` |
| F2 无歌者片段 | ✅ 有效歌者为空时不告警（`warnings=[]`），避免误报 | `instance\r-fix5.txt` |
| F2 `validate_only` 分支 | ⚠️ **MCP 面不可达**：该工具 schema 拒绝该属性（`invalid_argument, field_path=/validate_only, "Additional property is not allowed"`），代码层的告警只能由进程内调用触发；失败调用无副作用（`clips.get` 仍 `kor`） | `instance\r-fix5b.txt` |
| U-05 三档转换（新增探针） | ✅ `LanguageBridge::Depth` 三档在同一批词上的**形状契约**全部成立（Pronunciation 不带音素/onsets、Phonemes 不带 onsets、Onsets 一对一或为空），cmn/eng/jpn 各 2 词共 84 条断言全绿；跨档单调性一条 `NOTE` 都未触发 | `.tmp\r69-u05-full.txt`、`.tmp\r69-verify.txt` |
| U-05 夹具限制 | ⚠️ `TestVoicebankAudit` 把期望硬编码为某个特定转换产物（`main.cpp:374-375`：5 speakers / 3 languages），本机现有夹具（一份 2.3 转换产物）为 4 角色 / 4 linguist 语言，因此该程序的**既有**审计断言有 15 条 FAIL，**与三档探针无关**；该用例在 R68 门禁中为 Skipped，因此本次属于首次运行而非回归 | `.tmp\r68-ctest2.log` vs `.tmp\r69-ctest.log` |
| 门禁回归 | ✅ `TestAutomationEditingDomains` 曾因断言旧字段名而在 F6 修改后失败，同步期望后 **Passed**；随后撤掉 `LITE_AUDIT_*` 两个缓存变量并重建，门禁回到 `100% tests passed, 0 tests failed out of 76`（`TestVoicebankAudit` 复归 Skipped） | `.tmp\r69-gate2.txt`、`.tmp\r69-revert.txt` |

## 6. 范围之外的事项

- 不修改 synthrt、wolf 与 otter；不推送到远程；不使用 `--force`；不修改 GUI 页与启动路径；不修改 `gpus[]` schema（D4 采用推荐项时）。
- 不附带修复 R68 台账中的其他观察项（语言 id 在 clip/track/note 写面不校验、`-` sustain 约定未证实、`tracks.clear_voice` 之外的静默面），这些项另行分批处理。**其中"`clips` 语言静默无告警"已在 R69 单独修并验证（见 §5.1）**。

## 7. 未核实项（实施前须逐条落实）

1. ~~`InferenceApplyGate` 是否放行 `success=false` 的回退结果~~ → **已核实（支持 D2 主选）**：两个语言任务的 apply 分支显式 `checkSingerSpeaker = false`（`InferController.cpp:890`/`:939`），无歌者时结果也可写入；但 `:905` / `:995` 只在"有歌者"时才链式调用音素任务，因此无歌者路径须**调用两个任务**，否则音素不会被清除。
2. ~~引擎初始化那次枚举结果是否已被某处保留~~ → **已核实**：未被保留（`InferEngine.cpp:139` 的 `gpuDeviceList` 是局部变量）；启动时另有一次被丢弃的枚举（`LoggingBootstrap.cpp:14`），因此新建惰性缓存，不增加启动期开销。
3. `docs/design` 中是否存在关于"派生数据在声库消失后的语义"的既定契约（本轮 grep `清声|未分配|无歌者|残留` 仅命中 `async-project-loading-design.md:8` 的无关现象描述）。
