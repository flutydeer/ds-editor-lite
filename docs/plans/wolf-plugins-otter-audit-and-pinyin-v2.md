# wolf 插件与 otter 审查 + 拼音插件切 cpp-pinyin v2 的方案

> 状态：**方案阶段（未动任何代码）**。第 1 轮只读探索 + 子代理扇出，结论按「事实 / 推断 / 未证实」标注。
> 快照：lite `c2ed02e`（工作区有用户改动）、synthrt `0e97662`、wolf `38ad5b0`（工作区干净）、otter `6db876b`、cpp-pinyin 本地仓分支 `v2` @ `e7aacb7`（自述 2.0.0 未打 tag）。
> 幂等提醒：行号引用前须自行复核，本轮所有行号均按上述快照实测。

## 0. 结论速览

1. **两仓没有 P0，也没有经复核成立的 P1**。wolf 13 条（4 P2 + 9 P3），otter 10 条（2 P2 + 8 P3，另 1 条已撤销）。**复核轮推翻了两处**：子代理原报的 Lua「协程绕过中断钩子」不成立（LuaJIT 的钩子挂在 global_State，见 W-8），我原报的 otter「基类析构不等待」（O-7）也不成立（`srt::ITask` 的析构自己就 wait）。最值得先看的两条：Lua 沙箱无内存上限（W-9）、otter 的 Windows 子进程句柄继承可能让两个 ggml 分析器互相拖死（O-6）。
2. **拼音插件切 v2 的阻力不在 wolf 的 G2P 逻辑，在五件事**：包名从 `cpp-pinyin` 变 `cpp-pinyin2`、`Engine` 从「进程全局词典根 + 隐式单例」变「实例化 + `Outcome`」、词典根 arbiter 整套可删、**非 BMP 字符会整批失败**（§3.2 的地雷）、lite 的插件依赖 DLL 拷贝规则要跟着改。
3. **setdir 现状只覆盖 wolf**：`scripts/vcpkg-ports/wolf/portfile.cmake:17-28` 有 LOCAL-ONLY 块，synthrt 仍是 pin（`scripts/vcpkg-ports/synthrt/portfile.cmake:15`）。若迁移不碰 synthrt，可以维持。
4. **lite 自己不链接 cpp-pinyin**：只有 `src/app/CMakeLists.txt:24` 的 `find_package` 与 `cmake/LiteBuildApi.cmake:243` 的 DLL 拷贝。所以切换是「端口 + wolf 插件 + 一条拷贝规则」三处改动，不涉及 lite 的业务代码。

---

## 1. 现状快照（本轮实测）

| 项 | 事实 | 证据 |
| :-- | :-- | :-- |
| wolf 是否本地 | **是**，端口把 `D:/projects/wolf/` 正向拷进 `SOURCE_PATH` | `scripts/vcpkg-ports/wolf/portfile.cmake:17-28` |
| synthrt 是否本地 | **否**，仍是 commit pin `0e97662` | `scripts/vcpkg-ports/synthrt/portfile.cmake:15` |
| 已装 cpp-pinyin | **1.0.2**（v1） | `vcpkg/installed/vcpkg/status` 的 `Package: cpp-pinyin` / `Version: 1.0.2` |
| v1 端口来源 | 共享 overlay `scripts/vcpkg/ports/cpp-pinyin`（子模块），pin `3924631…` | 该端口 `portfile.cmake` |
| v1 词典布局 | `share/cpp-pinyin/dict/{mandarin,cantonese}` | 实测已装目录 |
| v2 词典布局 | `data/dialects/{mandarin,cantonese}`，安装到 `share/cpp-pinyin2/dict` | `cpp-pinyin/CMakeLists.txt:9`、`cpp-pinyin2Config.cmake.in` |
| wolf 侧唯一 cpp-pinyin 用户 | 只有 `inferenceinterpreters/pinyin`（其余 8 个插件不引用） | 仅 `pinyin/CMakeLists.txt:6,8` |
| lite 侧耦合 | `find_package(cpp-pinyin)` + 拷贝 `$<TARGET_FILE:cpp-pinyin::cpp-pinyin>`，**无 target 链接** | `src/app/CMakeLists.txt:24`、`cmake/LiteBuildApi.cmake:243` |

---

## 2. 审查结果

### 2.1 wolf 插件面（两轮合计 6 P2 / 7 P3）

> 范围：9 个插件目录、`src/lib/Support` 里被插件用到的部分、`src/plugins/CMakeLists.txt` 的约定。子代理只读审计 + 主代理逐条回读复核。

| # | 级别 | 位置 | 现象 | 主代理复核 |
| :-- | :-- | :-- | :-- | :-- |
| W-1 | P2 | `pinyin/main.cpp:334-338`、`s2p/main.cpp:238-240`、`onset/main.cpp:154`、`multig2p/main.cpp:345-346`、`chain/main.cpp:895-900`、`lua/main.cpp:283-284` | `createInference` 把 `spec.configuration()` 直接 `static_cast` 后即解引用，**不判空** | **已复核**：pinyin `:334-338` 确实无判空即访问 `configuration->languageMap`；对照 chain 自己的校验器 `chain/main.cpp:423-427` 就判了空——同仓自相矛盾 |
| W-2 | P2 | `chain/main.cpp:911-913`、`onset/main.cpp:166-168`、`s2p/main.cpp:253-254`、`pinyin/main.cpp:363-364`、`multig2p/main.cpp:396-397`、`linguistproviders/wolf/main.cpp:17-18` | `create()` 拒绝时消息是固定串，**不打印被拒的三元组实值** | **已复核**：chain `:913`、onset `:168` 均为固定串；`lua/main.cpp:349-351` 是唯一打印 variant 实值的实现 |
| W-3 | P2 | `src/plugins/CMakeLists.txt:25` | `file(GLOB_RECURSE _src …)` **没有 `CONFIGURE_DEPENDS`** | **已复核**：`:25` 原文如此。后果是新增/删除插件源文件后不重新配置就静默漏编 |
| W-4 | P3 | `s2p/main.cpp:253-259` | `create()` 是两段判定（先 interface+level 再 variant），与文档 §8.1「一次组合判定」的偏差清单只登记了 lua | 已复核形态 |
| W-5 | P3 | `s2p/main.cpp:210-216` | `direct` 变体下 `file` 键被接受但静默忽略，而 `multig2p/main.cpp:228-240` 对「不生效的键」取相反策略（`FeatureNotSupported`） | 已复核判据并存 |
| W-6 | P3 | `inferenceinterpreters/stub/CMakeLists.txt:43` | 生成的 manifest 路径硬编码 `stub`，与上层「目录名即插件名」的派生重复 | 已复核 |
| W-7 | P3 | `inferenceinterpreters/stub/main.cpp:14-27` | stub 的 `create()` 对 variant **完全不校验**，与构建期裁剪出的 manifest 零比对 | 已复核 |

**wolf 第 2 轮补审（子代理 + 主代理复核）**：

| # | 级别 | 位置 | 现象 | 复核 |
| :-- | :-- | :-- | :-- | :-- |
| W-8 | P3（**子代理原报 P1，主代理推翻**） | `src/plugins/inferenceinterpreters/lua/LuaSandbox.cpp:227-243` | 删除清单里没有 `coroutine`，脚本仍可用协程。**但「协程能绕过中断钩子」不成立**：LuaJIT 把钩子挂在 global_State 上（`luajit/src/lj_dispatch.c:339-346` 的 `lua_sethook` 写的是 `G(L)->hookmask`），协程共享同一 global_State，钩子照样生效 | **已实测推翻**：子代理按标准 Lua 5.1 的「每线程钩子」推断，而这份构建是 LuaJIT。剩下的只是「沙箱面比预期宽」的洁癖问题 |
| W-9 | P2 | `LuaSandbox.cpp:314` | 用 `luaL_newstate()`，**没有内存上限**。脚本可以把结果攒进全局表跨调用增长，宿主 OOM 且脚本不报错（`collectgarbage` 也被删了，连自省都做不到） | **已复核**：`:314` 原文如此，无自定义 allocator。降为 P2：脚本来自包作者，属半可信，影响是资源耗尽而非逃逸 |
| W-10 | P3（复核后降级） | `WolfLinguistProvider.cpp:168-171` + `:150` | role 目标只比 **interface** 就放行，紧接着 `exports->as<Exports>()` 是 unchecked `static_cast`。同文件另外三处（`:81-83`、`:108-113`、`:436-438`）都比三元组，注释 `:136-137` 还自称「三元组已被比对」——不成立。出现同 interface 的 L2 契约就是 UB | **部分成立，已降 P3**：降级依据是加载期还会用**目标自身** interpreter 的 `createImportOptions` 与 `validatePayloadIdentity` 校一遍 options/target 三元组（`PackageLoader.cpp:875-890`、`:839`），而 wolf 三个 provider 都固定用自己的 level 1；且 variant 在 `G2PApiL1.h:60` 是运行时字符串，同 interface 不同 variant 的指针类型本来就同一。真正要第三方自建类别才踩得到 |
| W-11 | P3（复核后降级） | `WolfLinguistProvider.cpp:416` ↔ `:419` | `defaultLanguage` 不随语言条目一起过滤：条目被丢弃后它仍指向已不在 `languages()` 里的 handle | **部分成立，已降 P3**：validators 先于 extensions 执行（`PackageLoader.cpp:995→1000→1005`），被过滤的条目早已让加载失败，过滤分支对合法包是死代码；且契约明写 `defaultLanguage` 只是给宿主的提示（`LinguistApiL1.h:293-298`） |
| W-12 | P3 | `include/wolf/Support/ResourceCache.h:58,63` | 缓存键无类型分量，正确性靠「kind 字符串全仓唯一」的约定；撞名会把外来对象直接重解释 | 已复核（现有 kind 实测互不冲突） |
| W-13 | P3 | `onset/OnsetRules.cpp:43` | rule 文件不拒重复 JSON 键（manifest 侧有专门的重复键扫描，这里没有） | 已复核 |

**已排除（子代理提供、主代理复核后不成立或不报）**：

- **`wolf_add_plugin` 并非缺失**：子代理全仓 0 命中，主代理查到了定义——它在 **qmsetup** 的 `BuildRepoHelpers.cmake:363` 的 `function(${_F}_add_plugin …)`，`${_F}` 即项目前缀 `wolf`。**不构成 P0 风险，后续不要重开。**
- `game-ggml` / `tifa-ggml` 一类带连字符的 variant 合法：`ContribLocator::isValidSeparatedId` 的 `isValidSegment` 允许 `-` 与 `_`。
- `readIntKnob` 把 `INT_MIN` 判为非整数与 otter 的 schema（`note-1-exports.schema.json:73-75` 只允许 `-2147483647`）自洽。
- **「Lua 协程绕过中断钩子」不成立**（子代理原报 P1）：LuaJIT 的 `lua_sethook` 写的是 `G(L)->hookmask`（`luajit/src/lj_dispatch.c:339-346`），协程共享 global_State，钩子照样生效。后续不要按标准 Lua 5.1 的「每线程钩子」再报一次。

### 2.2 otter（两轮合计 3 P2 / 9 P3）

> 范围：`src/plugins/inferenceinterpreters/**`、`src/lib/**`、`include/otter/**`。已知的格式面问题（F-5~F-9、F-21）明确不在本轮范围。

| # | 级别 | 位置 | 现象 | 主代理复核 |
| :-- | :-- | :-- | :-- | :-- |
| O-1 | P2 | `docs/schemas/{f0,note,align}-1-exports.schema.json` 的 `channelCount`（如 `f0-1-exports.schema.json:16-20`）+ `src/lib/Api/F0/1/F0ApiL1.cpp:50-56` + `src/lib/Analysis/AnalysisInput.cpp:11-19` | 契约允许 `channelCount ≥ 1`，读取器用 `readPositiveInt` 放行 2，而共享执行助手对任何 `!= 1` 直接 `FeatureNotSupported` | **已复核**：三处原文一致。第三方变体照 schema 声明 2 声道能过加载期，每次执行必失败，宿主执行前无法从 exports 预判 |
| O-2 | P3 | `include/otter/Api/F0/1/F0ApiL1.h:50-51`（Note `:83`、Align `:111` 同构）↔ `src/lib/Support/ManifestValues.cpp:55-65` | 头文件写「`0` 表示无限制」，但显式写 0 会被 `readPositiveDouble` 拒（只在键缺省时才等价于无限） | **已复核**：头 `:50` 与 `ManifestValues.cpp:60-62` 原文一致 |
| O-3 | P3 | `include/otter/Analysis/Provider/AnalysisInput.h:75-78` ↔ `src/lib/Analysis/AnalysisInput.cpp:12-14` | 「声道」一词两处指向不同量（音频声道 vs 模型声明声道），措辞相反 | 已复核 |
| O-4 | P3 | `src/plugins/inferenceinterpreters/stub/main.cpp:117` | `~StubExecutive()` 缺 `override`（其余四个执行体都有） | **已复核**：`:117` 原文如此 |
| O-5 | P3 | `src/plugins/inferenceinterpreters/onnx/OnnxSupport.h:126-132` | ONNX 路径的 `stop()` 跨线程调用会话，但契约头未声明该线程边界（CLI 路径反而写了） | 已复核 |

**otter 第 2 轮补审（子代理 + 主代理复核）**：

| # | 级别 | 位置 | 现象 | 复核 |
| :-- | :-- | :-- | :-- | :-- |
| O-6 | P2 | `src/plugins/inferenceinterpreters/external/CliProcess.cpp:145-177` | Windows 建进程用 `bInheritHandles=TRUE` 且没有 `HANDLE_LIST`，同进程内第二个引擎 spawn 时会继承前一个分析器的 `stdinRead`/`stdoutWrite`，读端等不到 EOF → **两个 ggml 分析器并发时可能挂死**（条件性：现行 `models-v0.1` 只发 ONNX 变体，lite 源码里也没有 ggml 引用，只有用户自装 ggml 包时才会走到 CLI 路径）。POSIX 分支用 `FD_CLOEXEC` 防住了同一个坑 | **已复核**：`:145` 的 `inheritable{…, TRUE}`、`:155-156` 只清「自己保留的那端」、`:166` 的 `TRUE` 三处原文一致 ⇒ 该分析成立 |
| ~~O-7~~ | ~~P2~~ | `src/lib/Analysis/AnalysisTask.cpp:17` | ~~基类析构不等待，第三方变体可能 UAF~~ —— **复核后撤销**：`srt::ITask::~ITask()` 本身调 `waitForAsyncExecution()`（`synthrt/lib/Task/ITask.cpp:15-17`，主代理已回读），而 `AnalysisTask.h:24-26` 原文就把「the wait」归给 `srt::ITask`，所以 `= default` 是合契约的 | **撤销**，不要再报 |
| O-8 | P3 | 同上文件 `:150-153` | 两次 `CreatePipe` 用 `||` 短路，第二次失败时第一对管道句柄泄漏 | **已复核**：`:150-152` 直接 return，未关 `stdinRead`/`stdinWrite` |
| O-9 | P3 | 同上文件 `:155-156` | `SetHandleInformation` 的返回值被丢弃，失败时管道端仍可继承——正是注释要防的情形 | 已复核 |
| O-10 | P3 | 同上文件 `:233-237` | `wait()` 在锁外 `WaitForSingleObject`，回来无条件 `CloseHandle(process)`，与另一线程的 `terminate()` 构成二次关闭 | 已复核形态（潜伏） |
| O-11 | P3 | `src/plugins/inferenceinterpreters/CMakeLists.txt:27-33` + qmsetup `BuildRepoHelpers.cmake:413-421` | DEVEL 打开时导入库（`.lib`）与 `.dll`、`plugin.json` 落进同一插件目录 | 已复核机制 |

**两条原本的「待确认」已关闭**：① `AnalysisTask` 的回调销毁路径**无悬垂**（worker lambda 只在 `:112`/`:117` 触碰 `this`，回调之后只碰按值捕获的共享对象），悬垂嫌疑不成立；② `.dll` 与 `plugin.json` **确实同目录**（`otter_add_plugin` 的 `RUNTIME DESTINATION` 与本仓 `install(FILES plugin.json)` 用同一 category 串）。

**otter 仍未证实的**：

1. 无控制台宿主里 `GetStdHandle(STD_ERROR_HANDLE)` 可能为 NULL，`:163` 直接填进 `hStdError` 后 `CreateProcessA` 是否失败——**未实测**（要跑才知道）。
2. dsinfer `InferenceSession::stop()` 与 `start()` 能否并发（ONNX/CLI 两条取消路径都依赖它）。
3. 「同一回调里先 `startAsync` 起后继、再销毁分析器」这个组合在 `AnalysisTask.h:36-42` 里没写（原文只有 "or"）。按 synthrt `ITask.cpp` 的等待实现，它会阻塞到后继结束而不是 UAF，但阻塞算契约本意还是缺一条规则，**需设计侧拍板**。
4. ggml 变体的 `game-ggml`/`tifa-ggml` 只读了停止与析构段，其余大文件未逐行审。

### 2.3 两仓「做对了」的清单（避免后续重复排查）

- 三元组校验覆盖完整：9 个 wolf 插件与 4 个 otter ONNX 执行体都精确比较 `(interface, level, variant)`。
- IID 与 synthrt 常量逐字一致（`org.openvpi.synthrt.plugin.InferenceInterpreter`），库名/目录/`plugin.json.name` 三者一致，Debug 后缀已清。
- 析构顺序正确：wolf 的 `Executive` 一律析构首行 `finish()`；otter 的 ONNX 执行体都先 `shutDown()` 再析构，CLI 执行体先 `stop()/waitForFinished()` 再拆进程。
- 配置解析严格（未知键拒绝）、路径基准统一为声明文件目录。
- wolf 的 `RootReservation` 放在 `Configuration` 里使 claim 生命周期落在加载事务内，是**有意设计**。
- otter 的 `AnalysisTask` 跨线程设计干净：回调之后只触碰按值捕获的共享对象，**回调内销毁分析器不会悬垂**（第 2 轮实测关闭了该嫌疑）。
- otter 的 `.dll` 与 `plugin.json` **确实落在同一目录**（安装目标串一致）。
- multig2p 的配置校验是本仓的正面样本：未知键拒绝（`main.cpp:251-254`）、`maxLen` 有上界（`:222-226`）、`beamSize`/`topK`/`lengthPenalty` 超出实现能力时明确 `FeatureNotSupported`（`:236-249`）——第 1 轮的 W-5（s2p 对不生效的键静默忽略）正是拿它做对照。

---

## 3. cpp-pinyin v2 迁移面

### 3.1 v1 → v2 符号级 delta（wolf 实际用到的面）

| # | v1 用法（wolf 侧） | v2 对应 | 破坏 |
| :-- | :-- | :-- | :-- |
| 1 | `#include <cpp-pinyin/{G2pglobal,Jyutping,Pinyin}.h>`（`PinyinEngines.cpp:8-10`） | `<cpp-pinyin2/core/Engine.h>`、`<cpp-pinyin2/ext/Registry.h>`、`<cpp-pinyin2/core/Types.h>` | 是（头路径全变） |
| 2 | `Pinyin::Pinyin m_engine`（`:64`） | **类消失**，改用 `cpp_pinyin::v2::Engine`（`core/Engine.h:33`） | 是 |
| 3 | `Pinyin::Jyutping m_engine`（`:80`） | 同类合并，方言由 `Engine::open` 的 `dialectId` 选（`Engine.h:41-48`） | 是 |
| 4 | `hanziToPinyin(chars, ManTone::NORMAL, Default, true, false, false)`（`:59-60`） | `Engine::query(vector<string>, EngineConfig{style=MandarinStyle::NORMAL, includeCandidates=true})`（`Engine.h:79`、`Types.h:382-394`） | 是 |
| 5 | `hanziToPinyin(chars, CanTone::NORMAL, Default, true)`（`:75-76`） | 同上，`CantoneseStyle::NORMAL` | 是 |
| 6 | `ManTone::NORMAL` / `CanTone::NORMAL` | `MandarinStyle::NORMAL` / `CantoneseStyle::NORMAL` | 名变，**数值不变**（都是 0） |
| 7 | `Pinyin::Default`（错误处理模式） | `ErrorHandling::Default`（`Types.h:133`） | 改名 |
| 8 | `PinyinResVector` 与 `.error/.pinyin/.candidates`（`:33-48`） | `Result`/`Segment`：`failure_kind == Code::Ok` ⇔ `!error`，`rendered` ⇔ `pinyin`，`candidates` 同名 | 结构改名，语义等价 |
| 9 | `m_engine.initialized()`（`:54`、`:70`） | 公开面**没有** `isReady()`，就绪由 `Engine::open` 的 `Outcome` 表达 | 判据迁移 |
| 10 | 进程全局词典根 + `RootReservation`/`reserveRoot`（`:89-120`、`main.cpp:66`、`:312`） | 无全局根，root 是 `Engine::open` 的实参 | **机制删除**，arbiter 整套可删 |
| 11 | `find_package(cpp-pinyin CONFIG REQUIRED)` / `cpp-pinyin::cpp-pinyin`（`pinyin/CMakeLists.txt:6,8`） | `cpp-pinyin2` / `cpp-pinyin2::cpp-pinyin2` | 是 |

### 3.2 一个必须先处理的地雷（P0 级语义差异）

v2 的 `query(std::vector<std::string>)` 要求**每个元素恰好 1 个 UTF-16 码元**，任一元素违例就**整批**返回 `Code::InvalidArgument`、零行（`cpp-pinyin/src/Engine.cpp:214-218`，注释 N16）。而 4 字节 UTF-8 字符在 v2 内部被转成**代理对**（`src/U16Str.cpp:157-162`，两个码元），wolf 又按码点切分喂入（`wolf/include/wolf/Support/Utf8.h:29-35`）。

⇒ **歌词里出现一个 CJK 扩展 B 区字（如 𠮷）或 emoji，整段转换就会失败**，而 wolf 现行设计是「逐行成败、失败行走链的兜底」（`PinyinEngines.cpp:25-48`）。迁移时必须在 wolf 侧显式处理：把非 BMP 字符单独拣出并记为无读音，或分批发，**不能直接照搬**。

### 3.2b 改写时要靠的几个 v2 事实（主代理实测）

- **`query(vector<string>)` 的行数与输入一一对应**：`EngineImpl::query` 把每个元素转成一条 token 再 `assemble`，`result(tokens.size())` 且 `row.text = token`（`cpp-pinyin/src/EngineImpl.cpp:513`、`:524-526`）。所以 wolf 的「一字符一结果」映射成立，**不需要**按 ASCII 串重新对齐（那个「ASCII 连跑成一行」的行为只属于 `query(string_view)` 路径）。
- **`Registry` 不可拷贝不可移动**（`ext/Registry.h:60-63`），构造即注册内置方言（`:39-41`）⇒ 只能放函数内 static，且**必须比所有 `Engine` 活得久**。
- **`Engine` 只能移动**（`core/Engine.h:52-55`），`open()` 返回 `Outcome<Engine>`（`:41-42`），失败时 `error().code` 区分 `NotReady`（未知方言）与 `Load`（词典加载失败），`detail` 带文件名与目录。
- **`EngineConfig` 的默认值**：`includeCandidates = true`、`includeReading = false`、`errorHandling = Default`（`core/Types.h:382-394`）⇒ wolf 只设 `style` 就能保持现有语义。

### 3.3 包 / 目标 / 词典路径

| 项 | v1 | v2 |
| :-- | :-- | :-- |
| CMake 包名 | `cpp-pinyin` | `cpp-pinyin2`（`CMakeLists.txt:6`） |
| 目标 | `cpp-pinyin::cpp-pinyin` | `cpp-pinyin2::cpp-pinyin2`（`:453`、`:582-585`） |
| 兼容策略 | 1.x | `SameMajorVersion`（`:556-567`）⇒ 1.x 的 `find_package` 不会被 2.0 满足 |
| 词典安装变量 | `CPP_PINYIN_VCPKG_DICT_DIR` → `share/cpp-pinyin/dict` | `CPP_PINYIN2_DICT_DIR` → `share/cpp-pinyin2/dict` |

**关键：wolf 运行时不读那个变量。** 它的词典根来自语言包声明里的 `"dictRoot": "./dict"`（已装包 `wolf-g2p-pinyin/inferences/pinyin/inference.json:20`），相对 `declarationPath()` 解析（`main.cpp:294-296`），实际落在 `.../wolf/packages/wolf-g2p-pinyin/inferences/pinyin/dict/{mandarin,cantonese}`。v2 的方言子目录名与 `Dialect` 的 `dictSubDir` 天然对齐，**所以词典数据不用搬**。

**数据代差（已量化，不阻断但影响等价性验证）**：wolf 语言包里那份词典与 v1（1.0.2）安装的词典**逐文件字节数完全相同**（`word.txt` 164,588、`phrases_dict.txt` 124,875、`phrases_map.txt` 6,725、`trans_word.txt` 49,550，实测两处一致），即 wolf 发的是 v1 时代的数据。而 v2 自带的表明显更大（`phrases_dict.txt` 146,439、`phrases_map.txt` 7,087、`word.txt` 164,968）并多了 `dialect.json` 与 `format.txt`（内容为 `2`，即 `kSupportedDictFormat = 2`，`src/Engine.cpp:120`）。v2 对「无格式标记」的旧目录放行（`src/dict/DictManifest.cpp:70-78`），**所以旧数据能加载，但用旧数据跑 v2 引擎与用 v2 自带数据跑出来的读音不是同一件事**——等价性验证必须固定用 wolf 包里那份数据。

### 3.4 wolf 侧改动点

1. `pinyin/CMakeLists.txt:6,8` 换成 `cpp-pinyin2`。
2. `PinyinEngines.cpp`：头与类型全换，`convert()` 改走 `query()` 并解 `Outcome`，把 `Segment` 映射回 wolf 的 `CharacterResult`。
3. **删掉 `RootState`/`RootReservation`/`reserveRoot` 与 `Configuration` 里的 `RootReservation` 字段**（`main.cpp:63-66`、`:312`）——净减少代码。
4. 新增一个进程级（函数内 static）`cpp_pinyin::v2::Registry`，**生存期必须长于所有 `Engine`**。
5. 处理 §3.2 的非 BMP 字符。
6. 诊断质量：必须走 `Engine::open`（`Outcome`）而不是直接构造，否则拿不到 `FileNotFound`/`InvalidFormat` 分级。
7. **测试要跟着改**（`src/tests/auto/Inference/test_PinyinBackend.cpp`）：其中两个用例测的是即将被删掉的 arbiter 行为——`test_PinyinBackend_RefusesASecondDictionaryRoot`（`:254`，「第二个词典根必须被拒」）与 `test_PinyinBackend_ReturnsTheRootWhenTheLoadFails`（`:280`，「加载失败要归还根」）。v2 没有全局根，这两条**要删或改写成「两个不同根的包可以共存」**。另外 `test_PinyinBackend_ServesBothLanguagesAtOnce`（`:78`）与 `test_PinyinBackend_BuildsEnginesConcurrently`（`:99`）在 v2 下仍然该过，可作为迁移后的回归网。

### 3.5 lite 侧改动点

| 文件 | 改动 | 不做的后果 |
| :-- | :-- | :-- |
| `src/app/CMakeLists.txt:24` | `find_package(cpp-pinyin2 CONFIG REQUIRED)`（共存期两者都留） | 拿不到 target |
| `cmake/LiteBuildApi.cmake:243` | 增加拷贝 `$<TARGET_FILE:cpp-pinyin2::cpp-pinyin2>` | **构建通过、运行期插件加载失败**（缺 DLL） |
| `scripts/vcpkg-manifest/vcpkg.json:23` | 增加 `cpp-pinyin2`（或替换） | 装不上包 |

**lite 自己不链接 cpp-pinyin**（全仓只有这两处引用），所以业务代码零改动。

**Debug 后缀不用特殊处理**：v2 在未定义时设 `CMAKE_DEBUG_POSTFIX "d"`（`cpp-pinyin/CMakeLists.txt:139-141`），Debug 产物是 `cpp-pinyin2d.dll`。lite 的拷贝规则用 `$<TARGET_FILE:…>` 生成器表达式，会按配置取到正确那个文件，不需要为两个名字各写一条。

### 3.5b 改写骨架（示意，**尚未落地**，等你确认后才动代码）

```cpp
// 进程级注册表：Registry 不可拷贝不可移动，且必须比所有 Engine 活得久
static cpp_pinyin::v2::Registry &registry() {
    static cpp_pinyin::v2::Registry instance;   // 构造即注册 mandarin / cantonese
    return instance;
}

// 每个方言一个引擎，构造点从「隐式单例」变成显式 open
auto opened = cpp_pinyin::v2::Engine::open(registry(), dictRoot, "mandarin");  // 或 "cantonese"
if (!opened) return /* 用 opened.error().code / .detail 组装诊断 */;

// 转换：EngineConfig 只改 style，其余保持默认（includeCandidates 默认 true）
cpp_pinyin::v2::EngineConfig cfg;
cfg.style = cpp_pinyin::v2::MandarinStyle::NORMAL;   // 粤语用 CantoneseStyle::NORMAL
auto queried = engine->query(characters, cfg);       // 行数与输入一一对应
if (!queried) {
    // 非 BMP 字符会让整批失败（Engine.cpp:214-218）：这里必须退化成「逐字符标记无读音」，
    // 不能把错误抛给 G2P 层，否则一个生僻字毁掉整段歌词
}
for (const auto &row : *queried) {
    CharacterResult r;
    if (row.failure_kind == cpp_pinyin::v2::Code::Ok) {
        r.pronunciation = row.rendered;
        r.candidates = row.candidates;               // 失败行这里是 {text}，不能当读音
    }
    results.push_back(std::move(r));
}
```

**删掉的部分**：`RootState`、`RootReservation`、`reserveRoot`、`Configuration::reservation`、`published` 标志，以及 `PinyinEngines.cpp:174-177` 那条针对 v1「缺目录会死循环」的防御注释（v2 用 `Code::Load` 取代）。

**顺带解除的一条限制**：v1 的 arbiter 强制「一个进程只能有一个词典根」（`PinyinEngines.cpp:164-169` 的 `FeatureNotSupported`），v2 的 `Engine::open` 每个引擎自带 root，**两个不同 root 的包可以共存**。这是净收益，但也意味着原来那条错误路径从此不可达——测试里若有针对它的用例要一起删。

### 3.6 setdir 需要什么（只描述，不改文件）

1. 新建 **`scripts/vcpkg-ports/cpp-pinyin2/`**（lite 自己的 overlay，**不是**子模块那份），从 `D:\projects\cpp-pinyin\packaging\vcpkg\ports\cpp-pinyin2\` 复制 `vcpkg.json` / `portfile.cmake` / `usage`，把其中 `vcpkg_from_github(...)`（含 `SHA512 0` 占位）整段换成 LOCAL-ONLY 块，形态照 `scripts/vcpkg-ports/wolf/portfile.cmake:17-28`。
   **为什么放这里**：`scripts/vcpkg-manifest/vcpkg.json:97-102` 的 `overlay-ports` 顺序是 `../vcpkg-ports` → `../vcpkg-project/ports` → `../vcpkg/ports`，lite 自己的 overlay **最先命中**。`scripts/vcpkg-ports` 是 lite 内的普通目录（git 跟踪），`scripts/vcpkg` 才是子模块。
2. LOCAL-ONLY 块**必须保留 `data/`**（安装规则从 `data/dialects/` 装词典），并排除 `.git`、`cmake-build-*`、`.tmp*`、`conformance`、`CMakeCache.txt`。
3. wolf 端口 `vcpkg.json` 增加 `cpp-pinyin2` 依赖。
4. **两份 ports 树的分工**：已存在的 `cpp-pinyin`（v1）只在子模块 `scripts/vcpkg/ports/` 里，所以它生效。**新建的 `cpp-pinyin2` 要放在 lite 自己的 `scripts/vcpkg-ports/`**，因为它在搜索顺序里更靠前（`vcpkg.json:97-102`）。`scripts/vcpkg` 是 git 子模块（`.gitmodules` 的 `path = scripts/vcpkg`），`scripts/vcpkg-ports` 是普通跟踪目录——两处的 LOCAL-ONLY 块都**绝不能提交**。
5. 顺带发现：`scripts/vcpkg/ports/synthrt/vcpkg.json:25` 声明了 `cpp-pinyin` 依赖，但 synthrt 源码 0 引用——疑似陈旧依赖，替换前值得问一下 synthrt 侧。

### 3.6b 端口草稿（**文本已备好，等你确认后才落盘到 `scripts/vcpkg-ports/cpp-pinyin2/`**）

`vcpkg.json` 照抄 `D:\projects\cpp-pinyin\packaging\vcpkg\ports\cpp-pinyin2\vcpkg.json`（版本 `2.0.0`）。`portfile.cmake` 用下面这份**替换**原骨架的 `vcpkg_from_github` 段：

```cmake
# cpp-pinyin2 -- the 2.x line of cpp-pinyin, installed beside the frozen 1.0.2 port.
#
# LOCAL-ONLY (DO NOT COMMIT): build the working tree at D:/projects/cpp-pinyin (branch v2) instead of
# the not-yet-released tag. Delete this block and restore the upstream vcpkg_from_github(...) block to
# return to the deliverable state.
#
# The exclusion list is measured, not guessed: the checkout is 0.84 GB, of which .tmp is 768 MB,
# csharp 48 MB, tests 18 MB. Only src/, include/, data/, cmake/ and the top-level CMake files are
# needed to build the library and its dictionaries.
file(REMOVE_RECURSE "${SOURCE_PATH}")
file(COPY "D:/projects/cpp-pinyin/" DESTINATION "${SOURCE_PATH}"
    PATTERN ".git" EXCLUDE
    PATTERN ".idea" EXCLUDE
    PATTERN ".dsh" EXCLUDE
    PATTERN ".tmp" EXCLUDE
    PATTERN ".tmp-firefly-disk" EXCLUDE
    PATTERN "cmake-build-*" EXCLUDE
    PATTERN "csharp" EXCLUDE
    PATTERN "python" EXCLUDE
    PATTERN "tests" EXCLUDE
    PATTERN "tools" EXCLUDE
    PATTERN "vendor" EXCLUDE
    PATTERN "conformance" EXCLUDE
    PATTERN "CMakeCache.txt" EXCLUDE
    PATTERN "CMakeFiles" EXCLUDE
)

# The rest of the portfile (vcpkg_cmake_configure / vcpkg_cmake_install / config fixup / copyright)
# is kept as upstream wrote it. CPP_PINYIN2_BUILD_TESTS=OFF makes tests/ and vendor/catch2 unused.
```

**必须保留 `data/`**（v2 的安装规则从 `data/dialects/` 装词典，`CMakeLists.txt:598-602`）。**必须保留顶层 `CMakeLists.txt`、`cmake/`、`src/`、`include/`**。

**数据兼容性已核**：v2 的 `checkDictManifest`（`src/dict/DictManifest.cpp:70-78`）对「既无 `format.txt` 也无 `dialect.json`」的目录**直接放行**，所以 wolf 语言包里那份 v1 时代的词典目录**能加载**，不用换数据。

---

## 4. 决策台账（需你拍板）

| # | 决策点 | 选项与取舍 | 推荐 |
| :-- | :-- | :-- | :-- |
| D-1 | v2 何时切（**已定：a**） | **(a) 现在就切本地 v2**，接受它「未打 tag、仍在收敛」／(b) 等 cpp-pinyin 打 2.0.0 tag 再切 | **(a)**：本目标就是「预备更换」，本地联动正合适，出问题也只影响 setdir 构建 |
| D-2 | 端口形态（**已定：a**） | **(a) 新建 `cpp-pinyin2` 端口与 v1 并存**／(b) 直接把 `cpp-pinyin` 端口升到 v2 | **(a)**：共存是 v2 自己的设计目标，回滚也容易 |
| D-3 | wolf 端口依赖 | **(a) 迁移完成后把 wolf 的依赖改为 `cpp-pinyin2`**／(b) 暂时两者都留 | **(a)**，但**分两笔**：先加新端口跑通，再删旧依赖 |
| D-4 | lite 的 DLL 拷贝 | **(a) 改成拷贝 v2，同时保留 v1 拷贝直到确认无其他使用者**／(b) 直接换成 v2 | **(a)**，因为插件 DLL 依赖缺失只在运行期暴露 |
| D-5 | synthrt 是否也切本地 | **(a) 维持 pin**（迁移不碰 synthrt）／(b) 也加 LOCAL-ONLY 块 | **(a)**，除非迁移中发现需要改 synthrt 的契约头 |
| D-6 | 本轮发现的 wolf/otter 缺陷是否顺手修（**已定：a**） | **(a) 只修 W-1/W-2/W-3 三条 P2 与 O-1，其余记录**／(b) 全修／(c) 都不修，先做迁移 | **(a)**：W-1/W-2 是十行级改动且与迁移同文件，O-1 需要你定「锁死 1 声道」还是「实现下混」 |
| D-7 | 非 BMP 字符怎么处理（§3.2 地雷，**已定：a**） | **(a) wolf 侧挑出非 BMP 字符记为无读音，其余照常送**／(b) 按「能转的一段」分段多次调用／(c) 推动 cpp-pinyin v2 放宽为「元素 ≤2 码元即可」 | **(a)**：不动上游、行为最可预测；(c) 需要 v2 侧合入，周期长 |
| D-8 | synthrt 端口那份陈旧的 `cpp-pinyin` 依赖 | **(a) 顺手清掉**／(b) 留着不动 | **(b)**：不在本目标范围，且清了要重建 synthrt 端口，风险大于收益 |
| D-9 | 是否同时把语言包里的词典换成 v2 自带那份（§3.3 的数据代差） | **(a) 这次只换引擎、词典不动**，用 v1 数据跑 v2 引擎／(b) 连词典一起换，读数会变、需要单独一轮审查 | **(a)**：换引擎已经是一次行为变更，同时换数据会让「变了是因为哪一步」无法归因 |

---

## 5. 验证计划（切 v2 后怎么证明没坏）

1. **构建**：`cmake --preset debug` + 构建（需你许可执行）。
2. **插件面**：确认 `wolfpinyin.dll` 与 `cpp-pinyin2.dll`（Debug 是 `cpp-pinyin2d.dll`）都在输出目录，且 `plugin.json` 能被发现（三元组 `org.openvpi.wolf.inference.G2P/1/algo-pinyin`）。
3. **转换等价性**：取同一批歌词，对比 v1 构建与 v2 构建的 G2P 输出（逐字音素串与候选表）。这是**行为等价验证**而非数值近似——两者应当逐字相同，不同则逐个定位（tone 处理、v→u、儿化、多音字候选顺序）。
4. **边界用例**：含 CJK 扩展 B 字（𠮷）、emoji、空串、标点、多音字短语各一条，确认逐行成败行为与 v1 一致。
5. **回归**：跑 wolf 自己的 G2P 测试与 lite 的语言相关测试。
6. **GUI 验收**：由你亲手跑新构建产物。

---

## 6. 对抗复核记录（第 4 轮）

一次独立复核，6 条断言逐条查反例，结果：**2 条完全成立**（W-9、O-6）、**2 条降级**（W-10、W-11，加载期还有一层校验使它们只是防御性缺口）、**1 条撤销**（O-7，`srt::ITask::~ITask()` 自己就 `waitForAsyncExecution`）、**1 条确认**（W-8 的推翻正确，LuaJIT 的 hook 挂在 global_State，`lj_obj.h:634-655` 的字段注释直说「shared by all threads of a Lua universe」）。

**教训**：O-7 是我读代码时把「头文件列义务」当成「实现没做」——`AnalysisTask.h:24-26` 原文明写「srt::ITask already provides … the wait」，我自己在 W-10 的注释里还引用过同一段，却漏了这句。复核员一次就看出来。

## 7. 未证实 / 未覆盖

- v1 与 v2 的**读音级逐字对比**未做（禁构建）——这是迁移合流前唯一必须补的验证。
- `cpp-pinyin2` 端口在两份 ports 树下的命中优先级无先例（只实测了 `cpp-pinyin` 的归属）。
- v2 在本机 MSVC + vcpkg x64-windows 的完整构建未验证。
- otter 的 `AnalysisTask.cpp` 回调内销毁路径（悬垂嫌疑）。
- otter 的 `.dll` 安装目标目录未实测。
- tifa/hfa/game 约 3300 行数值逻辑未逐行审。
- 本轮**未构建、未运行任何测试**，W-3 的「静默漏编」与 D-4 的「运行期缺 DLL」都属**推断**，需一次实际构建验证。

---

## 8. 实施记录（第 5 轮，已按你选定的 D-1/D-2/D-7/D-6 落地代码）

**决策**：D-1 现在切本地 v2／D-2 新建端口并存／D-7 wolf 侧挑出非 BMP 字符记为无读音／D-6 只修 W-1/W-2/W-3。

| # | 文件 | 改动 |
| :-- | :-- | :-- |
| 1 | `scripts/vcpkg-ports/cpp-pinyin2/{vcpkg.json,portfile.cmake,usage}`（新） | 新端口。`vcpkg_from_github` 整段换成 LOCAL-ONLY 拷贝块（排除 `.tmp`/`csharp`/`tests`/`vendor` 等，保留 `data/`），交付态形态注释保留在文件里 |
| 2 | `scripts/vcpkg-manifest/vcpkg.json` | 依赖加 `cpp-pinyin2`（v1 保留） |
| 3 | `scripts/vcpkg-ports/wolf/vcpkg.json` | 依赖加 `cpp-pinyin2`；**第 7 轮验证通过后已删 `cpp-pinyin`**（wolf 不再链接 v1，重装 33 秒通过） |
| 4 | `src/app/CMakeLists.txt` | 加 `find_package(cpp-pinyin2 CONFIG REQUIRED)` |
| 5 | `cmake/LiteBuildApi.cmake` | Windows 分支加一条拷贝 `$<TARGET_FILE:cpp-pinyin2::cpp-pinyin2>`，v1 那条保留 |
| 6 | wolf `pinyin/CMakeLists.txt` | 换 `cpp-pinyin2` 包与 target |
| 7 | wolf `pinyin/PinyinEngines.{h,cpp}` | 重写：删 arbiter（`RootState`/`RootReservation`/`reserveRoot`），改用 `cpp_pinyin::v2::Engine::open`，`query` + `Segment` 映射，非 BMP 字符单独挑出记为无读音，批级失败退化为「全部未转换」交给链兜底 |
| 8 | wolf `pinyin/main.cpp` | 删 `Configuration::reservation` 与 `reserveRoot` 调用，`createEngine` 改静态调用，`createInference` 加空配置守卫，`create()` 的拒绝消息带三元组实值 |
| 9 | wolf 其余 5 个插件的 `main.cpp` | 同上：`createInference` 空配置守卫（chain/multig2p/s2p/onset/lua×2）、`create()` 消息带三元组（chain/onset/s2p/pinyin/multig2p/linguist/singerstub） |
| 10 | wolf `src/plugins/CMakeLists.txt` | `file(GLOB_RECURSE … CONFIGURE_DEPENDS …)`（W-3） |
| 11 | wolf `tests/auto/Inference/test_PinyinBackend.cpp` | 「第二次 root 被拒」改成「两个 root 都能加载」，删掉「卸载归还 root」，rollback 用例改述但仍保留 |

**构建结果（第 6 轮，已获许可并执行）**：

| 阶段 | 命令 | 结果 |
| :-- | :-- | :-- |
| 装端口 | `vcpkg install --x-manifest-root=../scripts/vcpkg-manifest …` | `cpp-pinyin2:x64-windows@2.0.0` 建成（11 s），`wolf` 随之重建（45 s），总 45 s，退出码 0 |
| 插件真身 | `dumpbin /dependents wolfpinyin.dll` | 依赖从 `cpp-pinyin.dll` 变为 **`cpp-pinyin2d.dll`**，`synthrt`/`stdcorelib` 不变 |
| lite 配置+构建 | `cmake --preset debug` + `cmake --build --preset debug` | 退出码 0，`DsEditorLite.exe` 链接成功，`out/bin` 里同时有 `cpp-pinyind.dll` 与 `cpp-pinyin2d.dll` |
| lite 测试 | `ctest -j 8 -E Gui` | **23/23 通过**（含 `TestApplicationWorkflows` 81 s、`TestProcessIntegration` 51 s 这类走完整合成流程的用例） |

**一处要澄清的假失败**：单独跑 `TestSynthrtEngine.exe` 与 `TestLanguageBridge.exe` 退出码 `-1073741511`（`STATUS_ENTRYPOINT_NOT_FOUND`）。查 mtime，这两个 exe 是 **2026/9/22** 的旧产物，既没被本轮构建重建、也不在 ctest 注册表里，报错与本次改动无关，属既有的陈旧二进制。

**读音等价性已验（第 7 轮）**：给 wolf 另配一个消费 lite 依赖树的构建目录（`D:\projects\wolf\build\lite-deps`，`VCPKG_INSTALLED_DIR` 指 lite 的 `vcpkg/installed`，`VCPKG_MANIFEST_MODE=OFF`，`CMAKE_PREFIX_PATH` 再挂上 wolf 自己那棵带 Boost 的树），`test_PinyinBackend` 的 **9 个用例全过**（短语上下文、候选、链兜底、输入顺序、双语言并发、双词典根共存、失败加载不污染后续加载）。随后跑**整套 wolf 测试**（`ctest -C Release -j 8`）：**20/20 通过**，2 个跳过（`test_LinguistSession`、`test_HostFlow` 要求真实声库夹具即 `WOLF_VOICEBANK_FIXTURE_SOURCE`，与本迁移无关）。lite 侧同期 `ctest -j 8 -E Gui` **23/23 通过**。

**期间发现一个真实的行为差异（已修并记录）**：v2 **拒绝空词典表**，v1 会容忍空文件并静默转换不出东西。测试夹具 `pinyin-engine`/`pinyin-engine-rival`/`pinyin-rollback` 的粤语三张表原本是空文件，于是 v2 直接报 `failed to load: phrases_map.txt, phrases_dict.txt, trans_word.txt`，连带 2 个用例失败。处置：改 `scripts/make-test-fixtures.py`，给三处夹具的粤语表填最小内容（`user_dict.txt` 留空仍可，v2 不要求它）。**线上包不受影响**（`wolf-g2p-pinyin` 的词典是完整的），但这条差异要记进发布说明：手写的残缺词典包在 v2 下是加载失败，不是静默无声。

**踩坑记录**：`D:\projects\wolf\build\cmake`（wolf 原来那棵树）不能直接用，它指向自己的 `build/vcpkg_installed`，里面没有 `cpp-pinyin2`，而 lite 的树里没有 Boost（auto 测试要 Boost.Test）。解法是**两棵安装树都挂**：`VCPKG_INSTALLED_DIR` 指向 lite 的树拿到 cpp-pinyin2，`CMAKE_PREFIX_PATH` 追加 `D:/projects/wolf/build/vcpkg_installed/x64-windows` 拿到 Boost。跑测试时还要把两棵安装树的 `bin` 加进 `PATH`，否则 `test_PinyinBackend.exe` 直接 `0xC0000135`（缺 DLL）。

**第 6 轮静态清扫（不改代码，只找漏改的地方）**：

| 位置 | 现状 | 处置 |
| :-- | :-- | :-- |
| `wolf/scripts/convert-g2p-packages.py:751` 的用法提示写「vcpkg: share/cpp-pinyin/dict」 | 该脚本靠 `--cpp-pinyin-dict` 传路径，不是硬编码 | **不改**：按 D-9 词典不动，继续指向 v1 的 `share/cpp-pinyin/dict`（v1 端口按 D-2 保留） |
| `wolf/docs/linguist-distribution.md:73-74` 写「cpp-pinyin `Pinyin`/`Jyutping` 引擎」 | 引擎类名在 v2 已不存在 | **待改**：这是设计台账，改措辞要连上下文一起核，留给你评审时一并处理 |
| `wolf/docs/linguist-distribution.md:221-222` 写「一个进程只能有一个词典根」 | v2 下已不成立 | 同上 |
| `lite/.agents/skills/cantonese-song-synth/SKILL.md` 已经用 `cpp-pinyin` v2 的 Python 包 | 与本次改为 C++ 链接无关，但说明 v2 在本机已被实际使用 | 不改 |

**结论**：插件源码与 wolf 的 CMake target 引用里已无 v1 残留（`RootReservation`／`reserveRoot` 0 命中，`cpp-pinyin::` 0 命中）。**仍有 v1 措辞的地方是文档与注释**，包括 lite 的 `cmake/LiteBuildApi.cmake:603`（讲 Linux RPATH 时举 `cpp-pinyin` 为例）与上表三处，均不影响构建。