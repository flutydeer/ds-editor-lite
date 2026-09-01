# Automation Facade 全域 GUI 回归执行矩阵

## 1. 文档状态与口径

本文是一期 Automation Facade 全量 Computer Use 回归的稳定执行清单，不是测试结果或排期记录。
正式结果写入 `test-report.md`；矩阵只定义可重复引用的场景组，原始尝试、失败和修复证据保存在
仓库外私有归档。

矩阵遵循 `test-outline.md` 和 `migration-matrix.md` 的一期边界：真实 GUI 负责验证用户
入口、交互提交、可见状态与文件结果；内部 DTO、DocumentId、revision、History entry
数量、TaskId 和幂等状态仍由确定性测试或脱敏日志补证，不得用内部断言替代 GUI 可见
结果，也不得把一次 GUI 动作拆成多个“场景”夸大覆盖率。

结果只允许记为以下四类：

- **通过**：规定的可见断言、补充证据和清理全部成立；
- **失败**：产品行为与断言不符，保留本轮目录和日志；
- **阻塞**：测试基础设施、测试桥或隔离门禁不成立，不计入产品通过率；
- **环境未具备（未装备）**：某个条件子场景缺少已资格的声库、模型、codec、设备、外部工具
  或可控异步窗口，不得记为通过，也不得扩大为整个场景组的 blanket skip。

本文以 `GUI-G00～GUI-G24` 标识稳定的 GUI 场景组；它们不是 `test-report.md` 的执行轮次
编号。每个实际尝试（包括资格检查、失败、阻塞、未装备和修复后重跑）都在正式报告中分配
下一个全局 `Rxx`，并记录其对应的一个场景组或一个明确子场景；不得用场景组编号覆盖、复用
或合并历史轮次。

遇到崩溃、数据损坏、打开/保存等基础路径失败，或发现测试进程访问非隔离配置、缓存，
立即停止当前域；不要继续用后续动作覆盖现场。

## 2. 统一别名、fixture 与环境资格

本文只使用逻辑别名，不记录任何本机绝对路径或用户数据路径：

| 别名 | 用途 |
|---|---|
| `RUN_ROOT` | 名称以 `ds-editor-lite-gui-regression-` 开头的唯一系统临时目录 |
| `FIXTURES` | `RUN_ROOT` 下的只读测试输入副本 |
| `WORK` | 每轮独立的可写工程副本与中间文件 |
| `OUTPUT` | 保存、MIDI 导出等产物目录 |
| `TEST_CONFIG` | Qt test mode 解析出的隔离配置位置；实际位置只写入脱敏证据 |
| `TEST_CACHE` | Qt test mode 解析出的隔离推理缓存位置；启动时必须为空 |
| `VOICE_MULTI` | 经授权用于测试、含至少两个 speaker 的真实声库 |
| `VOICE_SINGLE` | 可选的单 speaker 声库，用于“不支持动态混合”分支 |
| `LIBRESVIP_CLI` | 已资格的 `libresvip-cli` 可执行文件；证据只记录版本与脱敏身份 |
| `AUDIO_TEST_ENDPOINT` | 不承载用户正在使用的音频、可安全发声和改参的测试输出端点 |
| `HOTPLUG_TEST_ENDPOINT` | 可受控添加/移除且不影响用户设备的辅助或虚拟音频端点 |

执行前准备以下受控 fixture；名称可以带本轮短 ID，但内容必须固定并记录 SHA-256：

| Fixture | 最小内容 |
|---|---|
| `base.dspx` | 两条歌声轨和一条音频轨；多个唱段、非零起点音符、歌词/语言/音素、参数点、两个 tempo、两个拍号和已保存 loop |
| `import-project.dspx` | 至少两轨；tempo、拍号和 loop 与 `base.dspx` 明显不同，便于区分“导入 timeline”与“不得导入 loop” |
| `midi-one.mid` | 两个有音符轨、Unicode 轨名/歌词、非默认 tempo 与拍号 |
| `midi-two.mid` | 一个有音符轨，内容与 `midi-one.mid` 可见地区分 |
| `audio-short.wav` | 短、可解码、非静音音频，导入后能看到非平直波形 |
| `audio-long.wav` | 仅用于取消资格；解码时间应足以稳定显示可取消任务对话框 |
| `bad-project.dspx` | 可选择但无法解析的损坏文件 |
| `unsupported.txt` | 不受支持格式 |
| `missing-audio.dspx` | 引用已移走的音频副本，用于缺失/重定位路径 |
| `legacy-nohash-audio.dspx` | 旧格式或已受控去除音频 SHA-512 的工程；引用一个固定文件名，用于同名候选确认 |
| `same-name-candidate.wav` | 与上述旧工程引用同名、内容与哈希固定的非静音候选；只放在工程相对目录或同级目录之一 |
| `infer-multi.dspx` | 至少两条歌声轨、每轨至少两个可区分唱段；全部绑定真实声库，能从零缓存完成完整推理 |
| `libresvip-project` | `libresvip-cli` 支持且内容固定的非 MIDI/DSPX 工程，至少两轨并含可区分 timeline 数据 |
| `package-good` | 可合法扫描并包含 `VOICE_MULTI` 的包目录 |
| `package-bad` | 可重复触发扫描/验证错误、且不含任何用户数据的坏包目录 |

fixture 资格本身不是产品通过项。若某 fixture 不能稳定产生上述可见状态，先修复 fixture，
不要临场改变预期。

### 2.1 能力装备表

执行前先建立逐项装备表；只记录逻辑别名、版本和 `已装备`/`未装备`，不记录本机路径。

| 能力 | 已装备判据 | 未装备时的影响 |
|---|---|---|
| `CAP-ISOLATION` | Qt test mode 在应用初始化前生效，配置与缓存均解析到全新隔离位置 | 阻塞全部产品场景，不以未装备放行 |
| `CAP-BRIDGE` | 修饰键锁存、鼠标/滚轮转发和显式清除均有脱敏日志 | 仅 `[BRIDGE]` 子场景阻塞，普通 GUI 动作继续 |
| `CAP-INFER` | `VOICE_MULTI`、模型、codec 与 `infer-multi.dspx` 可真实运行 | 真实推理、Speaker Mix 对应子场景未装备 |
| `CAP-INFER-DELAY` | 测试构建可显式开启、释放并取消受控推理延迟，且不改变产品提交逻辑 | 仅“推理进行中撤销”子场景未装备 |
| `CAP-AUDIO` | `AUDIO_TEST_ENDPOINT` 可播放测试音且音量已置于安全水平 | 真实听音与设备切换子场景未装备，视觉推理仍执行 |
| `CAP-HOTPLUG` | `HOTPLUG_TEST_ENDPOINT` 可安全地添加/移除并恢复 | 仅 hot-plug 子场景未装备 |
| `CAP-GPU` | 页面检测到符合门槛的 GPU，所选 provider 能完成一次隔离真实推理 | GPU 子场景未装备；CPU 子场景继续 |
| `CAP-DIRECT-MANIPULATION` | 构建显示该开关，且有可受控触摸/精密触控板输入 | 仅 Direct Manipulation 手势子场景未装备 |
| `CAP-RHI` | RHI backend 可启动且窗口可由 Computer Use 重新绑定 | 仅 RHI 重复交互子场景未装备或按可归因错误判失败 |
| `CAP-LIBRESVIP` | `LIBRESVIP_CLI` 可启动、版本已记录，`libresvip-project` 可转换 | LibreSVIP Open/Import 子场景未装备 |
| `CAP-EXTRACT` | Rmvpe/Game 所需模型与 `audio-short.wav` 可分别完成 Extract Pitch/MIDI | 仅对应提取完成子场景未装备 |
| `CAP-LONG-ASYNC` | 长解析、长解码或提取 fixture 能稳定提供可取消窗口 | 只影响对应 cancel/busy 子场景 |

同一场景组内必须逐子场景判定。例如没有 GPU 只能把 GPU provider 子场景记为未装备，不能
跳过 CPU 推理、Inference 页持久化或 cache 检查；没有热插拔端点也不能跳过 driver/device/
buffer/sample-rate 的安全恢复。若某控件按已装备条件本应出现却缺失，应记产品失败或基础设施
阻塞，不得改写为未装备。

## 3. 所有轮次共用的执行协议

### 3.1 构建与进程

1. 实际执行前完整读取 `computer-use` 与 `gui-smoke-test` skill；如需重建，再读取
   `CMake Build` skill。
2. 使用经资格的隔离 GUI 回归构建：具备初始化前 test mode、Modifier 输入桥，以及仅在
   `CAP-INFER-DELAY` 子场景显式开启的可取消推理延迟。证据记录构建身份和能力开关，不在
   矩阵固化临时提交；正式产品构建不得默认启用这些测试能力。
3. Debug 构建须启用 `LITE_ENABLE_GUI_TEST_INPUT_BRIDGE`，启动时仅对测试进程设置
   `DS_EDITOR_GUI_TEST_INPUT_BRIDGE=1`。
4. 每次启动记录可执行文件身份、提交、进程 ID、窗口 ID 和启动日志。不得复用、关闭或
   操作用户已有实例；若单实例机制把请求转发给已有实例，本轮直接判为阻塞。
5. 一切 Windows GUI 操作只用 Computer Use。每次观察后只做一个有意义动作，再重新
   观察；控件编号和坐标只对当前观察有效。
6. 记录当前 Editor rendering backend。`GUI-G04～GUI-G15` 的结果只归属于实际运行的
   backend；当 `CAP-RHI` 已装备并纳入本次资格表时，以独立子运行重复关键拖动、选择、缩放和
   History 场景，不把两个 backend 的证据混为一轮。

### 3.2 测试桥规则

- F9 锁存 Control，F10 锁存 Shift，F11 锁存 Alt，F12 显式清除；组合修饰键逐个锁存。
- 锁存会自动超时；鼠标手势在 release 后以 `mouse-gesture-complete` 清除；滚轮静默后以
  `wheel-complete` 清除；应用失焦以 `application-deactivated` 清除。
- 每个带修饰键的点击或拖动前重新锁存。Control 多选中的每一次附加点击都要重新按 F9。
- 打开原生文件对话框会使应用失焦，因此绝不能在打开对话框前锁存；返回编辑器后再锁存。
- 每轮开始和任何可疑中断后按 F12，并在日志中确认没有遗留修饰键。
- Speaker Mix 的 Alt 比例拖动依赖桥分支把事件修饰键传入编辑器；正式分支上的同一动作
  不作为有效回归证据。

### 3.3 证据与清理

- 每个原子场景至少保留“动作前、提交后、恢复后”三个观察点；文件类场景再独立记录
  存在性、字节数和 SHA-256。
- 屏幕证据只截应用与测试对话框；日志先脱敏，不记录路径、账户名或用户数据。
- 每轮从 `base.dspx` 的独立副本或明确的新工程开始。轮末停止播放、关闭模态框、按 F12、
  恢复本轮改变的设置，并保存或放弃专用工程。
- 只有整轮通过才允许删除该轮中间产物；整个任务通过后，重新规范化并验证 `RUN_ROOT`
  位于系统临时目录且名称前缀正确，再只删除这一目录。任一失败时保留现场。
- **本矩阵硬边界**：不得打开“导出音频”对话框，不执行音频预演、WAV/FLAC/MP3 导出或其文件
  校验。

## 4. 稳定场景组执行矩阵

### GUI-G00：隔离、桥与空缓存门禁

- **追踪**：测试基础设施；后续所有 `[BRIDGE]` 场景的共同前置。
- **前置**：`CAP-ISOLATION` 与 `CAP-BRIDGE` 已装备；GUI 回归构建完成；`RUN_ROOT`、
  `TEST_CONFIG`、`TEST_CACHE` 均新建且为空；未启动任何测试实例。
- **动作**：启动独立实例并记录 PID/WindowId；观察启动日志；打开设置的 Inference 页并
  刷新 Cache Size，随后关闭设置；在不选择声库的空工程中创建三个短音符，依次资格验证
  F9+点击、F10+点击、F11+轨道区滚轮、F9+F10+点击和 F12 清除，每个动作后重新观察。
- **可观察断言**：窗口是新建的测试实例；日志包含 `GUI_TEST_INPUT_BRIDGE enabled`；Cache
  Size 显示无缓存；桥日志分别出现 armed、forwarded 和正确清除原因；Control/Shift 点击、
  Alt 滚轮产生预定的选择/范围/轨高效果；没有插件错误、断言或意外模态框。
- **恢复/清理**：F12；新建空工程并放弃本轮临时音符；保留实例进入 `GUI-G01`。门禁失败时关闭
  测试进程但保留 `RUN_ROOT` 和日志，停止全部产品回归。
- **未能自动观察的限制**：Qt test mode 是否在 `AppEnvironment::preInit()` 前生效不能只
  靠窗口判断，必须用脱敏后的配置/缓存解析日志和文件系统空目录双重证明。

### GUI-G01：fixture 基线与可重复恢复

- **追踪**：`documents.get`、`project.get`、`timeline.get`、`packages.list` 的 GUI 基线。
- **前置**：`GUI-G00` 通过；`FIXTURES` 的哈希已记录；包扫描达到 Ready；真实声库若未装备，
  只标记依赖声库的断言，不影响工程结构基线。
- **动作**：用“打开”载入 `base.dspx`；等待所有资源检查与后台加载结束；逐一观察轨道名、
  唱段、音频波形、音符/歌词、tempo、拍号、loop、歌手选择器和窗口标题；关闭后从新的
  `WORK` 副本再打开一次。
- **可观察断言**：两次打开呈现相同轨道顺序和核心内容；首个可用唱段被激活且 Clip
  Editor 可见；音频资源状态稳定；没有意外未保存标记；第二次不会读取前一轮工作副本。
- **恢复/清理**：保留一份未修改工作副本作为后续各轮起点；关闭其他副本，不删除原始
  fixture。
- **未能自动观察的限制**：DocumentId、revision 0、DTO 完整性和包解析出的运行期 ID
  不在 GUI 显示，需由实现级测试或脱敏诊断补证。

### GUI-G02：新建、保存、另存为、打开与 Recent 单项删除

- **追踪**：`documents.commit_new`、`documents.commit_open`、`documents.save`、
  `recent_files.add/list/remove/clear`。
- **前置**：`GUI-G01` 通过；新建空工程；`OUTPUT` 为空。
- **动作**：创建一条轨道和一个可见唱段使标题出现未保存标记；另存为 `doc-a.dspx`；再做
  一次编辑并另存为 `doc-b.dspx`；继续编辑后用普通保存；打开 `doc-a.dspx` 与
  `doc-b.dspx` 对照内容；再依次打开九个受控副本，使本轮累计产生十一条 Recent 候选。
  先从标题栏文件按钮打开文件弹窗，确认只展示最近五项；在一个非当前项上依次点击
  `⋮`（More）→ `Remove`，并对当前项确认 `Remove` 禁用。再从 `File → Recent Projects`
  验证最多十项及顺序；只在 `RUN_ROOT` 内移走一个非当前副本并点击该 Recent 项；最后走
  `File → Recent Projects → Clear Recent Projects`（删除图标）清空列表。
- **可观察断言**：保存对话框关闭后标题显示对应文件名且未保存标记消失；两个文件均非空，
  普通保存只更新当前 `doc-b.dspx`；打开成功会整体替换可见工程；标题栏文件弹窗最近使用
  优先且最多五项，File 菜单最多十项；单项 `Remove` 只删除所选非当前条目而不删除文件，
  当前工程不能从该按钮删除；缺失文件点击后显示 File not found/Toast 且该项被移除；
  Clear Recent Projects 后两个入口都显示无 Recent。
- **恢复/清理**：把两个主要输出留到文件 round-trip 结束；删除九个受控副本；新建空工程；
  清空隔离 Recent。
- **未能自动观察的限制**：new/open 的 DocumentId 轮换、save 不轮换 ID/revision、幂等
  generation 清理和 Recent 路径规范化需要契约测试/日志，GUI 只能观察身份与内容变化。

### GUI-G03：未保存保护、失败回滚与文档忙状态

- **追踪**：文档状态机、`documents.commit_new/open`、`application.request_exit` 的保护路径。
- **前置**：打开一份工作副本并制造未保存编辑；准备 `bad-project.dspx` 和
  `unsupported.txt`。
- **动作**：请求 New，先 Cancel，再请求 New 并 Don't save；重新制造未保存编辑，请求
  Open，先 Cancel，再选择 Save 后继续打开；分别尝试打开损坏文件和不支持文件；在未保存
  状态请求 Exit 并 Cancel。若有稳定的长解析 fixture，再在进度对话框存在时触发第二个
  文档操作。
- **可观察断言**：Cancel 后原内容、路径、loop 和未保存标记不变；Don't save 后进入真正
  空工程；Save 分支先清除未保存标记再打开目标；损坏/不支持文件只显示错误，旧工程仍
  完整；Exit Cancel 保持同一窗口；busy 时第二个入口被禁用或显示“已有文档操作”。
- **恢复/清理**：保存诊断副本或明确放弃；关闭错误框；重新打开干净工作副本。
- **未能自动观察的限制**：失败前后的 Model/History/DocumentId/revision/幂等缓存完全不变
  不能由截图穷尽；若解析过快看不到 busy，只把 busy 子场景记 `CAP-LONG-ASYNC` 未装备，不能
  记通过或跳过其他文档保护路径。

### GUI-G04：轨道属性、颜色、排序、删除与调音台

- **追踪**：`tracks.insert/move/remove/set_color/set_default_language/set_properties`、
  `master.set_control`。
- **前置**：打开干净工作副本；至少保留三条名称可区分的轨道。
- **动作**：通过轨道头右键在中间插入轨道；改为含 Unicode 的名称；逐次切换 M、S 和默认
  语言；打开 Track color，先只 hover 后离开，再重新打开并点击另一颜色；仅从轨道编号
  区域拖动改变顺序；切到 Mix 页分别修改一条轨道和 Master 的 gain/pan；删除中间轨道，
  再 Undo/Redo。
- **可观察断言**：插入位置、编号与名称立即更新；M/S、语言和颜色在轨道头及调音台同步；
  hover 退出恢复原颜色，点击才持久且颜色提交可 Undo/Redo；拖动后轨道与其唱段整体换序；
  gain/pan 文本随提交更新；删除/撤销/重做保持正确对象与顺序。
- **恢复/清理**：Undo 到初始轨道状态，或放弃工作副本；Master 和轨道控制恢复基线。
- **未能自动观察的限制**：gain/pan 精确浮点值、一次提交对应一次 History/revision 和
  affected-object ID 需补充日志；仅 hover 预览明确不属于一期 Facade 提交。

### GUI-G05：歌声片段、跨轨移动、缩放与剪贴板

- **追踪**：`clips.insert/remove/set_default_language/set_properties`、selection、复制/粘贴；
  音频片段内部状态在 `GUI-G21` 补充。
- **前置**：至少两条可见轨；播放头位于非零 tick；当前没有选择或拖动预览。
- **动作**：在空白轨道位置创建歌声片段并改名、默认语言和歌手继承状态；拖片段主体做水平
  移动及跨轨移动；拖左右边缘调整长度；`[BRIDGE]` F11 后做一次不吸附移动；F9 后逐个
  追加片段到多选；执行 Copy/Paste、Cut/Paste 和 Delete；双击片段进入 Clip Editor。
- **可观察断言**：片段新位置、所属轨、起止边缘和标签可见且稳定；Alt 手势落在非网格位置，
  桥日志包含 Alt 与 gesture-complete；多选高亮正确；粘贴锚定播放头/目标轨且保留片段名、
  语言和歌手上下文；双击激活对应片段并展开 Clip Editor。
- **恢复/清理**：Undo 所有片段提交，确认剪贴板动作不再影响后续轮；F12；放弃工作副本。
- **未能自动观察的限制**：片段精确 tick、强类型 ID、复制后的运行期 ID 与参数深拷贝需要
  DTO/保存重开补证；Computer Use 不应把拖动中的临时图形当作提交结果。

### GUI-G06：音符插入、选择、移动、resize、split、量化与删除

- **追踪**：`notes.insert/move/resize_left/resize_right/split/quantize/remove`、
  `editor.set_selection/set_quantize/set_piano_roll_edit_mode`。
- **前置**：激活歌声片段；音符起点前至少留一拍；选择 Draw 工具并设定可见网格。
- **动作**：画三个互不重叠、歌词可见的音符；切 Select 工具单选；`[BRIDGE]` 用 F9 追加
  选择、F10 做范围选择、F9+F10 做附加范围选择；先把音符主体移到空白处，再把一个音符拖成
  与另一音符部分重叠，观察提交后 Undo 恢复；分别拖左右边缘；F11 后做一次非吸附移动/
  resize；用 Split 工具拆分一个音符；按 Q 对选中音符量化起点和长度；删除选中音符并
  Undo/Redo。
- **可观察断言**：每次 release 后只出现一个稳定结果；选择集合与桥日志匹配；空白移动和
  模型允许的部分重叠都稳定保留，且不产生负 tick；左右 resize 保持另一边不动；split 产生
  两个相邻音符；量化后边缘对齐所选网格；删除/撤销/重做恢复正确音符。
- **恢复/清理**：Undo 到本轮三音符基线后保存专用副本供 `GUI-G07`；F12。
- **未能自动观察的限制**：精确 tick、允许重叠的内部索引状态、edited 参数随
  move/resize/split 的数值保真和单 History/revision 仍需契约测试及保存重开对照。

### GUI-G07：歌词、音素与 Fill Lyric 的 Lyric/Splitter/Tagger/Test

- **追踪**：`notes.set_word_properties`、`notes.set_phoneme_offsets`、
  `settings.update_fill_lyric`，以及推理产生的 pronunciation/phoneme 可见结果。
- **前置**：使用 `GUI-G06` 的三音符副本；记录 Fill Lyric 的显示、字体、Skip Slur、
  split mode、内建规则启停、规则顺序与自定义规则基线。音素子场景要求 `CAP-INFER`；规则页
  CRUD 不依赖声库，不能随音素子场景一起跳过。
- **动作 A（Lyric/音素）**：双击音符正文编辑 Unicode 歌词；双击发音条编辑 pronunciation；
  从右键菜单选择语言和一个候选发音；多选音符后执行 `Fill lyrics...`，输入可区分文本并切换
  Skip Slur、split mode、左右预览和字体设置。对单音符打开 Edit Phonemes，先触发空名称或
  无 onset 的校验，再填写合法名称并确认；拖一个可编辑音素边界，Undo/Redo 后保存重开。
- **动作 B（Splitter）**：重新打开 Fill Lyric 并进入 `Splitter` 页；选择带锁图标的内建规则，
  确认详情只读且 `-` 不可用；切换内建规则启停并拖动改变顺序。用 `+` 新建自定义规则，先以
  空名称点击 `Apply`，再以非法 RE2 表达式点击 `Apply`，最后填写唯一名称和合法捕获组表达式
  并 Apply；经 `Test >>` 跳到 Test 页确认结果，关闭并重开 Fill Lyric 确认名称、启停和顺序
  持久，再回到 Splitter 删除该自定义规则并 Apply。
- **动作 C（Tagger/Test）**：在 `Tagger` 页验证内建规则只读、启停和拖放排序；用 `+` 新建
  自定义规则，覆盖空 Language、非法 regex 的校验，再填写已注册语言，分别添加 `regex` 与
  `array` Entry，设置 Tag 和 Discard，Apply。进入 `Test` 页输入固定混合语言文本，点击
  `Run Test`，观察 Step 1 Split Result 的 token count/顺序和 Step 2 Tag Result 的
  `lang`/`tag`/`discard`；使用 `Edit Splitter >>`、`Edit Tagger >>` 往返。删除自定义 Tagger
  规则前先关闭并重开确认其 Entry、启停和顺序持久；删除并 Apply 后再次关闭、重开 Fill Lyric。
- **可观察断言**：歌词、发音、语言标记立即更新；Fill Lyric 只作用于目标范围；非法音素
  保持对话框并给出错误，合法名称/边界、Undo/Redo 与保存重开一致。内建 Splitter/Tagger
  可启停和排序但不能编辑或删除；空名称/语言和非法 RE2 均阻止 Apply 并定位到规则；合法
  自定义规则能新增、编辑、启停、排序、测试、删除，Test 两步输出随已 Apply 的规则变化；
  删除后重开不再出现，自定义与基线规则状态均按恢复结果持久化。
- **恢复/清理**：删除所有唯一测试规则并 Apply；恢复内建规则启停、顺序和 Lyric 基线设置；
  放弃或删除本轮工程副本；F12。若清理失败，保留隔离配置并判失败。
- **未能自动观察的限制**：候选发音和音素边界依赖语言/推理模块；该子场景无结果时标记
  未装备，Splitter/Tagger/Test 仍须执行。音素 offset 精确值、自动推理写回与手工 edited
  值优先级需 DTO 补证；GUI 只验证规则引擎的可见输入、校验、顺序和输出。

### GUI-G08：参数曲线、锚点、擦除、Bake 与不支持参数

- **追踪**：`parameters.get/replace`；推理原始参数与 edited 参数的边界。
- **前置**：激活有真实声库、原始推理曲线可见的歌声片段；选择该声库明确支持的非 Pitch
  参数。
- **动作**：用 Draw 单次拖动写入一段曲线；用右键拖动或 Erase 工具擦除一段；切 Anchor
  模式新增、移动、删除锚点并在可用时切换 Linear/Hermite；交换前景/背景参数；对具有
  Original 曲线的参数执行 Bake；再选择声库不支持的参数，观察提示并点击 Edit Anyway；
  每类提交做一次 Undo/Redo，最后保存重开。
- **可观察断言**：曲线仅在目标区间改变，Escape/取消中的预览不提交；锚点及插值形态可见；
  Bake 始终可选；没有 Original 曲线的区间不产生编辑或 Undo 项，有 Original 曲线的区间生成
  可编辑结果；单次拖动固定使用按下时的 Original 快照，拖动中完成的新分段从下一笔生效；
  不支持参数先显示明确提示，确认后才允许编辑；Undo/Redo 和保存重开保持曲线结果。
- **恢复/清理**：Undo 或放弃工作副本；恢复前景/背景选择；F12。
- **未能自动观察的限制**：曲线采样点、step 边界、空曲线规范化和一次 gesture 的精确
  replacement DTO 不能从像素图完整验证；Bake/Original 依赖声库与推理环境。

### GUI-G09：轨道/片段歌手继承、单 speaker 与固定 Speaker Mix

- **追踪**：`speaker_mix.track.select_single/apply/replace`、
  `speaker_mix.clip.select_single/apply/use_track`。
- **前置**：`VOICE_MULTI` 已扫描，至少一条轨和一个歌声片段；片段初始 Follow Track。
- **动作**：在轨道歌手选择器选单 speaker，并在 Speaker Mix 参数页观察不可用分支；进入
  Manage mix presets，选择至少两个来源并调整权重后 OK，形成轨道固定混合；观察跟随轨道
  的片段；在片段工具栏改为单 speaker，再改为固定混合，最后选择 Follow/Use Track。
- **可观察断言**：选择器显示真实声库/说话人或 Mix 标识而非分组、空项；单 speaker 时显示
  Dynamic mix is unavailable；固定混合至少两个来源且显示权重总和 100%；片段跟随轨道时
  同步变化，脱离后保持自己的显示，Use Track 后重新同步；每次 Undo/Redo 恢复正确层级。
- **恢复/清理**：片段恢复 Follow Track，轨道恢复基线 speaker；关闭 Speaker Mix 对话框。
- **未能自动观察的限制**：归一化后的精确权重、Singer/Speaker 运行期 ID、Track 与 Clip
  序列化字段需要 DTO/保存重开；若只有单 speaker 声库，只把多 speaker 子场景记未装备。

### GUI-G10：动态 Speaker Mix、关键帧、Alt 比例拖动与停止

- **追踪**：`speaker_mix.clip.enable_dynamic/replace/apply`、selection；`[BRIDGE]` 必选。
- **前置**：隔离桥分支；轨道已有至少双来源固定混合；片段 Follow Track；Speaker Mix 参数
  视图可见。
- **动作**：点击 Copy and Enable Dynamic Mix，确认 tick 0 首关键帧；双击空白处新增关键帧；
  水平拖关键帧改时间，垂直拖分界改权重；先做普通相邻重分配，再 F11 后做比例重分配；框选
  多个关键帧并 Delete，尝试删除 tick 0；用上一/下一关键帧按钮导航；Bypass、Cancel
  Bypass；Stop Dynamic 先取消、再确认；Undo/Redo 关键提交。
- **可观察断言**：启用后片段不再跟随轨道且首帧位于 0；新帧、位置和堆叠面积变化可见；
  普通/Alt 拖动形态不同且 Alt 日志正确；tick 0 不可删除；导航改变播放头/可见区域；Bypass
  显示 `Bypassed`，取消后消失；Stop 取消不变，确认后清空动态帧并回到固定混合。
- **恢复/清理**：停止动态混合并让片段恢复 Follow Track；F12；放弃工作副本。
- **未能自动观察的限制**：Alt 算法的精确比例与所有权重归一化只能从图形近似观察，必须
  结合 Facade/DTO 断言；正式产品分支不含事件修饰键适配，不能替代桥分支证据。

### GUI-G11：Speaker Mix 预设 CRUD 与跨控件复用

- **追踪**：`speaker_mix_presets.list/save/delete`。
- **前置**：`VOICE_MULTI` 可用；使用唯一测试预设名 `gui-regression-<run-id>`；隔离配置可写。
- **动作**：打开 Manage mix presets，修改来源与权重，Save As 唯一名称；关闭并在另一轨道/
  片段的歌手菜单应用该预设；再次 Save As 同名以覆盖；重开对话框并切换 Init Preset/
  已保存预设；More -> Delete，先 No 再 Yes；重启测试实例后确认已删除。
- **可观察断言**：保存后预设出现在下拉框和其他适用歌手菜单；应用后来源与权重一致；同名
  覆盖不产生第二个可见条目；Init Preset 恢复等权；取消删除保留，确认删除后所有入口都
  消失；删除状态跨重启保留。
- **恢复/清理**：必须删除唯一测试预设；若删除失败，保留隔离配置并将本轮记为失败。
- **未能自动观察的限制**：预设 UUID、重复 ID/缺失 ID 错误、工程序列化隔离和持久化回调
  次数不可由 GUI 直接观察，仍需实现级测试。

### GUI-G12：tempo、拍号、锚点约束与 Master

- **追踪**：`tempos.set/delete`、`time_signatures.set/delete`、`timeline.get`、
  `master.set_control`。
- **前置**：干净工程；播放头在 bar 0；打开 tempo 与 time-signature lane。
- **动作**：在标题栏分别输入非法 tempo 和非法拍号，再输入合法值；在后续 bar 的 lane
  空白处双击新增 tempo 与拍号；编辑第二个点；右键尝试删除 0 锚点，再删除第二个点；移动
  播放头跨过变更点，观察标题栏值；在 Mix 页提交 Master gain/pan；Undo/Redo 并保存重开。
- **可观察断言**：tempo 仅接受正有限数；拍号分子/分母为正且分母为 2 的幂；点按 tick/bar
  排序，播放头跨点时标题显示当前 governing 值；0 锚点不可删除，后续点可删；Master 文本
  与推子同步；Undo/Redo 和重开保持结果。
- **恢复/清理**：恢复基线 tempo、拍号与 Master；关闭 lanes 或放弃工作副本。
- **未能自动观察的限制**：重复点拒绝、排序容器、精确 tick/bar、浮点 tempo 与单次
  History/revision 需要 Facade 测试补证。

### GUI-G13：History、savepoint、focus/reveal 与分支截断

- **追踪**：`history.get_state/undo/redo`、`editor.reveal`。
- **前置**：已保存工作副本；准备依次修改轨道、远处片段、音符和 tempo，使目标分布在不同
  panel/viewport。
- **动作**：完成四个可区分提交并观察未保存标记；先对当前可见目标 Undo/Redo；随后切到
  Mix 页或把目标滚出视口，对带 focus transition 的条目按一次 Undo，观察 Toast/导航但不
  再做其他动作，再按第二次 Undo；同样验证 Redo；Undo 到保存点；再 Undo 一步后做新编辑，
  展开 Edit 菜单检查 Redo。
- **可观察断言**：普通可见目标一次快捷键即提交；需要上下文切换时第一次只 reveal 并显示
  `Press Undo/Redo again to apply`，模型未变，第二次才提交；达到保存点标题圆点消失；保存点
  之前再次 Undo 后圆点出现；新分支建立后 Redo 禁用且旧分支不再出现。
- **恢复/清理**：回到保存点或放弃工作副本；确保没有 pending focus preview。
- **未能自动观察的限制**：History ID、栈深度、revision 每次恰增一次及空栈返回语义需
  内部状态补证；截图只证明可见导航和最终模型。

### GUI-G14：Editor 选择、模式、缩放、panel、页面与 auto-page

- **追踪**：`editor.set_active_clip/set_selection/set_panel_visibility/show_bottom_panel_page/`
  `set_piano_roll_edit_mode/set_quantize/set_auto_page_turn/center_* /set_*_scale`。
- **前置**：多轨、多片段、多音符工程；Track 与 Clip Editor 均展开；F12 已清除。
- **动作**：分别点击轨道画布、钢琴卷帘和参数区，执行 Select All，确认命令目标随焦点变化；
  `[BRIDGE]` 验证 Ctrl 横向缩放、Shift 横向滚动、Alt 轨高缩放；切换 V/B/N/M/F/G/H/J
  中环境支持的模式和 quantize；折叠 Track、恢复，再折叠 Bottom、恢复，不能同时折叠两者；
  切换 Clip/Mix 页；双击另一唱段激活；分别切换 Track 与 Piano Roll auto-page，并在播放头
  穿越视口时观察。
- **可观察断言**：Select All 只选择当前交互目标；三类 wheel 效果与桥日志一致；工具按钮、
  quantize 和页面状态可见；任意时刻至少一个主 panel 可见；切页与激活片段更新标题/内容；
  两个 auto-page 状态彼此独立，启用目标随播放头翻页，禁用目标不自动跳动。
- **恢复/清理**：恢复两个 panel、Clip 页、Select 模式、基线缩放和关闭 auto-page；F12。
- **未能自动观察的限制**：`editor.get_state/get_capabilities/restore_view` 没有用户直接入口；
  中心 tick、scale 数值和唯一 WindowId 需 DTO 测试，GUI 只验证同一路径的交互效果。

### GUI-G15：播放、定位、loop 与播放头行为

- **追踪**：`playback.play/pause/stop/set_position/set_last_position/set_loop/`
  `set_loop_enabled/get`；`playback.clear_loop` 见限制。
- **前置**：有可播放内容；记录窗口未保存标记、运输时间和播放头；关闭 auto-page 作为基线。
- **动作**：点击 timeline 和编辑时间文本定位；点击 Play，在工程结束前截图；Pause 后再次
  Space 恢复，最后 Stop；启用空 loop，观察按当前 bar 创建的一小节区域；拖 loop 起点、终点
  和主体，F11 后做一次无吸附拖动；禁用再启用 loop；Undo/Redo loop 提交。再从 Audio 设置
  选择一种非基线 Playhead behavior，关闭设置后验证 Stop 行为并恢复。
- **可观察断言**：定位立即改变时间和播放头但不产生未保存标记；Play 时按钮高亮、时间递增、
  播放头前进，Pause 时暂停按钮高亮且位置稳定，Stop 符合设置；首次 loop 是当前 bar 的一
  小节；拖动只在 release 后形成稳定区域，Alt 可落在非网格；loop 变更产生可撤销的未保存
  状态，禁用/重启用保留区域。
- **恢复/清理**：Stop；关闭 loop；恢复 Playhead behavior；F12。
- **未能自动观察的限制**：当前 GUI 只有启用/禁用，没有把 loop 长度清零的直接入口，
  `playback.clear_loop` 不能在本矩阵记为 GUI 通过；音频设备不可用时只把真实播放子场景记未装备。

### GUI-G16：General、界面语言、默认歌词与 Fill Lyric 基本设置持久化

- **追踪**：`settings.query/update_general/update_fill_lyric`。
- **前置**：记录隔离配置中的初始 UI language、默认歌唱语言、默认歌词和模型工具路径；没有
  打开的系统文件夹窗口。`LIBRESVIP_CLI` 只在 `CAP-LIBRESVIP` 已装备时作为有效工具路径；
  未装备时只验证字段持久化，不宣称 LibreSVIP 可运行。
- **动作**：General 中把 UI Language 切到另一语言并观察全局重译，再切回统一执行语言；
  修改默认歌唱语言与一个唯一默认歌词，关闭/重开设置；新建唱段并画音符验证默认值；用
  `RUN_ROOT` 内受控占位目录/文件填写 Game、Rmvpe 路径；LibreSVIP Path 使用
  `LIBRESVIP_CLI` 或明确的无效占位文件并重开确认。在 Fill Lyric 对话框修改预览可见性、
  字体或 Skip Slur，重开确认；完整规则 CRUD 只在 `GUI-G07` 统计。
- **可观察断言**：语言切换立即改变菜单/对话框标签且选项保持；新音符使用新默认歌词/语言；
  三个路径和 Fill Lyric 选项在关闭重开后仍显示；关闭设置不是“取消回滚”。
- **恢复/清理**：恢复全部基线设置并重开确认；不点击 Open Config/Log Folder。
- **未能自动观察的限制**：设置写盘次数、no-op 不写盘、路径规范化的内部形式和 G2P language
  order 不在此页可见；后者当前没有已注册的用户设置页，不能记为 GUI 通过。

### GUI-G17：Appearance、Developer、native frame、Direct Manipulation 与 backend

- **追踪**：`settings.update_appearance/update_developer/update_window`。
- **前置**：记录主题、字体、动画、Use native frame、Direct Manipulation、全部 Developer
  开关、Editor rendering backend 和窗口几何基线；工程已保存。分别记录
  `CAP-DIRECT-MANIPULATION`、`CAP-RHI`，不要因任一未装备跳过其他设置。
- **动作 A（即时项）**：切换 Light/Dark/System 中一个非基线主题并恢复；切换 Interface
  font、Enable animations 和 Duration scale。Developer 中开启 `Enable diagnostic output`，
  执行一组滚动/缩放后检查脱敏 debug output；逐项验证 Show log window、timeline/clip debug
  overlay、Enable panel detach，再重新附着 panel。拖动/缩放主窗口到明显不同且完全可见的
  位置。若 Windows Direct Manipulation 开关存在，切换后用受控触摸或精密触控板在 Track Editor 和
  Piano Roll 各做一次平移/缩放，再恢复。
- **动作 B（restart-required）**：对 `Use native frame` 先切换并在提示中选择 Restart Later，
  确认进程和边框未变；再执行 Restart Now，重新绑定窗口并观察原生/自绘标题栏差异，然后
  成对恢复并重启。以独立子运行对 `Embedded options dialog` 做同样流程，重启后从 Options
  入口确认设置页嵌入主窗口而不是独立模态窗口，再恢复。`CAP-RHI` 已装备时选择
  `Experimental (QRhiWidget)`，Restart Later 后再 Restart Now；在 RHI 下重复轨道/片段选择、
  拖动、缩放、钢琴卷帘画音符和参数曲线观察，随后恢复 `Legacy (QGraphicsView)` 并重启。
- **可观察断言**：主题、字体和动画即时反映；diagnostic 开关不仅持久，还在操作后产生预期
  性能诊断输出；Log window、debug overlay 和 detach 按钮随开关出现/消失。Direct
  Manipulation 开关存在时手势有效且关闭后回到基线。Restart Later 不替换进程，也不提前
  改变 native frame、Embedded Options 或 backend；Restart Now 后三者分别真实生效。
  RHI 与 Legacy 的核心编辑结果一致，无空白帧、输入偏移或持续闪烁；窗口几何跨正常重启恢复。
- **恢复/清理**：关闭 Log window 和 diagnostic output，重新附着 panel；恢复主题、字体、
  动画、Direct Manipulation、native frame、Embedded Options、Legacy backend 和窗口几何，
  对所有 restart-required 项完成成对重启。把一次已保存的几何变化留给 `GUI-G24` 复核。
- **未能自动观察的限制**：Direct Manipulation 控件由构建条件决定，且效果需要对应硬件；
  缺任一只标该子场景未装备。RHI 的内部渲染 API、diagnostic 统计精度和
  `settings.update_window` 回调次数仍需日志/契约测试；GUI 负责证明实际窗口形态、输入与画面。

### GUI-G18：Audio/Inference 设置的安全资格与持久化

- **追踪**：`settings.update_audio/update_inference`；实际推理写回在 `GUI-G22` 交叉验证。
- **前置**：记录 Audio driver/device、buffer size、sample rate、Hot plug notification、设备
  gain/pan、Playhead behavior、File reading buffer；记录 Execution Provider、GPU、Run
  Vocoder on CPU、Auto Start Infer、Sampling Steps、Depth、Playback Lookahead Window、Pitch
  Smooth Kernel Size、Capacity 和 Idle Timeout 基线。`TEST_CACHE` 仍为空或只含本任务产物。
- **动作 A（Audio 安全资格）**：先把系统与应用音量置于安全水平，展开 driver/device 列表并
  记录可用项；只在 `CAP-AUDIO` 已装备时切到 `AUDIO_TEST_ENDPOINT`。有已资格的备用 driver
  时先切换 driver 并确认 device、buffer、sample-rate 列表重建，再恢复；没有备用 driver 时
  只把 driver 切换子场景记未装备，仍验证当前 driver 的列表与设备。分别选择一个设备宣告支持的非基线 Buffer size 和
  Sample rate，点击 `Test` 听取短测试音；修改 gain/pan、Playhead behavior 和 File reading
  buffer，关闭整个 Options 后重开。`CAP-HOTPLUG` 已装备时依次测试三种 Hot plug
  notification 三种模式：Notify when any device added or removed 下添加/移除辅助端点应通知；
  Notify when current device removed 下添加无关端点不通知、移除当前测试端点才通知；Do not
  notify 下列表仍刷新但不弹通知。每步都恢复 `HOTPLUG_TEST_ENDPOINT` 和安全输出。不得打开
  Control Panel，也不得操作用户正在使用的端点。
- **动作 B（Inference 通用项）**：在 CPU provider 下修改 Sampling Steps、Depth、Auto Start
  Infer、Capacity、Idle Timeout、Playback Lookahead Window 和 Pitch Smooth Kernel Size，
  重开页面确认。以下效果验证要求 `CAP-INFER`；未装备时控件持久化仍须执行。关闭 Auto Start
  Infer、设置可区分的 lookahead，在零缓存工程中从播放头前后
  放置唱段并播放，观察只对前瞻窗口内片段启动推理；恢复 Auto Start 后观察其余片段启动。
  在同一 fixture 的独立副本上用两个合法 kernel 值触发新推理，确认自动音高曲线稳定生成，
  不把像素差异当作精确数值证明。
- **动作 C（restart-required 推理项）**：切换 Run Vocoder on CPU，先选 Restart Later；
  `CAP-INFER` 已装备时再在独立子运行中 Restart Now 并以真实短句验证声学/声音输出，随后成对恢复。`CAP-GPU`
  已装备时选择 DirectML 或构建实际提供的 CUDA，等待 GPU 列表从 Detecting 变为可选，选择
  Default 与一个明确 GPU 各一次；重启后做零缓存短句推理，再恢复 CPU 并重启。点击 Cache
  Refresh，并与 `TEST_CACHE` 的文件数/总大小对照。
- **可观察断言**：driver/device 切换失败时显示明确错误并回到仍可用项；已宣告的 buffer/rate
  能应用，Test 有短音且没有爆音或持续播放；hot-plug 只影响受控端点并可恢复。Audio 设置在
  页面销毁后持久，`GUI-G15` 可复核 Stop 行为。Inference 全部控件重开保持；lookahead 的
  推理范围、kernel 后的稳定曲线、CPU vocoder 和已资格 GPU 的真实推理均与所选配置一致；
  Restart Later 不提前切换 backend。Cache Refresh 只扫描隔离缓存，空门禁显示 No cache files。
- **恢复/清理**：先恢复安全 driver/device、buffer、sample rate 与 hot-plug，再恢复其余
  Audio/Inference 基线；对 provider、Run Vocoder on CPU 完成必要的成对重启。不点击 Cache
  Directory 或 Clean Up；关闭设置并复查测试音已停止。
- **未能自动观察的限制**：没有安全备用 driver/device 时分别把“切换”子场景记未装备，仍须
  验证列表、错误路径和无关设置。没有热插拔端点、GPU 或真实声库时也只标对应子场景；不得
  blanket skip 整页。provider、vocoder 与 kernel 的内部数值仍需脱敏日志/契约测试补证。

### GUI-G19：包搜索路径、Package Manager 与声音解析

- **追踪**：`packages.set_search_paths/list/validate/resolve_document_voices`。
- **前置**：`package-good`、`package-bad` 均在 `RUN_ROOT`；记录 Package Manager 初始数量；
  工程已保存，允许专用实例重启。
- **动作**：General 的 Package Search Paths 中添加 `package-good`、Unicode 别名/重复形式和
  `package-bad`，关闭重开观察规范化结果；按提示重启；打开 Package Manager，观察 Installed
  count、搜索、选择详情并 Verify 合法包；打开引用 `VOICE_MULTI` 的工程，再打开引用缺失
  voice 的副本；移除搜索路径并再次重启，复查列表和声音显示。
- **可观察断言**：路径编辑即时持久，重复规范化结果稳定；重启前后扫描状态从 Loading 到
  Ready；合法包出现在列表、搜索和详情中，Verify 给出明确成功/警告；工程 voice 在包存在时
  解析为真实名称，移除后显示缺失/不可用且不会崩溃；坏包只影响自身并有可诊断错误。
- **恢复/清理**：移除所有测试路径，重启确认包计数回到基线；关闭 Package Manager。
- **未能自动观察的限制**：坏包可能不会进入可选择列表，其验证只能依靠扫描日志；当前
  Package Manager 的 `Install...` 按钮在源码中没有连接安装动作，不把点击它算作任何
  Automation operation 通过；文档版本复检与运行期 voice ID 需内部证据。

### GUI-G20：MIDI 导入与 LibreSVIP Open/Import（条件子场景）

- **追踪**：`documents.commit_open/commit_import`、`imports.commit_batch`、相关
  track/clip/note/timeline 提交和 LibreSVIP 格式 descriptor。
- **前置**：新建空工程并建立一个可区分 loop，保存为专用工作工程；准备两个 MIDI fixture、
  一个损坏 MIDI 和 `libresvip-project`；单独记录 `CAP-LIBRESVIP`。
- **动作 A（MIDI）**：Import MIDI 选择单文件，在 Configure Import 中观察 Encoding、Track Selector、
  Lyrics Preview、Separate MIDI channels、Import tempo/time signature；只选一轨后先 Cancel，
  再重复并确认。随后在文件对话框中多选两个 MIDI，使用共享 Encoding/tempo/拍号对话框
  确认；最后把一个坏 MIDI 加入受控批次。对每次成功导入各按一次 Undo。
- **动作 B（LibreSVIP Open）**：`CAP-LIBRESVIP` 已装备时，从普通 `File → Open...` 的 All
  Supported Files 选择 `libresvip-project`；等待 Converting project with LibreSVIP 后，在
  Configure Import 先 Cancel 一次，再重试并只选一条轨确认。对照打开前后的轨道、timeline、
  loop、窗口显示名和 Recent；随后不保存地恢复 `base.dspx` 工作副本。
- **动作 C（LibreSVIP Import）**：走
  `File → Import → Project file (LibreSVIP)...` 选择同一 fixture；在 Configure Import 选择另一
  条轨，并分别验证 Import tempo/time signature 与现有 loop；先 Cancel、再确认，最后 Undo。
  若 `CAP-LIBRESVIP` 未装备，B、C 各自记录未装备及缺失判据，MIDI 子场景仍全部执行。
- **可观察断言**：MIDI Cancel 不新增轨道或 timeline 点；单文件只导入勾选轨，歌词/编码可见；
  选中的 tempo/拍号选项决定 timeline，原 loop 始终不变；多文件成功集合一次出现，第一次
  Undo 即整体移除该批；坏项显示汇总，成功项不因坏项产生半成品或重复提交。LibreSVIP
  Open 的 Cancel 保持旧文档，确认后整体替换为所选轨并采用源工程可表示的 timeline/loop；
  LibreSVIP Import 的 Cancel 无变化，确认后只追加所选轨，tempo/拍号遵循复选框且旧 loop
  不变，Undo 一次移除本次导入。两条入口都不会留下转换临时工程或意外未保存半状态。
- **恢复/清理**：Undo 所有导入或新建；关闭汇总框；恢复 General 中 LibreSVIP Path 基线；
  保留原 fixture 不变。
- **未能自动观察的限制**：原生文件对话框的多选不受应用内测试桥控制；若 Computer Use
  无法稳定多选，只把 MIDI 批选子场景记基础设施阻塞。LibreSVIP 转换依赖外部 CLI 与所选
  格式插件；未装备不能伪装成产品通过。`keep_successes`、转换中间文件清理、client_ref 和
  一次 History/revision 需日志/内部测试补证。

### GUI-G21：DSPX/音频导入、重定位与 `audio_clips.confirm_path`

- **追踪**：`documents.commit_import`、`imports.commit_batch`、
  `audio_clips.apply_decode_cache/apply_resolved_path/confirm_path/relocate/set_hash/set_path_status`。
- **前置**：打开干净 `base.dspx` 副本；记录现有轨道、tempo、拍号和 loop；准备工程与音频
  fixtures。为 no-hash 子场景在独立 `WORK` 子目录准备 `legacy-nohash-audio.dspx`，其记录的
  原路径不存在、SHA-512 为空，而工程相对目录中存在固定的 `same-name-candidate.wav`。
- **动作 A（DSPX/普通音频）**：Import DSPX，在 Track Selector 只选一轨，并分别组合 Import
  tempo/time signature；先 Cancel 一次，再确认一次。右键轨道空白处 Insert audio clip，
  选择 `audio-short.wav`，等待波形；打开 `missing-audio.dspx` 并通过 Check Project Resources
  → Audio Files → `Relink...` 指向测试副本。`CAP-LONG-ASYNC` 已装备时，对
  `audio-long.wav` 在任务框中 Cancel。用受控拖放分别尝试“工程+音频”混合批次和不支持文件。
- **动作 B（旧/无 hash 的同名候选确认）**：打开 `legacy-nohash-audio.dspx`；在 Check Project
  Resources 的 Audio Files 表中确认 Status 为 `Matched by name, please confirm`，选中该行，
  验证 `Confirm` 可用并点击，等待行变为 `Resolved`、波形稳定以及 `set_hash` 任务成功。保存为
  新工程并关闭。随后仅在 `RUN_ROOT` 内构造重开副本：让保存时的绝对候选失效，在记录的相对
  目录放一个同名但内容不同的受控候选，在工程同级放已确认内容的同名候选；重开已保存工程。
- **可观察断言**：DSPX 只追加选中轨，tempo/拍号遵循复选框，现有 loop 不变；Cancel 原工程
  完整。音频在播放头/目标轨生成一个片段并显示非平直波形；Relink 后 Missing 消失且保存重开
  仍可解析；取消长解码不留下空轨/片段；混合工程批次被整体拒绝并显示明确消息。无 hash
  工程必须先呈现 Unconfirmed/`Matched by name, please confirm`，Confirm 后状态变 Normal/
  Resolved，计算出的 SHA-512 与被确认候选一致；保存工程包含非空 hash。重开时错误同名候选
  被 hash 拒绝，正确同名候选自动解析，且不再出现 Confirm，波形与已确认内容一致。
- **恢复/清理**：Undo 导入；移走的音频仅在 `RUN_ROOT` 内恢复；停止任务；放弃工作副本。
  no-hash 工程、错误候选与正确候选均保留到证据核验结束，成功后再按统一清理规则处理。
- **未能自动观察的限制**：GUI 可直接证明 Unconfirmed/Resolved、Confirm 可用性、波形和
  重开结果；SHA-512 值、`set_hash` 终态、relative path 与 decode cache 必须用脱敏任务日志和
  保存文件结构补证。拖放不稳定记该子场景阻塞；长任务窗口过短只记 cancel 子场景未装备，
  不能跳过其余音频路径。

### GUI-G22：零缓存多唱轨/多 clip 真实推理、受控延迟撤销与提取

- **追踪**：12 个 `inference.*`、`extract.pitch.start`、`extract.midi.start`、
  `tasks.get/list/cancel` 的真实环境资格。
- **前置**：`GUI-G00` 通过；为本子运行重新建立空 `TEST_CACHE`，Inference 页 Refresh 显示
  No cache files，文件系统也为零。记录 `CAP-INFER`、`CAP-AUDIO`、`CAP-INFER-DELAY`、
  `CAP-EXTRACT` 和 `CAP-LONG-ASYNC` 的独立资格；打开只读副本 `infer-multi.dspx`，确认至少两条歌声轨且
  每轨至少两个可区分唱段。缺少某项只影响明确依赖它的子场景。
- **动作 A（全量 cache miss）**：打开工程后逐轨、逐 clip 轮换激活，在每个 clip 的 timeline
  底部观察 inference status strip 从 Pending/Running 到 Success；推理过程中在各 clip 间往返
  并重复观察。全部稳定后，对每个 clip 分别确认音符、自动音高线、音素文字/边界和非平直合成
  波形；切走再切回，并做一次水平缩放/滚动复查。`CAP-AUDIO` 已装备时逐轨 Solo 或静音其他轨，
  把播放头放入每个 clip 内逐一播放，听到与该 clip 内容对应的非静音声音，同时确认电平与
  播放头推进；每次检查后 Stop。
- **动作 B（受控延迟中的 Undo）**：在独立空缓存子运行显式开启可取消延迟，创建一个新短句；
  等待前置推理阶段完成、声学阶段已进入受控延迟且 status strip 仍为 Running，再通过真实 GUI
  执行 Undo。立即观察音符、分段、音高/音素/波形清理；随后释放延迟并等待旧任务到达终态，
  再次观察同一区域和任务日志。未命中 Running 窗口不得用“完成后 Undo”代替。
- **动作 C（提取与取消）**：对包含 `audio-short.wav` 的专用副本分别发起 Extract Pitch 与
  Extract MIDI；`CAP-EXTRACT` 已装备时在独立副本允许两者完成，`CAP-LONG-ASYNC` 已装备时
  另在可取消窗口中各 Cancel 一次；观察任务对话框、目标曲线/唱段、Undo 和日志终态。
- **可观察断言**：A 必须证明所有目标 clip 都是零缓存真实后端运行，而不是只检查首轨或首
  clip；每个 clip 均有独立、最终成功的状态条、音高、音素和合成波形，已装备音频端点时还要
  逐 clip 有声音。Running 期间的状态动画可以变化，但音高、音素或波形不得持续消失/重现；
  Success 后切换、缩放和重复观察均稳定，无持续闪烁、红色失败分段或跨 clip 串写。B 中 Undo
  后旧 generation/旧输入即使完成也不得闪回，且无残留声音或失败状态。C 的 Cancel 不产生
  曲线/唱段半提交，完成只提交一次且 Undo 一次可恢复；退出前没有遗留运行任务。
- **恢复/清理**：Stop；Undo 提取结果；关闭任务框；成功轮可删除本轮生成缓存，删除前必须
  验证只位于 `TEST_CACHE`；失败轮保留。
- **未能自动观察的限制**：Computer Use 的重复观察或局部录屏能证明“未见持续闪烁”，不能
  扩大为逐帧数学证明。没有安全音频端点时只把逐 clip 听音记未装备，视觉推理仍必须跑；没有
  受控延迟只把 B 记未装备；提取模型或稳定取消窗口不足时分别标记。具体 stage、TaskId、
  revision 重基、cache miss 计数、提交点和 generation 隔离依赖脱敏日志补证。

### GUI-G23：MIDI 导出、已存在目标覆盖确认与 round-trip

- **追踪**：`formats.list`、`exports.midi.start`；音频导出明确排除。
- **前置**：有已保存、含多轨音符/歌词/tempo/拍号的专用工程；在 `OUTPUT` 预放一个内容固定、
  SHA-256 已记录且与待导出工程不同的 `existing.mid`。
- **动作**：打开 `File → Export → MIDI file...`，观察 MIDI 过滤器并选择
  `OUTPUT/existing.mid`；在原生 overwrite 确认中先选择 Cancel/No，回到编辑器后复核文件。
  重复同一路径并选择 Confirm/Yes；等待导出结束，独立记录新字节数与 SHA-256。新建工程并从
  覆盖后的 MIDI 导入，选择全部轨与 timeline 选项。全程不得点击相邻的 Audio file...。
- **可观察断言**：第一次取消后没有成功 Toast/错误半状态，`existing.mid` 的字节数、mtime
  和 SHA-256 均保持；第二次确认后文件被一次性替换为非空、可解析的 MIDI，哈希与旧文件不同；
  round-trip 后轨数、音符大致位置、可表示歌词、tempo 和拍号与源工程一致。
- **恢复/清理**：关闭 round-trip 工程；成功时保留哈希到证据后删除 MIDI；失败时保留。
- **未能自动观察的限制**：MIDI 格式本身不承诺保留声库、参数、Speaker Mix 或音素；
  `formats.list` 的完整 descriptor 与原生 overwrite 对话框到任务提交的单次性仍需日志/DTO
  断言。不得借此轮打开任何音频导出对话框，
  `exports.audio.preview/start/cleanup` 不记 GUI 通过。

### GUI-G24：重启、退出、窗口恢复与最终清理

- **追踪**：`application.get_info/request_restart/request_exit`、`settings.update_window`，以及
  关闭期间任务终止。
- **前置**：`GUI-G17` 留有一个成对可恢复的 restart-required 设置；工程已保存；记录当前 PID、
  WindowId 和窗口几何；没有未决模态框。
- **动作**：在 RestartDialog 选择 Restart Now；等待旧 PID/窗口消失和新实例出现，记录新
  PID/WindowId，确认桥与 test mode 重新启用、窗口几何恢复、设置已生效；把该设置改回基线
  并再次 Restart Now；制造未保存编辑，请求 Exit 先 Cancel，再保存或 Don't save；最后在
  clean 状态从 File -> Exit。若有已资格的长任务，另用专用副本验证退出请求先取消任务再退出。
- **可观察断言**：每次重启只有一次旧进程退出和一次新进程启动；新进程仍使用隔离配置/
  缓存，窗口几何与设置符合预期；Exit Cancel 保持窗口；最终 Exit 后记录的测试 PID 和窗口
  均消失，无等待任务对话框残留、无用户实例受影响。
- **恢复/清理**：确认 restart-required 设置、主题、语言、包路径、音频/推理设置、预设和
  Recent 全部恢复基线。仅当所有必选场景通过，重新验证 `RUN_ROOT` 的位置与前缀后删除；否则
  保留并只在报告中使用脱敏别名。
- **未能自动观察的限制**：宿主回调“恰调用一次”、application info 和任务管理器内部状态
  必须由日志/契约测试补证；进程替换期间 Computer Use 若失去窗口绑定，必须重新观察并按
  新 WindowId 绑定，不能沿用旧坐标。

## 5. GUI 可达性与证据边界

以下项目在当前源码中没有可独立操作、可完整观察的 GUI 入口，不能因为相关页面“看起来
正常”就记为对应 operation 通过：

| 项目 | GUI 能证明什么 | 仍需的补证/处理 |
|---|---|---|
| 各域 `get`、Operation ID/路由 | 当前窗口中的一部分呈现 | DTO 完整性、快照无副作用、未知 ID/WindowId 由确定性测试覆盖 |
| DocumentId、revision、idempotency | 标题、路径、内容和保存点变化 | ID 轮换、revision 次数、generation 由日志/契约测试覆盖 |
| `playback.clear_loop` | toggle 只能启用/禁用并保留区域 | 当前无清零入口，只做契约测试，不记 GUI 通过 |
| `editor.restore_view/get_state/get_capabilities` | 可手工改变 view | 当前无用户触发 restore DTO 的入口，只做 Facade 测试 |
| `settings.update_g2p_language` | 推理模块可能内部清空顺序 | G2P 页面未注册到 Options/Fill Lyric，不能做用户 GUI 回归 |
| `settings.update_window` | 跨正常退出可见几何恢复 | 提交发生在事件循环退出后，需 `GUI-G24` 加日志补证 |
| 音频片段 cache/hash/path-status | Unconfirmed/Resolved、Confirm、波形与重开结果 | `set_hash` 终态、SHA-512 与文档版本复检用脱敏日志/保存结构补证 |
| 12 个 `inference.*` | 最终曲线、音素、波形及无闪回 | stage、revision 重基、目标复检用脱敏日志 |
| `tasks.get/list/cancel` | 任务框、取消与最终可见结果 | TaskId、状态机和终态保留用任务契约测试 |
| Package Manager `Install...` | 按钮存在 | 当前没有连接安装动作，不映射一期 package operation |
| `exports.audio.*` | 无 | 受 GUI smoke skill 边界约束，禁止打开音频导出对话框 |

### 5.1 已静态核对的 GUI 入口

下列路径已按现有 UI 源码核对；执行时仍以当次观察到的可访问名称为准，不缓存坐标：

| 能力 | GUI 路径 | 静态审阅锚点 |
|---|---|---|
| Recent 单项删除 | 标题栏文件按钮 → Recent 项 → `⋮` → Remove；当前工程禁用 Remove | `src/app/UI/Views/MainTitleBar/FilePopupWidget.cpp` |
| Recent 全清 | File → Recent Projects → Clear Recent Projects | `src/app/UI/Views/MainTitleBar/MainMenuView.cpp` |
| Fill Lyric 规则 | 选中音符 → Edit → Fill lyrics... → Lyric/Splitter/Tagger/Test | `src/app/UI/Dialogs/FillLyric/LyricDialog.cpp`、`src/app/Modules/FillLyric/Widgets/` |
| 音频路径确认 | Check Project Resources → Audio Files → 选中 Unconfirmed 行 → Confirm | `src/app/UI/Dialogs/ResourceCheck/AudioResourcePage.cpp`、`src/app/Controller/Tasks/ResolveAudioPathTask.cpp`、`src/app/Controller/TrackController.cpp` |
| LibreSVIP Open | File → Open... → All Supported Files 中选择支持格式 | `src/app/UI/Views/MainTitleBar/MainMenuView.cpp`、`src/app/Modules/ProjectFormats/LibreSVIPFormatHandler.cpp` |
| LibreSVIP Import | File → Import → Project file (LibreSVIP)... | 同上 |
| Audio 设置 | Options → Audio → driver/device/buffer/sample rate/hot plug/Test | `src/app/UI/Dialogs/Options/Pages/AudioPage.cpp` |
| Inference 设置 | Options → Inference → Device/Render/Singer Session Retention/Cache | `src/app/UI/Dialogs/Options/Pages/InferencePage.cpp` |
| Appearance/Developer | Options → Appearance 或 Developer Options | `src/app/UI/Dialogs/Options/Pages/AppearancePage.cpp`、`src/app/UI/Dialogs/Options/Pages/DeveloperPage.cpp` |
| 每 clip 推理状态/波形 | 激活唱段 → Clip Editor timeline 底部状态条、音高/音素/波形 | `src/app/UI/Views/Common/TimelineView.cpp`、`src/app/UI/Views/ClipEditor/PianoRoll/PhonemeView.cpp` |
| MIDI 导出 | File → Export → MIDI file... | `src/app/UI/Views/MainTitleBar/MainMenuView.cpp` |

## 6. 域到稳定场景组的追踪摘要

| 域 | 主要场景组 |
|---|---|
| 文档、Recent、失败回滚 | `GUI-G01～GUI-G03` |
| 轨道、片段、Master | `GUI-G04～GUI-G05`、`GUI-G12` |
| 音符、歌词、音素、Fill Lyric 规则 | `GUI-G06～GUI-G07` |
| 参数、Speaker Mix、预设 | `GUI-G08～GUI-G11` |
| tempo、拍号、History | `GUI-G12～GUI-G13` |
| 播放与 Editor 稳定状态 | `GUI-G14～GUI-G15` |
| General/Appearance/Developer/Audio/Inference/Window | `GUI-G16～GUI-G18`、`GUI-G24` |
| 包路径、包、voice 解析 | `GUI-G19` |
| MIDI/DSPX/LibreSVIP/音频导入 | `GUI-G20～GUI-G21` |
| 推理、提取、任务取消 | `GUI-G22`（逐能力条件子场景） |
| MIDI 导出/覆盖/格式 | `GUI-G23` |
| 应用重启、退出与最终清理 | `GUI-G24` |

所有未执行或未装备项都必须精确到子场景、能力编号和原因；禁止写“环境原因，整组跳过”。
条件子场景未装备不降低内部 operation 的确定性测试分母，只作为真实环境资格单独统计。
