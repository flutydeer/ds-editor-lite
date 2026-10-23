# spec 2.4 语言字段与 docs/synthrt 文档核对

> 快照：lite `c2ed02eb7`、synthrt `0e97662`、wolf `38ad5b0`、cpp-pinyin v2 `e7aacb7`（2026-10-21 实测）。
> 本文所有行号均为当轮现场读取，未构建、未运行、未联网。
> 标注：**事实**＝我本人回读代码/文档确认；**转述**＝子代理报告且我未逐条回读；**未证实**＝读不到或超预算。

---

## 0. 结论速览

1. **两个字段都有设计，也都写进了文档，但分属两份不同的权威文本**：歌手默认语言在**上位规范**里（`ds-spec-2.4.md:730-735`），wolf 每语种默认歌词在**wolf 自己的类别契约**里（`linguist-domain-contract.md:180-195`）。这一分层是对的——前者是 singer 类别追加字段，后者是第三方的 linguist 类别追加字段。
2. **但两份文档各有写过头的地方，合计 5 条与代码不符**（§2.1 的 L-1、L-2 与 §2.2 的 L-3..L-5）。其中「消费优先级」那条最实质：契约推荐的优先级链里有一层在实现中根本不存在。
3. **一处真缺口**：新建音符时，**语言**取声库声明的 `defaultLanguage`，**歌词**取应用设置，声明的 `defaultLyric` 被读入却无人消费。这条「语言由声库定、词由应用设置定」的规则**没有任何权威文本记载**，只活在 lite 代码与 plans 里（§2.4）。
4. **`docs/synthrt` 有 6 条事实错**（§3），最硬的两条：`PLAN.md:64` 与 `README.md:42-43` 对分发 zip 结构给出**互相矛盾**的结论；`conformance-yousa-2.4.md:18` 引用的审计工具 `TestVoicebankAudit` 已经不存在。
5. **冗余主要是数字**：`1,411,280`（导出波形字节）出现在 4 份文档，「15 个包」在 5 份文档出现 18 次。

---

## 1. 两个字段的权威位置

### 1.1 歌手默认语言 `defaultLanguage`

| 层面 | 位置 | 内容 |
| :-- | :-- | :-- |
| 上位规范 | `synthrt/docs/ds-spec-2.4.md:730-735` | 与 `languages` 成对引入。`:733` 三条规则：字符串、`languages` 非空时**必填**、必须是 `languages` 的一个键。`:735` 划清分工：singer 类别只验结构与 role 存在性，语义交给注册语言类别的库 |
| 实现 | `synthrt/synthrt/lib/SVS/SingerContrib.cpp:39-85` | 唯一解析点，调用点 `:279-283`，Probe 期执行 |
| wolf 契约 | `wolf/docs/linguist-domain-contract.md:510-529` | §10.4（无加载期与运行时语义、为何不能取「第一个」键）+ §10.5 校验分工表 |
| 决策 | `wolf/docs/linguist-decisions.md` A11（引入）/ A13（无运行时语义） | |
| lite 消费 | `VoicebankCatalog.cpp:459-465`、`:650`；`SingingClip.cpp:232-243` → `Note.cpp:158` | |

### 1.2 wolf 每语种默认歌词 `defaultLyric`

| 层面 | 位置 | 内容 |
| :-- | :-- | :-- |
| 上位规范 | **无**，这是正确的：它是 wolf 自定 linguist 类别的追加根字段，规范只规定类别是开放集合 | |
| wolf 契约 | `wolf/docs/linguist-domain-contract.md:180-195`（§2.5） | 五个面都铺开：作者归属、消费优先级建议、不为可发音性担保、取值容错、可读面 |
| 实现 | `wolf/src/lib/Linguist/LinguistContrib.cpp:107-127`（`readDefaultLyric`）、`:62-63`（白名单）、`:197`（调用） | Probe 期 |
| 决策 | 已实施：`VoicebankCatalog.cpp:459-462` 读入、`DefaultLyricRules.h:16-18` 明写不消费，即原方案 D1 / D5 的裁定结果（方案文档已删除） | |
| lite 消费 | `VoicebankCatalog.cpp:459-462` **读入**，`DefaultLyricRules.h:16-18` 明写**不消费** | |

---

## 2. 两个字段的文档缺陷

### 2.1 `defaultLanguage`：无实质缺陷，两处措辞过写

| # | 位置 | 文档写 | 代码实测 | 结论 |
| :-- | :-- | :-- | :-- | :-- |
| L-1 | `linguist-domain-contract.md:521` | 每个键是「非空合法 segment」 | `SingerContrib.cpp:49-53` 只判 `handle.empty()`，**无字符集校验**。segment 文法在同文 `:144` 定义为 `[A-Za-z0-9_-]+` | **事实**：文档超出实现 |
| L-2 | `linguist-decisions.md` A13 / `linguist-runtime.md:392` | 「语言域自身**从不读取**该字段」 | `WolfLinguistProvider.cpp:315-317` 确实读取 `m_languages.defaultLanguage` 并透传给宿主 | **事实**：「不参与行为」成立，「从不读取」字面不成立 |

### 2.2 `defaultLyric`：两个面写得比实现多

| # | 位置 | 文档写 | 代码实测 | 结论 |
| :-- | :-- | :-- | :-- | :-- |
| L-3 | `linguist-domain-contract.md:192` | 非字符串、空串、纯空白「一律视为未声明，**只报 warning** 并忽略」 | 只有非字符串报 warning（`LinguistContrib.cpp:120-124`）。空串与纯空白经 `trimmed()` 后**静默**返回空串（`:126`），无 warning。`null` 也静默返回（`:117`） | **事实** |
| L-4 | `linguist-domain-contract.md:188-189` | 推荐优先级「用户显式 > **本字段** > 消费方内置值」 | `DefaultLyricRules.h:62-70` 实为「用户 override > 内置表 > `la`」，**没有「本字段」这一层** | **事实**：契约推荐了实现里不存在的层 |
| L-5 | `linguist-domain-contract.md:193` | 「首尾空白会被裁掉」，未定义字符集 | `trimmed()`（`LinguistContrib.cpp:93-104`）只认空格 / 制表 / CR / LF，**全角空格 U+3000 不算空白** | **事实**：留白未收口 |

### 2.3 两字段的机器可读定义缺位

`wolf/docs/schemas/` 有 10 个 schema（g2p / s2p / onset / linguist 的 exports 与 import-options 两两成对，加 `language-scheme` 与 `string-set`），`exports` 对应 `linguist-1-exports.schema.json`，但全部 10 个文件里 `defaultLyric` **命中 0**（本人实测）。后果：打包 lint 对 `"defaultLyric": 7` 这种值零报告，只有加载期一条 warning。**推断**：这是有意取舍（规范 `:74` 只说框架 object 的未知字段不得导致拒绝，未要求每个根字段有 schema），但**取舍本身没被写下来**。

### 2.4 真缺口：语言与歌词走两条不同的路

| 值 | 来源 | 证据 |
| :-- | :-- | :-- |
| 新建音符的**初始语言** | 声库声明的 `defaultLanguage` | `SingingClip.cpp:232-243` → `Note.cpp:158` |
| 新建音符的**初始歌词** | 应用设置 / 内置表 | `DefaultLyricRules.h:62-70` |

声明的 `defaultLyric` 被 `VoicebankCatalog.cpp:459-462` 读入，**没有任何解析路径使用它**（`DefaultLyricRules.h:16-18` 原文 "deliberately does not consume it"）。

⇒ 实际语义是「**语言由声库定、词由应用设置定**」，与契约 `:188-189` 推荐的优先级方向相反，且**该优先级链在三份权威文本（spec / wolf 契约 / lite docs）里都没有**。

---

## 3. `docs/synthrt` 事实错（必须更新）

| # | 位置 | 原文断言 | 实测 | 建议改法 |
| :-- | :-- | :-- | :-- | :-- |
| F-1 | `PLAN.md:64` | 分发 zip「内部带顶层同名目录」 | `README.md:42-43` 说 2.4 是扁平结构、带外层同名目录的是 2.3 源；工具 `pack-voicebank-zip.py:4` 原文 "The layout is flat" | 改成「扁平：`desc.json` 与顶层目录直接在归档根」，与 README 对齐 |
| F-2 | `conformance-yousa-2.4.md:18`、`:134`、`:139-142` | 审计工具是 `TestVoicebankAudit`，命令 `TestVoicebankAudit.exe <包目录> <闭包目录>` | `src/tests` 下**已无该目录**（事实，我列过目录），逻辑并入 `TestModelResources/tst_voicebank_audit.cpp`，改用编译期缓存变量 `LITE_AUDIT_VOICEBANK` / `LITE_AUDIT_LANGUAGE_PACKAGES` 取参，无位置参数（转述） | 换成测试目标名 + 变量传参写法 |
| F-3 | `wolf-release-history.md:96-99`、`:106-108` | port 落点的 cmn 是 `variant: algo-pinyin` + 内嵌 `assets/ds-zh-pinyin-lite.txt`；其 `desc.json` 写 `1.0.1.0` | 现装树 `wolf-lang-cmn/desc.json` 是 `1.0.1.4` / compat `1.0.1.0`，g2p 声明是 **`variant: pipe-chain`**（事实，我读过装配树文件），与 release 归档同源 | 标注为 **2026-10-07 的历史观测**，并补一句「bundle 升到 0.1.2.0 后两条供给路径已同源（2026-10-21 实测）」 |
| F-4 | `packaging-voicebank-essentials.md:64` | 表格里 `wolf-lang-cmn` 的实现列写 `assets/ds-zh-pinyin-lite.txt` | 同上，现装树是 pipe-chain，且该包安装后只有 3 个文件 | 同 F-3，改这一行的实现列 |
| F-5 | `packaging-voicebank-essentials.md:186` | 「release 归档与 vcpkg port 的实现并不相同」 | 同 F-3，现为同源 | 同 F-3 |
| F-6 | `packaging-voicebank-essentials.md:69` | deu/fil/fra/ita/por/spa/jpn 内容「0.2~1.0 MB」 | 实测装配树：deu 1,031,455 B、fil 594,482 B、fra 514,409 B、**jpn 4,998 B** | 改成「从几千字节到约 1 MB 不等，jpn 最小」 |

---

## 4. `docs/synthrt` 行号失效与措辞（建议更新）

| # | 位置 | 问题 | 建议 |
| :-- | :-- | :-- | :-- |
| S-1 | `PLAN.md:41` | 引 `LiteBuildApi.cmake:327-449`，实测搜索链在 `:394-429`（事实） | 改行号 |
| S-2 | `PLAN.md:42` | 引 `SynthrtEngine.cpp:352`（转述：现值应为 385） | 改行号 |
| S-3 | `PLAN.md:39` | 引 `synthrt-main-migration.md:260-266`，内容不符（转述） | 复核后改指针 |
| S-4 | `developer-repro.md:53-55`、`README.md:37-38` | 说搜索链第 4 源「同级兜底」无条件生效、缺包时「配置阶段直接报错」 | 实测：该源由 `LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK` 控制，**默认 OFF**（`LiteBuildApi.cmake:424-425`）；缺包只在 `LITE_INSTALL` 时 FATAL（`:478-479`），否则 WARNING（事实） |
| S-5 | `packaging-voicebank-essentials.md:147-148` | 标注「未查证」 | 已由前序「wolf 发布包审计」查清（该报告已退役删除），建议改成直接陈述结论 |

---

## 5. 冗余与单一真相源

| 事实 | 重复次数 | 分布 |
| :-- | :-- | :-- |
| 导出波形 8 秒 = `1,411,280` 字节 | 5 处 / 4 份文档 | `README.md`、`PLAN.md`、`conformance-yousa-2.4.md`、`korean-via-wolf-kor.md` |
| 归档总字节 `18,113,900` | 3 份文档 | `README.md`、`PLAN.md`、`developer-repro.md` |
| 「15 个包」 | 18 处 / 5 份文档 | 同上加 `packaging-voicebank-essentials.md` |
| D-8 提供方政策 | 4 处 | `README.md`、`packaging-voicebank-essentials.md` 两节、`PLAN.md` 台账 |

建议：每个数字留一处权威（放在最贴近原始证据的文档），其余改指针。**这不改变任何事实**，纯维护性。

---

## 6. 另一处跨仓冗余：两份 spec 副本

`synthrt/docs/ds-spec-2.4.md`（79,991 B）与 `wolf/docs/ds-spec-2.4.md`（79,732 B）**都是 891 行**，逐行比对有 **102 行差异，全部为文风改写、无语义差异**（例：`分界在于字段的读取时机` vs `分界在什么时候读得到它`）。**事实。**

两份 80 KB 的规范副本长期并存，迟早漂移成真差异。可选处置：(a) wolf 那份改成一句「见 synthrt 正本」的指针；(b) 保留但在文件头注明「副本，同步于 <commit>」。

---

## 7. 决策台账（需你拍板）

| # | 决策 | 选项 | 推荐 |
| :-- | :-- | :-- | :-- |
| D-1 | 修到哪一档 | **(a) 只修 §3 的 6 条事实错**／(b) 加上 §4 的 5 条行号与措辞／(c) 再加上 §5 的冗余收敛 | **(a) 先做**：事实错会误导读者，行号失效只是不方便 |
| D-2 | §2 的 5 条字段文档缺陷改不改 | **(a) 改**：把契约里实现没做的事说清楚／(b) 不改文档，改为补实现（例如让空串也报 warning） | **(a)**：改文字零风险，改实现要动 wolf 并重建 |
| D-3 | §2.4 的「语言由声库定、词由应用设置定」要不要写成权威文本 | **(a) 写进 wolf 契约或 lite docs**／(b) 维持现状，只留 plans | **(a)**：现在只有代码知道这条规则 |
| D-4 | §2.3 的 `defaultLyric` 无 schema | (a) 补一份 schema 让 lint 能查／**(b) 在契约里明写「不做机器可读定义」是有意取舍** | **(b)**：先记录取舍，补 schema 等有第二份消费方再说 |
| D-5 | §6 的两份 spec 副本 | (a) wolf 那份改成指针／(b) 保留但标注同步基线 | **(b)**：指针会让 wolf 读者跨仓找文件 |
| D-6 | `wolf-release-history.md` §6.3 这类「当时为真的历史观测」怎么处理 | **(a) 加日期标注、不改结论**／(b) 整段删除 | **(a)**：历史台账的价值在于记录当时看到什么 |

---

## 8. 未证实 / 未覆盖

- `wolf-release-history.md:10-12` 的发行日期（09-09 / 09-28）与此前一次审计记录的 09-08 / 09-22 不一致（该审计报告已退役删除）。确认需要查 GitHub Release API（联网），本轮未做。
- `korean-via-wolf-kor.md` 的机体级数字（34 音素、`kor_dict.txt` 2.1 MB、`inference.json:8-42` 行号）未复核。
- `conformance-yousa-2.4.md` 的 60 文件 / 598,641,468 字节与「51 行规范差异」未复核。
- ~~`baselines/` 的 18 / 9 / 15 条数未核~~ —— **已核**：`cmn.txt` 18 行、`eng.txt` 9 行、`jpn.txt` 15 行，与 README 一致，不过时。
- ~~`packaging-voicebank-essentials.md:69` 的尺寸为子代理实测~~ —— **已改为本文实测值**（见 F-6）。
- ~~§2.3 的「10 个 schema 里没有 defaultLyric」~~ —— **已复核**：目录里 10 个 json，`defaultLyric` 命中 0。
