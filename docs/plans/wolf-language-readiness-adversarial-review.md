# wolf 语言就绪缺口：对抗复核报告

> 状态：**进行中**（2026-10-05）。对抗复核、spec 2.4 合规台账与 F1 端到端实测均已完成（§10）。**wolf 侧修复按用户裁定暂缓**（"暂不改 wolf，先把台账落盘"），续写落点见 §10.4 与 §10.3。lite 侧消费改动：D4（失败后呈现重算）已实施并通过全量测试，D5（音素表喂给 wolf）实施中。

- **快照（2026-10-05 更新）**：lite `HEAD=b0bf3c9b`（分支 `synthrt/inferutil-binary-read`）。wolf 侧以本地仓 `<wolf 检出>`（`3a3f4a0` ＝ 端口 `scripts/vcpkg-ports/wolf/portfile.cmake` 的 REF ＝ buildtrees 树内容，仅 CRLF 差异）为准，故同一条行号对"实际链接版本"与"可改仓库"同时有效（版本澄清见 §10）。初版所记的 lite `f37fb4b9` 与 buildtrees 路径已过时。
- **证据源**：wolf 头文件（`vcpkg/installed/x64-windows/include/wolf/Session/LinguistSession.h`）、wolf 实现（本地仓 `<wolf 检出>` 的 `src/lib/Session/LinguistSession.cpp`，与 buildtrees 树内容一致）、lite 源码、以及本机隔离实例的实验（命令见 §8、§10.7）。
- **本报告的目的**：对"语言级 G2P 损坏能否只禁那一个语言"这条需求做对抗复核——**先找反例推翻既有结论**，推翻不了才确认。阅读约定同早前的缺陷清单（该文已于 2026-10-06 并入 `docs/plans/synthrt-main-migration.md`）：每条标 **事实** / **推断** / **未证实**。

## 0. 先更正上一轮的自述结论（重要）

上一轮我在早前的缺陷清单 §7（该文已于 2026-10-06 并入 `docs/plans/synthrt-main-migration.md`）写成"资源级单语言损坏不会触发标灰，属**上游能力缺口**，需 wolf/synthrt 补 API"。**这句话一半是错的**，本轮读 wolf 实现后推翻：

- **事实**：`probe()` 会查**失败缓存**，命中即返回 `Unavailable` 并原样带上失败层的原因
  （`LinguistSession.cpp:710-716`："The reason is repeated verbatim from the failing layer."）。
- **事实**：失败在 `acquire()` 路径上写入缓存（`LinguistSession.cpp:459-468` `recordFailure()`），
  且**只有 `refresh()` 会清除**——`release()` 明确保留（`LinguistSession.cpp:667-673`）：
  "A cached failure is retained, because releasing resources does not repair a failed route, and only
  refresh() clears the failure cache."
- ⇒ 因此"**尝试过一次之后**"，资源级损坏也会让 `probe()` 报 `Unavailable`，而 lite 的标灰正是读
  `probe()`（见 §3）——**这条路径不需要上游新增任何 API**。真正的缺口只剩"**首次尝试之前**不可见"，
  而那是 wolf 明写的设计取舍（§2）。

## 0.5 决策台账（按轮次，标注来源）

| 轮次 | 议题 | 用户裁定 | 落点 |
| :-- | :-- | :-- | :-- |
| A（用户 D1） | "预检资源"是否可行 | 先去 synthrt 核实 spec 2.4 再说 | §6 G1 的查询期 `verify()` 草案**作废**。合规落点＝**加载期**取得/校验（§10.3、§10.6①） |
| B（用户 D2） | 逐词 `DriverUnavailable` 是否标灰 | **保持现状**（不缺资源时可能只是选错 ORT/EP 设置） | §1 W-L8、§6 G5。D4 因此只对**路由/执行体层**失败发信号 |
| C（用户 A1） | 语言粒度禁用与提示 | 只禁真坏的那一个语言、tooltip 显示引擎原文。失败包也照常显示但标灰 | §3。`LanguageComboBox` 既有行为（D1 核实后无需新增 API） |
| D（用户：只改对接部分） | 其他严重事项 | 对抗复核后**只落盘** | 早前的缺陷清单（该文已于 2026-10-06 并入 `docs/plans/synthrt-main-migration.md`） |
| E（用户：先收敛本地能做的） | wolf 缺口 | 形成完善的对抗复核报告 | 本文件 §1~§9 |
| F（2026-10-05 拍板） | wolf 合规修复 | **暂不改 wolf，先把台账落盘** | §10（含 §10.4 修复优先级/代价与消费约束） |
| F′（同日） | V6/F5 是否必须改 synthrt | 要求先解释机制再定 | §10.3：**不必**。给出可达性论证与重新评估的触发条件 |
| G（用户：lite 其他问题允许构建，开子代理解决） | lite D4~D8 | 授权实施 + 构建验证 | D4 失败后重算（`e90bebc2`）、D6 删零调用 API（`b0c72e78`）、D7 注释英文化（`305bc655`）、D8 文档登记（`5143d05a`）、D5 音素表（实施中） |
| H（用户指正） | multig2p 的可达性 | "被 eng-g2p（chain）间接引用，几个声库在词典查不到的时候都在用" | §10.6② 修正 + §10.7 实测。我此前"本机不可达"的结论是**输出被截断**导致的误判 |

## 1. 结论速览

| 编号 | 结论 | 类型 | 关键证据 | 影响 / 处置 |
| --- | --- | --- | --- | --- |
| W-L1 | wolf 的就绪是**三层**：`Ready`（已预热）/`Cold`（路由固定、未尝试）/`Unavailable`（带原因） | 事实（设计） | 头文件 `:41-50` | lite 目前只消费 `Unavailable` |
| W-L2 | `Cold` **不保证**资源可加载。"驱动是否存在、模型能否打开"只有 attempt（`warm()`/`convert()`）才能确定 | 事实（设计，明文注释） | 头文件 `:47-50` | 需求"尝试前就发现单语言损坏"**不可能**靠 probe 达成 |
| W-L3 | `probe()` 有 4 条 `Unavailable` 来源：目录/路由缺失、**未声明该语言**、**失败缓存**、覆盖率精确为零 | 事实（实现） | 实现 `:688-729` | 前两条与第三条 lite 已在用，第四条 lite 用不到 |
| W-L4 | 一次失败即被缓存并污染后续 `probe()`，直到 `refresh()` | 事实（实现） | 实现 `:459-468`、`:667-673`、`:710-716` | lite 侧只需"失败后重算语言呈现"即可让标灰出现 |
| W-L5 | 包内**数据**资源在加载期校验：缺文件 → 整包失败（连词典文件都算） | 事实（实验 C3 + 日志），**2026-10-05 更正范围** | 日志 `failed to size a resource` | 对词典/规则/脚本类变体成立。**multig2p 的 ONNX 模型不成立**——模型缺失/损坏时包照常加载、语言不标灰、OOV 词静默降级（§10.1 F1、§10.7 实测） |
| W-L6 | lite **从不调用 `warm()`**（全仓仅三处注释说明"无预热"是有意设计）。`setSingerPhonemes` 无应用侧调用者 ⇒ 覆盖率恒 `Unknown` | 事实（全仓 grep） | `InferEngine.cpp:108`、`LanguageBridge.h:23`、`SynthrtEngine.h:84`（注释）。`LanguageBridge.cpp:150-153`（定义，无应用侧调用者） | 覆盖率类诊断（含零覆盖 Unavailable）在 lite 不可达 |
| W-L7 | lite 的标灰只覆盖：未声明语言（兜底文案）+ 引擎给出原因的声明语言 | 事实 | `LanguageComboBox.cpp:186-222`、`:70-136` | 与 wolf 能力相比**少用了** coverage 一族信息 |
| W-L8 | wolf 有**两类失败**且处理完全不同：① 路由/执行体层失败（`build()` 失败）→ 写失败缓存 → `probe()` 报 `Unavailable`。② 逐词失败（`G2PApiL1::Error`，含 `DriverUnavailable`）→ 只在转换结果里，**不进缓存、不改变就绪** | 事实（实现 + API） | 实现 `:564-567`（`recordFailure` 在实现里**仅此一处**调用）。`G2PApiL1.h:41-49` | "某语言 G2P 坏掉"若表现为②，则既不会标灰也不会出现在语言下拉 |
| W-G1 | "首次尝试前的资源级不可用不可见"——**设计取舍**，不是缺陷。若要预检需新增不加载的校验入口 | 判断 + 建议 | 头文件 `:47-50` | 需产品拍板，建议见 §6 |
| W-G2 | 失败后 lite 不重算语言呈现 ⇒ 标灰要等到下次"歌手呈现刷新"才出现（**只对①类路由失败有意义**，因为只有它写缓存） | 事实（静态调用图，端到端未实测） | 语言下拉刷新的唯一调用点是 `ClipEditorToolBarView.cpp:566`（在 `refreshSingerComboPresentation()` 内）。属性变化回调 `:417-420` **不**刷新语言。轨道侧同理 `TrackControlView.cpp:391` | lite 侧小改即可，建议见 §6 |
| W-G3 | 未声明语言的引擎原文是英文（`this singer does not declare X`），lite 走兜底文案 | 事实（实现 + lite） | 实现 `:699-701`。`LanguageComboBox.cpp:198-202` | 文案口径需拍板 |
| W-G5 | 逐词失败（②类）**没有**就绪语义，现状不会标灰。是否要在"同一语言持续逐词失败"时降级为不可用，是产品决策 | 事实 + 建议 | `G2PApiL1.h:41-49`，实现 `:564-567`，lite 已翻译逐词文案 `LanguageBridge.cpp:63-80`、`:199-205` | 需拍板。**不建议由 lite 自造禁用**（见 §6） |

## 2. wolf 侧就绪模型（事实）

1. **枚举与语义**（头文件 `:41-50`）：`Ready` = warmed，下次转换不加载资源。`Cold` = 路由存在且未被尝试。
   `Unavailable` = 不可用，`LanguageStatus::reason` 记录原因。紧随其后的 `\note` 明确：
   "Cold does not guarantee that the resources load. It indicates only that no decision remains: the
   binding is fixed, and the reachable depth and the coverage are already computed. Whether a driver is
   present or a model opens is a property of the installation and is determined only by an attempt,
   which warm() performs."
2. **`probe()` 的实现分支**（实现 `:683-729`，按顺序）：
   - 目录里查不到该歌手：多版本 → `"several versions of this singer are loaded; name one"`（`:689-691`）。
     未挂载 → `unmountedReasonFor()`，为空则 `"no such singer"`（`:693-694`）。
   - 歌手在目录里但没声明该语言 → `"this singer does not declare <lang>"`（`:699-701`）。
   - **失败缓存命中** → `Unavailable` + 失败原文（`:712-716`）。
   - 覆盖率为精确 0 → `"this singer sings none of the N phonemes <lang> declares"`（`:721-727`），
     该判定要求宿主先经 `setSingerPhonemes()` 提供音素表（`:732-749`）。
   - 否则 `warmed` 命中 ? `Ready` : `Cold`（`:728`）。
3. **失败写入与清除**（已全仓核对，无第三处写入）：`failed` 声明于 `:267`，写入**仅两处**——`:465`
   （`recordFailure()` 内，由 `acquire()` 在 `:566` 调用）与 `:551`（`slotFor()` 失败时直接 `emplace`），
   读取两处（`:545` acquire、`:712` probe），**清空仅一处**——`refresh()` 内的 `:616-617`
   （`warmed.clear()` + `failed.clear()`），`release()` 只清 `warmed`、**保留** `failed`（`:667-673`）。
4. **"歌手已加载但没挂上路由"有专门的 per-language 原因表**（`unmountedReasonFor` 实现 `:378-393`。
   头文件 `:220-225` 说明其用途与成因），lite 侧该情形表现为歌手未解析。
5. **两类失败的边界（本轮新增，最关键的一条）**：
   - ①**路由/执行体层失败**：写缓存的路径有**两条，且都在 executive 建立之前**——
     `slotFor()` 失败直接 `failed.emplace()`（实现 `:549-553`），
     `build(slot, language)` 失败走 `recordFailure()`（实现 `:564-567`，`recordFailure` 本身定义在 `:460`，
     全实现仅 `:566` 一处调用）。失败后 `probe()` 报 `Unavailable` + 失败原文 ⇒ **这类失败最终会标灰**。
   - ②**逐词失败**：`G2PApiL1::Error`（`G2PApiL1.h:41-49`：`None/InvalidInput/ModelInferenceFailed/
     PhonemeGenerationFailed/DriverUnavailable/NotInitialized/UnknownError`）是**转换结果里的逐词字段**，
     不写缓存、不改变 `Readiness` ⇒ **这类失败永远不会标灰**。
   - 因此"某语言的 G2P 坏掉"落哪一类，决定用户看到的是"语言被禁用"还是"歌词/音素层报错"。

## 3. lite 侧消费现状（事实）

- 语言级查询只有一条：`SynthrtEngine::languageUnavailableReason` → `LanguageBridge::unavailableReason`
  → `session.probe()`（`LanguageBridge.cpp:130-148`，返回空串表示可用）。
- 自动化面同源：`voices.describe.g2p_ready = resolved && convertibleLanguages.contains(id)`
  （`PublicAutomationRegistry.cpp:2488-2506`），`convertibleLanguages` 由 `SynthrtEngine::canConvert` 填
  （`PackageAutomationAdapter.cpp:115-118`）——**UI 标灰与自动化观测读的是同一个探测**，故 §4 的实验结论可
  直接迁移到 UI。
- UI 的两组禁用（`LanguageComboBox.cpp:186-222`）：
  (a) 宿主已知但歌手**未声明**的语言 → 一直列出、禁用、用兜底文案（`:198-202`）。
  (b) 已声明但**引擎给出原因**的语言 → 只禁该语言、原因照抄引擎原文（`:204-221`）。
  未解析歌手直接早退（`:204-206`，避免噪音）。
- **未使用的能力**：`warm()` 在 lite 全仓**零调用**——实际只有三处**注释**说明这是有意为之：
  `src/app/Modules/Inference/InferEngine.cpp:108`（"No warm-up is required. wolf loads a language on its
  first conversion …"）、`src/libs/SynthrtEngine/LanguageBridge.h:23`（"… no warm-up pass."）、
  `src/libs/SynthrtEngine/SynthrtEngine.h:84`（"Initialization has a single stage, with no advance
  warm-up: wolf loads a language on its …"）。⇒ "首次尝试前不可见"**不只是 wolf 的取舍，也是 lite 的
  既定设计**。G1 若要改，等于同时修改 lite 的设计陈述。
  `setSingerPhonemes()` 只有 `LanguageBridge` 自己的声明与定义（`LanguageBridge.cpp:150-153`）而无调用者
  ⇒ `LanguageStatus::coverageKind/coverage/missingPhonemes` 与 `LanguageEntry::phonemes/openSet` 在 lite
  全程未被消费，覆盖率一族诊断（含"零覆盖即 Unavailable"）在实践中不可达。
  （唯一的 `setSingerPhonemes` 调用在测试里：`src/tests/TestVoicebankAudit/main.cpp:520`、`:572`。）
- **②类失败（逐词）已有文案、但无就绪语义**：`LanguageBridge.cpp:63-80` 把
  `wolf::Api::G2P::L1::Error` 翻成用户可读消息，`:199-205` 写进逐词结果。这些消息**不参与**语言就绪判定，
  因此不会影响下拉的禁用集（与 §2.5 对应）。

## 4. 实验记录（本机隔离实例，观测面 MCP `voices.list`/`voices.describe`）

| 实验 | 构造 | 观测 | 结论 |
| --- | --- | --- | --- |
| A | 藏 `out/bin/wolf/packages/wolf-lang-jpn`（后加 `wolf-g2p-multi`） | `jpn.g2p_ready=true` 不变，jpn 歌词仍出音素 `n`、`i` | 语言包目录不参与该探测，这些声库自带语言 stage |
| B | 整目录 `wolf/packages` 改名 | 7 歌手 × 全部声明语言全 `true`（`cmn/eng/jpn/yue/qzz` 矩阵无变化） | 同上，跨包资源**懒加载**：不影响 `Cold` |
| C1 | 复制声库到临时目录，`languages` 加一条无对应 import 的 `xyz→lang/xyz` | 整包失败，日志 `singer language xyz refers to a nonexistent import role: lang/xyz` | 坏路由 = 包级失败 |
| C2 | 把 `lang/jpn` 的 `ref` 改成 `:linguist/jpn-missing` | 整包失败，日志 `module import target does not exist` | 同上 |
| C3 | 保留路由、把包内词典 `assets/japanese_dict_full.txt`（由 `inferences/s2p-jpn/inference.json` 的 `configuration.file` 引用）改名 | 整包失败，日志 `failed to interpret module configuration: ...failed to size a resource` | **包内资源在加载期校验**：不存在"包可用、单语言资源坏"的中间态 |

## 5. 对抗复核：逐条反例尝试

1. **反例："wolf 缺资源级就绪能力"** → **推翻**：`warm()` 存在且语义就是"立刻加载并保留 executive"
   （头文件 `:228-233`，实现 `:751-759`），失败带原因缓存。`probe()` 读该缓存（`:712-716`）。
2. **反例："失败只影响当次调用"** → **推翻**：缓存写入 `failed` 且只在 `refresh()` 清除。`release()` 故意保留
   （`:667-673`）。
3. **反例："那 lite 只要全量 `warm()` 就能预检"** → **部分成立但有代价**：`warm()` 会创建并**保留** executive
   （实现 `:756-757`），全量预热 = 一次性加载所有歌手×语言的全部资源（模型/词典），启动代价与内存都要付。
   且预热失败会把该语言长期钉在 `Unavailable`，直到 `refresh()`。建议按需预热（选中/首次使用时），见 §6。
4. **反例："覆盖率规则可兜底单语言损坏"** → **推翻**：该规则需要宿主提供音素表，lite 的
   `setSingerPhonemes` 无调用者 ⇒ 覆盖率恒 `Unknown`，规则不可达（§3）。
5. **反例："包级失败会表现为语言标灰"** → **不成立**：包失败时歌手根本不在目录里，lite 侧该歌手
   `resolutionState != Resolved`，语言下拉早退（`LanguageComboBox.cpp:204-206`），呈现走"失败包/未解析"分支。
6. **反例："需求可以完全满足（未尝试前就标灰）"** → **不成立**：wolf 明文把"资源能否加载"定义为只有 attempt
   才能确定（头文件 `:47-50`）。未尝试前能拿到的只有路由级结论。⇒ 见 §6 G1。
7. **反例："逐词失败也会触发标灰，所以②类也被覆盖"** → **推翻**：写缓存的路径只有 `build()` 失败一条
   （实现 `:564-567`），逐词错误产生于 executive 已建成之后的转换过程，源码里没有任何把
   `G2PApiL1::Error` 写进 `failed` 的代码路径（`recordFailure` 全实现仅一处调用）。⇒ 见 §6 G5。
8. **反例："lite 从未处理逐词错误"** → **推翻**（对 lite 有利的一条）：`LanguageBridge.cpp:63-80`
   已经为全部 7 个逐词错误码提供用户文案，`:199-205` 写入结果。缺口不在"没有文案"，而在"文案与就绪/禁用
   语义脱钩"。
9. **反例："失败会经属性变化回调顺带刷新语言呈现，所以 G2 不成立"** → **推翻**：语言下拉刷新在整个
   ClipEditor 侧只有一个调用点（`ClipEditorToolBarView.cpp:566`，位于 `refreshSingerComboPresentation()`），
   而属性变化回调 `onClipPropertyChanged()`（`:417-420`）只同步名称输入框。轨道侧同理
   （`TrackControlView.cpp:391` 在 `refreshSingerComboPresentation()` 内）。⇒ ①类失败后的标灰确实要等下一次
   歌手呈现刷新。

## 6. 缺口定义与建议（分级，均未实施）

- **G1（wolf 侧，需产品拍板）· 尝试前的资源级不可见**
  现状取舍合理：`probe()` 保证"列表渲染期零加载"，代价是资源级不确定性。若产品要求"打开工程即看到哪个语言坏了"，
  需要 wolf 提供**不加载**的校验入口（建议签名草案，语义约束写清以免破坏渲染期安全性）：

  ```cpp
  /// Verifies that the resources of a route can be opened, without creating or retaining an
  /// executive and without touching the failure cache. Returns Unavailable when the resources are
  /// provably missing; leaves readiness Cold when the check cannot decide (e.g. a driver that only
  /// resolves at load time), so that "unknown" is never reported as "broken".
  LanguageStatus verify(const SingerRef &singer, std::string_view language) const;
  ```

  反向理由（为什么可以不做）：① 现有"失败即缓存"已覆盖真实使用路径（用过一次就标灰）。
  ② 预检会引入"文件在但打不开"的边界语义，容易把 unknown 误报成 broken。③ 加载期校验（实验 C3）已经让
  **包内**资源损坏表现为包级失败，用户看到的是包失败提示，不会静默。
- **G2（lite 侧，本地可做）· ①类失败后重算语言呈现**：`LanguageBridge::convert` 返回错误（路由层失败）后，
  触发一次语言下拉重填/工具提示刷新，使失败缓存立刻可见。**注意边界**：只对①类有意义——②类不写缓存，
  重算也不会改变禁用集（见 §2.5）。属对接层小改，符合"消费上游契约"而非"下游补偿"。
- **G3（lite 侧）· 恢复音素表喂给 wolf —— 数据源与三条硬规则已定（2026-10-05）**：**不用自己读文件**。包在
  `SynthUnit::Load` 时 dsinfer 已把 `config.json` 的 `phonemes` 表解释进配置对象
  （`dsinfer/Api/Inferences/{Acoustic,Duration,Pitch,Variance}/1/*ApiL1.h` 的 `std::map<std::string,int>`），
  lite 的 `capabilitiesOf()`（`VoicebankCatalog.cpp:296-349`）已握有这四个 target。三条硬规则：
  ① **必须去掉第一个 `/` 之前的部分**（真实声库表 200 键中 186 带 `<lang>/` 前缀，如 `eng/aa`、`cmn/A`。
  无前缀者=保留标记）——不去前缀则 wolf 的逐字比对（`LinguistSession.cpp:290-303`）把每个音素都算缺失，
  覆盖率一律 0% ⇒ **所有 Exact 语言被假判 `Unavailable`**。② **绝不喂空表**（空表=零覆盖，同样误伤全部
  Exact 语言），缺表/空表按"无声明"处理、保持 `Unknown`。③ 喂的时机只能是每次 `language->refresh()`
  **之后**（`SynthrtEngine.cpp:249`、`:475`），因为 wolf 只认当前 catalog（`LinguistSession.cpp:732-749`），
  且 refresh 只对仍在新 catalog 里的歌手保留表（`:618-627`）。四张表取**交集**（惯例见
  `scripts/convert-voicebank.py:285-286`）。静态模拟（未运行程序）：去前缀后 cmn/jpn/yue/eng 覆盖率
  97.4%~100%，`junninghua-yue` 的 50% 是真实缺口。
  实施落点：`VoicebankCatalog.h` 加字段、`VoicebankCatalog.cpp` 派生（去前缀＋交集）、`SynthrtEngine.cpp`
  两次 refresh 后按 catalog 统一喂。测试扩 `TestVoicebankAudit` 并删其手工喂表（`main.cpp:520`、`:572`）。
  **附带更正**：`docs/plans/optimization-and-integration-survey.md:78`（该文已于 2026-10-06 并入 `docs/plans/synthrt-main-migration.md`）「上游没有音素表可喂」**已被推翻**，勿沿用。
- **G4（文案，需拍板）**：未声明语言的引擎原文是英文 `this singer does not declare <lang>`。lite 目前对
  这类语言用兜底文案（`LanguageComboBox.cpp:198-202`）。是否统一为本地化包装 + 引擎原文放 tooltip，需拍板。
- **G5（产品语义，需拍板）· ②类失败要不要升级为"不可用"**：现状是"引擎不认为路由坏，所以不标灰"，
  用户若连续遇到同一语言的逐词失败（例如 `DriverUnavailable` 对该语言恒定），体感就是"这个语言坏了"。
  三条路线的取舍：
  ①**保持现状**（不改）：忠实反映 wolf 契约，失败在歌词/转换结果层呈现。
  ②**上游定策略**（推荐，符合"源头修复"约定）：由 wolf/synthrt 在**同类失败稳定复现**时把该
  (歌手,语言) 记为路由失败（等价于让 `recordFailure` 覆盖这一类），lite 侧零改动即可获得标灰。
  ③**lite 自造禁用**（不建议）：需要 lite 自己统计失败率并维护一份影子状态，违反单一真相源，
  且与 §2.5 的契约冲突。

## 7. 未证实清单（不得当缺陷排期）

- 转换失败后 lite 是否**一定**不刷新语言呈现（W-G2）：静态调用图已确认语言下拉刷新只挂在歌手/声部刷新链上
  （`ClipEditorToolBarView.cpp:566`、`TrackControlView.cpp:391`），但**未**做端到端实测（构造不出"包可加载、
  仅路由层失败"的夹具，见实验 C1~C3 全是加载期失败）。
- ②类逐词失败消息最终在 UI 的呈现深度（`LanguageBridge.cpp:199-205` 之后经 FillLyric/G2pService 到
  界面控件的链条未逐条追踪）。
- `warm()` 在 lite 上下文中的真实代价（未测：全量预热的耗时/内存）。
- `probe()` 读失败缓存的路径在实际 UI 中的可见时机（未做端到端截图，本机无 `computer-use`，且 125% DPI 下
  合成点击不可靠）。
- 覆盖率一族数据（`coverage`/`missingPhonemes`）在真实声库上的取值分布（未统计）。

## 8. 复核状态与自查记录（诚实记录，勿当成"已有独立第三方复核"）

- **方式**：本报告的对抗复核由**主代理逐条自查**完成（下述命令可复现）。同时派出过一名只读独立复核员
  （agent `c52af479`，任务=找反例推翻，带行号逐条核 8 组断言），**两次催办后仍未返回裁决，已中止**。
  因此：**本报告不宣称拥有独立第三方复核结论**。未返回的复核不构成任何背书。
- **逐条自查记录**（命令均为只读 `Select-String`/`read`，行号实测吻合）：

  | 断言 | 核查方式 | 结果 |
  | --- | --- | --- |
  | §2.1 三层就绪 + `Cold` 语义 | 读头文件 `:40-53` | 确认 |
  | §2.2 `probe()` 四条 `Unavailable` 来源 | 读实现 `:683-729` 全文 | 确认（无第 5 条，`:689-694` 属同一"目录缺失"分支的两个文案） |
  | §2.3 写入两处 / 清空一处 | `Select-String '\bfailed\b'` 全实现 | 确认：声明 `:267`、写入 `:465`/`:551`、读取 `:545`/`:712`、清空 `:617`（refresh 内） |
  | §2.5 逐词错误不写缓存 | `Select-String 'recordFailure\('`（仅 `:460` 定义、`:566` 调用）+ 读 `:761+` 的 convert 路径 | 确认 |
  | §3 lite 不调 `warm()` | 全仓 `Select-String '\bwarm\b'` | 确认：仅 3 处**注释**（`InferEngine.cpp:108`、`LanguageBridge.h:23`、`SynthrtEngine.h:84`） |
  | §3 `setSingerPhonemes` 无应用侧调用者 | 全仓 `Select-String` | 确认：仅 `LanguageBridge.cpp:150-153` 定义 + `TestVoicebankAudit/main.cpp:520,572` |
  | §3 两类禁用逻辑 | 读 `LanguageComboBox.cpp:186-222` | 确认（含未解析早退 `:204-206`） |
  | §1/W-G2 刷新触发链 | 读 `ClipEditorToolBarView.cpp:405-434`、`551-591`。`TrackControlView.cpp:368-399` | 确认：语言刷新唯一入口 `:566`。属性回调 `:417-420` 不刷新 ⇒ 已由"推断"升级为"事实（静态）" |
  | §4 实验记录 | 交叉核早前的缺陷清单 §7（该文已于 2026-10-06 并入 `docs/plans/synthrt-main-migration.md`）与本机临时产物（未入库） | 一致 |

- **自我收窄的两处**（初稿写强了，已改）：① 失败缓存写入不是一处而是两处（`:551`、`:566` 路径），但都在
  executive 建立之前。② W-G2 原先只写"推断"，现以静态调用图为据升级，同时保留"端到端未实测"的尾巴。
- **仍未被任何一方实测的**：①类失败后标灰在真实 UI 中的可见时机（构造不出"包可加载、仅路由层失败"的夹具，
  见 §4 实验 C1~C3 全为加载期失败。本机无 `computer-use`、125% DPI 合成点击不可靠，故无法截图）。

## 9. 复现命令附录

```powershell
# 语言就绪矩阵（7 歌手 × 全部声明语言）
cd <本机临时产物目录（未入库）>
.\gui-smoke.ps1 -OutRoot <本机临时产物目录（未入库）>\gui-probe -LaunchOnly
.\mcp-call.ps1  -OutRoot <本机临时产物目录（未入库）>\gui-probe -Handshake
.\lang-matrix.ps1 -OutRoot <本机临时产物目录（未入库）>\gui-probe

# "尝试一次后再读就绪"（用于验证 §0 的失败缓存路径）
.\lang-attempt.ps1 -OutRoot <本机临时产物目录（未入库）>\gui-probe -LanguageId jpn -Lyric a
```

包内资源损坏夹具（本机临时目录，未入库，含说明）：说明文档 `README.md` 与夹具包 `langdemo@1.0.0.0`
（当前状态：C3 变体——`assets/japanese_dict_full.txt` 已改名为 `.broken`，整包加载失败）。

## 10. wolf 相对 spec 2.4 的加载期合规台账（2026-10-05）

> 状态：**只落盘、暂不改 wolf**（用户 2026-10-05 拍板）。快照：lite `5143d05a`、wolf `3a3f4a0`
> （分支 `linguistic-level-1-v2` ＝ lite 端口 `scripts/vcpkg-ports/wolf/portfile.cmake` 的 `REF` ＝ 实际链接版本）、
> synthrt `63bef25`。
> 方法：两个只读子代理分域取证（F 域＝加载期取得运行时资源，V 域＝声明/exports 校验面与失败语义、诊断），
> 主代理对 F1/F2/F4/V4、V6 机制、覆盖率前缀规则逐条**亲自抽验**。引用行号前必须自行复核（本仓规约：按符号定位，不按行号）。
> **版本澄清（事实）**：`vcpkg/buildtrees/wolf/src/eb5b9772a9-…clean` 与 `<wolf 检出>` 内容一致
> （`git diff --no-index --ignore-cr-at-eol` 对 `include/`+`src/` 零差异，installed 头 SHA256 与 buildtrees 相同），
> 故两棵树不是两个版本，行号对"实际链接版本"与"本地可改仓库"同时有效。⚠️ `Get-FileHash` 会被 CRLF/LF 骗，
> 比对源码必须先归一化换行（本轮踩过这个坑，见 §0 式自我更正要求）。
> **更正既有结论**：§1 的 W-L5「包内资源都在加载期校验」对词典/规则/脚本类变体成立，对 **multig2p-onnx 的模型文件不成立**（见 F1）。

### 10.1 清单（两域合并，判定口径见 §10.3）

| 编号 | 问题 | 条款 | 证据（wolf 侧） | 判定 | 归属 |
| :-- | :-- | :-- | :-- | :-- | :-- |
| **F1** | 包内 ONNX 模型文件在 **Commit 之后**才验证/打开，且**实际可达**：`wolf-lang-*` 的 `pipe-chain` 链把 `wolf/g2p-multi:inference/multig2p` 作为 `model` 步的**后端 import**（`wolf-lang-eng/inferences/g2p/inference.json:44-49`、`:67-72`），由声库语言声明引入（`zzm-kl/linguists/eng-arpabet/linguist.json:56-59` 的 `wolf/lang-eng:inference/g2p`）。**词典未命中（OOV）**时才建该子执行体。模型缺失/损坏 ⇒ 包照样加载成功，且链尾 `fallback`（`useOriginal: true`）把这些逐词失败改写成 `error=None`＋`hitSource=Fallback`＋发音＝原词文本（`chain/main.cpp:734-753`。`eligible()` 明许"失败词仍进 fallback"，`:104-111`）⇒ **静默降级：语言不标灰、无错误、发音变垃圾** | `:425`/`:446`/`:444` | `multig2p/Decoder.cpp:147-180`（`:162` stat、`:167` createSession、`:173` open）。`multig2p/main.cpp:363-369`（运行期才 open）。`chain/main.cpp:757-782`（`resolveBackend` 才 `createChild`）。对照 `s2p/main.cpp:54-58`、`onset/main.cpp:121-122` | **轻不合规/取向差异**（参考实现同样运行期开会话，见 §10.6①）＋ **wolf 内部政策不一致** | **wolf**（仍列修复第一顺位：唯一"可达＋静默＋最小改动"项） |
| **F2** | 驱动/EP 缺失只在运行期以**逐词** `DriverUnavailable` 报告，而驱动可用性在加载期已知（宿主启动期注册）。`fallback` 步还会把失败改写成成功 | `:379`/`:446` | `multig2p/main.cpp:97-105`（逐词写码）、`:353-361`（查 runtime service，注释自述"安装属性"）。`chain/main.cpp:658-667`、`:734-753`（fallback 改写为成功） | 不合规（字面）**但有明示设计理由 A36** | wolf（需产品拍板） |
| **F3** | wolf 文档把"会话期执行体创建失败"称作"**加载失败**"，与规范"加载失败＝事务回滚"术语冲突 | `:444`/`:446` | 文档 `wolf/docs/linguist-decisions.md:753`。实现 `LinguistSession.cpp:564-568`（写失败缓存，包已 Commit） | 口径不合规（措辞） | wolf |
| **F4** | 无法绑定的 linguist import 角色（如 `linguist/onsets`）只 `srtWarning`、import 被忽略 ⇒ 声明与装配不一致的包仍 Commit（少一 stage） | `:426`/`:444` | `WolfLinguistProvider.cpp:217-227`（注释自述"拒绝会改变哪些包可加载，lint 负责拒绝"）。lint `wolf/scripts/check-declarations.py:265-273` | 不合规（弱）/取向差异 | wolf（改变存量包加载结果，需先扫描） |
| **F5 ＝ V6** | "语言映射未在加载期验证"只在"本 SynthUnit 从未创建过 wolf linguist interpreter"时成立 | `:444`/`:425` | wolf 自述 `LinguistSession.cpp:96-100`，机制见 §10.3 | **未证实可达**（当前单 linguist provider 下不可达） | 跨仓（synthrt/dsinfer），**本轮结论：不必改** |
| V1 | 校验面与阶段基本正确：声明/exports/configuration 在 Acquire。import options/binding 与 imports 集合校验在 Ready pass1/2 | `:425`/`:426` | `WolfLinguistProvider.cpp:406-451`、`:181-256`、synthrt `PackageLoader.cpp:833/845/994-1005` | **合规（正向）** | — |
| V2＝F4 | 同上（同一问题的两个编号，此处不重复） | — | — | — | — |
| V3 | role 目标只比 interface、不比 level/variant，之后也无 exports downcast 检查 | `:426` | `WolfLinguistProvider.cpp:162-177`（`:168`）、`:141-160`（`:150`） | 未证实/未来风险（当前全部契约仅 Level 1） | wolf（预防性） |
| V4 | chain↔backend 的 pair 兼容性延到首次转换。框架 `validateCompatibilityWith` **从不被加载器调用** | `:425`/`:446` | `chain/main.cpp:413-452`、`:770-771`。`multig2p/main.cpp:344-351`（"this bundle maps no language to X/Y"）。全 synthrt 仅 dsinfer 自家 `DiffSingerProvider.cpp:217` 与测试调用该 hook | 取向差异（可判轻不合规） | wolf（＋synthrt 调 hook 可选） |
| V7 | 失败缓存无 per-entry 清除、无重试。`release()` 故意保留 | 规范未规定 | `LinguistSession.cpp:459-468`、`:549-553`、`:564-567`、`:616-617`、`:667-673` | 取向差异 | wolf（可选） |
| V8 | `probe()` 的 reason 只回放 `Error::message()`，丢 code 与 cause 链 | 诊断取向 | `LinguistSession.cpp:710-716`、synthrt `Error.h:103-108`、`:171` | 取向差异 | wolf＋lite（可选） |
| V9 | `Cold` 不保证资源可加载，宿主视为可用（wolf 明文） | `:379`/`:444` 取向 | 头文件 `LinguistSession.h:40`、`:47-50`，`LinguistSession.cpp:728`，lite `LanguageBridge.cpp:134-135`、`:144-147`（无 warm） | 取向差异（已文档化）＋宿主消费 | wolf 已明文 / lite（W-L6） |
| V10 | 声明 inventory 与实际产出（符号越界）无任何运行期检测 | 规范未规定 | `LinguistApiL1.h:57-64`、`G2PApiL1.h:70-77`、`LinguistSession.cpp:280-304`、`:717-727` | 取向差异 | — |
| V11 | 加载期**无**线程/异步（正向，满足 `:433`/`:436`）。但资源缓存的等待方在别的加载线程上无超时阻塞 | `:433`/`:436` | `ExecutiveTask.h:16-25`、`:53-71`、`:98-117`、`ResourceCache.cpp:143-177`（`:165-171`） | 合规 + 低风险取向差异 | 框架级 |

### 10.2 主代理亲自抽验（对抗纪律：子代理断言必抽）

| 抽验项 | 结果 | 证据（我实读） |
| :-- | :-- | :-- |
| F1：multig2p 在 Acquire 只读 bundle/vocabulary，模型在运行期开 | **成立** | `multig2p/main.cpp:269-302`（Acquire 读+校验）vs `:363-369`（运行期 open）。`Decoder.cpp:162-176`（FileNotFound/FeatureNotSupported 只在 open 时报） |
| 对照：wolf 多数派政策是加载期失败 | **成立** | `s2p/main.cpp:54-57` 原文 "a malformed dictionary must fail the package rather than the first conversion that uses it"。`onset:121-135`、`chain:148-163`、`lua:107-129` 同政策 |
| F4：未知角色只告警 | **成立** | `WolfLinguistProvider.cpp:217-227` |
| V4：`validateCompatibilityWith` 无人调用 | **成立** | 全仓 grep：仅 `dsinfer/plugins/singerproviders/diffsinger/DiffSingerProvider.cpp:217` 与两个测试，定义为 `synthrt/include/synthrt/SVS/InferenceContrib.h:21` |
| V6/F5 机制：校验器跨事务累积 | **成立** | `synthrt/lib/Core/ContribPluginFactory.cpp:115-131`（创建 interpreter 时把 validator 追加到**工厂级** `m_importValidators`，工厂活在 `SynthUnit::_impl`）＋ `PackageLoader.cpp:925-942`（Ready pass2 对本事务每条 contribution 跑**全部已累积**校验器） |
| G3：覆盖率逐字比对、表 key 带 `<lang>/` 前缀、空表=0 覆盖 | **成立** | `LinguistSession.cpp:290-303`。`zzm-kl/inferences/acoustic/phonemes.json` 200 键中 186 带前缀（`eng/aa`、`cmn/A`），14 个为无前缀保留标记。`:283-288`（无表⇒Unknown）、`:732-749`（只认当前 catalog） |

### 10.3 V6/F5 机制详解，以及"必须改 synthrt 吗"（本轮结论：**不必**）

**机制（事实）**：synthrt 的校验器不是"按事务收集"，而是**按 interpreter 创建事件累积**——
`ContribPluginFactory` 在创建某个 (interface, level, variant) 的 interpreter 时调用其
`createImportValidators()`，并把结果**永久**并入工厂级列表（`ContribPluginFactory.cpp:115-131`）。
Ready pass2 对本事务（跳过已加载包）的**每一条** contribution 依次运行**当前已累积的全部**校验器
（`PackageLoader.cpp:925-942`）。工厂属于 `SynthUnit::_impl`，故其生命周期＝该 SynthUnit 的进程生命周期。

**推论**：wolf 自述的"本次事务没有创建 wolf provider ⇒ 语言映射未在加载期验证"（`LinguistSession.cpp:96-100`）
成立的前提是"**这个 SynthUnit 从未创建过任何 wolf linguist interpreter**"。lite 当前只有 wolf 一个 linguist
provider，且实测的声库都声明 linguist 路由 ⇒ **该前提在本机不可达**（子代理两轮尝试构造可达路径均失败）。

**即使可达**（某语言的 role 指向**非 linguist** 贡献的缺陷包）：结构面已由 **singer 类别**校验（role 必须存在于
imports、`defaultLanguage ∈ keys`。wolf 自述 `wolf/src/lib/Linguist/SingerLanguages.cpp:13-15`，V 域复核
`synthrt/…/SingerContrib.cpp:43-63`、`:66-82`）。按 `:426`，拒绝这种组合的责任在 **importing module 的 provider**
（singer provider ＝ dsinfer 的 `DiffSingerProvider`），而我抽验该 provider 的校验器**只覆盖
`singer/duration|pitch|variance|acoustic|vocoder` 五个推理 role**（`dsinfer/plugins/singerproviders/diffsinger/DiffSingerProvider.cpp:186-215`）。

**结论**：**不必修改 synthrt**。三条理由：① 可达性未证实，且真出现时属"缺陷包"级窄场景。② 若将来要闭环，
最小改动点是 dsinfer 的 DiffSinger 校验器补一条"语言 role 的 import 目标必须是 linguist 类别"，
**不需要**改 loader/工厂的校验器收集机制（后者会从"按需累积"退化为"向所有已安装插件索取"，代价更大）。
③ lite 对该场景**已优雅降级**：该语言被标灰并显示 wolf 的精确原文（`LinguistSession.cpp:109-114`），
缺的只是"按规范本该在加载期失败"。
**重新评估的触发条件**：出现非 wolf 的第三方 linguist provider 参与加载。或实测到"包加载成功但语言映射未验证"的实例。

### 10.4 若要动手：优先级与代价（本轮**未做**，仅登记）

| 顺序 | 项 | 做法 | 代价/风险 |
| :-- | :-- | :-- | :-- |
| 1 | F1 | 在 multig2p 的 `createConfiguration`（Acquire）对 `Bundle` 列出的每个模型做只读校验（`is_regular_file` + 可读），把"缺失/不可读"提前为加载失败。**不**在 Acquire 建 ORT 会话 | 每包 4 次 stat（可忽略）。须复用 bundle 的路径解析并与运行期 open 判定口径一致 |
| 2 | F3 | 文档把"加载失败"改词为"会话期执行体创建失败" | 零代码风险 |
| 3 | F2 | 二选一：(a) 保持现状 + 在契约/文档声明"驱动缺失＝安装降级、不属加载事务"（对齐 `:442` 执行域口径）。(b) Acquire 期查驱动服务并判加载失败（代价：无驱动机器上 9 个语言包同时不可加载，正是 A36 反对的） | **需产品拍板**，建议 (a) |
| 4 | F4 | 未知角色由 warning 升为 `InvalidFormat`，先跑 `scripts/check-declarations.py` 拿存量清单 | 会改变现有包的加载结果，须与 language 包子仓同步 |
| 5 | V4 | chain 侧补 pair 兼容性校验（或在 Ready 调 `validateCompatibilityWith`） | 拦下"能加载但必失败"的包（目的），两仓同步 |
| — | F5/V6 | 见 §10.3：不必做 | — |

**消费约束（事实）**：wolf 改动要进 lite，需要 push wolf + bump
`scripts/vcpkg-ports/wolf/portfile.cmake` 的 `REF`（当前 `3a3f4a09…`）+ 重装该端口。端口经
`vcpkg_from_git(URL https://github.com/diffscope/wolf.git)` 取源，且**代理不得执行 push**。

### 10.5 未证实清单（不得当缺陷排期）

- F1 的端到端行为**已实测复现**（见 §10.7）：包照常加载、语言全部就绪、词典未命中的词被 `fallback` 静默改写为原词文本。
- F2 的"无驱动"生产路径**已实测**（见 §10.8）：结论与预期相反——不是"加载期失败 ⇒ 标灰"，而是**逐词静默降级**，就绪矩阵前后全 `True`。
- V3 在当前契约集合（全为 Level 1）下不可达，属未来风险。
- V7/V11 未实测复现。F5/V6 的第三 provider 场景未构造。
- wolf CI 是否把 `check-declarations.py` 设为发布门禁未核实。

### 10.6 反证据与判定修正（2026-10-05 追加）

**① F1 的对照证据：参考实现同样"运行期开会话"。** dsinfer 自家五个模型解释器全部在**任务期**创建并打开 ORT
会话，加载期只读配置：`AcousticTask.cpp:152-155`、`DurationTask.cpp:169-181`、`PitchTask.cpp:120-132`、
`VarianceTask.cpp:139-151`、`VocoderTask.cpp:110-113`（均为 `m_driver->createSession()` + `session->open(config->…, …)`）。
⇒ "模型会话不在加载期打开"是**生态既有取向**，不是 wolf 的孤例。可判之处收窄为两点：规范 `:379` 把"模型、设备等
运行时资源"列为加载期资源。且 wolf 自家**数据文件**变体（s2p/onset/chain/lua）确实在 Acquire 校验包内文件
⇒ F1 的准确判定是 **轻不合规/取向差异（wolf 内部政策不一致）**，而非"wolf 独有违规"。若修，最小改动仍是
"Acquire 期只读校验（stat/可读），不建会话"。**优先级仍列第一顺位**——它是唯一"可达＋静默＋改动最小"的项（见 ②）。

**② F1 可达（自我更正：我先前的"本机不可达"结论是错的）。** 那个结论建立在一次**输出被截断**的 grep 上
（`Select-Object -First 8` 把 `linguists/*/linguist.json` 的命中挤出了屏幕，我照屏幕内容下了结论）。用户指正后复核：
- 声库的语言声明引用**外部语言包**的 G2P：`zzm-kl@1.0.0.0/linguists/eng-arpabet/linguist.json:56-59`
  `{"role": "linguist/g2p", "ref": "wolf/lang-eng:inference/g2p"}`（`jpn-romaji`、`cmn-pinyin` 同构）。
- 被引用的链把 multig2p 作为 `model` 步的**后端**：`wolf-lang-eng/inferences/g2p/inference.json:44-49`
  （`{"step":"model","params":{"role":"backend","batchSize":20}}`）＋ `:67-72`
  （`"imports": [{"role":"backend","ref":"wolf/g2p-multi:inference/multig2p"}]`）。
- 步骤序：`verify → dict(ds_cmudict-07b.txt) → format(lowercase) → dict → model(role=backend) → fallback(useOriginal)`。
⇒ **multig2p 是这些声库"词典未命中"路径上的隐式后端**（用户原话："几个声库在词典查不到的时候都在用"），
且失败被链尾 `fallback` 静默改写（§10.1 F1 行）。教训：**结论不得建立在被截断的输出上**。跨包引用要顺
role→ref 的绑定链去追，不能只搜字面包名。
⇒ 由此，F1 的端到端实测在本机**可行**（改坏 `wolf-g2p-multi/inferences/multig2p/*_int8.onnx` + 输入英文 OOV 词）。

### 10.7 F1 端到端实测（2026-10-05，已复现）

**口径先纠一处（否则会得到假阴性）**：运行期读的**不是** `vcpkg/installed/...`，而是**部署副本**
`build/Debug/out/bin/wolf/packages/…`（`DeployLayout.h` 的 `LANGUAGE_PACKAGES_DIR = "wolf/packages"`。
`SynthrtBootstrap.cpp:45-52` 用它组装类目目录）。两侧当前逐文件 SHA256 一致（7/7），故以 `vcpkg/installed`
为**金样本**、只改部署副本即可，且可事后逐文件比对还原。

**夹具与观测**：新增只读驱动脚本（本机临时脚本，未入库，复用 L3 MCP 通道）：同一次运行、同一 clip 内插两个
英文音符——对照词 `hello`（词典命中，`Select-String '^hello\s'` 命中 1 行）与探针词 `zzqx`（**0 命中**，
OOV。`verify` 步正则 `([A-Za-z'\-]+)` 通过）——随后读 `notes.list` 的音素与所有歌手的 `g2p_ready`。

| 条件 | 对照 `hello` | 探针 `zzqx`（OOV） | 就绪矩阵 |
| :-- | :-- | :-- | :-- |
| 模型完好 | `hh ax l ow`（词典） | `z iy k s k eh k s`（**multig2p 模型产出**，证明 OOV 走 model 步） | 7 歌手 × 语言 全 `True` |
| 把部署副本的 `encoder_int8.onnx` 改名为 `.broken`，**冷启动** | `hh ax l ow`（**不受影响**：词典路径不碰模型） | **`zzqx`**（发音＝原词字面文本，即 `fallback(useOriginal)` 把 `ModelInferenceFailed` 改写成 `error=None`） | **仍全 `True`（包照常加载、无任何预检）** |

**读出的结论（实测，非推断）**：
1. **F1 成立且后果是静默的**：包内模型缺失**不使加载失败**（`:425`/`:446` 的字面不合规有实测支撑），语言**不标灰**、
   界面**不报错**，用户只看到词典未命中词的"发音"变成原词文本（后续送进声学模型即为垃圾音素）。
2. **影响面＝词典未命中路径**：命中词完全不受影响。即 9 个语言包（deu/eng/fil/fra/ita/kor/por/rus/spa）的 OOV 词。
3. **逐词失败不进 wolf 失败缓存**（§2.5 的边界在实测中保持）：就绪矩阵前后都全 `True`，故 lite 侧不会也不应
   因此标灰——与用户 A1 的"只禁用真坏的语言"取向一致。
4. 这条路径同样解释了此前观察到的逐词 `DriverUnavailable`（驱动/EP 缺失时 `multig2p/main.cpp:353-361` 给的
   执行体只报逐词错）——同属"运行期资源缺席 ⇒ 逐词降级"这一类。

**还原证据（已执行）**：`.broken` 改回 `encoder_int8.onnx` 后，部署副本与 `vcpkg/installed` 原件**逐文件 SHA256
全部一致（7/7，0 不一致/缺失）**、无 `*.broken` 残留、无残留 `DsEditorLite` 进程。运行日志与截图：
本机临时产物（未入库），按「模型完好」与「模型损坏」两轮分别留存，各含 `shots/` 截图。

**未测**：~~驱动/EP 缺失（如 ORT 配置错误）时的表现~~（**已由 §10.8 补测**：不是"同路径"，而是**同为逐词静默降级**）。其它 8 个
语言包的 OOV 词未逐一复跑（机制同构，eng 已足以定案）。

### 10.8 F2 端到端实测：ONNX 驱动缺失（2026-10-05，已复现）

**打靶方式**：把部署副本 `build/Debug/out/bin/plugins/dsinfer/inferencedrivers/onnx/onnxdriver.dll` 改名为
`.broken` 后**冷启动**。这一改同时切断了 ORT 的来源——`out/bin` 全树只有
`plugins/dsinfer/inferencedrivers/onnx/runtime/onnxruntime.dll`，而该目录是靠驱动加载成功后被加进搜索路径的。
观测面：应用自身日志、`voices.describe` 就绪矩阵（探针前/后）、`notes.list` 音素。

| 观测面 | 结果 |
| :-- | :-- |
| 能否启动 | **启动成功**。日志一条 `SynthrtEngine: no ONNX driver was found; inference is unavailable`（与 `SynthrtEngine.cpp:233-238` 的"降级而非失败"注释一致） |
| 探针前就绪 | 7 歌手 × 各自语言 全 `True` |
| 词典命中词 `hello` | `hh ax l ow`（**不受影响**：`dict` 步不需要模型） |
| 词典未命中词 `zzqx` | **`[zzqx]`**（原词字面文本，与 §10.7 同形：链尾 `fallback(useOriginal)` 静默改写） |
| 探针后就绪 | **仍全 `True`**：无失败缓存命中 ⇒ 不标灰，语言下拉不变 |

**结论（实测）**：
1. F2 预期的"无驱动 ⇒ 加载/路由期失败"**不成立**：真实行为与 F1 **同类**——逐词失败 + 链尾静默改写。
   机制上也自洽：后端执行体是在**运行期**由 `chain/main.cpp:757-782` 创建的，而 multig2p 的会话/驱动失败被记成
   **逐词**错误（`multig2p/main.cpp:353-369`、`:97-105`），**不写**失败缓存——`LinguistSession.cpp:564-567`
   是唯一写入点，只在路由/执行体层失败时命中。
2. ⇒ **lite 的 D4 信号在此场景正确地保持静默**（`languageRouteFailed` 只在路由层失败时发）。用户 D2 的取向
   （逐词失败不标灰）被实测支持。
3. ⇒ **可达的"路由/执行体层失败"家族比想象中窄**：主要是打包/路由类缺陷（语言路由指向非 linguist 贡献、贡献
   缺失、变体不受支持等），而"资源缺失"这一类几乎都落进逐词家族。
4. 用户可见后果：发音退化为原词文本、**无任何错误提示、语言不标灰**。该议题属 §6 G1/G5 的产品决策，本轮按
   用户裁定**不改 wolf**。

**还原证据（已执行）**：`onnxdriver.dll` 改回后 SHA256 前缀 = `C3603EC1EDB3`（与改动前记录一致）、
无 `*.broken` 残留、无残留 `DsEditorLite` 进程。wolf 包目录仍与端口原件逐文件一致（7/7）。
运行日志与截图：本机临时产物（未入库，含 `editor.out.log` 与启动截图 `01-startup.png`）。

### 10.9 D5 消费侧闭环：把音素表喂给 wolf（2026-10-05，已实施并验证）

**动机**：§6 G3——wolf 的覆盖率诊断要求宿主提供歌手能发的音素集合（`LinguistSession.h` 明写 "wolf does not
read voicebank formats"），而 lite 生产路径**从不调用** `setSingerPhonemes` ⇒ 覆盖率恒 `Unknown`，覆盖率一族诊断
（含"精确零覆盖 ⇒ `Unavailable`"）在 lite 不可达。

**数据源（本轮证实的关键事实）**：表就在 dsinfer **解释后**的配置里——`config.json → configuration.phonemes`
指向的表已被解释成 `std::map<std::string,int> phonemes`（`Api/Inferences/{Acoustic,Duration,Pitch,Variance}/1/*ApiL1.h`），
而 `VoicebankCatalog::capabilitiesOf()` 本来就握着这四个 target 指针。**副产品证据**：`0913_wolf_club` 的四张表叫
`0913_mulaw_{as,dur,pit,var}.phonemes.json`（**不含**固定名 `phonemes.json`）⇒ **配置驱动**比按文件名猜稳。
测试里原有的 `phonemes.json` / `*.phonemes.json` 启发式只是测试侧的独立读数手段，不是生产路径。

**三条硬规则（缺一即误诊）**：① **必须去掉第一个 `/` 之前的部分**（真实表的 200~222 个键里绝大多数带
`<lang>/` 前缀。不去前缀 ⇒ wolf 逐字比对全不命中 ⇒ 覆盖率 0 ⇒ **Exact 语言被假判 `Unavailable`**）。
② **绝不喂空表或空交集**（wolf 会把"零覆盖"当真）。③ 只在**每次 `language->refresh()` 之后**喂
（表只对刷新后仍在新 catalog 里的歌手保留）。

**实现落点**：`VoicebankCatalog.h` 的 `SingerCapabilities::singablePhonemes`（派生字段，空 = 无声明）+
`.cpp` 的 `barePhonemeKeys` / `commonPhonemes`（跳过空表、其余求交）/ `feedSingers`（跳过空表与无语言的歌手）/
双入口 `feedSingerPhonemes(wolf::LinguistSession &| LanguageBridge &, catalog)`、`SynthrtEngine.cpp` 的
`Impl::feedSingerPhonemes()` 在**两处** refresh 之后调用（`start()`、`refreshVoicebanks()`），两处都在 lifecycle
独占锁内调用、wolf 侧自带同步。测试（`TestVoicebankAudit`）改为走**生产入口**喂表，并新增三条断言：独立读数
（自己读四张表求交）**相等**、逐模型**子集**必要条件（不重复交集算法）、**前缀规则钉子**（派生集合无带 `/` 的
key，且某个带前缀 key 的裸 token 必在派生集合里）。

**主代理独立复算（不依赖子代理数字，2026-10-05）**：

| 声库 | 四张表（键数 → 去前缀） | 交集 | 覆盖率 |
| :-- | :-- | :-- | :-- |
| `0913_wolf_club` | 4× `0913_mulaw_*.phonemes.json` 222 → 138 | 138 | cmn/eng/jpn/yue 全 **100%**（eng 为 openSet） |
| `junninghua-2.4` | 4× 149 → 103 | 103 | cmn 100%、jpn 100%、**yue 52.7%**（缺 35，真实缺口） |
| `yousa-2.4` / `zzm-kl` | 4× `phonemes.json` 200 → 117 | 117 | cmn 98.4%、jpn 97.4%（**缺 `um`**）、eng 100%（openSet）。zzm 另 qzz 97.4% |

**验证（已执行）**：全量构建 `exit 0`。全量 `ctest` **73/73 通过**（71.5 s）。其中 `TestVoicebankAudit` 的输出从
"覆盖率恒 `Unknown`"变成**实测值**——`cmn/pinyin coverage exact 98%`（unsingable: `um`）、
`eng/arpabet ... open set, coverage at least 100%`、`jpn/romaji exact 97%`（unsingable: `um`），且**没有任何
Exact 语言被判 `Unavailable`** ⇒ 与"本机无零覆盖语言"的预期一致，**用户可见行为不新增标灰**。

**诚实记录的局限**：① 测试侧独立读数用**文件名启发式**，若某包的 `configuration.phonemes` 指向别的名字，
独立侧会漏表而生产侧不会（等价性只在本机语料上成立）。② 两条读数共享"交集算法同构"，真正被独立验证的是
**数据源与去前缀规则**。③ 派生集合是**语言无关**的（wolf API 只接受"一个歌手一张表"），因此"某语言的 token
借另一语言前缀的条目获得覆盖"在符号层面成立（同符号通常同义。若打包方给同符号不同 token id，会少报缺口）。
④ `um` 与 yue 52.7% 是**真实缺口**，而 wolf 只在覆盖率**精确为零**时判 `Unavailable`，所以它们不标灰。

### 10.10 本轮实施与双侧验证：F1 + V4-a/V4-b/V8-a（2026-10-05，已实施并验证，wolf 侧已本地提交）

**裁定回顾**：范围＝"全量推荐档"（F1 加载期模型校验、V4-a 结构性失败、V4-b 失败缓存、V8-a reason 渲染、
F3/V7-a 文档口径）。失败语义＝"允许整包失败，但只是依赖 multi g2p 的不可用"。验证路线＝A（本地重装 wolf 端口）
+ 三组 lite 端到端实验 + **本地**提交（全程未 push）。

**wolf 侧落点（7 个本地提交，`linguistic-level-1-v2`，HEAD `8749bb2`）**

| 提交 | 改动 | 关键落点 |
| :-- | :-- | :-- |
| `9565c92` | multig2p 加载期模型校验 | `Decoder::resolveModels()/verifyModels()`，Acquire（`createConfiguration`）内调用 |
| `b490509` | chain 结构性失败 | `runModel` 在后端建不起来时返回执行体级错误（**有 eligible 词才失败**，无 eligible 词照旧成功，收到 stop 返回已完成部分） |
| `65db697` | 会话缓存 + reason 渲染 | `convert()` 整批失败写失败缓存（键=歌手+语言，只由 `refresh()` 清除、`release()` 保留）。`describeFailure()` 渲染"错误码 kind + 整条 cause 链" |
| `26b61f1` | 文档口径 | A36 术语/后果分离、A58 与 §4 规则 2/3、§11 新增"重扫声库 → `refresh()`"行 |
| `2872a26` | 测试 10 条 | `test_MultiG2P`×6、`test_LinguistSession`×4。相对 `3a3f4a0` 为**纯插入** 571 行、0 删除（`git diff --numstat 3a3f4a0..HEAD -- src/tests`：406/0、165/0）。F1 用例遍历 `bundle.json` 声明的**全部三个**模型并在还原后复载 |
| `172475c` | 复核修补（会话） | ①取消与失败竞态：`!converted` 分支先判 `token.cancelled()` ⇒ 返回会话已解析的词、**不写失败缓存**（此前"用户点停止"可能把该语言永久判死到下次重扫）。②词数不匹配分支改为经 `recordFailure()` 写缓存，与 `LinguistSession.h:241-245` 的措辞一致。两处都以"运行是否被停止"为前提 |
| `8749bb2` | 复核修补（multig2p） | `open()` 由"两个平行数组按位置配对"改为**按逻辑名取槽**：位置配对下重排 `LOGICAL` 会静默把模型装进同类型的另一个槽，编译与测试都不报。现在名字无槽可配即返回 `InvalidFormat` |

**验证 1：wolf 自带套件全绿，且新用例有转红证据**

- 全量构建 `exit 0`（0 编译错误）。`ctest` **20/20 通过、0 跳过**（含 MultiG2P `/` PipeChain `/` LinguistSession `/`
  ConvertedPackages `/` HostFlow）。
- **复现环境（必须照抄，否则假绿或全红）**：① `WOLF_TEST_FIXTURES_SOURCE=<wolf 检出>/build/test-fixtures`
  （缺它时 8 个数据相关测试 exit 77 被记为 **skipped** ⇒ 假绿）。② Boost.Test 的 Debug DLL
  （`vcpkg/installed-extra/x64-windows/debug/bin`）与调试 CRT（`VC/Redist/MSVC/<ver>/debug_nonredist/x64/Microsoft.VC145.DebugCRT`）
  都在 PATH（缺任一个 ⇒ 20 条全 `0xc0000135`）。已固化为一份本机临时脚本（未入库）。
- **转红（改前红/改后绿）**：把 `chain/main.cpp` 与 `multig2p/{main.cpp,Decoder.h,Decoder.cpp}` 取回 `3a3f4a0` 版本重建 ⇒
  A1/A2/B3/F1 **四红**（A1/A2/B3 因旧行为"成功 + fallback 改写原词"、F1 因"旧行为加载成功"），B1/B2 **两绿**
  （钉住"该步无 work 就不报错"）。把 `LinguistSession.{h,cpp}` 临时回退 ⇒ V4-b/V8-a **两红**，逐词失败与取消
  两条反例仍绿。回退均按哈希逐字节复原。
- **复核修补后复跑**（HEAD `8749bb2`）：全量构建 0 错误 0 警告，`ctest` **仍 20/20、输出无 skipped 行**。会话五条关键用例
  逐条单跑（`RemembersABatchFailure` / `KeepsAPerWordFailure` / `KeepsACancelledConversion` /
  `ReplaysACachedFailure` / `HonoursACancelledToken`）全部 `exit 0`。`test_MultiG2P` 全量 158/158 断言通过，
  其中真开模型那条的推理序列为 `encoder_int8 → decoder_step_init_int8 → decoder_step_int8`（按名取槽的实证：
  三个模型各进各槽）。**口径说明**：`20/20` 是 ctest 用例数（每个测试可执行文件一条），不是 Boost case 数——
  `test_MultiG2P` 与 `test_LinguistSession` 当前分别有十多个与 26 条 case。"0 跳过"是**环境相关**结果（需三个
  `WOLF_*` 变量 + 脚本化变体齐备），缺变量时相关测试 `exit 77` 被记为 skipped，故不是静态保证。

**验证 2：lite 端到端三组实验（部署副本，破坏前后留哈希、事后逐文件还原）**

观测面：应用日志（`QT_LOGGING_TO_CONSOLE=1`）、`voices.describe` 就绪矩阵（本机临时脚本，未入库）、
`notes.list` 音素。实例用本机临时启动脚本（未入库，隔离 APPDATA + `--mcp`）。

| 组 | 打靶方式 | 结果 |
| :-- | :-- | :-- |
| 基线 | 无 | 7 歌手全 `True`、`hello→hh ax l ow`、`zzqx→z iy k s k eh k s` |
| F1 加载期 | **启动前**把 `decoder_step_int8.onnx` 改名 | 三个依赖 multig2p 的歌手包**整体加载失败**：`failed to interpret module configuration: the bundle's decoder_step model is missing: <path>`。未依赖的 `Junninghua`(cmn/jpn/yue) **照常可用**（爆炸半径＝约定范围） |
| V4-a/b/8-a 会话期 | **启动后**把同一模型覆写成 1 KB 垃圾（内容损坏。加载期只查存在性，故加载通过） | 转换**整批失败**、两个音符 `phonemes=[]`（**不再**静默回退成原词字面）。日志给出完整因果链 `the backend of the model step "backend" could not be built: cannot open the decoder_step model: … Protobuf parsing failed.`。**只有 `yelin` 的 `eng` 变 `False`**，同歌手 cmn/jpn/yue 与其他 6 个歌手保持 `True`。二次转换 `onnxdriver … Try open` 增量 **0**（不重试）。`packages.refresh`（重扫声库）后**同一实例** `eng` 回 `True`、音素恢复 ⇒ V7-a"重试入口＝重扫"成立 |

**验证 2 补充（针对 V4-a 的反例检查）**：把 ONNX 驱动 `plugins/dsinfer/inferencedrivers/onnx/onnxdriver.dll` 移走后
冷启动重跑同一探测 ⇒ 仍是 `hello→hh ax l ow`（词典）、`zzqx→[zzqx]`（逐词失败 + 链尾兜底）、`eng` 保持 `True`、
日志只有启动期一条 `no ONNX driver was found; inference is unavailable`，**没有** `could not be built` ⇒ V4-a **没有**
把 §10.8 的"无驱动＝逐词失败、不标灰"家族升级成整批失败。**已验证的两支**：驱动服务缺失（`runtimeService()`
为空）＝逐词失败、不标灰。**模型文件缺失/内容坏/pair 查不到**＝整批失败 + 路由标灰。**未证实的第三支（复核反例 1，
据实修正此前的"两族边界成立"）**：`Decoder::open` 失败与"驱动服务缺失"是**两个不同分支**，凡是走到 `open` 的**环境型**
失败（显存/内存不足、DML 等 EP 初始化失败、provider 库按会话加载失败）同样落进"整批失败 + 标灰"，此时相对旧行为是
**从逐词兜底变为整条语言标灰**。此支属**推断**（路径存在、触发条件未实测），需在真实缺设备/资源受限机器上复现后
判定。若成立，候选改法是让"驱动型 `open` 失败"按 `DriverUnavailable` 逐词上报。另：此处曾引用代码注释
（"the fault lies in the package rather than in the installation"）当证据，属**循环举证**（注释是作者意图，
不是行为保证）——本节的证据是上面两组端到端实验，注释仅作旁证。

**机制级核对（主代理亲自抽验，不只依赖端到端观测）**

- **失败分层是代码里显式设计的**（`multig2p/main.cpp:342-385`）：语言表查不到 ⇒ 创建期错误
  `FeatureNotSupported`（`:357-363`，注释"an error rather than a case for substitution"）。**驱动服务缺失 ⇒
  返回 `decoder == nullptr` 的合法执行体**（`:369-374`），由 `:97-105` 逐词报 `DriverUnavailable`
  （注释写明"allows a chain to fall back instead of the whole language failing to load"）。驱动在但模型开不了 ⇒
  创建期错误（`:376-382`）。⇒ V4-a 升级的是**走到 `Decoder::open` 的整批失败**（丢模型/内容坏/pair 查不到，
  以及"驱动在但会话建不起来"的环境型失败——见上文未证实支）。**驱动服务缺失**仍是逐词兜底。注意：代码注释记录的是
  作者意图，只能当旁证，行为结论以上面的端到端实验为准。
- **失败缓存键 = `(SingerKey, language)`**（`LinguistSession.cpp:285` 的
  `std::map<std::pair<SingerKey, std::string>, srt::Error>`，构造在 `:573`）⇒ **语言级粒度**，正是用户要求
  "声库只有一个语言的 g2p 损坏，也只禁止那一个语言"的粒度。**撤回此前一句推测**：原文写的"代价是同语言的另一个
  scheme 会被一并判死"两个方向都不成立——键的第二元素是**调用方传入的 language handle 原样字符串**（与
  `LanguageEntry::handle` 的比较见 `:567`/`:727`），scheme 由 `binding(handle)` 决定，不同 handle 就是不同条目，
  互不牵连。同一 handle 只对应一个 binding/scheme，也谈不上"另一个 scheme"。`acquire()` 先查缓存
  （`:574-576`）⇒ 失败后该路线在 `refresh()`（清空点 `:646`）之前不可用，因此不存在"失败条目残留、后续又成功"的
  矛盾状态。写入点共 3 处（`:580` 建槽失败、`:595` 建执行体失败、`:488` 经 `recordFailure`，含本轮新增的转换期
  整批失败与词数不匹配，后者见 `172475c`）。
- **`LOGICAL` 与槽位的配对（复核反例 5，已修）**：原实现把"逻辑名列表"和"成员指针数组"按**位置**配对，
  `static_assert` 只对齐个数 ⇒ 重排 `LOGICAL` 会把模型装进同类型的另一个槽，编译通过、测试也不报。`8749bb2`
  改成**按名取槽**，名字找不到槽即返回 `InvalidFormat`。
- **加载期校验只对新实例生效（复核反例 8）**：`synthrt/lib/Core/PackageLoader.cpp:814-818` 对已 `loaded` 的包直接
  跳过，全仓唯一的 `createConfiguration` 调用点在 `:845`（Acquire 内）⇒ 同一进程内二次 `load` 不会重校验。
  这正好解释了实验 C（加载成功后破坏磁盘文件）为何必然走**会话期**路径：加载期校验只能兜住"加载那一刻"。
- **校验点位置改变了诊断优先级（复核反例 9，事实/低危）**：模型校验排在 vocabulary（`main.cpp:302`）与
  languageMap 解析（`:320-327`）**之前**，同一包既缺模型又缺语言引用时现在先报模型错。两者都是加载失败、包一律被拒，
  且未找到"必须先报语言引用"的规范依据，故不改，仅记录。

**用户可见面映射（三个失败家族 → 三种 lite 呈现，含两端代码证据）**

| 家族 | 触发点 | lite 呈现（代码证据） | 本轮实测 |
| :-- | :-- | :-- | :-- |
| **包加载失败**（F1：缺模型） | 加载期 Acquire 拒绝 | 歌手菜单尾部"⚠ `<包>` — Unable to load"**禁用行**，tooltip = loader 原因**原文透传**（`SingerMenuUnavailablePackages.h:26-43`，注释明写"passed through unchanged"，单测 `TestSingerMenuDisplay`） | 实验 B：依赖包整体消失于歌手列表，日志原文 `failed to interpret module configuration: the bundle's decoder_step model is missing: <path>` |
| **路由/执行体故障**（V4-a/V4-b：模型坏、pair 查不到、建槽或建执行体失败） | 转换期整批失败 → 写失败缓存 → `probe()` 报 `Unavailable` | 语言下拉该项**禁用** + tooltip = 引擎原文（`LanguageComboBox.cpp:159-163`、`:186-223`），当前语言被禁时下拉框自身 tooltip 也给原因（`:165-184`） | 实验 C：只有 `yelin` 的 `eng` 变灰，其余语言/歌手不受影响。重扫后恢复 |
| **逐词失败**（无驱动、单次解码失败） | 运行期 | **不标灰**，链尾 `fallback(useOriginal)` 兜底 | 反例检查：`zzqx → [zzqx]`、`eng` 保持 True |

**对抗复核轮（只读子代理，HEAD `2872a26` 快照）与主代理裁定**

复核只读、未改仓库、未构建，交回 11 条候选反例 + 13 条"站得住"的断言。逐条裁定如下（**采纳=已改**）：

| # | 复核结论 | 主代理裁定 |
| :-- | :-- | :-- |
| 1 | "两族边界成立"过强、用被审代码的注释当证据＝循环举证 | **采纳**：改写"验证 2 补充"与机制级核对，并新增"未证实的第三支"（环境型 `Decoder::open` 失败） |
| 2 | "无驱动 ⇒ 逐词"成立（补 lite 侧 `SynthrtBootstrap.cpp:84-89`：找不到 loader 即不注册服务、启动照常） | 与主代理抽验一致，补证 |
| 3 | 取消可被写成永久路由失败（`!converted` 不检查 `token.cancelled()`） | **采纳并修复**：`172475c` |
| 4 | `describeFailure()` 去重只对最外层成立，链上仍可能重复 | **推断/未证实可达**（生产代码未见单参数 `Error(ErrorCode)` 构造，只在测试里，reason 只作显示、无解析方）⇒ 不改，仅记录 |
| 5 | `static_assert` 只对齐"个数"不对齐"顺序"，重排 `LOGICAL` 静默错配 | **采纳并修复**：`8749bb2`（按名取槽） |
| 6 | 台账"同语言另一 scheme 被一并判死"与键语义矛盾 | **采纳**：已撤回该句（键=调用方传入的 handle 原样字符串） |
| 7 | 词数不匹配分支不写缓存，与 `LinguistSession.h:241-245` 措辞不符 | **采纳并修复**：`172475c`（经 `recordFailure()` 写缓存，被停止的运行仍不写） |
| 8 | 重载已提交实例不重跑加载期校验（`PackageLoader.cpp:814-818`，唯一 `createConfiguration` 在 `:845`） | **采纳为事实记录**：正好解释实验 C 为何必须走会话期路径 |
| 9 | 校验点排在 vocabulary/languageMap 之前 ⇒ 同一包同时坏两处时报模型错（诊断优先级变化） | **采纳为事实记录**（低危、不改：两者都是加载失败，包一律被拒） |
| 10 | lite 侧注释过期（`SynthrtEngine.h:272-273` 仍称 reason "verbatim"） | **采纳并修**（随本轮 lite 提交） |
| 11 | "0 skipped" 环境相关。静态 case 数与"20/20"口径对不上 | **采纳**：已在"验证 1"加口径说明（20/20＝ctest 用例数） |

复核自己也标注了**未判定项**（本轮主代理亦未证）：取消竞态的实际发生率（需脚本化 stage + 定时 stop 反复跑）、
链上重复文本的可达性、handle 的真实字符串形态（需真实声库打印）、两种环境下 skip 计数的对照。

**验证 3：端口构建链路（路线 A 的实施修正）**

- `vcpkg install --editable` 在 **manifest 模式不被支持**（vcpkg 明确报 `In manifest mode, vcpkg install does not
  support individual package arguments` 与 `The option --editable is not supported in manifest mode`）。
- 等价做法：临时把 `scripts/vcpkg-ports/wolf/portfile.cmake` 的取源改为本地工作树（`set(SOURCE_PATH "<wolf 检出>")`），
  再跑 `vcpkg install --x-manifest-root=… --x-install-root=…` ⇒ vcpkg **只 Build wolf**（其余 7 个依赖从缓存 Install），
  即真实端口构建。随后端口文件按 SHA256 还原（`6B5348A1…`，`git status` 干净）。
- 产物链路证据：`wolf 工作树 → 端口构建（19:00:24）→ installed → lite 重建部署`、部署侧
  `plugins/wolf/inferenceinterpreters/multig2p/wolfmultig2p.dll` 与 installed 侧**同哈希** `7C1792AA…`
  （改前 `889303BA…`）。
- **注意**：端口文件已还原，任何后续 `vcpkg install` 都会按 pin 提交重装 wolf，本轮本地构建的 DLL 会被替换。
  正式生效仍需 push + bump REF（§10.6①）。

**验证 4：lite 自带套件回归（换用新 wolf DLL 后）**

本机临时脚本（未入库，vcvars + `QT_DIR` + `out\bin` 入 PATH）⇒ `ctest` **73/73 通过、`CTEST_EXIT=0`**，与 §10.9
的改前基线同数 ⇒ wolf 侧改动（加载期校验、结构失败、失败缓存、reason 渲染）**没有**引起 lite 侧回归。

**未做/未证实（诚实记录）**

1. V8-a 的 **cause 链在缓存 reason 里的渲染**没有**像素级**端到端直读面（要看悬停截图）：`voices.describe` 不暴露
   reason 字段（只有 `g2p_ready`），日志里的因果链来自 lite 记录转换错误。但**字符串通路已两端验证**：wolf 侧由单测
   `*ReplaysACachedFailureWithItsCodeKind*` 锚定（含转红），lite 侧是"原样照抄"——`LanguageBridge.cpp:141-147`
   （只有确定的 `Unavailable` 才带原因，`return status.reason;`）→ `SynthrtEngine.cpp:662-672`
   （`languageUnavailableReason()`，头注释 `SynthrtEngine.h:237-239` 明写"only wolf's … 原文"）→
   `LanguageComboBox.cpp:186-223`（"原因照抄引擎原文"）→ `markUnavailableItem()` `:159-163`（禁用 + `Qt::ToolTipRole`）
   与 `refreshCurrentToolTip()` `:165-184`（当前语言被禁时下拉框自身 tooltip 也给原因）。
2. 内容损坏（ONNX 内部坏）按设计**不在加载期发现**（只查存在性/常规文件）⇒ 仍表现为会话期失败（本轮实验即此路径）。
3. V4-a 的"失败期间收到 stop ⇒ 返回已完成部分"分支无新用例（需一个既可中断又建不起来的后端）。F1 的"三个逻辑名"
   已由扩展后的用例覆盖（含"还原后能复载"）。尚未单独造例的是"`bundle.json` 指向不存在的文件名"这条入口——它与
   已覆盖路线共用 `resolveModels()` → `is_regular_file()` 这同一段代码（**推断**：等价覆盖，未独立验证）。
4. `resolveBackend` 的失败码（`FeatureNotSupported`）未被断言（只断文案 + `Failed` 状态），避免"文案对、码不同"误判。
5. **第三组实验的原始设计（"后端 pair 不匹配"）在 lite 侧不可达**，这一点本轮实测确认：两条**既有**加载期不变量
   把所有"声明层不一致"都拦在加载期——把 pair 从 `bundle.json.languages` 删掉 ⇒ `languageMap names eng/default,
   which the bundle does not list`；把 pair 从插件 `configuration.languageMap` 删掉 ⇒ `exports languages and
   languageMap describe different pairs`。两者都表现为**加载失败**（整包不可用），到不了 V4-a 的运行期路径。因此
   V4-a 的 pair 失配路径改由 **wolf 单测**覆盖（`test_MultiG2P` 的 A1/A2/B1 用宿主式 `G2PRuntimeOptions.binding`
   把夹具链绑到 `eng/unmapped` ⇒ `createInference` 查表失败 ⇒ 整批失败），lite E2E 改用"启动后内容损坏"触发
   （设计里写明的现实路径：加载期只查存在性）。**推论（未证实）**：宿主只要显式绑定 pair（synthrt 契约允许），
   就能在 lite 之外复现这条路径。
6. **附带发现（事实）**：`wolf-g2p-multi` 的 `bundle.json` 声明了 4 个模型文件（`encoder`/`decoder`/
   `decoder_step_init`/`decoder_step`），但**只有 3 个被消费**——`Bundle::fileName()` 的唯一调用点是
   `Decoder.cpp:162`，它按 `LOGICAL[]`（`Decoder.cpp:46`，三个名字）循环。全仓无 `decoder_int8` 的其它引用。
   ⇒ F1 的校验名单**没有覆盖漏洞**（多余的 `decoder` 条目不构成任何运行期失败家族），但 `decoder_int8.onnx`
   （5.0 MB）是**上游数据包里的冗余载荷**。「是否有仓外消费方」未证实（推断：属打包层冗余，不在本轮范围）。

7. **复核修补的两处 guard 没有确定性用例（据实）**：①"取消与失败竞态"需要停止请求恰好落在批次内部失败的一瞬，
   现成夹具无法构造。②"词数不匹配"要触达**会话层**的词数检查，需要一个"上层直接少返词"的模块——夹具里的
   `stub-miscount` 只实现 G2P/S2P/Onset 接口（`src/plugins/inferenceinterpreters/stub/StubExecutives.cpp:80-140`），
   它丢词会被上层模块自己的 guard 拦下（既有用例 `*RemembersABatchFailureUntilRefresh*` 收到的错误文案里带 "g2p"
   即是上层文案），到不了 `LinguistSession.cpp` 的检查。⇒ 两条修正都只到**代码级**（读码 + 编译 + 全量回归），
   未配新用例。"整批失败被缓存"的机制本身已由该既有用例覆盖，`172475c` 只是让词数不匹配也走同一条路。
8. **环境型 `Decoder::open` 失败未实测**（复核反例 1 的第三支）：要在真实缺 DML 设备 / 显存不足的机器上复现，
   才能判定该情形下"整条语言标灰"是否是可接受语义（旧行为是逐词兜底 + 链尾还原原词）。
9. **`describeFailure()` 的链上重复文本未证实可达**（复核反例 4）：生产代码未发现单参数 `Error(ErrorCode)` 构造。
   若将来出现，reason 可能读成 `kind: <上下文>: kind`。当前 reason 只作显示、无解析方，故不改。
10. **测试二进制的 CRT 泄漏转储属既有现象（非本轮引入）**：单跑与全量都会出现一次 `Object dump complete`，且在
    **不调用 `Decoder::open`** 的用例（如 F1 拒绝用例）里同样出现 ⇒ 与 `8749bb2` 的重构无关。未深查（不在本轮范围）。

**复现命令**

```
# wolf 套件（含两个必需环境变量，见验证 1）
cmd /c <本机临时产物目录（未入库）>\wolf-ctest.cmd   # 期望: 100% tests passed, 0 tests failed out of 20

# lite 端到端（破坏前/后各跑一次）
pwsh -File <本机临时产物目录（未入库）>\gui-launch.ps1 -OutRoot <本机临时产物目录（未入库）>\wolf-verify-d
pwsh -File <本机临时产物目录（未入库）>\mcp-call.ps1  -OutRoot <本机临时产物目录（未入库）>\wolf-verify-d -Handshake
pwsh -File <本机临时产物目录（未入库）>\f1-probe.ps1  -OutRoot <本机临时产物目录（未入库）>\wolf-verify-d -LanguageId eng -Control hello -Probe zzqx -WaitSeconds 25
pwsh -File <本机临时产物目录（未入库）>\wolf-readiness.ps1 -OutRoot <本机临时产物目录（未入库）>\wolf-verify-d
```

打靶文件（部署副本 `build/Debug/out/bin/wolf/packages/wolf-g2p-multi/inferences/multig2p/`）与改前哈希：
`decoder_step_int8.onnx`=`276A7451…`、`bundle.json`=`E782A588…`、`inference.json`=`96DB6614…`。

---

## 11. 发音层查询与"异步启动各包"调研（2026-10-05，wolf 侧已本地提交 `1ead7c7`）

### 11.1 决策台账（来源＝用户本轮选项式提问的回答）

| # | 决策点 | 取值 | 来源 |
| :-- | :-- | :-- | :-- |
| D1 | 新布尔值的语义 | **有发音层 = `true`。无发音层、直出音素 = `false`。音素层始终存在**（最深可达层的符号即音素层） | 用户原话：「有发音层的时候为true，无发音层、直出音素的为false。有发音层的也会经过s2p得到音素层，音素层一致都存在」 |
| D2 | 判据来源 | **内容推导优先、不可推导由作者声明**：`direct` 定义即恒等。`dict`/`mapping` 的表在打包期读（lint 报警）。`lua` 不可静态判定 ⇒ 一律按独立层。**不动 schema、不改 15 个已装包** | 用户选的推荐项 |
| D3 | 暴露形态 | **事前查询**（与 `maxDepth` 同构），不进 `LinguistConvertResult` | 用户选的推荐项 |
| D4 | 本轮范围 | wolf：新查询 + 文档 + 测试。**顺带接线 lite 的 `maxDepth` 缺口**（lite 消费该布尔值不在本轮） | 用户多选 |

### 11.2 为什么 `maxDepth` 不够（正交性，事实）

- `maxDepth` 只由 `imports` 集合决定：有 S2P ⇒ `Phonemes`，再有 Onset ⇒ `Onsets`，否则 `Pronunciation`（`WolfLinguistProvider.cpp:283-296`）。
- eng 的 S2P 是 `direct`（按空格切分、不重写符号），cmn 的声库侧 S2P 是 `dict`（真分解，`wolf-voicebank-zh` 夹具的 `s2p-cmn`）⇒ **两者的 `maxDepth` 相同**，`imports` 集合区分不出「发音与音素是不是两层」。
- lite 此前唯一的替代品是长度兜底启发式（`GetPhonemeNameTask.cpp` 的 `onsets.size() != phonemes.size()`），其注释已自陈应先读会话状态。

### 11.3 wolf 侧落地（提交 `1ead7c7`，13 文件 +212/−5）

| 层 | 位置 | 内容 |
| :-- | :-- | :-- |
| 接口语义单一真相源 | `include/wolf/Api/Inferences/S2P/1/S2PApiL1.h` | `VARIANT_DIRECT` 与 `variantKeepsSymbols()`：`direct` 是唯一可静态断言的恒等变体 |
| 事前查询 | `include/wolf/Api/Linguists/Linguist/1/LinguistApiL1.h:327-345` | `virtual bool hasSeparatePronunciationLayer(std::string_view) const = 0;` |
| provider | `WolfLinguistProvider.cpp:282-303` 读出、`:346-353` 查询 | 构造 extension 时读 S2P 目标的 `variant()`，与 `maxDepth` 同一处、不创建对象 |
| 会话状态 | `LinguistSession.h`（两个结构）、`LinguistSession.cpp:298-301`、`:377-381` | 与 `maxDepth` 逐点同构：`scan()` 填目录条目、`describeCoverage()` 拷贝进状态 |
| 文档 | 域契约新增 **§5.0.2**，runtime §2.3，session 两处结构，variants §3.2，Status 降级段，决策 **A81** + 未决项 **Q8** | 内容级例外（表逐行恒等）交给 lint。`lua` 一律按独立层（保守方向） |

### 11.4 验证（两侧可判，逐条断言有 file:line 证据）

| 形态 | 夹具 / 语言 | 期望 | 实测 |
| :-- | :-- | :-- | :-- |
| `dict`（真分解） | `test_HostFlow` + `wolf-voicebank-zh` 的 cmn | `true` | `test_HostFlow.cpp:476` `has passed` |
| 不可用 | 同用例的 eng | `false` | `test_HostFlow.cpp:477` `has passed` |
| `direct`（恒等切分） | `test_LinguistSession` + `singer-zxx` 的 zxx | `false` | `test_LinguistSession.cpp:691` `has passed` |
| 未声明 handle | 同用例的 cmn | `false` | `test_LinguistSession.cpp:694` `has passed` |
| 无 S2P 成员 | `singer-shallow` 的 nld | `false` | `test_LinguistSession.cpp:657` `has passed` |
| 不可用（两条早退路径） | 未声明语言 + 不存在的歌手 | `false` | `test_LinguistSession.cpp:141`、`:142` `has passed` |

门禁：wolf 全量 **20/20、无跳过**（三个 `WOLF_*` 环境变量已设），构建 **0 warning 0 error**。

### 11.5 「wolf 是否适合异步启动各包」（用户问题的只读调研结论）

**结论：不需要为启动做异步加载——包加载本来就在工作线程上，且并行加载无收益。**

| 判据 | 事实 | 证据 |
| :-- | :-- | :-- |
| 包扫描已在工作线程 | `PackageManager::initialize` 经 `GetInstalledPackagesTask` 走 `TaskManager`/`QThreadPool`，与主窗口构建并行 | `PackageManager.cpp:134-148`、`TaskManager.cpp:63` |
| 扫描早于窗口显示完成 | `Package scan completed` 比 `App launched` 早 **1.004 s / 0.54 s** | 本机两份采样日志（`wolf-verify-c` / `wolf-verify-a`） |
| 启动期不加载模型 | 首次 open `multig2p/encoder_int8.onnx` 出现在用户首次转换时。`warm()` 全仓零调用 | 运行日志、`SynthrtEngine.h:84` 等 3 处注释 |
| 并行加载会被串行化 | 整次 `openPackage` 独占 `SynthUnit` 一把递归锁（可从任意线程调用） | `synthrt/include/synthrt/Core/SynthUnit.h:24-34` |
| 真正的加载成本 | **每次 openPackage 都重枚举搜包路径并全量解析每个包 `desc.json`（无跨调用缓存）⇒ 顺序加载 N 包约 O(N²)** | `synthrt/lib/Core/PackageLoader.cpp:602-646`、早退点 `:1218-1221` |
| UI 触发重扫不卡界面 | 两条调用路径都在工作线程（任务框架 / automation 专用 `QThreadPool`） | `PackageManager.cpp:153-174`、`PackageAutomationAdapter.cpp:251-264` |
| 唯一已知阻塞面 | 同一时刻的**第二个** `refreshInstalledPackages` 调用者会在自己线程上等条件变量 | `PackageManager.cpp:156-172` |

⇒ **保持现状**。若将来"包多时变慢"，根因在 synthrt 的重复目录扫描与全量解析（上游优化项，与本仓线程模型无关），不在"异步启动各包"。

### 11.6 lite 侧接线（`maxDepth` 缺口，本轮已落地并验证）

**结论先行**：第二趟不再用长度兜底猜"有没有 onset 层"，改为先读 wolf 报出的层数上限。现网夹具三语言实测 `maxDepth == Onsets`，行为**逐行不变**。`Phonemes` / `Pronunciation` 两条分支**未获运行时证据**（缺夹具，见文末）。

| 改动 | 位置 | 内容 |
| :-- | :-- | :-- |
| 语言层读取 | `src/libs/SynthrtEngine/LanguageBridge.h:101-114`、`.cpp:165-177` | `std::optional<Depth> maxDepth(singer, language)`：走与 `canConvert()` / `unavailableReason()` **同一个** `session.probe()`，不加载资源。**不可用返回空值**，不折叠成 `Pronunciation`（否则调用方会把"路由不可用"当成"该组合没有 onset 层"而静默成功） |
| 引擎转发 | `SynthrtEngine.h:246`、`.cpp:675-683` | 锁与前置判断与 `canConvert()` 一致，引擎未初始化 ⇒ 空值 |
| 深度选择 | `src/app/Modules/Inference/Tasks/GetPhonemeNameTask.cpp:109-113`（按语言缓存，一批只读一次）、`:149-161`、`:163-174` | `Pronunciation` ⇒ **不发转换**、`success = true`、空结果（发音仍来自第一趟）。否则请求深度取"本任务需要的 `Onsets`"与上限的较小者。上限为空值（路由不可用）时按旧行为请求 `Onsets` 并照旧报错 |
| 兜底判据收紧 | 同文件 `:200-211` | `!phonemes.empty() && onsets.size() != phonemes.size()` → `!onsets.empty() && onsets.size() != phonemes.size()`：**"有 phonemes、无 onsets"是合法结果**，不再告警。只有"有 onsets 却不与 phonemes 一一对应"才告警 |
| 审计加测 | `src/tests/TestVoicebankAudit/main.cpp:698-724`、`:776-781` | 转换前打印每语言 `maxDepth`。"上限为 `Phonemes` 却报 onsets"记为 `finding`（**非 expect**，不影响门禁） |

**实测（夹具 `yousa-2.4@1.65.1.0` + `<wolf 检出>\build\lang-packages-current`）**

| 项 | 改前 | 改后 |
| :-- | :-- | :-- |
| ctest | 73/73、0 failed、`CTEST_EXIT=0` | 73/73、0 failed、`CTEST_EXIT=0`（同一 skip：`TestOtterExtraction`） |
| 构建 | — | `BUILD_EXIT=0` |
| 层数上限（新增打印） | — | `cmn -> Onsets`、`eng -> Onsets`、`jpn -> Onsets` |
| 转换输出 | `cmn 你 -> ni [n i] onsets . ^`、`eng hello -> hh ax l ow [hh ax l ow] onsets . ^ . ^`、`jpn こ -> ko [k o] onsets . ^` | 与改前**逐行一致**。`audit clean, 2 finding(s)` |

**为什么英语在本夹具是 `Onsets`（更正 §11.2 的措辞）**：运行时绑定的不是**语言包**的 `wolf-lang-eng:linguist/eng-arpabet`（那里只有 `linguist/g2p` + `linguist/s2p`，按 imports 推导 `maxDepth` 会是 `Phonemes`），而是**声库自带**的 `linguists/eng-arpabet/linguist.json`——它 import 三个成员：`linguist/g2p -> wolf/lang-eng:inference/g2p`、`linguist/s2p -> :inference/s2p-eng`（variant **`direct`**）、`linguist/onset -> :inference/onset-eng` ⇒ `maxDepth == Onsets`（主代理亲自核对声库文件与 `s2p-eng/inference.json`）。该 S2P 为 `direct` ⇒ 新布尔值对英语报 **false**，与"英语直出音素、普通话拼音是发音层"的口径一致。

**未证实（不得当缺陷排期）**：lite 侧 `GetPhonemeNameTask` 的 `Phonemes` / `Pronunciation` 两条消费分支在本机跑不到——现有唯一真实声库的三语言都是 `Onsets`，而该任务没有可注入假语言层的单测夹具（依赖 `appStatus` / `SingerInfo` / `SynthrtEngine` 单例）。**wolf 侧对应形状已补齐运行时证据**（`Pronunciation` 早已断言，`Phonemes` 见 §11.7 第 1 条）。`Pronunciation` 分支在 lite 侧返回 `success = true` + 空音素名，仍只有代码级证据。

**复核修正（第二轮，已提交 `2538c041`）**：只读复核（快照 `ed0a4c1b`）判定现网可达范围内无反例，但提了四处，均已修：

| 项 | 原状 | 修正 |
| :-- | :-- | :-- |
| 未知层数上限 | `depthFrom()` 未命中时回落 `Pronunciation`，而该值在调用方等于"不转换 + 报成功" ⇒ wolf 将来追加枚举值会静默丢音素层 | 回落 `Onsets`（与同文件 `depthOf()` 同向），把成败交给转换自己判定 |
| 审计 NOTE 判据 | "上限 `Phonemes` 却 onsets 非空"**必然误报**：无 onset 成员时 wolf 返回**与音素等长的全 false** 列表 | 改为"上限 `Phonemes` 却**标记**了 onset"（`std::any_of`），并把 `limits[...]` 换成 `find()`，路由不可用时不检查 |
| 注释过度承诺 | "路由不可用 ⇒ 转换仍报失败"在**覆盖度为 0** 这条不可用路径下为假（`acquire()` 不看覆盖率） | 改为"不改变请求深度、让转换自己决定成败" |
| 机制叙述 | 我把旧判据说成"会把合法的无 onsets 当异常" | **更正**：旧代码请求的 `Onsets` 深度上"有 phonemes、无 onsets"不可达 ⇒ 旧判据是死代码。本改动的价值是按上限请求、让"无 onsets"成为可请求的合法结果，并把"未知上限"这一失败方向堵住 |

**我亲自否掉的一条子代理建议**：它主张给恒等判据再加"键内无空格"的前置条件。核对装载器后**不采纳**——`S2PTables.cpp:83` 以**原文**做键、`:95` 以**原文**查表、`main.cpp:179/184` 原文透传，键含空格的行仍能被同形输入命中。加严反而会静默掉"整表死行"的退化表。警告文案本就是条件句，且 `docs/linguist-variants.md` 已写明 dict 未命中会产出空序列、不能无条件改成 `direct`。

### 11.7 对抗复核裁定与修补（两仓，2026-10-05 深夜）

**快照**：wolf `2451375`（复核）→ 修补提交 **`ffec87f`** → 收敛补测 **`8fa6ac7`**（`Phonemes` 形状的运行时断言）。lite `ed0a4c1b`（复核）→ 修补提交 **`2538c041`**。两轮复核均为**只读**（不写文件、不构建、不 git 写），行号由复核方自行复核。

| 仓 | 判定 | 反例 | 处理 |
| :-- | :-- | :-- | :-- |
| wolf | **查询本体站得住**：三条攻击全部失败（variant 与运行期同源 `LinguistExecutiveImpl.cpp:149`，两个填充点成对，`direct` 带 `file` 也从不读表） | 1 高 + 2 中 + 5 低 + 6 条边界 | 全部修补于 `ffec87f`：**ABI 版本抬到 0.2.0.0**、lint 判据镜像 `splitPronunciation` + 字节级读表 + `mapping` 空表告警、Unavailable 语义纠错 + 新用例、S2P 头文档、计数同步、三处互指注释、粒度与非阻断说明 |
| lite | **站得住**：现网可达范围内无反例。三语言上限全 `Onsets`，改前改后音素名与 `isOnset` 逐项相同 | 1 中 + 3 低 | 全部修补于 `2538c041`（见 §11.6 末） |

**门禁（两仓均由主代理亲自复跑）**：wolf 构建 exit 0、0 warning。ctest **20/20、0 failed、0 skipped**。Python **33 tests OK**。真实数据 lint 三处 **0 error 0 warning**。lite 构建 exit 0。ctest **73/73、0 failed**、同一 skip（`TestOtterExtraction`）。

**高项详情（制度性教训）**：`WolfPipelineExtension` 新增纯虚函数改变 vtable、两个公开值类型各加字段，按 `README.md:100-104` 与 D1「ABI 破坏性变更必须与一次 `WOLF_VERSION` 变更同批」必须同批抬版——我原先在 A81 里以"纯增量、先例 A70"为由不抬，**是错的**：A70 讲的是**包** `compatVersion`，而它那一批（`ad2d356`）恰在同批把库版本从 0.0.1.0 抬到 0.1.0.0。现为 **0.2.0.0**（`CMakeLists.txt:3-13` 记录该版本含什么，`wolfConfigVersion.cmake` 已刷新）。

**跨仓影响**：lite 的 `find_package(wolf CONFIG REQUIRED)` **不带版本**、wolf 端口 `version-string` 是分支名 ⇒ 抬版不影响 lite 配置。**本轮已按用户选择完成本地重装（不 push）**：用一份本机临时 overlay 端口（未入库，源指向 `<wolf 检出>`，**仓内 portfile 未改动**、哈希仍 `6B5348A1…`）执行 manifest install ⇒ 装出的 wolf 与 wolf HEAD `8fa6ac7` **逐字节一致**（`LinguistSession.h` 两侧哈希同为 `921B4421…`）、`wolfConfigVersion.cmake` = `0.2.0.0`、ABI 哈希 `6500a162…`。随后重建 lite（构建 exit 0）、ctest **73/73**、审计输出与基线一致（`audit clean, 2 finding(s) about the voicebank itself`）。远端复现仍走 `REF 3a3f4a0`。要长期固化仍需 push + 抬 REF（**用户门控**）。

**操作踩坑（务必记住）**：本机构建脚本（未入库）只构建 app（默认目标到 `DsEditorLite.exe` 为止），而 `TestVoicebankAudit` **静态链接 wolf** ⇒ 换 wolf 后必须显式重建该目标（本机临时脚本（未入库），或 `cmake --build --preset debug --target TestVoicebankAudit`），否则**旧 ABI 的二进制跑在新插件上会表现为卡死**——本轮真踩到：ctest 卡在该用例 20 分钟、CPU 仅 5.2 s（已加载 onnxruntime 与各 wolf 插件、未加载 `wolflua`），重建后同一二进制立刻跑通。**不要误判成 wolf 回归**。全仓只有 `src\tests\TestVoicebankAudit` 含 `#include <wolf/...>`，故只需重建这一个目标。

**未证实 / 未做（不得当缺陷排期）**

1. **层形状的运行时证据：wolf 侧本轮闭环，lite 侧仍缺一条。**
   - wolf 侧：`Pronunciation` 早有断言（`test_LinguistSession.cpp:653-660`）。`Phonemes` 本轮补上——新增用例 `test_LinguistSession_ReportsAPhonemeLayerAsTheDeepestLayer`（提交 `8fa6ac7`）用现有 `singer-chain` 夹具（g2p + s2p(`direct`)、**无 onset 成员**）断言 `maxDepth == Phonemes` 且 `hasSeparatePronunciationLayer == false`（同时证明两个答案**互相独立**、不能互推），并断言"请求比组合更深的 `Onsets` 时返回音素层而非报错、onsets 为空或与音素等长"。主代理单跑实测通过（`:679` Entering→Leaving、`*** No errors detected`），ctest 20/20。
   - lite 侧：`GetPhonemeNameTask` 的 `Phonemes` / `Pronunciation` 两条**消费**分支仍无运行时证据——现网唯一真实声库三语言全是 `Onsets`，而该任务没有可注入假语言层的夹具（依赖 `appStatus` / `SingerInfo` / `SynthrtEngine` 单例）⇒ 要闭环需给 lite 加可注入的层来源（属重构，建议单独立项）。
2. `foundOnset` 既有退化（复核发现）：无 onset 标记时音素会被折进**上一个词**、时长偏移按"前一整词"算（`InferTaskHelper.cpp:132-138`、`InferDurationTask.cpp:315-338`）——**改前也如此**（wolf 对无 onset 成员的语言按全 false 填充），不是本改动引入。建议单独立项。
3. `PhonemeEditorDialog` 要求"至少一个音素标记为 onset"（`:143-153`），与"无 onsets 合法"冲突 ⇒ 潜在，需夹具才可复现。
4. `lua` 脚本恒等**无法静态检出**（本仓夹具 `lang-runaway/script.lua` 即恒等而报 `true`）。新增读表变体需三处同改（C++ `variantKeepsSymbols()`、域契约 §5.0.2 表、Python `TABLE_VARIANTS`），**无脚本交叉校验**（已记入 wolf 未决项 Q8）。
5. `DataOnly` 下 `binding()==nullptr` 的哨兵会让 `maxDepth` 等查询一律返回"未声明"（`maxDepth` 早已如此，新字段继承）。DataOnly 是否真能走到 `createExtensions` **未证实**。
6. 内容级例外只是 **warning**，**拦不住发布**（`make-lang-release.py:191-197` 只看退出码）⇒ 已写进域契约 §5.0.2 作为容错性说明。
7. 反斜杠路径改写（`\`→`/`）在 Windows 上**无法**做红/绿验证（属恒等变换）。
8. 两个未重配的构建目录（`build/cmake`、`build/cmake-debug`）里 `wolfConfigVersion.cmake` 仍是 0.1.0.0——非源文件、不在改动范围。

**行号口径说明**：§10.1–§10.9 的行号以基线 `3a3f4a0` 为准，§10.10 与 §11 以各段所述提交为准。复用时请按当前工作树复核。

### 11.8 foundOnset 退化：只读调研结论与候选方案（未改代码）

来源：只读调研子代理（快照 lite `8a599701`，未构建未写文件），引用行号由我抽查。

**设计来源与上游口径（查证结果，2026-10-06 补）**：onsets 原本**不是上游给的**，而是 **lite 自己按语言规则算的卡拍标记器**——2026-05-03 `177c4af6`/`d06a712a` 引入 `IOnsetMarker` + `OnsetMarkerMgr`（普通话规则：**1 个音素 ⇒ 它卡拍。2 个 ⇒ 第 2 个卡拍**，即卡拍点通常落在**韵母**而非首音素。设计文档 `docs/plans/english-phoneme-support-plan.md:39-45`），当时任务里是 `OnsetMarkerMgr::instance()->marker(input.language)`（`88f785f7^` 版本 `:11`、`:94-95`）。标记器随后作为"未使用"被删除（`88f785f7` 2026-06-05、`e70eadcb` 2026-07-15「route G2P through synthrt」——**两笔都没有提交正文**），同日 `e331fa3f`「consolidate task pipeline and remove legacy loader」把音素名与 onsets 改成**读上游结果的 `syllable.onsets`**。`af7ac5a5`（2026-07-28，正文仅 "update synthrt"）与 `828b8f98`（2026-10-01 迁移到 synthrt+wolf+otter）沿用该写法（blame 把当前行归到 `af7ac5a5` 是因那一笔重建了周边循环）。⇒ **老设计里不存在"该语言没有 onset 层"这一状态**，本退化是设计转移留下的**能力落差**，不是哪一笔写错。

**数据流（事实）**：wolf 结果 → `LanguageBridge::Result::onsets`（`LanguageBridge.h:58`，赋值 `.cpp:236`）→ `GetPhonemeNameTask`：按语言读上限 `:149-153`、`Pronunciation` 短路 `:154-161`、按 `limit.value_or(Onsets)` 请求 `:170-177` → **折平点 `:222`** `pn.isOnset = (k < syllable.onsets.size()) ? syllable.onsets[k] : false;` → `InferControllerHelper.cpp:271` 写入 `Note` → `InferInputNote.cpp:14` 进推断输入。

- **区分能力**：请求前可区分（契约 + lite 运行时断言：空 onsets ⟺ 组合/请求停在音素层。等长全 false ⟺ 有 onset 层但本音节无起音音素）。**`:222` 之后不可区分**（两种形状折成同一个 false 向量）。`maxDepth` 为 `nullopt`（路由不可用）时仍按 `Onsets` 请求 ⇒ 又回到"等长全 false"，歧义回归。
- **真读者**：`InferTaskHelper.cpp:105-116`（首 note 的头音素进 SP 头词）、`:133-138`、`:179-193`（下一个 note 的头音素折进当前 word）。`InferDurationTask.cpp:330-334`/`:338`/`:358-370`。`SingingClipSlicer.cpp:82-89`。`DspxPhonemeCompat.cpp:15`/`:121`。`ProjectModel/Utils/Syllabification.cpp` 与应用侧 `Syllabification.cpp:69`。`SingingClipPhonemeNormalizer.cpp:63-64`。`InferInputBase.cpp:38`（`Phonemes.cpp:11` 含 isOnset ⇒ 进缓存签名）。**死字段**：`PhonemeViewModel::isOnset`（`PhonemeView.cpp:634` 只写，全仓无读者）。`InferPhoneme::is_onset` 不进引擎载荷（`GenericInferModel.cpp:59-66` 不序列化）。

**后果分级**

| 级别 | 内容 |
| :-- | :-- |
| 结构性（事实） | 全 false 时第 k≥2 个 note 的音素被折进**前一个 note 的 word**（`:179-193`），该 note 自身因空 phoneBuffer 早退（`:77-84`）不再产生 word。首 note 全部音素进 SP 头词（`:105-116`）⇒ 模型 word↔note 关联**整体错位一个 note**（音素 token 顺序不变，故 `:324-329` 的 token 检查不报错） |
| 时长/合成 | 偏移基准 `InferDurationTask.cpp:330-334`、头长 `SingingClipSlicer.cpp:82-89` 变化（事实）。**可能**因 `:338`/`:358-370` 门禁返回 false 而整片推断失败（**推断，需夹具判决**） |
| 仅显示/内部（事实） | 音素**名字**不受影响。编辑器 onset 列全未勾选并拒绝保存（§11.7 第 3 条）。dspx 写出全 false。缓存键/签名变化 |

**可达性**：现网**不可达**（三语言上限全是 `Onsets`）。可达条件 = 该语言的 linguist 组合**没有 onset 成员**（`scripts/convert-voicebank.py:576-597`。仓库内 `src/tests/TestVoicebankLanguages/main.cpp:180-186` 的 eng 条目即该形状）。

**候选方案（只给方案，未动手）**

1. **不改**：语义上把"该语言没有 onset 层"与"本音节无起音音素"混同是错的，但只对无 onset 层的语言可见 ⇒ 只适合作"不可达、不排期"的依据。
2. **推荐：缺层时回退到"按语言规则的标记"** —— 检测到组合没有 onset 层时，用**显式的、按语言的规则**补标记（老语义：普通话 1 个音素 ⇒ 它卡拍，2 个 ⇒ 第 2 个卡拍），而不是通用启发式。**注意：「首音素即 onset」是错的**——卡拍点通常是**韵母**（`n i` 卡拍在 `i`），与老规则相反。实现面：一处判据 + 一份最小规则表（仅在该语言没有 onset 层时启用）。
3. **消费侧兜底**（改 `InferTaskHelper`/`InferDurationTask`/Slicer 把"整词无 onset"当"首音素即 onset"）：改动面更大、属下游补偿设计，且按上面的口径连语义都不对 ⇒ 不推荐。
4. 附带小改：`PhonemeEditorDialog.cpp:143-153` 的"至少一个 onset"校验对这种语言应放宽。
5. **上游负责（与 2 并列的"忠实方向"）**：让语言层总是提供 onset 层（语言包声明/校验器要求），lite 就永远不需要 fallback ⇒ 属 wolf 侧声明与校验的口径变更，会牵动 15 个已装包与 lint（用户此前明确要求不改这些包）。

**注意**：`hasSeparatePronunciationLayer` **不可替代** `maxDepth` 判定——它答的是"是否有独立发音层"，与 onset 层互不可推（§11.7 第 1 条）。

**可验证性**：`TestSyllabification/main.cpp:239-250` 已示范"构造可控 isOnset 的 `Note` → `InferInputNote` → `InferTaskHelper::buildWords(input, true)`" ⇒ 加一条"两 note、全部 `isOnset=false`"的用例断言 words 的 phones/notes 归属，即为回归门禁（当天可做，且可先红后绿）。要覆盖 `GetPhonemeNameTask` 的消费分支仍需可注入的层来源（属重构，见 §11.7 第 1 条）。

### 11.9 GUI 验收清单（用户执行）

前置：`build\Debug\out\bin\DsEditorLite.exe`（已用本地安装的新 wolf 重建，插件与语言包随构建部署）。

1. 启动编辑器：无启动报错，歌手/语言下拉正常（yousa 声库应给出 cmn / eng / jpn）。
2. 画 2–3 个音符、填中文歌词（如"你好"）。
3. 触发音素推断：每个音符的音素名应与本轮改动前**完全一致**（拼音拆分），**不出现空音素**。
4. 不应出现"该语言没有起音音素"一类告警，也不应整片推断失败。
5. 合成并播放一次：仍能出声，时长/波形与改动前无异常。
6. 失败面验收（上一轮 A1）：临时把某个语言包目录改名（`build\Debug\out\bin\wolf\packages\<语言>`），重开编辑器 ⇒ **只有该语言标灰禁用**、tooltip 说明原因，其他语言照常可用。
7. 恢复该目录，重开确认恢复正常。
8. 保存工程后重开：音素行与歌词显示不变。

本轮 lite 侧实测"转换输出逐行一致"，故上述任一步出现差异都属**新发现**，请把界面文案与日志给我。

### 11.10 卡拍层口径收口：按“推给上游”落地（B1，组合级 + 被绑定的 linguist）

**快照**：lite `6d575727`（分支 `synthrt/inferutil-binary-read`）、wolf `2a53aae`（分支 `linguistic-level-1-v2`）。两仓均为**本地提交、未 push**（`git log --branches --not --remotes` 可列出）。本节行号分别以这两个提交的工作树为准。跨提交引用处并列标注原行号。

**用户拍板（本节规格来源，编号沿用 §11.8 候选方案）**：foundOnset 退化**不做 lite 侧行为补偿**，改为**推给上游**——要求「**被绑定到的那个 linguist** 必须达到卡拍层」。缺层必须**显式登记**，不允许默默缺层。lite 侧只出**非阻断告警**（不加 error、退出码仍只由 error 决定、转换产物不变）⇒ §11.8 的候选方案 2（lite 侧按语言规则补标记）与 3（消费侧兜底）**作废**，方案 5（上游负责）成为既定方向。

**事实一：层深是链式的，不是各自独立的开关（本轮新增，wolf 侧）**

- 深度起点是 `Depth::Pronunciation`（`src/plugins/linguistproviders/wolf/WolfLinguistProvider.cpp:285`）。
- 只有在**被绑定的那个 linguist 声明了 `linguist/s2p`**（同上 `:293`）时才抬深：有 `linguist/onset` 成员 ⇒ `Depth::Onsets`，否则 ⇒ `Depth::Phonemes`（`:294-296`）。
- ⇒ **s2p 缺席时深度停在发音层，即使声明了 onset 成员也永远到不了卡拍层**。
- 枚举序 `Pronunciation < Phonemes < Onsets`（`include/wolf/Api/Linguists/Linguist/1/LinguistApiL1.h:100-104`）⇒ `Phonemes` **比** `Pronunciation` **深**，两者都到不了卡拍层。
- 运行期走同一条链：s2p 执行体缺席即返回，走不到 onset pass（`src/plugins/linguistproviders/wolf/LinguistExecutiveImpl.cpp:316-325`）。
- 宿主读值：`src/lib/Session/LinguistSession.cpp:379-381`（`maxDepth` 与 `hasSeparatePronunciationLayer` 相邻取出）。
- 契约锚点：`docs/linguist-domain-contract.md:276-286`（§5.0.1 深度上限）与 `:390-392`（检测方式）。
- **正交项**：`hasSeparatePronunciationLayer` 由 S2P 成员的**变体**决定（`WolfLinguistProvider.cpp:297-300` 的 `separateLayer = !variantKeepsSymbols(...)`，`include/wolf/Api/Inferences/S2P/1/S2PApiL1.h:59-61` 仅 `direct` 为 true），与深度**正交**（契约 `:307-308`），**不能**替代 `maxDepth` 判定。

**事实二：lite 侧三笔提交（口径逐轮收紧，每笔都有红/绿与真实数据双侧读数）**

| 提交 | 口径变化 | 门禁（本机留存日志，可与提交正文对照） |
| :-- | :-- | :-- |
| `c2ae55ad` | 新增非阻断告警 `report_the_onset_layer()`（**该提交内** `:515` 起。HEAD 工作树 `:533`），缺卡拍规则时不再静默。文件内补中文注释写明为何是组合级而非语言包级 | `python -m unittest discover -s scripts -p "test_*.py"` **28 tests OK**（本机临时日志，未入库）。对改动前脚本跑同一组用例 **3 failures**（本机临时日志，未入库） |
| `b7f6a30e` | 静默条件收紧为「被绑定的那个 linguist」。新增「成员存在但**未被绑定**」告警（自建 linguist 分支只认条目自己的 `onsetFile`） | **30 tests OK**（本机临时日志，未入库）、旧脚本 **4 failures** |
| `6d575727` | 静默条件改为**整条链** `s2p and onset`（`:589-590`）。缺 s2p 时文案点名「停在 pronunciation 层、声明的成员永远到不了」（`:590-596`） | **32 tests OK**（本机临时日志，未入库，两份）、旧脚本 **3 failures** |

用例落在 `scripts/test_convert_voicebank.py` 的 `ConvertVoicebankOnsetLayerTest`（`:239` 起，`c2ae55ad` 的新增块为 `:236-371` 共 5 条，`b7f6a30e` +2、`6d575727` +2 并改名 1 条）。

**真实数据等价（本轮我亲自复核，非转抄）**

- 两个真实声库 `qixuan@2.7.0.0`、`zhibin@26.7.16.0`：均 **0 error**，告警数与改动前**一致**（qixuan 3、zhibin 1），**无新增告警**（两者声明的语言全部由自建 linguist 绑定，而自建 linguist 一定导入 `linguist/s2p`）——读数为本机临时日志（未入库）。
- 产物逐文件 SHA-256 与**改动前基线全同**：qixuan **46/46**、zhibin **56/56** 个文件，0 差异（改动前基线与改动后产物各留一份本机目录，逐文件比对）。我另用本机留存的各轮临时产物（未入库）复核了**共 8 组**两两对比，**全部 0 差异**（含 46/46、56/56）。
- ⇒ 本轮**只改报告口径**：不碰转换代码路径、不改转换结果。（事实）

**事实三：wolf 侧四笔提交（均未 push）**

| 提交 | 内容 |
| :-- | :-- |
| `3a5ffa3` | 契约新增 **§5.4 发布口径**（`docs/linguist-domain-contract.md:367` 起）。`docs/linguist-session.md:422` 与 `docs/linguist-variants.md:186` 各加一句指针（不复制正文）。`scripts/check-declarations.py:329-337` 补注释说明本 lint 为何**不**强制 `linguist/onset`。33 tests OK、三处 lint 0 error 0 warning |
| `2e87a08` | 口径收紧为「被绑定的 linguist」（契约 `:376-384`）＋「缺层必须显式登记，不允许默默缺层」（`:385-389`）。消解 `docs/linguist-distribution.md:297-300`（§3 第 4 条）的语言包级矛盾，改为组合级口径。新增 **§3.3 逐语言达标核对表**（`:397-442`，发布核对清单而非引擎约束）。33 tests OK、三处 lint 0/0 |
| `8562d06` | 新增回归用例 `test_LinguistSession_ReportsTheOnsetLayerAsTheDeepestLayer`（`src/tests/auto/Runtime/test_LinguistSession.cpp:716-758`）：对夹具组合里歌手声明的每个语言断言 `probe().maxDepth == Depth::Onsets`，并按该深度转换一次证明深度**已实现**。**负控**：把期望值临时改成 `Phonemes` 时 cmn、yue 两行都失败 |
| `2a53aae` | 同步因上条失真的两处表述（§3.3「没有任何用例守住达标」→「已由回归用例守住」＋契约 §5.4 末加指针到该用例） |

门禁：wolf 端本机构建目录的 ctest 留档（16:42）**20/20 全跑、0 failed、0 skipped**，其中 `test_LinguistSession` 跑 30 cases、`*** No errors detected`。Python **33 tests OK**（本机临时日志，未入库）。该 ctest 日志时间晚于 `8562d06`(16:37) 与 `2a53aae`(16:41)，一次读数同时覆盖后两笔（事实），我**未**单独复跑。

**未做 / 待排期（四条，本轮均未做）**

1. **变体口径转换器暂不报告**：`hasSeparatePronunciationLayer` 由 S2P 变体决定（`:297-300`），而现有告警只判 imports 链（`direct` 与 `dict` 同样算满足链，与 wolf 一致）⇒ 若要把「独立发音层」也纳入发布清单，需**另加一条非阻断告警**。
2. **发布文档存量漂移（另一子代理正在 wolf 仓清理，本节只登记、不给结论）**：① `wolf-lang-zxx` 在 wolf `docs/` 共 **7 处**（`docs/linguist-distribution.md` 6 处 ＋ `docs/Status.md:113`。其中字面 `packages/wolf-lang-zxx` 4 处：`linguist-distribution.md:269`、`:320`、`:730`、`:741`），而该路径在当前工作树与索引中**都不存在**（来源提交 `d3699c3` 不属于任何本地分支）。② `docs/linguist-distribution.md:294`（§3 第 2 条）仍写 `linguist.json` 含「三条 role imports」，与实测（`eng`/`ita`/`kor`/`por` 与 `zxx` 的 linguist **只有 `linguist/g2p` 与 `linguist/s2p` 两条**）不符。
3. **是否单独立一条「闭包语言的语言包必须提供 S2P」**：`2e87a08` 已删去原句，待用户裁决（立条属发布口径新增，不是代码）。
4. **达标表的「未实测」登记属发布动作**：`docs/linguist-distribution.md:439-442` 已写明——需拿到真实歌手包后按 `maxDepth` 复核再改表。本轮未做（本机无这些语言的歌手包数据）。

**未证实 / 限制**

- 真实语言包集合里 **5 个带 linguist、声明 onset 者 0、声明 s2p 者 5**（`build/lang-packages-current` 的 14 包中 `eng`/`ita`/`kor`/`por` 4 个，加 `build/test-fixtures/wolf-lang-zxx` 1 个。本轮复扫各包 `linguists/*/linguist.json` 的 `imports[].role` 确认）⇒ 「**被绑定 linguist 有 onset 但无 s2p**」这一分支在真实集**无实例**，仅由夹具覆盖（`test_reports_a_bound_linguist_that_has_onset_without_s2p`）。「**成员存在但未被绑定**」分支同理只有夹具 ＋ 注入探针覆盖（`test_reports_a_package_member_that_is_not_bound`）。
- **lite 侧行为仍然未改**：§11.8 的折平后果照旧成立（`InferTaskHelper.cpp:132-138`、`InferDurationTask.cpp:315-338`）。本轮只是把「缺层」从**静默**变成**告警**，该退化在现网仍必须由上游语言层消除。

**行号口径说明（续 §11.7 末）**：§11.10 的 lite 行号以 `6d575727` 为准、wolf 行号以 `2a53aae` 为准。跨提交引用处已并列原行号。

**与任务书不一致的复核结果（按实测写，其余行号逐条复核一致）**

1. `report_the_onset_layer()` 任务书给「`:515` 起」——那是 `c2ae55ad` **提交内**的行号。HEAD（`6d575727`）实际为 **`:533`**（`b7f6a30e` 后为 `:519`）。
2. `scripts/test_convert_voicebank.py:239-371`（任务书给的新增用例范围）：`:239` 只是**类声明**起始，`c2ae55ad` 的新增块实为 **`:236-371`**（`:236-238` 是空行与类注释）。
3. 「10 组对比 0 差异」：三笔提交正文逐笔记载 2 组、6 组、4 组（累加 12 组），而我能用本机留存的临时产物（未入库）复现并复核的是 **8 组**（均 0 差异）。本段按**可复核的 8 组 ＋ 提交正文记载**写。
4. `S2PApiL1.h` 的完整路径是 `include/wolf/Api/**Inferences**/S2P/1/S2PApiL1.h`（不是 `Api/Linguists/S2P/...`），`:59-61` 内容与任务书一致。
5. 其余行号（`WolfLinguistProvider.cpp:285/293/294-296/297-300`、`LinguistApiL1.h:100-104`、`LinguistExecutiveImpl.cpp:316-325`、`LinguistSession.cpp:379-381`、域契约 `:276-286`/`:307-308`/`:390-392`、`test_LinguistSession.cpp:716-758`、`linguist-distribution.md:294`/`:297-300`、lite `convert-voicebank.py:589-590`）**逐条复核一致**。


