# spec 2.4 四仓联合调研：格式设计缺陷与已发布包一致性

> 工具清理说明：本文引用的声库转换、下载审计、G2P 测量与 ZIP 工具及其专属测试已移除。
> 相关记录与行号仅适用于[移除前快照](https://github.com/flutydeer/ds-editor-lite/tree/10c1f19827239cb2072d1e5616ae5232be589728)。
> 当前运行时回归使用仓库内已升级为 spec 2.4 的 `voicebank-fixture.zip`，不依赖转换器。

> 状态：**调研已完成（第 7 轮收尾）**。七轮追加式记录，旧轮结论不删只更正，被推翻的断言保留原文并标注。**后续实施需先裁定 §5 的 D-1~D-12。**
>
> **快照**（第 1 轮取证于 2026-10-19，第 2 轮增补同日）：ds-editor-lite `c2ed02e`（工作区另有未提交改动，见 §3.4）、synthrt `0e97662`、wolf `38ad5b0`、otter `6db876b`（基分支 `origin/analysis-level-1`，该仓无 `origin/main`）。
> **已发布件**：wolf 语言包 `lang-v0.1.2.0`（15 归档，与安装树逐字节一致——前序审计 §2.1 的实测结论，本轮未重跑）、otter 模型包 `models-v0.1`（文档记 4 包；本机有历史批次 `otter-v0.2.0.0` 的 3 包，以及第 3 轮找到的**本地重组版** `build/release-models-v0.1` 4 包，远端原物未核）。
>
> **方法**：3 个只读子代理按互不重叠的范围扇出（synthrt 格式面、wolf linguist 格式面、otter 分支与契约面），**全部断言由主代理逐条回读行号抽验**，再开对抗复核轮找反例（§6）。取证纪律见 §7。**本轮未改动任何仓的代码，未构建，未运行产品**（例外：只读文件与 `git` 只读命令）。
>
> **前序报告（本文不重复其结论，只引用）**：[wolf 语言就绪对抗复核](./wolf-language-readiness-adversarial-review.md)（加载期合规 F1~F5/V1~V11）、[迁移台账](./synthrt-main-migration.md)（`MG-*`/`IP-*`/`F-*` 唯一编号）、[yousa 2.4 合规](../synthrt/conformance-yousa-2.4.md)（声库侧静态+语义+真加载）。原「wolf 发布包审计」报告（15 个语言包的声明/数据/打包，14 条缺陷）已随 cpp-pinyin v2 迁移落地而退役删除，其结论以各包 `desc.json` 与 `scripts/vcpkg-ports/wolf-lang-packages/assets.cmake` 为准。

---

## 执行摘要（一页，给不想读全文的人）

**做了什么**：对 lite / synthrt / wolf / otter 四仓的 spec 2.4 线做只读联合调研，覆盖**声明与清单格式面**、**已发布件**（wolf 15 个语言包、otter 4 个模型包、yousa 声库）、**规范正文自身**。四个只读审计/扫描子代理加五个对抗复核子代理（其中两个超时被中止后按窄范围重派），全部断言由主代理回读行号抽验，被推翻与被降级的结论都保留在文内。

**三条最重要的结论**：

1. **引擎与装载实现是干净的，问题在「格式契约的载体」。** 装载器、版本区间、provider 选择、路径与变量展开都做对了（§2.2 末尾的「做对」表）。缺陷集中在同一件事的三种落法：**有 schema 的走 schema，没 schema 的只剩十几行文档加一个 C++ 成员**。结果是发布侧写不出（F-1）、写错无人查（F-2/F-3）、消费侧各自解释（F-10/F-12）。

2. **已发布件的格式是自洽的，真正的格式缺陷在「身份」与「元数据」两处。** wolf 与 otter 的归档摘要、成员路径、贡献声明全部对得上（otter 4/4 归档 SHA512 与 size 精确匹配、0 条断链）。真问题：**otter 把已发布过的 `0.1.0.0` 回落到新布局的包上**（F-21，与 spec `:181`/`:198` 冲突，条件性成立），以及 **manifest 的逐文件摘要用裸文件名做 key**（F-9，tifa 有 4/11 定位不到，且根文件与子目录同名时会坍缩）。

3. **规范正文自己有 12 处不一致**（`S-*`），其中 3 条真不一致、3 条缺判定。最危险的是 **S-4**：未配对的 `${` 被规范判为「未定义」，而实现**静默把整个字符串清空**，拼错与空值不可区分。

**若要动手，先做这四件（成本最低、直接消除规范违背）**：D-3（未知字段宽容边界一次定清）→ D-4（版本比较改按段字符串比较）→ D-8(a)（otter lint 两处，十行内）→ D-1（otter 版本约束改文档）。完整台账见 §5 的 D-1~D-12。

**还没做的**：otter 远端原物核对（需联网）、两处门禁实跑（需授权）、dspkg 生产者交叉对比（该工具仍是 2.3，另行登记）。

---

## 0. 结论（第 7 轮结束时的状态）

**一句话**：四仓的**引擎与装载实现**经得起审，缺陷集中在**「格式契约的载体」缺失**——同一个格式概念在两边的落点不同（有 schema 的走 schema，没 schema 的只有十几行文档加一个 C++ 成员），于是**发布侧写不出、写错无人查、消费侧各自解释**。

**对抗复核后的定级（第 2 轮结束时的状态）**：第 1 轮的 20 条里，**1 条被推翻（F-11）**、**4 条被修正定级或范围（F-1 P1→P2、F-5 P0→P2、F-7 改为 lint 覆盖、F-9 P2→P3）**，另有 F-14 的前提被证实。**第 3 轮新增 1 条（F-21），另给 F-9 补了实测到的格式缺陷。**

**当前总账：`F-*` 21 条（0 条 P0、4 条 P1、14 条 P2、3 条 P3）；另有 `S-*` 12 条规范正文内部不一致（单列 §2.3，不计入缺陷数）。**「0 条 P0」是复核的直接结果，第 1 轮唯一的 P0 经查是 CMake 官方模板的既定语义，不是本仓缺陷。

**发布件与消费面（§2.1）**：

| # | 级 | 仓 | 一句话 |
| :-- | :-- | :-- | :-- |
| F-1 | P2 | wolf | `defaultLyric` 只有 zxx 引导包（夹具生成器）能写，**转换链的 4 个自带 linguist 语言包写不出**，而契约文档正把后者点名为承载者 |
| F-2 | P2 | wolf | 类别追加的**根字段没有任何机器可读定义**，打包 lint 对它完全失明（`"defaultLyric": 7` 零报告通过） |
| F-3 | P2 | wolf | lint 对未收录 `(interface, level)` 只对 linguist / inference 告警，**singer 类别拼错 interface 时静默通过** |
| F-4 | P2 | wolf | `defaultLyric` 的容错语义**文档与代码三处不一致**（空串不告警、`null` 静默、全角空白不算空白） |
| F-5 | P2 | otter | 「1.0 前不保证 ABI，宿主不得混用版本」的承诺**无法用标准 CMake 版本文件兑现**（模板固定忽略第四段），要收严只能自写版本文件 |
| F-6 | P1 | otter | 发布前 lint 的 `level` 只判 `!= 1`（`true` 与 `1.0` 都通过），且 2.3 的**裸字符串 ref 静默放行** |
| F-7 | P2 | otter | 声明 lint 对 `imports[].role` 的必选/文法/唯一**零覆盖**（加载期由 synthrt 强制，属 lint 漏项而非规范失效） |
| F-8 | P2 | otter | lint 不覆盖 manifest 信封字段（`$version`/`runtimeLevel`/`dependencies`）与 `plugin.json` 三元组 |
| F-9 | P3 | otter | 发布 manifest 的逐文件 SHA512 **无人消费**（取包脚本只验归档摘要，冗余元数据而非漏洞） |
| F-10 | P2 | lite | 语言模块下转型 `linguist->as<wolf::LinguistSpec>()` **没有 interface/level 守卫**，与本文件自己的约定相反（wolf 审计 §3.2 的消费侧孪生） |
| ~~F-11~~ | ~~P3~~ | ~~跨仓~~ | ~~两份发布件归档布局不一致~~ —— **已撤回，见 §3.5**：wolf 的归档同样是单层包裹，与 otter 同形态 |
| F-12 | P3 | spec | `desc.json` 的字段表**没有 `name`**，但《多语言文本》把它列为标准字段，现实包（yousa 等）在用 |
| F-21 | P2 | otter | 同一 `(id, version)` 在两批发布件里指向**不同内容**（`analysis.*` 旧布局 vs `inference.*` 新布局，版本都是 `0.1.0.0`） |
| F-22 | P3 | 跨仓 | 2.4 归档布局三方不一致（lite 扁平、wolf 与 otter 带包裹），而 lite 的交接文档断言「只有扁平才是 2.4 约定」 |

**synthrt 清单/声明格式面（§2.2，第 2 批并入）**：

| # | 级 | 一句话 |
| :-- | :-- | :-- |
| F-13 | P1 | `dependencies[]` 条目**拒绝未知字段**，违背 §74「未知字段不得导致拒绝」 |
| F-14 | P1 | 版本分量受 `std::array<int, 4>` 限制，**超出 INT_MAX 的合法版本被拒**，违背 §177 的任意精度要求 |
| F-15 | P1 | JSON profile 的重复 key 预扫描**没有深度上限**（解析器有 512），深嵌套声明先爆栈而不是被可控拒绝 |
| F-16 | P2 | 内置类别在 **contribution entry** 上拒绝未知字段，与同一文件对 declaration 的宽容策略自相矛盾 |
| F-17 | P2 | 缺解释器的报错**不含三元组与模块定位**，未兑现 §564「说清楚缺的是哪一个」 |
| F-18 | P2 | synthrt 仓内**没有任何契约 JSON Schema**（全仓 0 命中），§598-599 要求的材料没产出 |
| F-19 | P2 | 路径与多语言路径的解析助手在匿名命名空间，第三方类别只能复制一份（`SingerContrib` 已复制） |
| F-20 | P2 | 模块声明出现 `$version` 直接拒绝，属规范留白（§508 与 §163 的措辞不一致） |

**未成立、已撤回或属有意设计的候选**见 §3，其中两条是常见误报（「`defaultLyric` 无人供给即缺陷」「两个规范副本漂移」），后续轮次不要再排。

**未覆盖**（下一轮起点）：otter **远端**发布件原物（本机只找到本地重组版）、wolf 已发布件的**运行时**行为（继续只看格式面）、dspkg 生产者的格式交叉对比。详见 §4。

**第 7 轮增量（同日，收尾轮）**：

- **全文引用新鲜度复核**：约 220 处 `文件:行号` 逐条回读，**217 一致、0 缺失、3 处需处理且已改正**（明细见 §7）。其中 **S-10 的断言被推翻**：我原写「代码里没有 `Executive` 类型」，实际 `Core/ContribExecutive.h`、`SVS/InferenceExecutive.h`、`SVS/SingerPipelineExecutive.h` 全在，已改述为「规范没写清 Executive 与 Interpreter 的分工」。
- **F-10 加强**：`:459` 现在写成了 `spec != nullptr` 的守卫形式，但 `as<>()` 就是 `static_cast`，**对合法对象永远非空**——这个守卫是空转，interface 与 level 仍然没比。
- **文首加了一页执行摘要**（三条最重要的结论 + 建议先做的四件事），并补上缺失的 `### 2.1` 小节标题（正文有 `§2.1` 引用但此前没有对应标题）。

**第 6 轮增量（同日，无新增缺陷）**：

- **交付 §2.4「规范留白 → synthrt 实际取哪边」对照表**：12 条 `S-*` 里 10 条能在代码里找到现行取向。最值得记的两条：**S-4**（未配对 `${` 被**静默清空成空串**，不报错）与 **S-7**（`languages` 缺省但写了 `defaultLanguage` 时**拒绝**）。
- **S-4 的性质**：规范自己写「加载行为未定义」，实现选择了确定性但危险的读法——拼错变量与变量为空在现象上不可区分，错误推迟到使用路径时才以「文件不存在」暴露。已并入 D-12 的 (a) 项。
- **`Executive` 术语**（S-10）**第 7 轮更正**：我原写「代码里没有这个类型」，实为错——`Core/ContribExecutive.h`、`SVS/InferenceExecutive.h`、`SVS/SingerPipelineExecutive.h` 都在。真实问题是规范没定义 `Executive` 与 `ContribInterpreter`/`InferenceInterpreter` 的分工。

**第 5 轮增量（同日）**：

- **新增 F-22（P3）**：2.4 归档布局三方不一致——lite 的打包工具产出**扁平**，wolf 与 otter 的 2.4 发布件都**带一层包裹**，而 lite 的打包脚本 docstring（`:4-7`）断言「带包裹的是 2.3，不是 2.4 的约定」。规范两种都允许（`:355` 以 `desc.json` 所在目录定义 root）。问题在文档写了单边断言，会让第三方误判上游归档。
- **规范正文内部一致性扫描**：子代理逐类扫描 + 主代理抽验，得 **12 条 `S-*`**（单列为 §2.3，**不与 F 混算**），其中 **3 条真不一致**（S-1/S-2/S-3）、**3 条缺判定**（S-4/S-6/S-7）、**6 条措辞或交叉引用**（S-5/S-8~S-12）。示例、版本文法、变更表、表格对齐、provider 排序等**已查过且干净**，记在 §2.3 末尾防重排。
- **放过 dspkg**：`D:\projects\dspkg`（Jobsecond 的 OpenUtau→dsinfer 转换器）实测仍是 **2.3 格式**（`dsinfer/package.rs:7` 的 `contributes`、`:30` 的 `class`、`:38` 的 `arch`），与本目标四个仓的 2.4 线不同源，**不作本报告缺陷**，仅登记为「社区生产者尚未跟进 2.4」。

**第 4 轮增量（同日，无新增缺陷，产出两份可执行材料）**：

- **决策台账**（§5 已改写）：D-1~D-10，每条带推荐项与取舍。实施顺序建议：D-3（宽窄边界一次定清）→ D-4（版本比较）→ D-8(a) → D-1（文档）→ 其余。
- **契约材料落点分析**（新增 §9）：实测官方 6 个契约零 schema、wolf 10 份、otter 6 份；给出三个方案并推荐「synthrt 补 12 份 JSON Schema + 三方一致性测试」。关键事实是**倒挂**——第三方类别都按规范交了 schema，官方接口一个都没交，而发布出去的声库（yousa 实测）恰恰只用了官方接口加 wolf 的三类。**量化**：已发布件声明过的 13 个 interface 里，7 个有 schema（wolf 4 + otter 3），6 个没有（dsinfer 的 Acoustic / Duration / Pitch / Variance / Vocoder / singer.DiffSinger）。
- **载体核查**：`dsinfer/Api/**/1/*ApiL1.h` 里的 `DurationSchema` / `DurationConfiguration` 等类证明官方契约**有代码载体**（字段名、类型、默认值齐全），只是不是规范要的「第三方校验器可读材料」。这一条把 F-18 从「没有契约」精确成「载体不合规」。

**第 3 轮增量（同日）**：

- **新增 F-21（P2）**：otter 已发布过的 `0.1.0.0` 被回落到新布局的包上（`analysis.*` → `inference.*`），若按 `models-v0.1` 发布即违反 spec `:181` 与 `:198`。复核确认这次改动**有决策记录**（A26），真正的问题是「版本回落」与「归因到不含该决定的 A31」。
- **F-9 补实测**：manifest 的 `models` 表用**裸基名**做 key，tifa 的 4 本词典因此无法按路径定位（4/11 不对应），且 key 规则在「根文件与子目录同名」时会坍缩成一条。`REBUILD-REPORT.md:112-114` 的 4 行误报就是同一根因。
- **N-1 已解决大半**：本机找到 `build/release-models-v0.1`（4 包，自述未发布），据此做了逐成员验证——4/4 归档 SHA512 与 size 全对、贡献路径无断链、逐文件摘要 19/23 可复现。远端原物仍需联网核。

**第 2 轮增量（同日）**：

- **F-18 范围扩大**：从「synthrt 没有 schema」扩到「**synthrt/dsinfer 的官方接口全都没有 schema**，反而是第三方类别（wolf 10 份、otter 6 份）都发了」。
- **一条负结果**：本机 `D:\projects\otter\build\packages\` 是**混合的陈旧构建状态**（bundle `0.3.0.0`、tifa 声明 `0.2.0.0`、三个包仍用已废弃的 `org.openvpi.otter.analysis.*`），**不能当作发布件证据**，详见 §4。
- **复核轮重派**：首轮两名对抗复核员超时未回报，已中止，改为按更窄范围（各 4 条）重派，见 §6。

---

## 1. 覆盖矩阵（四仓 × 面 × 状态）

| 仓 | 面 | 状态 | 证据指针 |
| :-- | :-- | :-- | :-- |
| wolf | 发布包声明面、数据面、打包链 | **已覆盖**（前序报告已退役删除） | 结论以各包 `desc.json` 与 `scripts/vcpkg-ports/wolf-lang-packages/assets.cmake` 为准 |
| wolf | linguist 类别的声明格式与追加根字段 | **本轮新增** | 本文 F-1~F-4 |
| wolf | 加载期合规（multig2p/ORT/覆盖率） | **已覆盖**（前序） | `wolf-language-readiness-adversarial-review.md` §10 |
| synthrt | 清单/声明格式面（未知字段、`$version`、类别追加字段、路径、变量、JSON profile） | **已覆盖（第 2 批）** | 本文 F-13~F-20、§2.2 的「做对」表 |
| synthrt | 加载器版本区间、provider 选择、`as<T>()` 语义 | **已覆盖**（前序） | wolf 审计 §2 抽验表 |
| otter | 分支状态、契约格式面、CI 阻塞 | **本轮新增** | 本文 F-5~F-9 |
| otter | 已发布模型包格式 | **第 3 轮完成大半**（本地重组版逐成员验证；远端原物未核） | 本文 F-9、F-21、§3.6、§3.7 |
| lite | 消费面格式处理（声库目录、默认歌词、下转型、打包约定） | **本轮新增** | 本文 F-10、F-22、§3.3 |
| 四仓 | spec 双副本一致性 | **已覆盖**（前序，本轮复核计数一致） | `conformance-yousa-2.4.md` §4 |

---

## 2. 本轮发现

### 2.1 发布件与消费面（F-1~F-12）

### F-1 P2｜wolf：只有 zxx 引导包能声明 `defaultLyric`，转换链的 4 个语言包写不出（范围已按复核收窄）

**事实（逐条已回读）**：

- 已发布件与安装树（共 15 个包目录、5 份 `linguist.json`）中 `defaultLyric` **命中 0**（本轮逐文件实测）。原因是发布件早于特性提交 `38ad5b0`（2026-10-12），zxx 那份的 mtime 是 2026-09-21。
- 4 个「自带 linguist」的语言包由 `scripts/convert-g2p-packages.py` 生成，其 `close_language`（`:534` 起）**没有该参数**，根对象只写 `interface/level/variant/name/language/scheme/exports/configuration/imports`（`:575-590`）→ **转换链在 HEAD 重跑也写不出这个字段**。
- **但 zxx 引导包能写出**（这是复核推翻我原判的地方）：`scripts/make-test-fixtures.py:26-28` 自述「The package this repository ships, rather than a test fixture: the release script archives it and the port pins the hash of that archive」，其 zxx 表项 `:33` 已写 `"defaultLyric": " la "`（`:66` 另有 `"defaultLyric": 7` 的负例）。该生成物经 `make-lang-release.py:169-170` 的 `--extra`（help 原文「authored packages to include, such as wolf/lang-zxx」）并入发布源（`:186`）。实测 `build/test-fixtures/wolf-lang-zxx/.../linguist.json` 已带该字段。
- 而 wolf 自己的契约文档把承载者点名到**转换链那批**：`docs/linguist-domain-contract.md:185-186`「语言包自带 linguist 时（`eng`、`por`、`kor`、`ita` 属这一类）该值就是该语言的引擎默认层」。

**⇒ 收窄后的结论**：`defaultLyric` **不是**「发布链不可达」（下一版 zxx 会带），而是「**转换链不可达**」——被契约文档点名为承载者的 4 个语言包，生成器里没有注入点。zxx 与它们走两条不同的生产路径，能写与不能写的分界在于「手写夹具」还是「脚本转换」。

**影响**：消费方（lite）读到的永远是空值，只能走自己的内置缺省。这不构成「功能缺陷」，因为 lite 的取值链**有意不消费**它（见 §3.3）。格式面的事实是：契约文档指的承载者与生成器能力不匹配。

**最短修复方向**（未实施）：给 `close_language` 加参数并把值来源写清（值无法从词典推导，必须由作者或来源仓库给出），并按既有修订规则 bump `PACKAGING_REVISION`（现为 4，`:145`）。若判定「语言包不该声明引擎默认层」，则须改契约 §2.5 的举例。

**连锁后果（第 2 轮新增，与时序有关）**：zxx 的版本在夹具里**硬编码为 `1.0.0.0`**（`make-test-fixtures.py:30` 的 `"version"` 与 `"compatVersion"`），而夹具现在已经会产出带 `defaultLyric` 的内容。⇒ **下一版发布时，同一份归档名 `wolf-lang-zxx-1.0.0.0.zip` 下内容会变**，正是前序审计 §3.5 记的那类「同身份不同内容」。发布脚本的守卫会拦（`make-lang-release.py:266-279` 明确「The package version must be raised instead」并中止），但守卫给出的「抬版本」建议**在夹具路径上无法执行**（没有版本入口，只能改字面量）。**⇒ 这与前序审计的建议是同一件事：给 zxx 生成器加显式版本入口。** 不做的话，下一版发布会卡在守卫处。

### F-2 P2｜wolf：类别追加根字段没有机器可读定义，lint 失明

- `docs/schemas/` 共 10 份，全为 `*-exports`、`*-import-options`、`language-scheme`、`string-set`，**没有声明根 schema**（实测列举）。
- 打包闸门只校验 `exports`（`scripts/check-declarations.py:690-692` 一带）与 `imports[i].options`，身份只查 `language`/`scheme`。引入该字段的提交 `38ad5b0` 只动了 5 个文件（契约文档、头文件、夹具、实现、测试），**未触碰 schema、lint、decisions、architecture**。
- 后果：`"defaultLyric": 7` 这种值在**打包 lint 上零报告**（夹具里就摆着这个值，`:66`），只有加载期 warning。

**这是不是缺陷的分界（重要）**：规范 `ds-spec-2.4.md:596-599` 只要求每个 (`interface`, `level`) 提供 **`exports` 与 `imports[].options`** 两份 schema，`:74` 又规定未知根字段不得导致拒绝。**所以「根字段无 schema」本身不违反规范，是有意取舍。** 缺陷在于**取舍没被写下来**：同一族字段 `openSet` 有 schema、有 A60/A70 两条决策、有生成侧推导、有 lint 双向告警，`defaultLyric` 一条都没有。同一族两套流程，是本次审计最值得修的结构性问题。

**最短修复方向**：在契约 §2.5 或 `Status.md` 写明「类别追加根字段由加载器手写代码承担校验，不出 schema」，并在 lint 的身份检查旁补一条类型检查（十行内），让 lint 与加载器判定重新对齐。

### F-3 P2｜wolf：lint 对未收录三元组只对两类告警，singer 静默

- `check-declarations.py:678-686`：`CONTRACTS` 无命中时 `continue`，且 warning 只在 `if category in (LINGUIST_CATEGORY, INFERENCE_CATEGORY)` 时发出（`:682`）。
- 同处注释（`:679-681`）自述目的：「The declaration is reported instead of passing silently, so that a misspelt interface is detected before load.」——**这个目的对 `singer` 类别未达成**：interface 拼错时既不报错也不告警。

**判断**：singer 契约属 synthrt，wolf 不便发行其 schema，属有意留白（**推断**）。但「静默」与「告警」混在同一个 `continue` 分支，应显式区分。

### F-4 P2｜wolf：`defaultLyric` 容错语义三处文档/代码不一致

契约 `docs/linguist-domain-contract.md:192-193`：「非字符串、空串与纯空白一律视为未声明，**只报 warning** 并忽略……首尾空白会被裁掉。」
实现 `src/lib/Linguist/LinguistContrib.cpp`：

| # | 文档说 | 代码做 | 行号 |
| :-- | :-- | :-- | :-- |
| 1 | 空串/纯空白也告警 | 裁后为空直接返回 `{}`，**无日志** | `:126` |
| 2 | 非字符串（含 `null`）告警 | `null` 在 `:117` 静默返回，只有非 null 的非字符串才告警（`:120-124`） | `:117` |
| 3 | 纯空白视为未声明 | `isSpace` 只认 `' ' '\t' '\r' '\n'`（`:94-97`），**U+3000／NBSP 不算空白** | `:94-97` |

第 3 条对中日文包是现实风险（全角空格串会被当作有效歌词上报）。第 2 条的背景是规范 `:74`「`null` 只在对应 schema 明确允许时合法」，而本字段**没有 schema**，所以「显式 null 合法」没有依据。

**最短修复方向**：文档与代码取一。全角空白是否纳入 `isSpace` 需单独决策（涉及既有 `" la "` 之外的 CJK 值）。

### F-5 P2｜otter：「宿主不得混用版本」的承诺无法用标准 CMake 版本文件兑现（原判 P0，复核后降级）

- 承诺：`src/CMakeLists.txt:19-25`「otter guarantees no ABI stability before 1.0, so a host built against one version must not load another; this file therefore writes the CMake package with **ExactVersion**」，`README.md:80-82` 对外重复同一条。调用点确实传了 `COMPATIBILITY ExactVersion`（`src/CMakeLists.txt:39`）。
- 实现：生成物 `build/cmake/src/otterConfigVersion.cmake` 与**本机 CMake 3.30 的官方模板** `Modules/BasicConfigVersion-ExactVersion.cmake.in` 逐行同构——模板第 8 行原文就是「The tweak version component is **ignored**.」，`:21` 剥掉第四段，`:62-66` 只比前三段（主代理已回读该模板确认）。
- 后果（**按生成物逻辑推演，未实跑 cmake**）：请求 `0.1.0.0.1` 时 `PACKAGE_VERSION_COMPATIBLE=TRUE`。残余窗口比原判窄：**`PACKAGE_VERSION_EXACT` 仍按整串相等**（模板 `:68-70`），所以带 `EXACT` 的请求照旧被拒，只有「不带 EXACT 且只差第四段」的请求会被放行。
- **为什么从 P0 降到 P2（复核的贡献）**：这不是 otter 写错，而是 CMake 的既定语义。otter 能选的旋钮（`COMPATIBILITY`）已经选对了。**剩下的缺口是「约束表达不够严」**：要让「任何版本变更都必须重建宿主」真正成立，只能自写 `otterConfigVersion.cmake`，或在文档里承认第四段差异不被拦截。
- **与分支状态的关系（事实）**：工作区有一份**未提交**的检查改动（`M src/tests/auto/Analysis/check-installed-surface.cmake`，+89/−4），它把「匹配模板里的字面量 `ExactVersion`」改成「真跑一遍版本文件看 verdict」，脚本自述「this check **now fails, and it is right to**」（`:107-110`）。该检查经 ctest 注册（`src/tests/auto/Analysis/CMakeLists.txt:219-226`），**一提交会让三平台的 Test 步一起变红**（`FATAL_ERROR` 在 `:201-204`）。这是「脚本在正确地暴露问题」，不是脚本写错。

### F-6 P1｜otter：lint 的 `level` 类型宽松 + 2.3 裸 ref 静默放行

- `scripts/check-declarations.py:528`：`if level != 1:`。Python 语义下 JSON `"level": true` 与 `"level": 1.0` 都与 `1` 相等 → **两者都通过发布闸门**，而规范 `:147`、`:749` 要求正整数。同文件已有严格助手 `positive_int`（`:294-296`，显式排除 `bool`），此处未用。
- 同文件 `:502-503`：`if not isinstance(reference, str) or not reference.startswith(":"): continue` —— **2.3 的裸字符串 ref（`"acoustic"`）被静默跳过**，而规范 `:658` 明确 2.4 不再接受这种写法。一份 2.3 写法的声明在 lint 眼里零错误。

### F-7 P2｜otter：声明 lint 对 `imports[].role` 零覆盖（加载期由 synthrt 强制，原判 P1，复核后收窄）

- 事实：规范 `:629`（role 必选）、`:634`（模块内唯一）、`:636-642`（文法）都有硬要求。实测 `role` 在 `D:\projects\otter\scripts\check-declarations.py` 中**命中 0**，C++ 侧同样零引用，`check_imports`（`:485-509`）只查 `ref` 与 `options`。
- **修正（复核的贡献）**：这三条硬要求**不是无人执行**——synthrt 的加载器在加载期强制：`PackageLoader.cpp:38-50` 的 `isValidImportRole` 逐 segment 校验文法，`:1332-1336` 要求必选且是合法字符串，`:1369` 要求模块内唯一（主代理已回读确认）。**所以这是 lint 覆盖漏项，不是规范失效。**
- 漏项的证据反而更强：otter 的 lint 自述目的是「rejects every declaration that the loader or a shipped variant rejects at load」（`:4`），不查 role 与自述矛盾。**跨仓对照**：wolf 的同名脚本查 role（`wolf/scripts/check-declarations.py:325-341`：缺 `linguist/g2p` 报错、未知 role 报错），但也不查文法与唯一性。**两家都只部分覆盖，otter 连部分都没有。**
- 影响：2.3 风格的坏 role 能过 Otter 的打包门禁与 CI，到加载期才被 synthrt 拒。属「早发现 vs 晚发现」，不是「漏检」。负例矩阵里也没有 bad-role 用例。

### F-8 P2｜otter：manifest 信封字段与 `plugin.json` 不在 lint 覆盖内

- `runtimeLevel`、`$version`、`dependencies` 在 `check-declarations.py` 中**命中 0**（本轮 grep 实测），而规范 `:144`、`:163`、`:210`、`:656` 均为硬要求。生成侧确实都写了（`"$version": "1.0"`、`"runtimeLevel": 1`），但合法性无人校验，负例矩阵也没有 bad-runtime-level / bad-$version。
- `plugin.json` 与 lint 的变体表**无交叉校验**：`stub` 变体已在 `src/plugins/inferenceinterpreters/stub/plugin.json` 注册，但 `MODEL_KEYS`/`CONFIGURATION_KEYS` 里没有它，于是 stub 声明只会拿到 warn（`:540-542`）。

### F-9 P3｜otter：发布 manifest 的逐文件 SHA512 无人消费（已降级）

- `manifest.json` 每个包除归档 `sha512`/`size` 外，还带一张 `models` 表（逐文件 SHA512，实测本机历史批次 3 包均带）。
- 消费方 `scripts/fetch-models.cmake` 只对**归档**做 SHA512 校验（`:230-250`），`models` 表仅被 `LENGTH` 出来数个数（`:168-170`，打印在 `:195`）——**逐文件摘要从未被验证**。生成侧确实在写它（`make-package.py:422-427`，注释写明目的是「让宿主能区分重新打包的归档与被替换的权重」）。
- **对抗复核后的定性（降级 P2 → P3）**：归档摘要已经传递覆盖了包内每个文件，且规范 `:371` 明确不要求内容摘要绑定身份。所以这是**冗余元数据没人消费**，不是完整性漏洞。要修的是「写了不读」，不是「缺校验」。

**第 3 轮新增：这张表本身还有格式缺陷（已实测）**。`make-package.py:424-427` 的 key 规则是「取**文件名**，仅当该文件名已被占用时才改用包内相对路径」：

```python
models = {}
for inside in sorted(shipped):
    models[inside.as_posix() if inside.name in models else inside.name] = digest(...)
```

- **后果 A（实测，口径已按复核收窄）**：新批次 `otter-tifa-0.1.0.0.zip` 的 4 本词典在包内位于 `otter-tifa/dictionaries/<name>`，而 manifest 的 key 是裸文件名（`ds-zh-pinyin-lite.txt` 等）。**按「包内相对路径」口径：7/11 命中、4/11 不对应任何包内路径。按「zip 成员全路径」口径：11/11 都不对应**（成员都带 `otter-tifa/` 前缀）。要按这张表验文件，消费方必须先猜「这个名字在哪个子目录」。`shipped` 确实是包内相对路径（`make-package.py:215`、`:232`），所以问题在 key 规则本身。
- **后果 B（推演，复核加了两个限定）**：若某包同时有根文件 `x.txt` 与 `a/x.txt`，`sorted(shipped)` 按路径分量排序，`a/x.txt` 在前 → 先写 key `x.txt` = `a/x.txt` 的摘要。轮到根文件时 `inside.as_posix()` 仍是 `x.txt`，于是**两条坍缩成一条**：`a/x.txt` 的摘要整条丢失，且该文件从此不在 manifest 的覆盖范围内。触发条件**只在子目录名排序早于根文件名时**（写成 `zz/x.txt` 就不触发）。四个现行出厂变体都不触发（game/hfa 全在包根，tifa 四词典同处 `dictionaries/`），属**潜在缺陷**。
- **无测试兜底**：`scripts/test_make_package.py:182-210` 只测「按文件名查找被拒」与 `a/dict.txt`/`b/dict.txt` 都进 `shipped`，**从不构造「根文件 + 子目录同名」**，也不测 `main()` 的 key 生成。Merge 类测试全用手写 fragment（`:244`）。
- **现实证据（值得引）**：`build/release-models-v0.1/REBUILD-REPORT.md:112-114` 自己就踩到同一根因——按裸文件名去比已发布 manifest，tifa 的 `model.onnx`/`config.json` 与 hfa 的同名文件产生 4 行「DIFFERENT digest」误报。**裸基名做键会误导人，这不是假想的风险。**
- 影响仍属「元数据无人消费」那一档（P3），仓内也确实没有按 key 定位文件的消费方（`fetch-models.cmake` 只用 `LENGTH` 计键数，`src/` 与 CI 都不读发布 manifest）。但一旦有人按注释里写的目的（「不解释归档也能分辨权重是否被替换」）去接线，这张表**定位不到 4 个文件，且藏着一处会坍缩的冲突**。

**修复方向**：key 一律用包内相对路径（`inside.as_posix()`），不设基名回退，并补一条「根文件与子目录同名」的回归用例。

### F-10 P2｜lite：语言模块下转型缺少守卫

- `src/libs/SynthrtEngine/VoicebankCatalog.cpp:459`：`linguist->as<wolf::LinguistSpec>()`，**此前未比较 interface 与 level**。
- 同文件自己的约定相反：`:57-59` 的注释写着「The contract is compared first, **because the downcast of a configuration is not checked**」，`:61-68` 的 `configurationOf` 实现里确有 `target->interface() != interfaceName` 守卫。otter 的 `Note` 契约也是先比 interface（`VoicebankCatalog.cpp:647`）。
- synthrt 的 `as<T>()` 是**纯 `static_cast`**，只有 `static_assert`（`synthrt/include/synthrt/synthrt_global.h:20-32`），运行期零检查。
- **第 7 轮复核（新增）**：该处现在写成了 `if (const auto *spec = linguist->as<wolf::LinguistSpec>(); spec != nullptr && ...)`（`VoicebankCatalog.cpp:459-460`）。看着像加了守卫，但 `as<>()` 返回的就是 `static_cast<T*>(this)`（`synthrt_global.h:20-30`），**对合法对象永远非空**，所以这个 `spec != nullptr` 是空转，interface 与 level 依旧没有比较。

**可达性（如实标注）**：加载期 wolf 的 `validateLinguistRole` 只比 `interface`（wolf 审计 §3.2，`WolfLinguistProvider.cpp:168-171`），**不比 level**。所以「绑到别的类别」今天会被 wolf 拦住，「wolf 语言模块升到 level 2」则拦不住，此时 lite 的 L1 转型就是跨类型读。**属推断（未运行验证），按「上游若能出 level 2 就会踩」的预防性缺陷记 P2。**

### ~~F-11 P3｜跨仓：两份发布件的归档布局不一致~~（**已被对抗复核推翻，撤回**）

**原标题与结论都不成立**，保留编号只作记录，详见 §3.5 与 §6。

我原先据 `build/lang-packages/unpacked/<pkg>/desc.json`（**解包后**的形态）推断 wolf 归档是平铺的。实测归档本身：

| 发布件 | 归档内首个成员 | 实际形态 |
| :-- | :-- | :-- |
| `wolf-lang-eng-1.0.0.4.zip` | `wolf-lang-eng/desc.json` | **单层包裹** |
| `wolf-g2p-pinyin-1.0.2.4.zip` | `wolf-g2p-pinyin/desc.json` | **单层包裹** |
| `wolf-lang-cmn-1.0.1.4.zip` | `wolf-lang-cmn/desc.json` | **单层包裹** |
| otter 各包 | `otter-game/desc.json` | 单层包裹 |

**⇒ 两份发布件形态相同，不存在跨仓不一致。** 规范也不禁止包裹：`:61-66` 只是「基本结构为」的示意图，规范性条文是 `:355`「Package 根目录必须恰好包含一个名为 `desc.json` 的普通文件」，`:94` 把 `${root}` 定义为 `desc.json` 所在目录，`:491` 的安装形状 `packages/foo/desc.json` 恰好就是包裹归档解压到 `packages/` 后的结果。

### F-12 P3｜spec：`desc.json` 字段表没有 `name`

- `ds-spec-2.4.md:143-160` 的 `desc.json` 必选/可选字段清单里**没有 `name`**（必选 4 条、可选 9 条，本轮逐行读）。
- 而《多语言文本》的标准字段表 `:293-298` 把 `name` 列为「多语言文本」。
- 现实：先前记录（`conformance-yousa-2.4.md` §5）已指出「`desc.json.name` 规范未列为必选也未列为可选」，yousa 等第三方包在用。反过来，**wolf 的 15 个已发布包全部没有 `name`**（本轮实测：`vendor`/`copyright` 全有、`name`/`url` 全无）。
- **消费侧实测（本轮新增，把本条从文档问题升级为格式-消费一致性问题）**：synthrt 的加载器先写死 `package->name = DisplayText(package->id)`（`synthrt/lib/Core/PackageLoader.cpp:1140`），随后只读 `vendor`/`copyright`/`description`/`readme`/`url`（`:1156-1173`）——**`name` 根本不在读取之列**。所以第三方包在 `desc.json` 里写的 `name` 既不在规范的字段表里，也不会被加载器消费，只能作为未知字段保留。

**判断**：靠 `:74` 的「未知字段不得导致拒绝」兜住，能工作，但规范自身对同一字段给了两种说法，且实现层面「写成 `id` 当名字」这条默认规则**没有写进规范**。属规范与实现共同留下的格式缺口（P3，文档级）。

### F-21 P2｜otter：已发布过的 `0.1.0.0` 被回落到新布局的包上（第 3 轮新增，已按复核改述）

**事实（两份本地发布件材料逐成员实测）**：

| | 旧批次（`D:\projects\models\otter-v0.2.0.0\`，下载物） | 新批次（`build\release-models-v0.1\`，本地重组） |
| :-- | :-- | :-- |
| `otter/game` 的 `version` / `compatVersion` | `0.1.0.0` / `0.1.0.0` | `0.1.0.0` / `0.1.0.0` |
| 贡献类别 | **`analysis`** | **`inference`** |
| 声明路径 | `analyzers/note/analysis.json` | `inferences/note/inference.json` |
| 接口 | `org.openvpi.otter.analysis.Note` | `org.openvpi.otter.inference.Note` |
| 归档体积 | 45,713,320 B | 45,724,747 B |

⇒ **包标识与版本完全相同，类别、路径、接口、内容全变。**

**这次改动本身是有记录的，不是暗改**（复核纠正了我的原叙述）：`docs/otter-design.md:868-874`（决策 A26）写明「删除类别层」的得失，原文「代价是接口改名、包布局从 `analyzers/*/analysis.json` 改为 `inferences/*/inference.json`，当时三个包升级到 0.2.0.0 重新发布……`models-v0.1.0.0`、`models-v0.2.0.0` 中均为 0.1.0.0 的旧布局包，三个标签此后都已删除；**A31 起包版本回到 0.1.0.0**」，版本统一另有取舍记录（`docs/plans/tifa-align.md:54` 的 D7：四包同版便于 manifest 组装）。

**真正的格式问题在「版本回落」这一步**，规范有明文：

- `:181`「Package 的身份由 `id` 与规范化后的 `version` 共同确定。**发布者必须保证使用同一身份发布的 Package 内容相同**……违反上述要求属于 Package 缺陷，其运行时行为未定义」。
- `:189-198` 列出兼容区间内必须保持的公开表面，其中包含「**已有贡献的类别**、模块 ID 及其含义」与「已有模块的 `interface` 与 `level`」，并规定「任何破坏上述承诺的版本都必须将 `compatVersion` 提高到不再覆盖受影响的旧版本」。

本例类别与 interface 双改、`compatVersion` 仍是 `0.1.0.0`，**两条同时违反**。

**为什么仍是 P2 而不是 P0（减责事实）**：新批次自述「本地，未发布」（`REBUILD-REPORT.md:1`、`:208`），docbase 也记三个旧标签已删除，所以**线上目前没有两个共存的发布批次**。违反是**条件性**的——一旦按 `models-v0.1` 发布即发生。对本地持有旧件的用户（本机就有）而言，同身份不同内容已经成立。

**附带文档缺陷（P3 级，可单独订正）**：「包版本回到 0.1.0.0」在多处被归因到 **A31**（`otter-design.md:860,874,906`、`plans/hfa-align.md:22,412`），但 A31 正文（`otter-design.md:947-957`）只讲宿主面与提供者面的头文件拆分，**不含这个决定、也没给理由**。⇒ 「为什么把已发布的 0.2.0.0 降回 0.1.0.0」在仓内**没有专门条目**。

**未证实**：仓内两份文档对远端现状说法矛盾——`docs/packages.md:32` 说现行发布是 `models-v0.1`、旧标签已删，`REBUILD-REPORT.md:5` 说远端只有 `models-v0.1.0.0/0.2.0.0/0.3.0.0`。**要核清需联网，本轮未做。**

**最短处置（发布前阻断项）**：若要以 `models-v0.1` 发布，先决定是「抬包版本」还是「在 release notes 与 `packages.md` 里明确声明 `0.1.0.0` 的旧布局已作废」，并把 A31 的错误归因改成一条独立决策记录。

### F-22 P3｜三方对「2.4 归档布局」的约定不一致，lite 的交接文档断言只有扁平才对（第 5 轮新增）

**实测三种形态并存**：

| 来源 | 归档首个成员 | 形态 | 依据 |
| :-- | :-- | :-- | :-- |
| lite 的打包工具 | 由 `os.path.relpath` 生成 | **扁平**（`desc.json` 在根） | `docs/synthrt/tools/pack-voicebank-zip.py:37-38` 的 `member_name`，docstring `:4-7` |
| wolf 发布件 | `wolf-lang-eng/desc.json` | **单层包裹** | 第 2 轮实测 `dist/*.zip` |
| otter 发布件 | `otter-game/desc.json` | **单层包裹** | 第 3 轮实测 `release-models-v0.1/*.zip` |

**问题在文档的断言，不在实现**。lite 的打包脚本 docstring 原文：

> The layout is flat … which is what the published 2.4 deliverable of this package used. The archives kept beside the voicebanks (qixuan@2.7.0.0.zip, zhibin@26.7.16.0.zip) carry a wrapper directory, **but they are 2.3 sources, so they are not the convention for a 2.4 deliverable.**

**这句话对 2.4 上游是错的**：wolf 与 otter 的 **2.4** 发布件都带包裹层。交接文档 `packaging-voicebank-essentials.md:29` 同样只写「归档扁平……与上一轮已发布产物一致」（对本仓自己的 yousa 产物成立，对上游不成立）。

**规范怎么说的**（第 2 轮已核）：`:61-66` 是「基本结构为」的示意图，规范性条文是 `:355`（Package root = 恰好含一个 `desc.json` 的目录）、`:94`（`${root}` 同义）、`:491`（安装后平铺）。**两种形态都合法**，规范不选边。

**影响**：第三方开发者照 lite 的文档，会把 wolf/otter 那类带包裹的 2.4 归档判成「不是 2.4 约定」，进而在自己的安装器里按扁平假设解析。这不是谁的 bug，是**约定没统一且被写成了单边断言**。

**最短修复方向**：把 docstring 与交接文档改成「本仓约定扁平；wolf 与 otter 的 2.4 发布件带一层包裹，解包时以归档内 `desc.json` 所在目录为 Package root」。若想统一，则统一在**消费方按内容定位**（找 `desc.json`），而不是在打包方强制形态。

---

## 2.2 synthrt 清单/声明格式面（第 2 批并入）


> 范围：`synthrt/lib/Core`、`synthrt/lib/Support`、`synthrt/include/synthrt`，对照 synthrt 自己的 `docs/ds-spec-2.4.md`。只读子代理取证 + 主代理抽验，**P0 一条都没有**。下文行号以 `synthrt/lib/Core/PackageLoader.cpp` 等相对路径给出。

### F-13 P1｜`dependencies[]` 的未知字段导致整包拒绝

- 证据：`PackageLoader.cpp:460-466` 的 `allowedKeys = {"id", "version"}`，命中不到即 `InvalidFormat: unknown dependency field`（**已回读原文**）。
- 规范 `:74`：「框架定义的 object 中，框架只验证其认识的字段，**未知字段不得导致拒绝**，并必须保留在对外暴露的已展开 manifest object 中」。`dependencies[]` 的字段由 `:157-159` 定义，属框架定义 object。
- 旁证：同仓插件元数据读取是宽容的（`lib/Core/ContribPluginFactory.cpp:44-67` 只取 interface/variant/level），两处策略相反。测试把这个拒绝行为固化了（`tests/auto/Core/test_SynthUnit.cpp:461-471`），**说明是有意为之，不是笔误**。
- **对已发布包的影响：无**。wolf 的 15 个已发布包的 `dependencies[]` 只有 `id`/`version`（前序审计 §2 已逐条验）。这是**给未来第三方包的绊索**。

### F-14 P1｜版本的**单个分量**受机器整数限制（前提已证实，措辞已修正）

- 证据：`PackageLoader.cpp:446-451` 走 `stdc::VersionNumber::fromString`，失败即报 `version is not representable`。**构建实际使用**的 `vcpkg/installed/x64-windows/include/stdcorelib/support/versionnumber.h:62` 是 `std::array<int, 4>`，实现用 `std::from_chars` 读进 `int`，越界返回 `nullopt`（主代理已回读该头文件确认，第 2 轮复核员另行确认了依赖树来源：synthrt 的 `cmake-build-*` 指向 lite 的 `vcpkg/installed`）。
- 规范 `:177`：版本各分量「没有固定数值上限，实现必须按任意精度非负整数比较，不得因超出机器整数范围而改变比较结果」。
- ⇒ `"version": "1.0.0.2147483648"` 会被判非法，而规范要求它合法且参与比较。现实命中率低，但**属明确违背**，且会随 `compatVersion` 与依赖求解一起出现。
- **措辞修正（复核指出）**：**分量个数**上限（1~4 段）是 synthrt 自己的自校验，合规。受限的只是**单分量数值上限 INT_MAX**，来源是 `stdcorelib` 的 `array<int,4>` 加 `from_chars`。两者不能混说。
- **修复形态（待选，见 §5.4）**：版本比较改成「按段十进制字符串比较（长度加字典序即整数序）」最省，不引依赖。

### F-15 P1｜JSON profile 预扫描没有深度上限

- 证据：`PackageLoader.cpp:263` 先跑 `JsonProfileValidator(text).hasDuplicateObjectKey()`，`:269` 才交给带深度上限的解析器。而 `JsonProfileValidator` 的 `scanValue/scanObject/scanArray`（`:151`、`:175`、`:213`）**互相递归且无深度计数**（**已回读类体，无 `depth`/`maxDepth` 符号**），对照解析器 `stdcorelib` 的 `maxDepth = 512`。
- 后果：深嵌套的 `desc.json`（约十万层、约 200KB 文本）**先让预扫描爆栈**，而不是走解析器的「超限拒绝」。规范 `:74` 允许设资源限制并拒绝，即期望可控拒绝。
- 附带（P3）：预扫描失败时 `hasDuplicateObjectKey()` 直接返回 false（`:84-87` 的 `scanValue() && m_duplicate`），**重复 key 检测被静默关闭**，随后交给解析器。

### F-16 P2｜内置类别在 entry 上拒绝未知字段，与 declaration 的宽容策略自相矛盾

- `lib/SVS/InferenceContrib.cpp:49-58` 的 `fields = {"id", "path"}`，命中不到即 `inference contribution entry has an unknown field`（**已回读**），`lib/SVS/SingerContrib.cpp:22-31` 同构。
- 而同一文件在声明文件上明确按 `:74` 宽容（`SingerContrib.cpp:233-250` 只记 debug 日志）。规范 `:74` 末句「贡献类别、interface 与 variant 定义的 object 按各自 schema 处理」与 `:229`「其余字段由该类别自行规定」让 **entry 的归属含糊**，两种实现都能自圆其说。但对包作者不可预期，且与 F-13 是同一类问题的两处表现。

### F-17 P2｜缺解释器的报错不含三元组与定位

- `PackageLoader.cpp:772-775`：`"no interpreter provides the requested module triple"`，既无 `spec->locator()` 也无 interface/level/variant，且未用 `.withContext()`（相邻的类别失败路径 `:1376-1381` 是带的）。
- 规范 `:564`：「把那个实现的名字写出来，加载器才能在找不到解释器时说清楚缺的是哪一个」。属可诊断性缺口，零正确性风险。

### F-18 P2｜synthrt / dsinfer 侧没有任何契约 JSON Schema

- synthrt 仓内 `**/*schema*.json` **0 命中**（排除 build 目录），现存契约材料是散文表格（`docs/dsinfer-level-1-revised.md:32-42`）。
- **本轮补充（把范围从 synthrt 扩到官方接口）**：`synthrt/dsinfer/` 子仓同样 **0 份 schema**，目录下只有 `plugin.json` 与测试夹具的 `*.json`（本轮实测列举）。也就是说，**真实声库（如 yousa）实际使用的 5 个 dsinfer inference 契约与 singer 契约，都没有机器可读的 `exports`/`imports[].options` Schema**。反过来，第三方类别倒是都发了：wolf 10 份（4 exports + 4 import-options + 2 共享定义）、otter 6 份（3 契约 × exports/import-options）。
- 规范 `:596-599` 要求每个 (`interface`, `level`) 的契约规范**至少包含** `exports` 与 `imports[].options` 的机器可读 JSON Schema。
- **运行侧不做 schema 定位是合规的**：`:606` 明确「可定位」不要求运行时刻发现、下载或校验 Schema，实测 `synthrt/{include,lib}` 无 schema 代码，唯一契约检查是插件元数据三元组精确匹配（`ContribPluginFactory.cpp:20-70`）。**缺口在材料，不在运行时。**
- **生态后果**：官方接口的事实契约是 C++ 结构体加散文表（lite 用 `configurationOf<T>` 手写解析），第三方接口是 JSON Schema。两类接口的契约载体不同，**「同一 (interface, level) 的契约规范」在生态里没有统一落点**。

### F-19 P2｜路径与多语言路径解析助手未公开

- `resolvePath`（`PackageLoader.cpp:493-499`）与 `readDisplayText`（`:501-549`）在匿名命名空间（`:31`、`:551`），公共头（`include/synthrt/Core/ContribCategory.h`、`ContribSpec.h`、`Support/DisplayText.h`）里没有任何等价助手。
- 后果：第三方类别只能自己重写。`lib/SVS/SingerContrib.cpp:114-169` 已经复制了一份（`readDisplayPath`）。
- 规范 `:302` 要求多语言路径「每一个本地化的值各自解析，基路径都是该声明文件所在的目录」，且 map 的 value 必须是展开+定基+规范化后的路径。**这是最容易漏实现的一条，且没有测试兜底第三方实现。**

### F-20 P2｜模块声明出现 `$version` 直接拒绝，属规范留白

- `PackageLoader.cpp:1270-1273`：`"module declaration must not contain $version"`。
- 规范 `:508` 只说 `$version`「不在其中」，`:163` 却描述「写在模块声明文件里的版本号，要等到整个 Package 已被接受、依赖已被解析之后才轮得到检查」，**隐含模块声明可以带版本号并被晚检查**。规范未规定必须拒绝，两个实现可各自解释。

### 已验证做对的关键点（同等重要，避免重复排查）

| 项 | 结论 | 证据 |
| :-- | :-- | :-- |
| 未知字段保留 | `desc.json` 与模块声明的未知字段**确实保留**在 `manifestDeclaration()` | `PackageLoader.cpp:1393`、`PackageHandle.h:70-71`、`ContribSpec.h:140-141` |
| `$version`/`runtimeLevel` 时机 | 在变量展开与 `contributions` 之前整包拒绝，符合 `:163` | `PackageLoader.cpp:1080-1098` 先于 `:1100`、`:1195` |
| 版本等价 | `1`≡`1.0`≡`1.0.0`≡`1.0.0.0` 有测试 | `PackageLoader.cpp:34`、`:1088`，`test_SynthUnit.cpp:746-757` |
| 路径解析 | 反斜杠→`/`、相对定基+`lexically_normal`、绝对直用、NUL 拒绝、`..` 可越出 root | `PackageLoader.cpp:487-491`、`:493-499`、`:1257`、`:1261-1262` |
| 字符串变量 | 一次性求值、`$$` 防二次展开、作用域只含外层与更早项、`root`/`dir` 禁重定义 | `PackageLoader.cpp:288-312`、`:314-357`、`:388-417` |
| 多语言文本 | `_` 必选且值必须字符串、路径逐值定基、`name` 缺省=id、显式空串原样保留 | `PackageLoader.cpp:521-528`、`:531-548`、`:1304-1311` |
| 类别注册 | 类别自带条目模式与解析器，框架只校 envelope，重名注册报错 | `ContribCategory.h:118-125`、`SynthUnit.cpp:76-78` |

---

## 2.3 规范正文的内部不一致（第 5 轮；只读子代理扫描 + 主代理抽验）

> 与 §2/§2.2 的区别：这些**不是实现缺陷**，而是规范文本自身给出的两种读法，或该给的判定没给。编号 `S-*`，**不与 `F-*` 混算**（当前 F 仍是 21 条）。行号以 891 行快照（mtime 2026-09-30 9:49）为准，引用前应复核。

| # | 位置 | 现象 | 判定 | 影响 |
| :-- | :-- | :-- | :-- | :-- |
| S-1 | `:84` × `:74` | `vars` 每项「**只包含** `name` 与 `value`」，而 `:74` 规定框架定义的 object 里未知字段**不得导致拒绝**。`vars[]` 项正属框架定义的 object | **真不一致（已实测取哪边）** | 带额外字段的 `vars` 项：**synthrt 取严格读法**——`PackageLoader.cpp:329-333` 用 `item.size() != 2` 直接整包拒绝，与 `:74` 冲突，与 F-13 同族 |
| S-2 | `:84` × `:29`/`:144`/`:508` | 「除**各层级**的 `$version` 和 `vars[].name` 外」，暗示 `$version` 有多层，与「只写在 `desc.json`」冲突 | **真不一致**（已知 ① 的同族第三处） | 措辞误导，无运行时后果 |
| S-3 | `:554` × `:306` 文法 | 正文称 `interface` 是「反向域名形式」，但文法只要求 `dotted-id`，允许单段裸名（如 `foo`） | **真不一致（弱）** | 裸 `interface` 名的包是否有效，两实现可各自判定 |
| S-4 | `:86` | 原始字符串存在未配对 `${` 时「**加载行为未定义**」 | 缺口 | 同一个包可被一个实现接受、另一个拒绝，规范不兜底 |
| S-5 | `:406` × `:68`/`:80`/`:90` | 「相对路径均以该实际来源为基础解析」未限定作用范围，字面可读成把声明的所有相对路径重定基到来源根 | 措辞歧义 | 路径基可能分叉 |
| S-6 | `:504` | 用户搜索路径列表的分隔符、相对条目基准、是否规范化/快照均未规定，而候选选择完全依赖该顺序 | 缺口 | 依赖解析结果依赖宿主解释 |
| S-7 | `:733` | `defaultLanguage` 要求「`languages` 非空时必填」，但 `languages` 缺省而 `defaultLanguage` 出现时是否有效未规定 | 缺口 | 包有效性无判定 |
| S-8 | `:691` | 「本章 `desc.json` 中 `singer/zhibin`」：`desc.json` 示例在 §1 不在本章，且 `singer/zhibin` 少了前导 `:`（`:329`/`:333` 要求 `:category/id`） | 交叉引用错位 | 文档级 |
| S-9 | `:230` × `:234` | 「其余字段由该类别自行规定」与「模块类别的条目**必须**使用 `path`」：`path` 由框架强制还是类别定义没有一处说清 | 措辞 | 第三方模块类别的条目校验会分叉 |
| S-10 | `:650`/`:823` × `:889` | 规范用 `Executive` 但从不定义它，也没说它与 `ContribInterpreter`/`InferenceInterpreter` 的分工（代码里这三类都存在，见 §2.4） | 引用缺失 | 文档级 |
| S-11 | `:163` × `:210` | 同类「整包拒绝」要求，一处「应当」一处「必须」 | 措辞（强制力） | 文档级 |
| S-12 | `:28` × `:157`/`:158` | 承诺「统一用 Package，不再用 Library / lib」，`dependencies` 的说明仍写「依赖的库」 | 措辞 | 文档级 |

**已排除的候选（本轮查过、不成立，后续不要重复排查）**：规范里的 **JSON 示例全部自洽**（`desc.json` 示例 `:113-141` 的必选字段、版本类型、贡献 `path`、依赖；Singer 示例 `:693-721` 的变量与引用；插件 metadata 示例 `:828-838`）；**版本文法**与散文一致（`:169-183`）；**变更表 36 条**逐条对回正文无相悖；**表格无错列**；provider 排序（`:576` × `:887`）、卸载与回滚顺序（`:456`/`:479`/`:468`）、「变体不可互换」与「兼容承诺允许改 `variant`」均不构成冲突；ZIP 特性集「实现定义」（`:361`/`:363`）属规范明确留白。

---

## 2.4 规范留白 → synthrt 实际取哪边（N-6，第 6 轮）

> 做法：对 §2.3 的每一条，去 synthrt 源码里找「这一处它到底怎么判」，**只读**取证。这张表的价值在于：规范修订时不必猜「改了这一句会不会破坏兼容」，直接能看到现状取向。

| `S-*` | 规范没定的问题 | **synthrt 的实际取向** | 证据 |
| :-- | :-- | :-- | :-- |
| S-1 | `vars` 项出现 `name`/`value` 之外的字段 | **严格**：`item.size() != 2` 直接整包拒绝 | `PackageLoader.cpp:329-333` |
| S-2 | `$version` 是否可以出现在模块声明里 | **只认 `desc.json`**：模块声明里出现即拒绝（见 F-20） | `PackageLoader.cpp:1270-1273` |
| S-3 | `interface` 是否必须是反向域名（多段） | **只按文法**：`isValidSeparatedId` 不设段数下限，单段裸名如 `foo` 能过 | `ContribLocator.cpp:59-63`、`PackageLoader.cpp:1285-1287` |
| S-4 | 字符串里有未配对的 `${` | **静默把整个字符串变成空串**：`varexp` 遇不平衡花括号返回 `{}`，`expandString` 把它当成功值返回 | `stdc::str::varexp`（本机 vcpkg 构建树 `vcpkg/buildtrees/stdcorelib/src/0ce48abbbf-326196d581.clean/src/str.cpp:292-297`，stdcorelib 副本，非本仓源）、`PackageLoader.cpp:311` |
| S-5 | 相对路径以谁为基准 | **按声明文件所在目录**（与 `:68`/`:80` 一致，不重定基到来源根） | `PackageLoader.cpp:493-499` 的 `resolvePath` |
| S-6 | 用户搜索路径列表的分隔符与基准 | **不作为字符串列表**：API 直接收 `array_view<std::filesystem::path>`，分隔符语义留给宿主 | `SynthUnit.h:72` |
| S-7 | `languages` 缺省但写了 `defaultLanguage` | **拒绝**：`defaultLanguage` 必须是 `languages` 的键，否则报 `not a key of languages` | `SingerContrib.cpp:66-77` |
| S-9 | 模块条目的 `path` 由谁强制 | **框架强制，但以类别注册的 `declarationMode` 为前提** | `PackageLoader.cpp:1250-1255` |
| S-10 | `Executive` 与解释器是什么关系 | **两类类型都存在，分工没写清**：`ContribExecutive`/`InferenceExecutive`/`SingerPipelineExecutive` 与 `ContribInterpreter`（`Core/`）/`InferenceInterpreter`（`SVS/`）并存，规范 `:650` 只提 `Executive`、`:823` 说插件创建 `ContribInterpreter`、`:889` 说创建 `InferenceInterpreter`，三者关系无定义 | `include/synthrt/Core/ContribExecutive.h`、`Core/ContribInterpreter.h`、`SVS/InferenceExecutive.h`、`SVS/InferenceInterpreter.h`、`SVS/SingerPipelineExecutive.h` |
| S-11 | 「应当」与「必须」 | 行为无差别：两条路径都是整包拒绝 | `PackageLoader.cpp:1080-1098` |
| S-8、S-12 | 交叉引用与术语 | 纯文档级，无实现可对 | — |

**最值得注意的一条是 S-4**：规范写「加载行为未定义」，实现却是**确定性的静默清空**。一个 `${assets` 的笔误不会报错，只会让那个字段变成空字符串，错误推迟到使用该路径时才以「文件不存在」的形式冒出来，而且**变量拼错与变量为空在现象上不可区分**。这不是实现违规（规范自己说未定义），但是一个**规范该补、实现该改报错**的点，已并入 D-12 的 (a) 项。

---

## 3. 有意设计、已撤回、不要重开的候选

### 3.1 撤回：「`defaultLyric` 没有已发布包供给，所以是缺陷」

**不成立。** lite 的取值链**有意不消费**它：`src/app/Model/AppOptions/Options/DefaultLyricRules.h:15-18` 原文——「The editor reads that statement …, but **the resolution below deliberately does not consume it**: it stays driven by the settings and by the built-in table, so the reading can be reviewed before anything depends on it.」实测 `SingerInfo::defaultLyricForLanguage` 只被测试调用（`tst_package_catalog.cpp:122-127`）。**所以「没有包供给」与「消费方不消费」是配套的两步走，不是缺陷。** 该字段的真实问题只有 F-1 的「发布链写不出」与 F-2 的「lint 失明」。

### 3.2 撤回：「两个 `ds-spec-2.4.md` 副本在漂移」

**已有结论，本轮复核一致**：`conformance-yousa-2.4.md:158-161` 已做过逐行比对（891 行、51 行不同、全为同索引替换、情态词计数无变化、两处小节标题改名、以 synthrt 版为权威）。本轮独立重算差分行数得 **51**，与该结论吻合。**不要重复排查。**

### 3.3 有意设计（不是缺陷）

| 候选 | 结论 | 依据 |
| :-- | :-- | :-- |
| 根字段无 JSON Schema | 规范不要求（只要求 `exports` 与 `imports[].options`） | `ds-spec-2.4.md:596-599`、`:74` |
| 运行时不定位/加载契约 Schema | spec 明确允许（「可定位」≠ 运行时发现） | `ds-spec-2.4.md:606` |
| synthrt 报错码的选择（`runtimeLevel` 超域报 InvalidFormat） | 规范只要求「拒绝」，码值差异不影响合规 | `ds-spec-2.4.md:210` |
| `${root}` 在 Windows 保留反斜杠 | 用于路径字段时后续会归一，用于非路径文本也无害 | `ds-spec-2.4.md:97`、`PackageLoader.cpp:487-491` |
| `defaultLyric` 空串折叠为「未声明」、单值访问器 | 契约明写，实现一致（除 F-4 的三处细节） | `linguist-domain-contract.md:192`、`LinguistContrib.h:56,67` |
| 非字符串不阻塞加载 | 契约与代码注释同源 | `linguist-domain-contract.md:192`、`LinguistContrib.cpp:107-113` |
| wolf `openSet` / `coverage` 口径 | **一致，非缺陷**（仅供一处边界洞，见 §5） | schema `default:false` ↔ A60/A70 ↔ `LinguistSession.cpp:754` |
| otter 的 `hfa`/`rmvpe` 包内无 `LICENSE` | **文档已记录**（来源与许可在 release notes 里） | `docs/packages.md:32`、`docs/otter-design.md:862` |
| otter 归档可为单层包裹 | 其取包脚本靠 manifest `directory` 自洽 | `fetch-models.cmake:41`、`make-package.py:294` |
| otter 的契约名从 `org.openvpi.otter.analysis.*` 改为 `.inference.*`（类别也从 `analysis` 改到 `inference`） | **有决策记录**（A26：`analysis` 类别已删除），旧发布标签已删除，新发布的包一律用新名 | `docs/otter-design.md:656`、`:812`、`docs/packages.md:7-10` |
| 发布件用 `.zip` 而非 `.dspk` | 项目既定（声库分发约定） | `docs/synthrt/PLAN.md:25` |
| 语言包 G2P 省略 `exports.languages` | 规范允许，责任在语言包一侧 | `conformance-yousa-2.4.md:172` |

### 3.4 lite 工作区未提交改动的归属（如实交代）

本轮在 lite 仓看到 7 处未提交改动（`docs/synthrt/developer-repro.md`、`docs/synthrt/packaging-voicebank-essentials.md`、两个工具脚本、`scripts/vcpkg-ports/wolf/portfile.cmake`）。其中 `portfile.cmake` 的 `LOCAL-ONLY (DO NOT COMMIT)` 块是 setdir 本地联动态（把 `D:/projects/wolf` 工作树 COPY 进 `SOURCE_PATH`），**属用户既定状态，本文不动、不提**。

### 3.5 撤回：跨仓「归档布局不一致」（原 F-11）

**我的错误**：我读了 `build/lang-packages/unpacked/<pkg>/desc.json`（**解包后**的形态）就断言 wolf 发布归档是平铺的，没有去读归档本身。对抗复核员第一句就把这条推翻，我随后亲自复核确认：

```
wolf-lang-eng-1.0.0.4.zip   → wolf-lang-eng/desc.json, wolf-lang-eng/inferences/…
wolf-g2p-pinyin-1.0.2.4.zip → wolf-g2p-pinyin/desc.json, …
wolf-lang-cmn-1.0.1.4.zip   → wolf-lang-cmn/desc.json, …
otter-game-0.1.0.0.zip      → otter-game/desc.json, …
```

**两份发布件都是单层包裹，形态相同**，「wolf 平铺 vs otter 包裹」的跨仓不一致**不存在**。规范也不禁止包裹：`:61-66` 是示意图，`:355` 以「`desc.json` 所在目录」定义 Package root（`:94` 的 `${root}` 同义），`:491` 的安装形状 `packages/foo/desc.json` 恰是包裹归档解压到 `packages/` 的结果。**本条不再作为缺陷，也不再作为待确认项。**

**教训（值得记）**：解包树与归档不是同一层事实。前序审计 §2.1 比的是「归档条目 vs 解包树内容」，条目名含包名前缀这件事当时就在比对里出现过，我却没把结论带到布局上。

### 3.6 负结果：本机 `D:\projects\otter\build\packages\` 不能当发布件证据

实测该目录是**混合的陈旧构建状态**：`manifest.json` 的 `bundleVersion=0.3.0.0`，四个包版本为 `0.1.0.0`×3 加 `otter-tifa 0.2.0.0`，且 game/hfa/rmvpe 三个包仍用**已废弃**的 `org.openvpi.otter.analysis.*`（A26 之后当前头文件是 `org.openvpi.otter.inference.*`）。而 `docs/packages.md:30,32` 称「四个声明版本一致，当前均为 0.1.0.0」。**⇒ 这个目录只能证明打包器机制（如 `make-package.py:294` 的包裹式 arcname），不能作语义/版本证据。**

### 3.7 第 3 轮更正：找到了更接近当前发布件的本地产物

第 2 轮把「otter 当前发布件」列为必须联网才能验。第 3 轮在本机发现了 `D:\projects\otter\build\release-models-v0.1\`：`bundleVersion=0.1.0.0`、**四个包版本全为 `0.1.0.0`**、四份 zip 加一份 manifest，与 `docs/packages.md:32` 的描述一致，且附有 `REBUILD-REPORT.md`（16 KB）自述这是按现行布局**重组**的 `models-v0.1`、**本地未发布**。

**它不是远端原物的副本，而是「按现行规则应产出什么」的忠实重建**，因此可以拿来做格式验证（逐项结果见 F-9 与 F-21）。**远端原物仍未核对**（需联网），所以「远端 manifest 与本地重建是否逐字节一致」仍属未证实。

---

## 4. 未覆盖（下一轮起点）

| 项 | 原因 | 建议取证方式 |
| :-- | :-- | :-- |
| otter **远端**发布件 `models-v0.1` 原物 | 第 3 轮已在本机找到重组版 `build/release-models-v0.1`（见 §3.7），但那是本地重建、自述未发布 | 要核「远端 manifest 与本地重建是否逐字节一致」只能联网取 |
| wolf 发布包的**运行时**格式行为 | 本轮只做格式面 | 前序审计 §6 已列未证实项，不重复 |
| dspkg（Rust 转换器）产出的声库格式 | 属另一生产者，本轮只借用了它的声明做对照 | 若要做生产者交叉对比，单列一轮 |
| 契约 Schema 与契约文档的**落点统一**（F-18 ↔ F-2） | 需要先定「契约材料放哪个仓」 | 读 synthrt `docs/dsinfer-level-1-revised.md` 与 wolf `docs/schemas/` 的对应关系 |

---

## 5. 决策台账（需人判，第 4~5 轮整理成选项式）

> 规则：每条给「推荐项 + 备选 + 取舍」，来源轮次标在括号里。**未裁定之前不实施任何修复**（本轮仍只读）。

| # | 决策点（来源） | 选项与取舍 | 推荐 |
| :-- | :-- | :-- | :-- |
| D-1 | otter 安装面的版本约束怎么表达（F-5，第 1/2 轮） | **(a) 文档承认第四段差异不被拦截**，零成本，但承诺强度下调／**(b) 自写 `otterConfigVersion.cmake`**，真兑现「任何版本变更都要重建」，代价是多维护一个 CMake 脚本／(c) 把项目版本降成三段，改动 `CMakeLists.txt` 语义 | **(a)** 先落低成本版本，(b) 留到有真实跨版本混装事故时再做 |
| D-2 | `defaultLyric` 的值从哪来（F-1，第 1/2 轮） | **(a) 给 `close_language` 加作者参数**，值来自来源仓库，须 bump `PACKAGING_REVISION`／(b) 从词典或资源推导，需要新规则且不保证可推／(c) 语言包不声明该字段，改契约 §2.5 的举例 | **(a)** |
| D-3 | §74「未知字段不得导致拒绝」的边界（F-13、F-16，第 1/2 轮） | **(a) 两处都改宽容**（`dependencies[]` 与 contribution entry），与规范一致／(b) 保持严格，在规范里写明这两处豁免／(c) 只改 `dependencies[]` | **(a)**，一次定清，不要两处各定一次 |
| D-4 | 版本比较怎么修（F-14，第 1/2 轮） | **(a) 按段十进制字符串比较**（长度加字典序即整数序），不引依赖／(b) 引入任意精度整数库，改动大／(c) 不改，改为在规范里限值 | **(a)** |
| D-5 | 模块声明出现 `$version`（F-20，第 1/2 轮） | **(a) 保持拒绝并改报错措辞**／(b) 改为忽略并告警／(c) 先让规范择一，代码跟随 | **(a)+规范措辞对齐** |
| D-6 | 空音素表 + `openSet: false` 是否判不可用（wolf 覆盖率边界，第 1 轮） | **(a) 判 `Unavailable`**，让 A70 判据可达／(b) 保持 `Unknown` 并在文档写明这是有意的空洞 | **(a)**，语义上「声明全集却一个音素都没有」就是连接错误 |
| D-7 | 全角/NBSP 空白算不算空白（F-4，第 1 轮） | **(a) 纳入 `isSpace`**，与契约「纯空白视为未声明」一致／(b) 不纳入，改文档 | **(a)** |
| D-8 | otter lint 的加固批次（F-6、F-7，第 1/2 轮） | **(a) 本轮就补 `level` 类型与裸 ref**，十行内，风险低／(b) 连 `role` 语法与唯一性一起补，需定归口（otter 还是 dsinfer）／(c) 都不补，靠加载期兜 | **(a) 先做，(b) 单列** |
| D-9 | otter 已发布过的 `0.1.0.0` 被回落到新布局（F-21，第 3 轮） | **(a) 发布前抬包版本**／(b) 在 release notes 与 `packages.md` 声明旧 `0.1.0.0` 作废／(c) 两者都做 | **(c)**，并把误归因到 A31 的段落改成独立决策记录 |
| D-10 | 官方契约的 Schema 落点（F-18，见 §9） | 见 §9 的三选项 | **(a) synthrt 补 12 份 JSON Schema + 三方一致性测试** |
| D-11 | 2.4 归档是否统一形态（F-22，第 5 轮） | **(a) 改文档**：说明「本仓扁平、上游带包裹、以 `desc.json` 定位 root」，不改工具／(b) lite 打包工具改成带包裹，与 wolf/otter 一致／(c) 推动上游改成扁平 | **(a)**；(b) 会让已发布的 yousa zip 与新产物形态分裂，成本大于收益 |
| D-12 | 规范正文的 12 条 `S-*` 怎么修（§2.3/§2.4，第 5~6 轮） | **(a) 只修影响包有效性判定的**：S-1（`vars` 未知字段）与 **S-4（未配对 `${`）**——后者现状是**静默清空成空串**（`str.cpp:292-297`），拼错与空值不可区分／(b) 12 条一次全修（含措辞、交叉引用、编号结构）／(c) 不修，写进实现说明 | **(a)**；S-4 建议连实现一起改（不平衡花括号直接报 InvalidFormat，而不是返回空串） |

**若要开始实施，建议顺序**：D-3（一次定清宽容边界）→ D-4（版本比较）→ D-8 的 (a) → D-1（文档）→ 其余。理由：前三条是「规范违背 + 十行内改动」，收益/成本最高。

---

## 6. 对抗复核记录


**复核轮的组织（第 2 轮重派）**：首轮两名复核员（每名 8~10 条）超时未回报，已 `interrupt` 并改为按更窄范围重派——**每名 4 条、要求 15 分钟内按「已完成 / 未完成 / 卡在哪」三段交付**。任务设定是**逐条找反例推翻**，且明确要求检查四类反例：「有意设计被报成缺陷」「行号漂移」「把推断写成事实」「规范原文其实允许」。

### 6.1 已回执（第 2 批复核员，4 条）

| 条目 | 判定 | 关键证据 |
| :-- | :-- | :-- |
| F-14 synthrt 版本分量受 `int` 限制 | **成立（前提已证实）** | 构建实际使用的 `vcpkg/installed/x64-windows/include/stdcorelib/support/versionnumber.h:62` 是 `std::array<int,4>`（主代理已亲自回读同一文件确认），实现用 `std::from_chars` 读 `int`，越界即 `nullopt`，`PackageLoader.cpp:446-451` 因此整包拒收 |
| F-14 的措辞 | **需修正** | 「四分量上限」是 synthrt **自己**的合规自校验，`array<int,4>` 卡住的只是**单分量 INT_MAX**，两者不能混说 |
| F-2 wolf 根字段无 schema / lint 失明 | **成立** | 反例方向全部失败：`check-desc`/`check_identity`/`validate(exports)` 都没有根字段校验路径；且 wolf 加载器对非字符串只 warn（`LinguistContrib.cpp:114-124`），lint 是唯一可能的门禁而它不管 |
| ~~F-11~~ 跨仓归档布局不一致 | **推翻（第一条就被证伪）** | wolf 自家构建的 `build/lang-packages/dist/*.zip` 首个成员就是 `wolf-lang-xxx/desc.json`，与 otter 同形态。规格 `:355`/`:94`/`:491` 也不禁止包裹。**已撤回，见 §3.5** |
| F-9 otter manifest 逐文件摘要无人校验 | **成立但降级** | 事实无误，但归档摘要已传递覆盖内容，规范 `:371` 不要求内容摘要绑定身份 ⇒ 属「冗余元数据没人消费」，由 P2 降 P3 |

**我自己的更正（如实记录）**：F-11 是我读错了层级——用**解包后**的 `unpacked/<pkg>/desc.json` 推断了**归档**内的布局。复核员指出后我亲自读 `dist/*.zip` 的条目，确认自己是错的。这条按纪律只加更正、不删原文（§2.1 保留带删除线的标题与更正说明）。

### 6.2 已回执（第 2 批另一名复核员，4 条）

| 条目 | 判定 | 关键证据与我的处理 |
| :-- | :-- | :-- |
| F-5 otter 版本文件忽略第四段 | **不成立（归因错误）** | 生成物与本机 CMake 3.30 的 `BasicConfigVersion-ExactVersion.cmake.in` 逐行同构，模板第 8 行原文即「tweak 段被忽略」。otter 的旋钮 `COMPATIBILITY` 已选对（`src/CMakeLists.txt:39`）。**P0 → P2，改为「约束表达不够严」**。另：「实测」改成「按生成物逻辑推演」，因为探针从未被允许运行 |
| F-13 synthrt `dependencies[]` 拒绝未知字段 | **成立** | 规范唯一豁免（贡献类别 / interface / variant）盖不住 `dependencies[]`，且 `test_SynthUnit.cpp:462` 把违规行为写成断言。**维持 P1** |
| F-1 wolf `defaultLyric` 发布链不可达 | **不成立（范围错误）** | `make-test-fixtures.py:26-28` 自述是「本仓随附的包，不是测试夹具，发布脚本会归档它」，`make-lang-release.py:169-170/186` 的 `--extra` 会把它并入发布源，`build/test-fixtures/.../linguist.json` 已带该字段。**收窄为「转换链不可达」，P1 → P2**。我的另一处错误：`lang-v0.1.2.0` 不是 HEAD 的祖先，不能由「旧 tag 没这脚本」推出「HEAD 写不出」 |
| F-7 otter `role` 零校验 | **事实半对** | otter 仓内确实零校验，但 synthrt 加载器在 `PackageLoader.cpp:38-50/1332-1336/1369` 加载期强制必选、文法、唯一。**改述为「lint 覆盖漏项」，P1 → P2** |

**我自己的第二处更正**：F-5 的 P0 定级建立在「otter 实现与承诺不一致」上，实际是 CMake 模板固化的语义，otter 能选的旋钮已经选对了。**这一条如果不在复核轮被推翻，会被当成「本仓必修」排进计划**——这正是对抗复核的价值所在。

### 6.4 已回执（第 3 轮复核员，2 条：F-21 与 F-9 的扩充）

| 条目 | 判定 | 处理 |
| :-- | :-- | :-- |
| F-21 otter 已发布过的 `0.1.0.0` 被回落到新布局包 | **成立，但我的叙述要改** | 复核找到反例：这次改动**有决策记录**（`otter-design.md:868-874` 的 A26、`tifa-align.md:54` 的 D7），且 spec `:181` 是**明文禁止**「同一身份内容不同」，`:189-198` 还要求抬 `compatVersion`。已改写成「版本回落才是问题，且是条件性违反（新批次未发布）」，并把「A31 归因错位」列为附带文档缺陷 |
| F-9 扩充：manifest `models` 表的 key 规则 | **成立，需收窄** | 复核确认 `shipped` 是包内相对路径、代码逐字一致、无测试覆盖、仓内无按 key 定位的消费方。已补：A 的两种口径（4/11 或 11/11）、B 的两个限定（只在子目录名早于根文件名时触发，效果是坍缩不是覆盖）、`REBUILD-REPORT.md:112-114` 的真实误报作证 |

**复核替我更正的三处取材细节**：① 旧批次目录只有 **3 个 zip**（不是 4 个，另有两个解包目录）；② 旧批次 `bundleVersion` 是 `0.2.0.0`、新批次是 `0.1.0.0`；③ 我对 F-21 的原叙述「两批发布件违反唯一性假设」把**未发布的重组件**算成了发布件。全部已改进正文。

### 6.5 主代理抽验清单（复核员之外）

§2 与 §2.2 的每一条 P1/P2 的关键行号均已在当前工作区回读原文：`PackageLoader.cpp:460-466/446-451/263/151/769-775/1270-1273/1140/1156-1173`、两仓 `check-declarations.py` 的对应行、`otterConfigVersion.cmake:8,21,62-66`、`check-installed-surface.cmake:107-110,170-194,201-204`、`LinguistContrib.cpp:94-126`、`make-test-fixtures.py:33,66`、`make-package.py:294`、`fetch-models.cmake:168-170,195,230-250`、`VoicebankCatalog.cpp:57-59,459`、`synthrt_global.h:20-32`、`installed/.../versionnumber.h:62`。

---

## 7. 取证纪律与工具自证

- **只读**：本轮对 synthrt / wolf / otter 三仓未做任何写操作（`git status` 复核干净，otter 的两处未提交改动是用户既有状态）。lite 侧只写了本文。
- **未构建、未运行任何脚本**：包括 wolf 的 `check-declarations.py` 与 otter 的 `check-installed-surface.cmake`（后者只读源码）。**「让这些闸门真正跑一次」属需授权的动作，不在本轮。**
- **工具自证（字段普查脚本）**：对已发布包的普查先证「装载非空」——`desc.json found: 15`、5 份 `linguist.json` 全部被读到（否则 `defaultLyric` 的 0 命中就可能是空转）。zip 成员列举用 `System.IO.Compression` 直接读归档，不依赖 `unzip`。
- **断言分级**：正文中标「事实」者均已由主代理回读行号，标「推断」「未证实」者不得当既定缺陷排期。
- **引用新鲜度复核（第 7 轮，子代理 + 主代理抽验）**：对全文约 **220 处** `文件:行号` 引用做逐条回读，结果 **217 处一致、0 处文件缺失、3 处需处理**，且 3 处已在本轮改正：① F-10 的 otter `:647` 补上文件名（`VoicebankCatalog.cpp:647`）；② S-4 的 `str.cpp` 补全为 vcpkg 构建树里的 stdcorelib 副本路径；③ **S-10 的「代码里没有 `Executive` 类型」被推翻**——`Core/ContribExecutive.h` 等三个头文件都在，已改述为「规范没写清分工」。另经主代理复核确认 `PackageLoader.cpp` 的 18 处引用、wolf 与 otter 的 lint 行号、lite 的 4 处引用全部吻合当前工作区。**spec 快照（891 行，mtime 2026-09-30 9:49）与 §2.3 声明一致，27 处规范行号全对。**

---

## 8. 后续可选工作（需授权或需裁定）

| # | 事项 | 前置条件 | 产出 |
| :-- | :-- | :-- | :-- |
| N-1 | ~~取 otter 当前发布件~~ | **第 3 轮完成大半**：本机找到重组版 `build/release-models-v0.1`，4/4 归档 SHA512 与 size 全对、贡献路径无断链、逐文件摘要 19/23 可复现，据此产出 F-21 与 F-9 的实测结论。**远端原物仍未核**（需联网） | 剩余：远端 manifest 与本地重建的一致性 |
| N-2 | ~~用实际 pin 的 `stdcorelib` 复核 F-14~~ | **第 2 轮已完成**：构建实际使用的 `installed/.../versionnumber.h:62` 确认是 `array<int,4>`，F-14 成立（措辞已修正） | 已完成，无需再查 |
| N-3 | 契约 Schema 的落点统一（F-18 ↔ F-2） | **第 4 轮已完成分析**：见 §9 | 选项式方案已落盘，等裁定（D-10） |
| N-4 | §5 的待确认逐条给选项 | **第 4 轮已完成**：§5 已改写成 D-1~D-10 决策台账 | 等裁定 |
| N-5 | 若用户授权：把两处「已写好但从未跑到」的闸门跑一次（wolf `check-declarations.py`、otter `check-installed-surface.cmake`），验证本轮结论的运行时形态 | 用户授权执行 | 门禁现状读数 |
| N-6 | 把 §2.3 的 12 条 `S-*` 逐条对到实现 | **第 6 轮已完成**：见 §2.4 的对照表（10 条可对，2 条纯文档） | 已交付 |

**明确不做**（避免范围膨胀）：不在本轮改任何仓的代码、不重开 §3 已撤回的候选、不重复前序审计已覆盖的 wolf 发布包数据面。

---

## 9. 契约材料的落点（N-3，第 4 轮）

**问题**：规范 `:594-599` 要求每个 (`interface`, `level`) 有一份「由该 interface 所有者发布的、稳定且可定位的契约规范」，并且**至少包含** `exports` 与 `imports[].options` 的机器可读 JSON Schema。实测三方的落点完全不同：

| 契约（interface / level） | 发布面谁在用 | 现有契约材料 | 机器可读 Schema |
| :-- | :-- | :-- | :-- |
| dsinfer `inference.Acoustic` / `Duration` / `Pitch` / `Variance` / `Vocoder`（L1） | **所有声库**（yousa 实测用了这 5 个） | `synthrt/docs/dsinfer-level-1-revised.md` 散文表（`:30-300`）+ `dsinfer/Api/Inferences/*/1/*ApiL1.h` 的 C++ 类 | **0 份** |
| dsinfer `singer.DiffSinger`（L1） | 所有声库 | 同上 | **0 份** |
| wolf `inference.G2P` / `S2P` / `Onset` / `linguist.WolfLinguist`（L1） | wolf 语言包、声库自建 linguist、yousa 的 3 个 onset 与 3 个 s2p | `wolf/docs/linguist-*.md` + `docs/schemas/` **10 份** | ✓ 4 exports + 4 import-options + 2 共享定义 |
| otter `inference.F0` / `Note` / `Align`（L1） | otter 模型包 | `otter/docs/*.md` + `docs/schemas/` **6 份** | ✓ 3 exports + 3 import-options |

**关键事实**：官方契约**并非不存在**，而是载体是 C++ 源码——`DurationApiL1.h:30-95` 里 `DurationSchema` / `DurationConfiguration` 把字段名、类型、默认值都写死了。问题在于规范要的是**第三方校验器能读的材料**，C++ 头不满足这一点。于是出现一个倒挂：**第三方类别（wolf、otter）都按规范交了 schema，官方接口一个都没交。**

**三个选项**：

| # | 方案 | 取舍 |
| :-- | :-- | :-- |
| (a) | **synthrt 补 12 份 JSON Schema**（6 契约 × exports / import-options），与 C++ 类同源，并加一条「schema ↔ 读取器 ↔ 文档表」三方一致性测试 | 代价：12 个文件 + 一条测试。收益：官方契约可被 dspkg-validator、wolf/otter 的 lint、第三方包作者直接用；otter 的 `test_CheckDeclarations` 已有同型做法可照抄。风险：三真源（C++ / 散文 / schema）要有校验守住 |
| (b) | 规范改为「官方 interface 的契约材料是所有者随实现发布的头文件」，不要求 JSON Schema | 代价：改规范一行。风险：第三方校验器无法校验官方契约，`dspkg-validator` 这类工具只能覆盖 wolf/otter 契约。**会削弱规范对开放生态的意义** |
| (c) | 新建独立「契约材料仓」，三方都指向它 | 代价：新仓 + 三处 pin + 发布流程。收益：单一落点、跨实现共享。风险：为一个尚不成熟的需求引入长期维护面，违背最小改动 |

**推荐 (a)**。理由有三：规范明文要求，官方一侧目前是零；工具链已存在（`check-declarations.py --schemas`、otter 的三方互校都验证过这条路可行）；不需要新仓或新概念。

**若采纳，验证方式**：把新 schema 喂给一个**独立**校验器（如 dspkg-validator），对真实发布件跑一次（yousa 声库 + wolf 与 otter 的语言/模型包），证明能接受；再故意改坏一个字段，证明能拒绝。**两个方向都过才算数。**
