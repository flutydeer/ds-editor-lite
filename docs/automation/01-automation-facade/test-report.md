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
- 修复前日志：
  `C:/Users/yqzhishen/AppData/Local/Temp/ds-editor-lite-gui-smoke-RwzOg2/stdout.log`。
- 修复后日志：
  `C:/Users/yqzhishen/AppData/Local/Temp/ds-editor-lite-gui-smoke-wdSlY6/stdout.log`。

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

- 临时工作树：`D:/OpenVPI/ds-editor-lite-gui-regression`。
- 隔离运行目录：
  `C:/Users/yqzhishen/AppData/Local/Temp/ds-editor-lite-gui-regression-jJ7Zjz`。
- 进程日志：`C:/Users/yqzhishen/.fastctx/jobs/j-dlgjrr/output.log`。
- 关键时间：04:11:53.141 锁存 Control；04:12:03.134 因 timeout 清除；鼠标动作发生
  于 04:12:06 之后。

### 判定

失败（测试基础设施时序不足，非产品功能失败）。隔离桥锁存时限需适配 Computer Use 的
观察—动作往返，再以新轮次复测；本轮记录永久保留。
