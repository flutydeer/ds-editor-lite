# spec 2.4 分支对抗审计报告（lite / synthrt / wolf / otter）

> 审计对象：`spec2.4-uptake` 分支族。方式：4 域只读扇出审计 → 主代理构建实测 → 对抗复核轮（专门找反例推翻）→ 主代理亲自抽复。
> 每条断言标注「事实 / 推断 / 未证实」，行号均在本报告快照上实测。**复核推翻了首轮的 2 条断言，本报告是修正后的版本**，见第 6 节。

## 0. 快照与实测环境

| 仓 | 分支 | HEAD |
|---|---|---|
| ds-editor-lite | `synthrt/spec2.4-uptake` | `9b55767b9`（工作区有 `scripts/vcpkg-ports/wolf/portfile.cmake` 的 LOCAL-ONLY 手改，属既定事实） |
| synthrt | `spec2.4-uptake` | `0e97662` |
| wolf | `spec2.4-uptake` | `dc49820` |
| otter | `spec2.4-uptake` | `6db876b` |

- 权威规范：`D:\projects\synthrt\docs\ds-spec-2.4.md`（891 行）。`D:\projects\wolf\docs\ds-spec-2.4.md` 是同名副本。
- 测试声库：`D:\projects\OpenUtau\OpenUtau.Test\bin\Release\net8.0-windows\Singers\2.4`，含 `qixuan@2.7.0.0`、`yousa@1.65.0.0`、`zhibin@26.7.16.0`（共 11 份 `linguist.json`）。
- 语言包：`D:\projects\ds-editor-lite\vcpkg\installed\x64-windows\share\wolf\packages`（bundle `0.1.2.0`，15 个包）。
- 实测工具链：MSVC `14.51.36231`（`D:\Programs\vs2026`），链接 vcpkg `installed\x64-windows` 的 `synthrt` / `synthrt-wolf`。
- 实测产物（`.tmp/spec24-audit/`，被 `.git/info/exclude` 排除，未入库）：审计 harness 源码 `audit_main.cpp`、全量运行日志 `run-dataonly.txt` / `run-load2.txt`、探针副本 `probe/`。

## 1. 需人工确认的关键点

> 需要拍板的设计决策，不是缺陷。缺陷见第 3 节。

- **D1｜spec 2.4 的单一真相源在哪？** synthrt 与 wolf 各持一份全文（各 891 行）。主代理逐行索引比对：**只有 51 行不同，全部同段落中文改写，无一条规则增删或宽严变化**（差异行号：29,31,163,236,238,241,260,262,264,266,270,283,289,489,508,515,517,535,537,541,547,549,551,552,554,558,564,618,652,658,662,687,730,733,735,737,739,741,747,749,755,759,763,764,765,766,768,770,774,776,780）。选：①保持两副本并加一个校验副本漂移的脚本（推荐，成本最低）②抽成独立仓、两仓只写引用 ③合并到 synthrt 单仓。
- **D2｜`desc.json` 的 `name` 字段去留。** spec 的 desc.json 字段清单（`:143-159`）没有它，《多语言文本·用在哪些字段》表（`:296`）却列了它，三个测试声库都写了它，实现零消费（F1）。选：①补进 spec 并在 synthrt 落地读取（推荐，声库数据不用改）②从 `:296` 删除并要求声库改数据。
- **D3｜保留音素的缺省语义统一到哪一套。** spec `:739`「缺省为空」＝不声明就等于「无保留音素」。wolf 是覆盖语义 + `{SP,AP}` 兜底（`LinguistSession.cpp:287,614-623`），lite 是并集语义 + `{SP,AP}` 强制（`ReservedPhonemes.h:20,27-37`）。选：①统一为「声明即权威，缺省为空」，SP/AP 只在**合成输入层**补齐（推荐）②把「宿主强制集」写进 spec，承认它是规范语义。
- **D4｜「每语言默认歌词」是否上升进 spec。** spec 全文 `defaultLyric` 零命中，但它已是生态既成事实（wolf 类别字段 + lite 内置表 + 用户设置 + 3 份 `"la"`）。选：①在 singer 或语言类别里成组定义「编辑器列语言所需字段」（推荐）②留给类别私定，但要求读者接受消费者必须硬编码 wolf 类别知识。
- **D5｜`openSet` 描述谁。** 它同时出现在 `linguist.json` 与其 `linguist/g2p` 成员的 `exports.openSet`，两处无一致性校验，实测已出现矛盾（F5）。选：①保留两处并加蕴含校验（推荐）②只保留 linguist 层。
- **D6｜lite `effectiveDefaultLanguage()` 让歌手默认语言压过轨道选择，是否符合产品意图。** 实现与 `SingingClip.h:66-68` 的注释、`docs/plans/note-language-auto-resolution-plan.md:129` 的终决策一致，所以不是笔误；残留风险是「轨道级选择在合成路径失效」（F6）。

## 2. 结论先行

1. **三套测试声库在 spec 2.4 的强制项上全部自洽**（实测，`Load` 模式全绿）：`defaultLanguage` 都在 `languages` 键内，每条语言 role 都解析到 `WolfLinguist` 且其 `language` 与句柄逐字相等，全部保留音素在 4 个模型的 `phonemes.json` 里都存在。**没有 P0。**
2. **最大的债不是"字段存了几份"，而是"规范没写、数据与代码已经在用"**：`desc.json.name`（F1）、语言句柄的语义规则（F2）、「每语言默认歌词」（F3）、`openSet` 的归属（F5）。四类都是"实现先行、规范缺位"，读者只能靠读代码回答"谁是真相源"。
3. **spec 2.4 的两份副本没有规则漂移**（实测 51 行纯措辞）。真正的问题在「类别追加字段」这条缝：类别开放给了扩展自由，但编辑器必须在选 provider 前就拿到语言、头像、默认歌词，spec 只覆盖了其中一半。
4. **最硬的一条冗余链有真实后果**：`exports.varianceControls` 与 `configuration.parameters` 描述同一事实（模型吃哪些参数）却各写一遍、零交叉校验；zhibin 因此声明 `energy` 被支持，而模型侧根本不会收到它（F7）。
5. **校验的覆盖面被高估了**：保留音素 ↔ 模型音素表的校验确实存在且生效（主代理探针证实），但它**只覆盖 `reservedPhonemes`，不覆盖 linguist 的 `exports.phonemes`**，于是 F9 的 `um` 缺口完全逃逸。
6. **lite 与 wolf 对同一事实各有一套口径**：保留音素（并集 vs 覆盖）、默认歌词（内置表 vs 类别字段）、默认语言（`ClipController.cpp:634` 用 `defaultLanguage()`，合成走 `effectiveDefaultLanguage()`）。各自单看都能自圆其说，合起来看是分叉。

| 级别 | 条数 |
|---|---|
| P0 必修 | 0 |
| P1 建议尽快 | 7 |
| P2 择期 | 6 |
| P3 顺手清理 | 7 |

## 3. 发现清单

### P1 建议尽快

#### F1 [事实] `desc.json` 的 `name`：规范没定义，实现没读，数据在写
- 规范：`ds-spec-2.4.md:143-147` 列 4 个必选字段、`:148-159` 列 9 个可选字段，**均无 `name`**；`:296` 的《多语言文本·用在哪些字段》表却写 `| \`name\` | 多语言文本 |`。同表 `:298` 还把 `avatar`/`background`/`demoAudio` 列为"标准字段"，而 `:724-727` 明说这三个是 `singer` 类别追加字段 —— 这张表混了「desc.json 字段」「模块公共字段」「类别追加字段」三个容器，且没限定作用域。`name` 在 spec 里的正式定义在 `:520`（**模块声明**的可选字段）与 `:525`。
- 实现：`synthrt/lib/Core/PackageLoader.cpp:1140` 原文 `package->name = DisplayText(package->id);`，随后 `:1156-1173` 依次读 `vendor`/`copyright`/`description`/`readme`/`url`，**独独不读 `name`**。另注：`:1304` 的 `declaration.find("name")` 是模块声明的名字，不是 desc.json 根。
- 消费：`lite/src/libs/SynthrtEngine/VoicebankCatalog.cpp:523` 把 `package.name()` 塞进 `SingerEntry::packageName`（`VoicebankCatalog.h:186`），但该字段**全仓无读取点**；`lite/src/libs/PackageManager/PackageManager.cpp:265-267` 构造 `PackageInfo` 时也没传包名。链路终端为空。
- 数据：`qixuan@2.7.0.0/desc.json:21-24` 原文 `"name": { "_": "Qixuan", "zh-CN": "绮萱" }`，yousa、zhibin 同。**多语言包名完全空转**，UI 标题退化为 id。

#### F2 [事实] 语言句柄：没有文法，三条真正的约束全在 spec 之外
- `ds-spec-2.4.md:306` 给 Package ID / category / contrib-id / interface / variant 都定了 ASCII 文法并规定区分大小写；`:732` 只说「键为语言句柄（推荐 ISO 639-3 代码）」，**无文法**，形成双标准。
- 真正生效的规则是「role 必须解析到 `linguist` 类别」+「目标 `language` 必须等于句柄」，落在 `wolf/src/plugins/linguistproviders/wolf/WolfLinguistProvider.cpp:95-125`（另见 `:141-160` 的 `validateLanguageMatch` 校验 `(language, scheme)` 必须命中成员的 `exports.languages`）。**主代理探针 D 实测确认它生效**：把 zhibin 副本的 `lang/cmn` role 改指 `:inference/s2p-cmn`（非 linguist 模块），Load 直接失败；还原后 Load 成功。
- `ds-spec-2.4.md:735` 把语义校验"外包"给「注册语言类别的库」，但没给触达点。若宿主不装任何 linguist 类别库，这两条规则无人执行。
- **同一 role 被两个句柄复用无人禁止**：`:634` 要求 `role` 在模块内唯一，但 `languages` 的值侧无唯一性要求，`synthrt/lib/SVS/SingerContrib.cpp:49-64` 只做「句柄非空 + role 存在于 imports」两项检查。

#### F3 [事实] 「每语言默认歌词」在 spec 里不存在，在生态里已经三层，而声库这一层是空的
- spec 全文 `defaultLyric` **零命中**（实测 `Select-String` 计数 0）。
- 三个承载者：wolf 的 linguist 类别字段（`wolf/src/lib/Linguist/LinguistContrib.cpp:62-63` 白名单 `{language, scheme, defaultLyric}`，`:114-127` 解析，`LinguistContrib.h:56` 暴露）；lite 的内置表（`lite/src/app/Model/AppOptions/Options/DefaultLyricRules.h:27-35`，cmn→啦、eng→la、jpn→ら、yue→啦）；用户设置 `general.defaultLyrics`。
- **链路"读到了但不生效"**：wolf → `VoicebankCatalog.cpp:457-462`（注释明写 "read here"）→ `PackageManager.cpp:122-127` → `LanguageInfo::setDefaultLyric` → `SingerInfo::defaultLyricForLanguage()`（`SingerInfo.cpp:143-149`），该函数**无生产调用方**（仅 `tst_package_catalog.cpp:122-127`）。`DefaultLyricRules.h:15-18` 原文承认："the editor reads that statement … but the resolution below deliberately does not consume it"。
- **主代理实测：11 份 `linguist.json` 全部未声明 `defaultLyric`**（15 个已装 wolf 包也零声明）。所以「每语言的默认歌词」今天完全由 lite 内置表 + 用户设置 + 硬编码 `"la"` 决定，声库的声明能力是空的。
- 硬编码 `"la"` 三份副本：`DefaultLyricRules.h:69`、`PublicAutomationRegistry.cpp:909`、`:2763`。
- 附带文档矛盾：`GeneralOption.h:22-25` 声称优先级里有「`declared`，即已安装歌手为该语言声明的值」这一层，但 `:26` 的函数签名没有 `declared` 形参。

#### F4 [事实] 保留音素：spec 说「缺省为空」，两条参考实现都不遵守，且口径互不相同
- spec `:739` 原文「元素必须是互不相同的非空字符串，缺省为空」；`:741` 原文「宿主与语言库均从该字段读取保留音素」。
- wolf：会话级 `std::vector<std::string> reserved = {"SP", "AP"};`（`wolf/src/lib/Session/LinguistSession.cpp:287`），`reservedFor()` 是**覆盖**语义 —— 歌手声明非空则只用声明，否则用会话兜底（`:614-623`）。另有公开 setter `setReservedMarkers()`（`:910-917`）。
- lite：`FORCED_RESERVED_PHONEMES{"SP","AP"}`（`lite/src/libs/SynthrtEngine/ReservedPhonemes.h:20`）+ `withForcedReservedPhonemes()` 是**并集**语义（`:27-37`）；`Note::isReservedPhoneme()` 先硬编码判 `AP`/`SP`，再 OR 歌手声明集（`lite/src/libs/ProjectModel/AppModel/Note.cpp:194-202`）。
- 后果：不声明 `reservedPhonemes` 的歌手（spec 语义是"无保留音素"）在两边都仍然拥有 SP/AP；声明 `["br"]` 的歌手在 lite 得到 `{SP,AP,br}`、在 wolf 得到 `{br}`。**主代理实测三套声库都声明了 AP 与 SP，所以今天两侧集合靠数据巧合相同，代码层永不汇合。**
- 修法现成但没接线：`lite/src/libs/SynthrtEngine/LanguageBridge.h:133` / `.cpp:191-193` 的 `setReservedMarkers` **全仓无生产调用方**（仅 `tst_voicebank_audit.cpp:637,692`）。

#### F5 [事实] `openSet` 在两处各声明一遍，无蕴含校验，实测已矛盾
- wolf 契约 `wolf/include/wolf/Api/Linguists/Linguist/1/LinguistApiL1.h:49` 定义该结构为 "phoneme inventory declared by one linguist composition"，`:57-64` 原文：「For the twelve shipped chains the list is incomplete, because each chain ends with a fallback to the original word and its output set is therefore unbounded.」—— 即这个标志正是为 fallback 链设的。
- 同一个「产出集合是否有界」既写在 `linguist.json` 的 `exports.openSet`（读点 `WolfLinguistProvider.cpp:450-462`），也写在 `linguist/g2p` 成员模块的 `exports.openSet`。
- 实测：

| 语言 | 包 | linguist.openSet | g2p 成员 | g2p 链末步 | g2p.openSet |
|---|---|---|---|---|---|
| cmn | qixuan/yousa/zhibin | false | `wolf/lang-cmn:inference/g2p` | `verify > model > fallback` | **无 `exports` 块**（默认 false） |
| jpn | qixuan/yousa/zhibin | false | `wolf/lang-jpn:inference/g2p` | `verify > dict > fallback` | **无 `exports` 块** |
| yue | zhibin | false | `wolf/lang-yue:inference/g2p` | `verify > model > fallback` | **无 `exports` 块** |
| eng | qixuan/yousa/zhibin | true | `wolf/lang-eng:inference/g2p` | `… > model > fallback` | true |
| kor | yousa | **false** | 同包 `:inference/g2p-ko` | 仅 `fallback{useOriginal:true}` | **true** |

- **两处矛盾**：①`yousa/linguists/kor-hangul` 与它自己的 g2p 成员直接相反（一条只有 fallback 的链，产出集合必然无界，成员诚实写了 `true`，组合层写 `false`）。②`wolf/lang-cmn|jpn|yue` 的 g2p 声明**连 `exports` 块都没有**，于是 `openSet` 恒为默认 false，与 `LinguistApiL1.h:57-64` 的自述相悖。已装包里 eng/ita/kor/por/zxx 六个包两侧都写 true，只有 cmn/jpn/yue 两侧都没写。
- 校验确实不存在：`WolfLinguistProvider.cpp:450-462` 只读 linguist 自己的 `exports`；`WolfImportValidator`（`:181-256`）只查 role 契约与 `language` 匹配，不碰 `openSet`。三套声库 Load 全绿，没有一条被拦下。

#### F6 [事实] lite 的默认语言：同一 clip 两个接口两个答案（已文档化，仍需裁决）
- `lite/src/libs/ProjectModel/AppModel/SingingClip.cpp:221-230` 的 `defaultLanguage()`：clip 显式值 → 父级 `Track::defaultLanguage()`（`Track.cpp:84-86`）。
- `SingingClip.cpp:232-243` 的 `effectiveDefaultLanguage()`：clip 显式值 → **歌手的 `defaultLanguage`** → 父级 track。
- 两者在「clip 未显式指定语言、track 有语言」时给出不同答案。`effectiveDefaultLanguage()` 是合成路径的上游（`Note.cpp:158`、`PianoRollGraphicsViewHelper.cpp:27`、`ProjectAutomationFacade.cpp:222-223`、`PublicAutomationRegistry.cpp:2768/4401/4646/4702`）。
- **不是笔误**：`SingingClip.h:66-68` 的注释原文就是 "effective language usable at inference: explicit clip language > singer package default > defaultLanguage() fallback"，实现与该注释一致。`docs/plans/note-language-auto-resolution-plan.md:129` 另记录了 clip 默认值保持 `"unknown"` 的终决策（理由正是 `effectiveDefaultLanguage()` 已把 `unknown` 视为 auto）。可达性也有限：`AppModel.cpp:195-210` 载入时把空/"unknown" 回写为 track 语言。
- 残留风险（事实）：`ClipController.cpp:634` 构造候选语言优先级列表时**首项**取 `defaultLanguage()`（`:634`），而合成走 `effectiveDefaultLanguage()`。当 clip 语言为空且 track 已选语言时，用户在轨道上选的语言只影响发音候选列表，不影响合成语言。→ 需 D6 裁决。

#### F7 [事实] `exports.varianceControls` 与 `configuration.parameters` 描述同一事实却各写一遍，zhibin 因此声明了一个模型收不到的参数
- 语义上两者必须相容：`varianceControls` 文档是「Variance parameters **required** as acoustic model inputs」（`dsinfer/include/dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h:45-46`），`parameters` 文档是「Parameters consumed by the acoustic model」（`:80-81`）。即 **`varianceControls ∪ transitionControls ⊆ parameters`** 必须成立。
- `zhibin@26.7.16.0/inferences/acoustic/config.json`：`exports.varianceControls` = `["breathiness","tension","voicing","energy"]`，而同文件 `configuration.parameters` = `["gender","velocity","breathiness","tension","voicing"]` —— **没有 `energy`**。
- 后果链（已逐段实测）：`VoicebankCatalog.cpp:383` 把 `varianceControls` 收进能力集 → `PackageManager.cpp:81-84` 并入 `acousticParameters` → `ParamUtils.cpp:100-103` 使 `ParamInfo::Energy` 对该歌手**受支持**（UI 可画 Energy 曲线）→ 但 `AcousticTask.cpp:289` `satisfyEnergy = !hasParam(Energy)` 因 `parameters` 无 `energy` 而为 `true` → `:378-382` `if (!satisfyEnergy && param.tag == Energy)` 永不成立，**energy 张量从不写入 ONNX 输入**。
- 净结果：用户画在 Energy 轨上的值被静默丢弃，而 UI 依据 `varianceControls` 认为该歌手支持 Energy。
- 无人发现的原因：`varianceControls`/`transitionControls` 在 synthrt 里**只被解析、零跨校验**（全仓 grep 只有 `AcousticInterpreter.cpp:37-49` 的 parse 代码）。
- 同理，variance 模块里 `configuration.predict_breathiness/voicing/tension: true` 与 `exports.predictions: ["breathiness","tension","voicing"]` 是同一组事实的两个副本，同样无校验（当前恰好一致）。

### P2 择期

#### F8 [事实] yousa 的 kor 与发行版语言包走了两条并行链路
- `yousa/inferences/g2p-ko/inference.json` 的 `configuration.steps` **只有一步** `fallback{useOriginal:true}`，即 G2P 是恒等变换。
- 发行版已有对等能力的包 `wolf-lang-kor`：`scheme: "romaja"`，链为 `verify > dict(kor_dict.txt) > model(multig2p) > fallback`，`openSet: true`，linguist 是 `kor-romaja`。
- yousa 自建了一套 `kor-hangul`（`scheme: "hangul"`）+ 同包 `g2p-ko`/`s2p-ko`，`korean_dict.txt` 放在自己的 assets 里，且 `desc.json` **没有**声明 `wolf/lang-kor` 依赖。
- 这不违反 spec（`contributions` 挂同包 inference 完全合法），但属分发层的重复投资：同一语言两套资源、两套 scheme、两套 `openSet` 声明（且相互矛盾，见 F5）。

#### F9 [事实] 3 份 linguist 声明了模型表里不存在的 `um`，且校验覆盖不到
- 实测：把每个 linguist 的 `exports.phonemes` 与该语言在 acoustic `phonemes.json` 的 `"<lang>/<phone>"`（或裸键）求差集：

| 包 | linguist | 声明数 | 表里缺失 |
|---|---|---|---|
| qixuan | cmn-pinyin | 64 | — |
| qixuan | eng-arpabet | 42 | — |
| qixuan | **jpn-romaji** | 40 | **um** |
| yousa | **cmn-pinyin** | 64 | **um** |
| yousa | eng-arpabet | 42 | — |
| yousa | **jpn-romaji** | 39 | **um** |
| yousa | kor-hangul | 34 | — |
| zhibin | 全部 4 个 | 63/42/39/70 | — |

- 对照：`qixuan` 只有 `cmn/um`（id 109），没有 `jpn/um`；`yousa` 一个 `um` 类键都没有（4 份表各 200 键）；`zhibin` 有裸 `um`（id 79）且把它列进 `reservedPhonemes`，因此被 F7 同源的 `validateReservedPhonemes` 保护。
- **加载期零校验**：`DiffSingerProvider.cpp` 的校验只对 `reservedPhonemes` 求差集，不查 linguist 的 `exports.phonemes`（见 F12）。
- 后果（推断，未证实）：若某条歌词被语言层产出为 `um`，模型查表会失败，报错或静音。可达性未证实 —— pinyin/romaji 正常不产出该音节。

#### F10 [事实] synthrt 对两处 spec 未要求的加严
- `synthrt/lib/SVS/SingerContrib.cpp:66-76`：只要声明了 `defaultLanguage` 就要求它是 `languages` 的键。spec `:733` 整句受「`languages` 非空时」限定，因此 `{"languages":{},"defaultLanguage":"cmn"}` 按字面不违规。**主代理探针 C 实测：该组合 Load 失败（`category failed to parse contribution`）**。
- `SingerContrib.cpp:50`：额外要求句柄非空、role 值非空。role 非空已由 `synthrt/lib/Core/PackageLoader.cpp:1334` 的 `isValidImportRole` 保证，属重复校验。

#### F11 [事实] 类型负例零测试
- `synthrt/tests/auto/SVS/test_SVSContrib.cpp:235-286` 只有 5 个负例（`:259` role 不存在、`:264` 默认值非键、`:269` 缺默认值、`:273` 保留音素重复、`:277` 保留音素空串），以下 spec 强制规则**无任何负例**：`languages` 非 object、`languages` 值非字符串、`defaultLanguage` 非字符串、`reservedPhonemes` 非 array、元素非字符串。
- 5 条断言全是 `BOOST_CHECK(!unit.openPackage(root, srt::SynthUnit::DataOnly))`，不判原因，任一条被别处先拦下测试仍绿。
- 正面覆盖是有的：`:183-225` 断言了一个带三个字段的声明被正确解析。
- `dsinfer/tests/auto/Inference/test_InferenceCompatibility.cpp:232-237` 用 `["SP","SP"]` 断言失败，但声明会先被 `SingerCategory`（`SingerContrib.cpp:105-108`）拦下，dsinfer 侧那段代码永远看不到重复项 —— 测试仍绿但意图漂移。

#### F12 [事实] 校验覆盖面：保留音素查了，语言音素表没查；未知字段放宽为 debug
- 正面：`dsinfer/plugins/singerproviders/diffsinger/DiffSingerProvider.cpp:147-183` 的 `validateReservedPhonemes` 对 duration/pitch/variance/acoustic 四处求差集，调用点 `:221`，注册于 `:230-235`，由 `PackageLoader.cpp:934` 在 `validatePreparedImports` 阶段调用。执行顺序 `PackageLoader.cpp:845`（createConfiguration）→ `:995`（prepareImportBindings）→ `:1000`（validatePreparedImports）→ `:1005`（attachSpecExtensions）保证校验时配置已解析完。**主代理探针 A/B 实测：加一个 `ZZZ_BOGUS` 保留音素 → Load 失败；还原 → Load 成功。**
- 缺口：该校验**只覆盖 `reservedPhonemes`**，不覆盖 linguist 的 `exports.phonemes`（F9 因此逃逸）；`reserved.empty()` 时直接早退（`:156-158`）。
- `synthrt/lib/SVS/SingerContrib.cpp:233-250`：声明里不认识的字段现在只 `srtDebug`，不再拒绝。**这符合 spec `:74`「未知字段不得导致拒绝」**，不是违规；代价是 `"reservedPhoneme"`（少一个 s）这类拼写错误会静默失效，AP/SP 重新被当普通词送进音素转换。`InferenceContrib.cpp:119-131` 同型改动。

#### F13 [事实] `DurationType::None` 与 `Abs` 语义完全相同
- `dsinfer/include/dsinfer/Api/Inferences/Duration/1/DurationApiL1.h:42-49` 两者注释同为「Declares no word level input」；`DurationTask.cpp:268-272` 用 `case None: case Abs: break;` 合并。属可接受的枚举冗余，但将来加分支时容易只顾一个。

### P3 顺手清理

- **F14** 宿主自实现的保留标记判断：`wolf/src/tests/auto/EndToEnd/test_HostFlow.cpp:161-163` 硬编码 `SP`/`AP`，同文件 `:180-181` 注释自认「every host implements the rule again」。
- **F15** `SingerEntry::defaultLanguage` 在 wolf 内只写不读：写于 `LinguistSession.cpp:368`，字段声明 `LinguistSession.h:138-140`，`src/lib` 与 `src/plugins` 内无读取点（`WolfLinguistProvider.cpp:316,416` 读的是另外的对象）。
- **F16** `WolfLinguistProvider.cpp:267-269` 的 `m_handles` 与 `m_languages.entries` 恒等（`:312` 返回、`:375` 声明）；`locate()` 每次调用重跑 `findImport`（`:319-330`）。
- **F17** 同一 lint 规则两处写：`wolf/scripts/check-declarations.py:394-400` 与 `synthrt/lib/SVS/SingerContrib.cpp:89-112` 各一份 `reservedPhonemes` 形状校验（脚本 docstring `:350-351` 自认是 mirror）。脚本另有 `:391-392`「`reservedPhonemes` 属于声明根、不属于 configuration」的规则，synthrt 侧没有。
- **F18** `dsinfer/util/inferutil/src/InputWord.cpp:76-77` 查表失败的是 `phone.language`（`:73`），错误消息却打印 `phone.token`；`:20-21` 另有 `SP_TOKEN`/`AP_TOKEN` 硬编码旁路，未从 `reservedPhonemes` 读。
- **F19** 文档与实现小偏差：`wolf/docs/linguist-domain-contract.md:192` 说非字符串/空串/纯空白都告警，实现只对非字符串告警（`LinguistContrib.cpp:120-125`）；`synthrt/lib/SVS/SingerContrib.cpp:87-88` 的注释把「校验保留音素在模型表中」的执行者说成 "the singer validator"，实际执行者是 `DiffSingerProvider` 的 import validator。
- **F20** otter：`docs/schemas/note-1-exports.schema.json:34-36` 把「`defaultLanguage ∈ languages`、`languages` 非空时必填」只写在 description 里（无 `if/then`），而 `otter/src/lib/Api/Note/1/NoteApiL1.cpp:81-91` 是硬拒绝 —— schema 弱于校验器。另 `otter/README.md` 的声明例子是全仓唯一无测试守护的第四份字段副本。附注：otter 的 `Note` 契约独立重实现了一遍「`languages` + `defaultLanguage`」形状与规则，与 spec 2.4 的 singer 侧形成第三处同规则实现（synthrt / otter / spec 文案）。

## 4. 真相源盘点

### 4.1 保留音素 `reservedPhonemes`

| 环节 | 位置 | 说明 |
|---|---|---|
| 声明 | `characters/*/config.json` | 三套声库分别 4 / 14 / 5 项 |
| 类别解析（唯一） | `synthrt/lib/SVS/SingerContrib.cpp:89-112` | 类型/互异/非空；单一存储 `SingerSpec::m_reservedPhonemes` |
| 宿主读（lite） | `ReservedPhonemes.h:20,27-37`；`Note.cpp:194-202`；`SynthrtEngine.cpp:819-824` | **并集**语义，强制 `{SP,AP}` |
| 语言库读（wolf） | `LinguistSession.cpp:369` → `SingerEntry::reservedMarkers` → `reservedFor():615-623` → 拦截 `:826-838` | **覆盖**语义，兜底 `{SP,AP}`（`:287`） |
| 契约校验 | `DiffSingerProvider.cpp:147-183`，调用点 `:221` | 对 4 个模型 `phonemes.json` 求差集；实测生效 |
| 同一 token 语义漂移 | `zhibin` 保留 `um`（裸键 79）；`qixuan` 有 `cmn/um`（109）且不保留；`yousa` 声明 `um` 可产出但表里没有 | 见 F9 |

**结论**：解析与存储唯一；**读取口径不唯一（并集 vs 覆盖）**；模型表校验齐全且生效，但只覆盖本字段。

### 4.2 语言 `languages` / `defaultLanguage`

| 环节 | 位置 | 唯一性 |
|---|---|---|
| 声明 | `characters/*/config.json` | — |
| 结构 + role 存在性 | `synthrt/lib/SVS/SingerContrib.cpp:39-85` | 唯一 |
| 语言语义（role→linguist、language==句柄） | `wolf/src/plugins/linguistproviders/wolf/WolfLinguistProvider.cpp:95-125` | 唯一，仅对声明了 languageMap 的 singer 生效 |
| 宿主消费 | lite 六级继承，见 F6 | **`ClipController.cpp:634` 与合成路径读不同接口** |
| 模型语言 id | `inferences/*/languages.json`（`cmn→4` 等） | 与句柄**零交叉校验**；运行期汇合点 `dsinfer/util/inferutil/src/InputWord.cpp:73`，缺键即 `InvalidArgument` |

**结论**：句柄集合在「歌手声明 / 模型 languages.json / linguist 模块」三处并存，全部靠内容作者手工对齐。

### 4.3 模型的参数声明（新增）

| 冻结事实 | 声明处 A | 声明处 B | 校验 |
|---|---|---|---|
| 模型吃哪些参数 | `configuration.parameters`（`AcousticTask.cpp:283` 真正用它决定是否喂 tensor） | `exports.varianceControls` + `exports.transitionControls`（lite 用它决定 UI 是否开放参数轨） | **无**（F7） |
| variance 预测哪些参数 | `configuration.predict_energy/breathiness/voicing/tension` | `exports.predictions` | **无** |

### 4.4 每语言默认歌词

| 来源 | 位置 | 是否生效 |
|---|---|---|
| 声库声明 | `linguist.json` 的 `defaultLyric` → `LinguistContrib.cpp:114-127` → … → `SingerInfo::defaultLyricForLanguage()` | **不生效**（末端无生产调用方；11 份声明实测全空） |
| lite 内置表 | `DefaultLyricRules.h:27-35` | 生效 |
| 用户设置 | `general.defaultLyrics` | 生效（优先级最高） |
| 硬编码兜底 | `DefaultLyricRules.h:69`、`PublicAutomationRegistry.cpp:909`、`:2763` | 生效，3 份副本 |

## 5. 实测记录

### 5.1 三套声库全量加载（`Load` 模式，含依赖与 provider）

```
mode=Load id=qixuan version=2.7      compat=2.7      runtimeLevel=1
mode=Load id=yousa  version=1.65     compat=1.65     runtimeLevel=1
mode=Load id=zhibin version=26.7.16  compat=26.7.16  runtimeLevel=1
```

| 检查 | qixuan | yousa | zhibin |
|---|---|---|---|
| `defaultLanguage` ∈ `languages` | 是（cmn） | 是（cmn） | 是（cmn） |
| 每条语言 role 解析到 `WolfLinguist` | 3/3 | 4/4 | 4/4 |
| 目标 `language` == 句柄 | 3/3 `MATCH=1` | 4/4 | 4/4 |
| 保留音素在 4 个模型表全在 | `reservedMissing=[]` ×4 | 同 | 同 |
| linguist `defaultLyric` | 全空 | 全空 | 全空 |

依赖版本对齐：`wolf/lang-cmn` 要求 `1.0.1.0`、装 `1.0.1.4`（compat `1.0.1.0`）；`wolf/lang-eng` `1.0.0.0` → `1.0.0.4`；`wolf/lang-jpn` `0.0.1.0` → `0.0.1.4`；`wolf/lang-yue` `1.0.1.0` → `1.0.1.4`。

### 5.2 对抗探针（含控制组，用于自证工具可信）

在 `.tmp/spec24-audit/probe/` 的 zhibin 副本上逐个改动（大文件用硬链接、仅 json 复制；原始声库全程未改，已核对仍是 5 项）：

| 探针 | 改动 | 结果 |
|---|---|---|
| 控制组 B | 不改 | `mode=Load`，`reservedMissing=[]` |
| A | `reservedPhonemes` 追加 `"ZZZ_BOGUS"` | `LOAD FAILED: prepared contribution imports failed validation` |
| C | `"languages": {}` 但保留 `defaultLanguage: "cmn"` | `LOAD FAILED: category failed to parse contribution` |
| D | `lang/cmn` 的 ref 改为 `:inference/s2p-cmn`（非 linguist） | `LOAD FAILED: prepared contribution imports failed validation` |
| 还原 | 恢复原值 | `mode=Load` |

每组改动后都跑一次控制组确认基线为绿，确保失败来自被测变量而不是副本本身。

### 5.3 数据层交叉校验（无需构建）

- 保留音素 ∩ 各语言 `exports.phonemes`（区分大小写）：三套声库均为空集。大小写不敏感下 `zhibin` 的 `AP` 与 `yue` 的 `ap` 会碰撞，但模型表把 `AP`（裸键 1）与 `yue/ap`（94）分开，**实际不冲突**。
- `yousa` 的 `reservedPhonemes` 大小写成对（`AP`/`ap`、`CL`/`cl`、…），模型表也成对存在且**指向同一 id**（`AP=1, ap=1`）。这是刻意的别名设计。
- linguist `exports.phonemes` 对模型表的差集：11 份里 3 份缺 `um`（见 F9）。

> ⚠️ 取证陷阱：`ConvertFrom-Json` 对属性名**大小写不敏感**，会把 `jpn/n` 与 `jpn/N`、`AP` 与 `ap` 误报为"重复键"。必须用大小写敏感方式取键（`[regex]::Matches($raw,'"([^"]+)"\s*:')` 或 `Group-Object -CaseSensitive`）。

## 6. 对抗复核结果与本节修订

复核轮任务是**找反例推翻**首轮断言，10 条里 2 条被推翻/修正，2 条部分成立。诚实记录：

| # | 首轮断言 | 裁决 | 处理 |
|---|---|---|---|
| A1 | `desc.json` 的 `name` 无消费者 | 成立 | 保留为 F1 |
| A2 | 两个默认语言接口"矛盾"是缺陷 | **部分成立** | 事实成立，但 `SingingClip.h:66-68` + `note-language-auto-resolution-plan.md:129` 证明是有意设计 → **降级为 P2 的 F6**，改为"需 D6 裁决" |
| A3 | 保留音素两侧语义分叉、永不汇合 | 成立 | 保留为 F4 |
| A4 | `defaultLyric` 生产路径无消费者；**12 份**未声明 | **部分成立** | lite 生产代码确实读它（`VoicebankCatalog.cpp:457-462`），真正断点是末端无调用方 → 措辞改为"读到了但不生效"；**份数改为 11**（qixuan 3 + yousa 4 + zhibin 4） |
| A5 | `openSet` 两处声明无校验 | 成立且更严重 | 补充：`wolf/lang-cmn|jpn|yue` 的 g2p 声明**连 `exports` 块都没有** → F5 表格重写 |
| A6 | 保留音素↔模型表校验生效 | **成立，未能反驳** | 复核攻了执行顺序（校验时 `configuration()` 是否为空），被 `PackageLoader.cpp:845→995→1000→1005` 的顺序堵死；但确认其覆盖面只到 `reservedPhonemes` → 计入 F12 |
| A7 | wolf 语言校验器真的校验 | 成立 | 保留为 F2 |
| A8 | spec 无 `defaultLyric`、字段表无 `name`、多语言表有 `name` | 成立 | 保留为 F1/F3 |
| A9 | yousa 声明 `um` 但模型没有 | 成立且范围更大 | **新增 `qixuan/jpn-romaji`**，共 3 份 → F9 表格重写 |
| A10 | zhibin 的 `energy` 是死声明 | **反驳** | `energy` 是宿主绘制的载荷参数，链路完整（`VoicebankCatalog.cpp:383` → `PackageManager.cpp:81-84` → `ParamUtils.cpp:100-103` → `InferControllerHelper.cpp:171`）。首轮的"无人产出"框架是错的 → **F7 重写**为真正的缺陷：`varianceControls ⊄ parameters` 导致 `AcousticTask.cpp:378-382` 永不写入 energy 张量 |

**主代理第一轮自身的错误**：曾凭 `Compare-Object` 的集合差异断言两份 spec 副本"部分内容实质不同"。**该说法是错的** —— 逐行索引比对证明 891 行对 891 行、仅 51 行不同、全部为中文改写，无规则变更。`Compare-Object` 报 100+ 条是集合差异 + 重复行折叠，不能当行数差用。

**子代理的一处严重错误**：曾断言「保留音素与模型音素表的一致性校验在 wolf / synthrt / lite 三仓均无实现」，并据此把三处文档定为"承诺落空"。**该说法是错的** —— 校验在 `synthrt/dsinfer/plugins/singerproviders/diffsinger/DiffSingerProvider.cpp:147-183`（调用点 `:221`），主代理用探针 A/B 实测证明它生效。该子代理只 grep 了 `wolf/src`、`synthrt/synthrt`、`lite/src`，漏掉同仓的 `dsinfer/`。

**不报为缺陷（有意设计）**：
- `s2p-*` / `onset-*` / `g2p-*` 挂在 `contributions.inference` 类别下但 `interface` 是 `org.openvpi.wolf.inference.*` —— spec `:570`「category 是选择 factory 与搜索路径的外部上下文，不属于 provider 的匹配键」+ `:556` 第三方 interface 用自家命名空间，合规。建议只在 spec 补一句「category 与 interface 无对应关系」。
- `Probe` / `load` 之间「必须拒绝但无法提前知道」的窗口 —— spec `:42`、`:384`、`:580`、`:590` 前后一致，不是缺口。
- singer 类别追加字段计数 3+2+1=6 与 spec `:743`「这六个字段」自洽。
- `desc.json` 里 `yousa` 的 `"name": "YOUSA 1.65"` 是裸字符串 —— 按 spec `:283`「只写一个字符串，等价于只有 `_` 一项」，合法。
- `SingerContrib.cpp:233-250` 未知字段不拒绝 —— spec `:74` 明文要求，合规（只有可诊断性代价，见 F12）。
- yousa 的 `kor-hangul` 只有 g2p + s2p 没有 onset —— `wolf/docs/linguist-domain-contract.md:296`「`linguist/g2p` 是唯一必选的 role」，合法。
- lite `PackageInfo.license` 由 `copyright` 填充 —— 消费方命名，非 spec 字段。
- `AudioSlicer.cpp` 里 grep `maxSegment` 零命中 —— 参数实名为 `maximumSpan`，不是"声称未实现"。
- otter 的 schema↔lint↔C++ reader 三份字段清单已被 `test_check_declarations.py:589-621` 机械闭环 —— **受控冗余，属优点**。
- `src/3rdparty/`、`vcpkg/`、`.tmp/` 下的第三方代码不在审计范围。

## 7. 建议的处置顺序

1. **先裁决 D3 / D4 / D6**。这三条定了，F3/F4/F6 的修法就唯一，且能把 lite 的 `setReservedMarkers` 这条现成的断头线接上。
2. **F7 优先修**：它是本报告里唯一会让用户白画一条参数曲线的缺陷，修法也最简单 —— 在 `DiffSingerProvider` 的 import 校验里加一条 `varianceControls ∪ transitionControls ⊆ configuration.parameters`；同时修 zhibin 的数据（`parameters` 补 `energy`，或从 `varianceControls` 删 `energy`，取决于模型到底吃不吃 energy，需听模型作者）。
3. **F5 的 `openSet` 蕴含校验**：在 `WolfImportValidator` 里加「g2p 成员 `openSet=true` ⇒ 组合 `openSet=true`」，同时修 `wolf/lang-cmn|jpn|yue` 三包缺失的 `exports` 块与 yousa 的 `kor-hangul`。
4. **F9 把现有差集校验扩到 linguist `exports.phonemes`**：复用 `validateReservedPhonemes` 的写法，按语言前缀展开。
5. **D2 落地 `desc.json.name`**：改动小（synthrt 一个 `readOptionalText` 调用），收益是多语言包名不再空转。
6. **F11 补类型负例**、**F10 决定加严是否保留**、**F12 决定拼写错误的诊断级别**。
7. 其余 P3 随下一次接口整理一起做。

---

_本报告为只读审计产物，未修改任何仓库文件，未提交 git。临时实测产物位于 `.tmp/spec24-audit/`（已被 `.git/info/exclude` 排除）。_
