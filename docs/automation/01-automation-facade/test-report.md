# 一期 Automation Facade 全量测试报告

## 报告维护规则

本报告采用只追加（append-only）方式维护。已经记录的测试轮次、失败、环境限制和证据
不得删除或改写；修复和复测使用新的轮次追加。若早期记录需要订正，只能追加勘误说明，
保留原始记录。

## 执行授权与基线

- 执行日期：2026-08-23（Asia/Shanghai）。
- 正式分支：`codex/automation-facade-phase-1`。
- 基线提交：`72dc5e86ca7ce526bbc311cb62de68d66837e88f`。
- 用户已完成人工 GUI 冒烟并确认通过。
- 用户已批准 `test-outline.md`，并授权执行全量 Computer Use 回归。
- 用户已授权在隔离临时分支和构建目录中加入测试专用 Modifier 输入桥；该桥不得进入
  一期正式交付代码，桥辅助场景须在本报告中明确标识。
- Catalog 分母：122 个 operation。
- 正式工作树在执行开始时干净。
- Submodule：`scripts/vcpkg@1fdb4bc2`、`src/3rdparty/qtmediate@e0f82a92`。

## 回归轮次 00：进入全量阶段前门禁

### 范围

记录用户批准前完成的推理竞态修复门禁，作为本轮全量回归的起点。

### 结果

- Debug 全目标构建：通过。
- CTest：38/38 通过，0 失败；总耗时 2.23 秒。
- 指定多轨工程定向 GUI 复测：通过。
- 真实 cache miss 音符推理中撤销：通过；时长、音高、唱法、声学四阶段均启动，随后
  目标任务被取消、pipeline 被销毁，未残留红色失败分段。
- 修复后日志中 `document-revision-mismatch` 为 0；目标级快照有效的 revision 重基为
  324 次；测试进程生命周期内 pipeline 创建/销毁均为 43 次。

### 证据

- 修复提交：`72dc5e86 fix(inference): allow validated sibling writebacks`。
- 修复前日志：证据 `E-R00-BEFORE`。
- 修复后日志：证据 `E-R00-AFTER`。

### 判定

通过。允许进入已批准的全量 Computer Use 回归。

## 回归轮次 01：Modifier 输入桥首次资格验证

### 范围

在隔离工作树、隔离 AppData/LocalAppData 和独立进程中验证测试专用 Modifier 输入桥。
测试步骤为 F9 锁存 Control，刷新窗口状态后，以鼠标左键点击当前已选片段，预期点击事件
携带 Control 且锁存在手势结束时清除。

### 结果

- 测试桥 Debug 全目标构建：599/599 通过。
- 隔离实例启动：通过；DirectML、G2P 与应用初始化均成功，未出现阻塞弹窗。
- Control 锁存：按键被桥接收，日志记录 `armed "Control"`。
- Modifier+鼠标注入：未执行到有效锁存窗口内。锁存在鼠标动作前因 10 秒超时被清除，
  因此本轮不能证明鼠标事件携带 Control。
- 正式产品代码与用户配置未被修改；测试实例使用独立配置和缓存目录。

### 证据

- 临时工作树与正式工作树隔离，具体本机路径不记入报告。
- 构建日志：证据 `E-R01-BUILD`；进程日志：证据 `E-R01-RUNTIME`。
- 配置、缓存和运行目录均为本轮独立目录，具体本机路径不记入报告。
- 关键时间：04:11:53.141 锁存 Control；04:12:03.134 因 timeout 清除；鼠标动作发生
  于 04:12:06 之后。

### 判定

失败（测试基础设施时序不足，非产品功能失败）。隔离桥锁存时限需适配 Computer Use 的
观察—动作往返，再以新轮次复测；本轮记录永久保留。

## 回归轮次 02：Modifier 输入桥修正后资格验证

### 范围

将隔离桥锁存时限从 10 秒调整为 60 秒后，重新验证 Control、Shift、Alt、组合修饰键、
鼠标点击、滚轮以及显式清除。所有操作均作用于独立测试实例。

### 结果

- 增量 Debug 构建：通过；桥实现重新编译并成功链接 `DsEditorLite.exe`。
- Control+左键：通过；转发事件为 `ControlModifier`，手势结束后自动清除。
- Shift+左键：通过；转发事件为 `ShiftModifier`，手势结束后自动清除。
- Alt+滚轮：通过；轨道高度发生可见变化，350 ms 后以 `wheel-complete` 自动清除。
- Control+Shift+左键：通过；转发事件同时包含 `ShiftModifier|ControlModifier`。
- F12 显式清除：通过；日志记录 `explicit-clear`，未遗留锁存状态。
- 窗口与进程检查：无 Qt 插件错误、断言、Debug Error 或其他阻塞模态窗口。
- 临时桥仅提交到 `codex/automation-facade-gui-regression`，正式产品分支不包含桥代码。

### 证据

- 临时桥提交：`8639056e test(gui): add isolated modifier input bridge`。
- 进程日志：证据 `E-R02-RUNTIME`。
- 配置、缓存和运行目录均为本轮独立目录，具体本机路径不记入报告。
- 关键日志：04:14:23 Control 鼠标手势；04:14:56 Shift 鼠标手势；04:15:23 Alt
  滚轮完成；04:16:15 Control+Shift 鼠标手势；04:16:49 F12 显式清除。

### 判定

通过。该桥具备补足 Computer Use Modifier+鼠标/滚轮能力缺口的资格，可用于后续回归；
桥辅助场景将在对应轮次中明确标识。

## 隐私脱敏说明 01

用户在回归轮次 02 后要求测试报告不得泄漏本机路径。为满足该要求，既有轮次中的七处
绝对路径被原位替换为匿名证据编号或隔离性说明；测试轮次、步骤、时间、结果、失败和
判定均未删除或改写。该变更是只追加规则的唯一隐私例外。此后报告只记录证据编号、
SHA-256、提交号和不含本机路径的统计；证据映射见 `evidence-index.md`。
