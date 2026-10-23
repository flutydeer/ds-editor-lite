# spec 2.4 向后兼容性深度审计（版本区间 / 语言包更新 / 老声库）

> 工具清理说明：本文引用的声库转换、下载审计、G2P 测量与 ZIP 工具及其专属测试已移除。
> 相关记录与行号仅适用于[移除前快照](https://github.com/flutydeer/ds-editor-lite/tree/10c1f19827239cb2072d1e5616ae5232be589728)。
> 当前运行时回归使用仓库内已升级为 spec 2.4 的 `voicebank-fixture.zip`，不依赖转换器。

> 第二轮深挖，专题是**规约层的向后兼容**：老声库 + 新语言包、新声库 + 老编辑器，以及"编辑器携带的 g2p 升一个小版本"会不会打死老声库。
> 方式：主代理构建实测（6 个对抗探针 + wolf 自带 lint 实跑）+ 2 域子代理只读深挖 + 主代理逐条抽验。
> 上一轮的通用审计见 [spec2.4-audit-report.md](spec2.4-audit-report.md)，本报告不重复其结论。

## 0. 快照与实测环境

| 仓 | 分支 | HEAD |
|---|---|---|
| ds-editor-lite | `synthrt/spec2.4-uptake` | `9b55767b9` |
| synthrt | `spec2.4-uptake` | `0e97662` |
| wolf | `spec2.4-uptake` | `dc49820` |

- 权威规范：`D:\projects\synthrt\docs\ds-spec-2.4.md`。
- 语言包 bundle：`0.1.2.0`（`wolf-lang-packages` 端口），实测安装 15 个包；`wolf/lang-cmn` `1.0.1.4`/compat `1.0.1.0`、`eng` `1.0.0.4`/`1.0.0.0`、`jpn` `0.0.1.4`/`0.0.1.0`、`yue` `1.0.1.4`/`1.0.1.0`。
- 测试声库：`Singers\2.4` 三包（qixuan / yousa / zhibin），依赖目标全部指向语言包**下沿**。
- 实测产物：`.tmp/spec24-audit/`（git 已排除）—— harness `audit_main.cpp`、探针语言包树 `lang-b/c/d/e/g`、探针声库 `vb-f`。

## 1. 直接回答你的问题

> "声库里写的依赖是 `1.0 =< x < 2.0` 比较合适，不会因为编辑器更新时携带的 g2p 更新一个小版本、导致老声库的不兼容"

**三句话说清：**

1. **spec 2.4 不支持区间写法。** `dependencies[].version` 只能是**单个目标版本**（spec `:161` 原文「表示依赖方要求依赖 Package 兼容到的版本，不表示必须加载该精确版本」；实现 `synthrt/lib/Core/PackageLoader.cpp:460` 的 `allowedKeys = {"id", "version"}`，`:419-452` 的 `readVersion` 只收 1~4 段十进制）。写 `">=1.0 <2.0"` 直接报 `dependency version has an invalid version`。所以你想表达的那层能力，规范和实现**都缺**。
2. **但你担心的具体场景本身是安全的。** 判定式是 `compatVersion <= target <= version`（spec `:186`、`:398`）。语言包从 `1.0.1.4` 升到 `1.0.1.5` 而 `compatVersion` 停在 `1.0.1.0` 时，写 `1.0.1.0` 的老声库**照常加载**——主代理实测 PROBE C 通过。
3. **真正会打死老声库的是 `compatVersion`（下沿）上移，与"小版本"无关。** 只要被依赖方把下沿从 `1.0.1.0` 抬到 `1.0.1.3`（哪怕 version 只动第 4 段），三套声库**全灭**（实测 PROBE G）。决定生死的是下沿，不是版本号涨了多少。

**所以这不是"区间太窄"，而是"区间宽度由被依赖方单方面决定、依赖方无权表达"的问题。** 你想要的语义现在只能靠一条纪律实现：**被依赖方永不抬下沿 + 依赖方永远指向下沿**。

## 2. 实测失效矩阵（6 个对抗探针，全部同一 harness、同一控制组）

| 探针 | 机器上的 `wolf/lang-cmn` | 声库依赖目标 | 实测结果 |
|---|---|---|---|
| CONTROL | `1.0.1.4` / compat `1.0.1.0`（现装） | `1.0.1.0` | 3/3 **Load 通过** |
| **B** 下沿上移到新三段 | `1.0.2.4` / compat `1.0.2.0` | `1.0.1.0` | 3/3 **失败** |
| **C** 仅修订号 +1 | `1.0.1.5` / compat `1.0.1.0` | `1.0.1.0` | 3/3 **通过** ✅ 你的场景 |
| **D** 编辑器携带更旧的构建 | `1.0.1.2` / compat `1.0.1.0` | `1.0.1.0` | 3/3 **通过** |
| **E** 下沿误设=当前版本 | `1.0.1.4` / compat `1.0.1.4` | `1.0.1.0` | 3/3 **失败** |
| **F2** 声库指向当前版本 | `1.0.1.2` / compat `1.0.1.0` | `1.0.1.4` | 1/1 **失败** |
| **G** 下沿在第四段内上移 | `1.0.1.5` / compat `1.0.1.3` | `1.0.1.0` | 3/3 **失败** |

失败时的完整错误链（`Error::toString()`，实测原文）：

```
failed to resolve dependency of zhibin: no installed Package satisfies dependency wolf/lang-cmn
```

**只有依赖 id。** 没有"要求 `1.0.1.0`、机器上是 `1.0.2.4`（下沿 `1.0.2.0`）"。老声库在用户机器上炸掉时，这是用户能拿到的唯一线索（见 C2）。

**一个附带的好消息与坏消息**：若机器上同时存在新旧两份语言包，`PackageLoader.cpp:658-669` 按搜索路径顺序逐条找，第一条有满足条件的候选就停 —— 所以只要旧包排在**靠前**的路径里，老声库仍能开，且静默用旧包（spec 允许的版本降级）。坏消息是同一个机制让"有时能开、有时不能"取决于安装路径顺序，而 C2 的诊断不提示这一点。

## 3. 现行规约体系：判据其实是对的，缺的是执行

这一节是本轮最重要的正面结论——**不要重造轮子，先补执行**。

### 3.1 spec 2.4 的区间语义
`:183-200`：Package 身份是 `(id, 规范化 version)`；`compatVersion <= target <= version` 才算兼容；兼容承诺覆盖**六项公开表面**（贡献类别与模块 ID、`interface` 与 `level`、`exports` 能力语义、`options` 写法、契约的运行时 IO 与错误语义、类别必选字段）；**当前 Package 可以增加贡献与可选能力，也可以改「实现细节、私有 configuration、模型与算法」**；破坏六项者必须抬下沿。

### 3.2 wolf 已经就此立过一条决策（A68）
`docs/linguist-decisions.md:1440-1477`：
- 语言包 `compatVersion` 取**修订号 0 下沿**，不是等于 `version`；
- 理由逐字引 spec `:198`：打包修订不破坏六项，抬下沿是**不实声明**，而 spec 明定「承诺不实属于 Package 缺陷」；
- **实证记录**：把 `wolf-lang-cmn` 的 version 与 compatVersion 一起提到 `1.0.1.3` 后，`test_HostFlow` 六个用例全部报 `no installed Package satisfies dependency wolf/lang-cmn`。

这与你的直觉完全一致，而且比"写区间"更省：**下沿不动，区间就无限宽**。`convert-g2p-packages.py:153-178` 的 `compat_floor()` 就是它的实现，`PACKAGING_REVISION` 已走到 4、下沿始终停在 `.0`。

### 3.3 依赖方的义务也已经写进 lite 的转换器
`lite/scripts/convert-voicebank.py:564-568` 原文：

> "The oldest version that the package declares compatibility with, not its current version: a dependency on the current version stops resolving as soon as the packaging revision changes."

所以三套测试声库的依赖目标恰好都等于下沿（cmn `1.0.1.0` / eng `1.0.0.0` / jpn `0.0.1.0` / yue `1.0.1.0`）——**这不是巧合，是工具算出来的**。

### 3.4 还有一条 lint 把"依赖方必须写下沿"定为 error
`wolf/scripts/check-declarations.py:596-655`。主代理**实跑**：把 3 个声库目录与 15 个语言包目录一起传给 `check-declarations.py`，结果

```
checked 58 declarations in 18 packages, 0 error(s), 16 warning(s)
```

**0 error** —— 说明数据侧今天是合规的，且这条 lint **能够**作用于第三方声库（CLI 是 `nargs="+"`，`check-declarations.py:704`）。

顺带：这次实跑还**独立复现**了上一轮 F5——lint 自己报出
`warning: yousa:kor-hangul: its G2P ends by returning the original word, so the phonemes list cannot be complete; declare openSet`。

### 3.5 结论
判据（A68 + spec）、工具（`compat_floor`、`convert-voicebank.py`、`check-declarations.py`）**都已经存在且方向正确**。缺的是让它们**必然被运行**。

## 4. 发现清单

### P0 必修

#### C1 [事实] 转换器的引擎版本源与实际引擎脱钩，下一次重打包会产出"同版本号、不同内容"的包
- `wolf/scripts/convert-g2p-packages.py:357` 原文 `port = repo_root / "scripts" / "vcpkg" / "ports" / "cpp-pinyin"`，该端口 `vcpkg.json` 仍是 `"version": "1.0.2"`；`pinyin_engine_version()` 的注释（`:353-355`）说明「词典与读它的引擎是同一个产物，所以版本跟随端口版本」。
- 但 wolf 自己的清单已经**只装 `cpp-pinyin2`**（`scripts/vcpkg-manifest/vcpkg.json` 里 `"cpp-pinyin"` 出现 0 次），`lite/scripts/vcpkg-ports/cpp-pinyin2/vcpkg.json` 是 `2.0.0`。
- 两套词典树形状不同：`share/cpp-pinyin2/dict/{mandarin,cantonese}` 比 `share/cpp-pinyin/dict` **多两个文件**（`dialect.json`、`format.txt`）。
- 主代理实测当前发布物是否已经错位：`share/wolf/packages/wolf-g2p-pinyin/inferences/pinyin/dict` 的三份词典
  - `mandarin/word.txt`、`mandarin/phrases_dict.txt`、`cantonese/word.txt` 与 **cpp-pinyin 1.0.2 字节相同**，与 **cpp-pinyin2 2.0.0 不同**。
  - 即**今天是对的**：`wolf-g2p-pinyin-1.0.2.4` 装的确实是 1.0.2 词典。
- 失效条件：`--cpp-pinyin-dict` 是 `required=True`（`:749`），而按现清单只能指向 `share/cpp-pinyin2/dict`。下一次重跑转换器 → 产出 `version 1.0.2.4`、内容为 2.x 词典、且**多出两个文件**。这正是 spec `:181`「发布者必须保证使用同一身份发布的 Package 内容相同」所禁止的 Package 缺陷。
- 这是本轮唯一"下一次例行动作必然踩到"的问题，所以定 P0，但**尚未发生**。
- 建议：`pinyin_engine_version()` 改为从清单里实际选中的端口解析（或从安装树读身份），并在 `make-lang-release.py` 之前加一条「引擎版本 == 包版本前三位」断言。

### P1 建议尽快

#### C2 [事实] 老声库失效时，用户拿不到任何可操作的信息
- `synthrt/lib/Core/PackageLoader.cpp:692-693` 原文 `return Error(Error::FileNotFound, "no installed Package satisfies dependency " + dependency.id);`；外层 `:749-750` 加 `"failed to resolve dependency of " + package->id`。
- `dependency.version` 就在作用域内但未被使用。实测最终文本见第 2 节。
- 影响：用户看到"依赖 `wolf/lang-cmn` 无法满足"，无法判断是自己该升级声库、还是编辑器带了不兼容的包。这一条恰好是本报告全部失效场景的共同出口。
- 建议：消息补 `dependency.version` 与所有已见候选的 `[compatVersion, version]` 清单。

#### C3 [事实] 候选被拒的原因被静默吞掉
- `PackageLoader.cpp:629-632` 原文 `auto packageResult = readPackage(directory, true); if (!packageResult) { continue; }`。
- 于是 `$version` 不支持（`:1088`）、`runtimeLevel` 过高（`:1091-1098`）、`compatVersion > version`（`:1129-1130`）、类别未注册（`:1210-1213`）、`desc.json` 坏（`:1072-1075`）全部退化为"这个包不存在"。
- 后果与 C2 叠加：语言包装了但要求更高 `runtimeLevel` 时，报错与"没装"完全不可区分。

#### C4 [事实] A68 亲自指定的回归方式被工具反向实现
- `wolf/docs/linguist-decisions.md:1476-1477` 原文：「回归测试必须使用「**旧夹具 + 新包**」的组合。夹具与包一起重新生成时该失效不会出现，A54 那次正是因此漏检。」
- 而 `wolf/scripts/make-voicebank-fixture.py:166-169` 原文「Require the version that the package declares as its compatibility floor instead of a version written here」，`:169` `required = json.loads((package / "desc.json").read_text())["compatVersion"]`，`:227` 写进 `dependencies`。
- 也就是说夹具的依赖目标**永远跟包同步**，`PACKAGING_REVISION` 抬到 5 时夹具自动跟随——A68 说的那种失效**不可能被复现**。

#### C5 [事实] 唯一的真实链路回归默认不跑
- lite 侧：`src/tests/TestModelResources/CMakeLists.txt:21` `set(LITE_AUDIT_VOICEBANK "" CACHE PATH ...)`，`tst_model_resources.cpp:277` `QSKIP("Set LITE_AUDIT_VOICEBANK and LITE_AUDIT_LANGUAGE_PACKAGES")`。`tst_voicebank_audit.cpp:425-427` 正是用 `{languagePackages, voicebank.parent_path()}` 打开真实声库——这就是"老声库 + 新语言包"的回归，但**默认 skip**。
- wolf 侧：`test_HostFlow.cpp:36-40` 需要 `WOLF_LANG_PACKAGES_SOURCE` 与 `WOLF_VOICEBANK_FIXTURE_SOURCE` 两个环境变量；`.github/workflows/ci.yml:80` 只传 `-DWOLF_TEST_FIXTURES_SOURCE`，`:27` 的注释自认「the voicebank fixture, which this workflow never generates」。`:92` 的 lint 只对 `build/test-fixtures/wolf-lang-zxx` 一个**无依赖**的包运行，`check_dependencies` 在 CI 里从未执行过依赖分支。

#### C6 [事实] 第三方声库**可以**被 lint，但没人跑
- `check-declarations.py:643-645` 原文 `target = known.get(dependency.get("id"))` / `if target is None: continue  # target not among the given packages; not checked`。
- 主代理实测：把 3 个声库与 15 个语言包一起传入即可检查（0 error）。**能力有、调用无**：wolf 的发布脚本 `make-lang-release.py:193` 只传转换输出，从不传声库。

#### C7 [事实] "产出内容变化"落在兼容承诺的留白区，且已经被用过一次
- spec `:189` 把承诺限定在「数据格式与 API 契约层面」，六项公开表面**没有一项**是"同一输入的产出集合"；`:198` 明允许改「模型与算法」。
- 而 `convert-g2p-packages.py:139-144` 的 `PACKAGING_REVISION 4` 做的是「给每个链字典加假名的片假名拼写」，实测 `share/wolf/packages/wolf-lang-jpn/inferences/g2p/kana2romaji.txt` 现在有 **176 条片假名 + 176 条平假名**。片假名词此前走 fallback 原样透传，此后有真读法——**产出变了，下沿没动**。
- wolf 侧唯一的正面表态是 `docs/linguist-distribution.md:511`「非破坏性内容更新（词典增补…）：只抬升 `version`，`compatVersion` 不变」；全仓 `docs/` 下"片假名/katakana"零命中，rev4 的决策依据只存在于代码注释里。
- 后果：老声库不会加载失败，但可能产出模型表里没有的音素。上一轮 F9 的 `um` 就是同一类漂移的现成实例（3 份 linguist 声明可产出、模型表里没有），且加载期零校验。

### P2 择期

#### C8 [事实] 区间表达能力在 spec 与实现里同时缺失
- spec `:161` 单点目标；实现 `PackageLoader.cpp:460` 只允许 `{id, version}`。写区间直接报错。
- 不判为"必须改设计"，因为 A68 的"下沿恒 0"用更少的机制达成了同样的效果。但如果将来要允许多个并发主版本（`1.x` 与 `2.x` 同时在线），单点目标会不够用——那时才需要区间。**当前建议不改。**

#### C9 [事实] 版本比较不是任意精度
- spec `:177` 原文「实现必须按任意精度非负整数比较，不得因超出机器整数范围而改变比较结果」。
- 实现用的是 `stdc::VersionNumber`，成员 `std::array<int, 4> m_numbers;`（`vcpkg/installed/x64-windows/include/stdcorelib/support/versionnumber.h:62`），逐段 32 位比较；分量超 `int` 时 `PackageLoader.cpp:446-450` 报 `"version is not representable"`。
- 触发概率极低，属明文偏差。

#### C10 [事实] 语言包端口在两仓各一份，已出现行为差异
- lite `scripts/vcpkg-ports/wolf-lang-packages/` 与 wolf `scripts/vcpkg-ports/wolf-lang-packages/`：`assets.cmake`、`vcpkg.json`、`config.cmake.in` **字节一致**（bundle 都是 `0.1.2.0`），但 `portfile.cmake` 不同。
- 差异里有一条是**行为差异**：wolf 版 `file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" ${_copyright})` 未加引号，lite 版加了引号。CMake 里未加引号的变量展开按分号切列表，`file(WRITE)` 会把各段**直接拼接**——主代理实测 `set(x "alpha;beta;gamma")` 得到 `alphabetagamma`（加了引号才是 `alpha;beta;gamma`）。
- 即 wolf 版会把版权文本里的分号**吞掉**。端口是"复制目录"式同步（portfile 头注释自认），这条差异证明该机制没有护栏。

#### C11 [事实] `compatVersion > version` 不被 lint
- spec `:183` 原文「`compatVersion`不得大于`version`，否则清单无效」；`check_declarations.check_desc`（`:453-473`）只校验格式，不比较两者大小。
- 加载器侧是否拒绝、错误是什么，未实测（**未证实**）。

#### C12 [事实] 抬下沿的制度从未被执行过一次
- 修订历史：rev1/rev2 语言包 `compatVersion == version`（每版必移），rev3 是**下移到** `.0`，rev4 不动。**从未出现过"version 涨、compatVersion 同步上抬"**。
- 也就是说「破坏公开表面就必须抬下沿」这条 spec 强制的制度，**没有先例、没有演练、没有基线**。

### P3 顺手清理

- **C13** 版本序列化不规范化：`stdc::VersionNumber::toString()` 省略尾部零分量（`1.0.1.0` → `"1.0.1"`），而 spec `:179` 要求「序列化规范化版本时必须输出补齐后的四个分量」。被 `synthrt/tests/auto/Core/test_SynthUnit.cpp:706` 的期望值锁住。
- **C14** 修订号数字在多处手写副本且已失真：`wolf/docs/linguist-distribution.md:475` 说「当前修订号为 4」，而 `convert-g2p-packages.py:157` 与 `linguist-decisions.md:1136` 仍说「revision 3」。建议只留一处，其余写指针。
- **C15** `check_desc` 对 `compatVersion == version` 只报 warning 且有豁免（`:634` 要求第 4 段非 0），A68 只把"依赖方写下沿"升级成了 error。
- **C16** `four_part()`（`:148-150`）用 `(version.split(".") + ["0","0","0"])[:3]` 补齐/截断，四位源版本会被静默截断。
- **C17** 目录枚举错误未检查：`PackageLoader.cpp:613-619` 的 `while (!error && ...)` 因 `increment` 出错退出后不再检查 `error`，对部分列表继续排序。

## 5. 责任链：谁保证老声库不炸

| 环节 | 机制 | 位置 | 今天是否生效 |
|---|---|---|---|
| 1 打包修订递增 | 脚本注释 + 人 | `convert-g2p-packages.py:130-145` | 靠人 |
| 2 下沿恒取 0 | **脚本自动** | `convert-g2p-packages.py:153-178` | ✅ 唯一可靠的自动化 |
| 3 breaking 时手工抬下沿 | 人 + 三处文档 | `A54:1144`、`A68:1476`、`distribution:507` | 无脚本、无基线、从未演练 |
| 4 依赖方写下沿 | lint（error）+ 转换器 | `check-declarations.py:640-653`、`convert-voicebank.py:564-573` | ✅ 数据合规（实测 0 error），但第三方声库逃逸（C6） |
| 5 区间判定 | 加载器 | `PackageLoader.cpp:661-663` | ✅ 与 spec 一致（实测 6 探针） |
| 6 旧夹具 + 新包回归 | **无人** | `make-voicebank-fixture.py:169,227` 反向实现 | ❌ C4 |
| 7 CI 巡检 | CI | `ci.yml:80,92` | ❌ C5 |
| 8 发布物下沿审计 | **无人** | `make-lang-release.py:230` 照抄、`:233` `"breaking": False` 恒 false | ❌ |
| 9 引擎版本 ↔ 包版本 | `pinyin_engine_version()` | `convert-g2p-packages.py:350-365` | ❌ C1 |

**一句话**：唯一自动的是"下沿恒取 0"；其余全靠人，而"人"这一环已经被 A54 那次实证证明会漏。

## 6. 探针与工具自证记录

### 6.1 我自己的探针 bug（必须记录，否则读数不可信）
第一版探针用 `[regex]::Replace($raw, '("version"\s*:\s*)"[^"]+"', $repl, 1)` 打补丁。**`1` 被 PowerShell 当成 `RegexOptions.IgnoreCase`**，于是该调用替换了**全部**匹配——包括 `dependencies[].version`。结果 PROBE C/D 变成"`wolf/lang-cmn` 要求 `wolf/g2p-pinyin 1.0.1.5`（低于其下沿 1.0.2.0）"，失败原因与我要测的变量无关。

修正：改成锚定行首四空格缩进的 `(?m)^(    "version"\s*:\s*)"[^"]+"`，并在每轮打补丁后打印 `version / compatVersion / dep(g2p-pinyin)` 三元组确认依赖项未被污染。第 2 节的读数**全部来自修正后的版本**。

### 6.2 控制组纪律
每个探针改动后都先跑一次 CONTROl（现装语言包）确认基线为绿，再跑改动组；原始声库与原始语言包全程未修改（已核对 `zhibin/desc.json` 仍是 `1.0.1.0`）。

### 6.3 环境陷阱
插件加载器不接受 `PATH` 查找（`LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`），必须把 `vcpkg/installed/x64-windows/bin/*.dll` 复制到 harness 同目录，否则 `mode=Load` 一律报"找不到指定的模块"。这一点上一轮已经踩过并记录。

## 7. 建议的处置顺序

1. **C1 先修**（唯一"下一次必然踩到"的问题）：`pinyin_engine_version()` 改为解析清单实际选中的端口名/版本，并在发布前断言"引擎版本 == 包版本前三位"。
2. **C5 + C4 一起修**，它们是同一件事的两半：
   - 提交一份**固定版本**的声库夹具快照（写死 `1.0.1.0` 等下沿），与动态生成的配对使用；
   - CI 增加一条 `convert → make-voicebank-fixture → 用固定夹具 ctest` 的作业；
   - lite 侧把 `LITE_AUDIT_VOICEBANK`/`LITE_AUDIT_LANGUAGE_PACKAGES` 接进至少一条 CI 作业，让 `tst_voicebank_audit` 不再默认 skip。
3. **C2 + C3 一起修**（诊断）：依赖失败消息补"要求版本 + 已装候选的 `[下沿, 版本]` 清单"，catalog 保留 per-directory 拒绝原因。这是所有兼容性失效场景的共同出口，收益最高、改动最小。
4. **C6**：把「升级语言包前，用 `check-declarations.py` 同时传入已安装声库与语言包」写成宿主可执行的命令（wolf `docs/linguist-distribution.md` §4.3 现在只写了义务，没写命令）。
5. **C7**：如果要让"产出内容"进兼容面，最轻的做法是不动 spec，而是把 `PACKAGING_REVISION` 的判据从"声明形状是否变"扩展到"产出是否变"，并把 rev4 这类变更在 `linguist-decisions.md` 补一条决策记录（现在只有代码注释）。
6. C8-C17 随下一次接口整理。

---

_本报告为只读审计产物，未修改任何仓库文件，未提交 git。临时实测产物在 `.tmp/spec24-audit/`（已被 `.git/info/exclude` 排除）。_
