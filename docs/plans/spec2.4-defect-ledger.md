# spec 2.4 四仓缺陷收敛台账

> 快照：wolf `3fb6faf`、synthrt `fd4d916`、ds-editor-lite `ec119377e`、otter `6db876b`（均 `spec2.4-uptake`）。
> 实测数据来自本轮：vcpkg 重装到 `wolf-lang-packages@1.0.0.0`、三套 2.4 声库 `Load` 实测、wolf lint 全量跑。
> 前序报告：[通用审计](spec2.4-audit-report.md)、[兼容性审计](spec2.4-compat-audit-report.md)、[收口方案](spec2.4-compat-remediation-plan.md)。

## 0. 本轮基线（已实测）

| 项 | 结果 |
| :-- | :-- |
| vcpkg 重装 | `wolf-lang-packages[...]:x64-windows@1.0.0.0#1` 成功，1.1 min，exit 0。这同时证明 `lang-v1.0.0.0` 的远端上传可被端口取到并校验 SHA512 |
| 安装树版本 | `g2p-multi` / `g2p-pinyin` / `lang-cmn` 等全部 `1.0.0.0` |
| cpp-pinyin 1.x 是否绝迹 | `bin/` 只剩 `cpp-pinyin2.dll`，`share/cpp-pinyin` 不存在。harness 的 DLL 依赖从 50 个降到 49 个 |
| 三套声库 `Load` | qixuan / yousa / zhibin **全部通过**，`reservedMissing` 全空，零失败 |
| wolf lint（3 声库 + 15 包） | `checked 58 declarations in 18 packages, 0 error(s), 16 warning(s)` |

## 1. 已修复并验证（本轮及前序）

| # | 问题 | 验证方式 |
| :-- | :-- | :-- |
| F1 | 转换器引擎版本源脱钩（读冻结的 cpp-pinyin 1.0.2） | 改为读 `scripts/vcpkg-ports/cpp-pinyin2`；重跑转换器产出 14 包，词典与 2.x 树逐字节一致 |
| F2 | 依赖失败消息只有 id | synthrt `PackageLoader.cpp` 现带要求版本与已装候选 `[下沿, 版本]`；生产 TU 编译通过 |
| F3 | A68 要求的「旧夹具 + 新包」回归不可能存在 | 新增 `scripts/fixtures/voicebank-pinned/` + `test_pinned_voicebank_fixture.py`；实测把下沿抬到 `1.0.2.0` 该测试转红 |
| F4 | 产出内容变化没有任何判据 | `PACKAGING_REVISION` 判据扩到「产出变」；决策记录 A83 |
| F5 | 全部包版本重置 | 15 包 `1.0.0.0`，`lang-v1.0.0.0` 已发布；`check-release-assets.py` 用本次 manifest `35 passed / 0 failed` |
| F6 | lite 仍安装 cpp-pinyin 1.x | manifest / `find_package` / DLL 拷贝三处移除；vcpkg 重装后 `bin/` 无 1.x |
| F7 | 三套声库依赖目标过期 | 改为 `1.0.0.0`；对发行版包 `Load` 全绿 |

## 2. 未修复缺陷

### 2.1 wolf

| # | 级别 | 缺陷 | 证据 | 收敛动作 |
| :-- | :-- | :-- | :-- | :-- |
| W1 | 部分修 | lint **没有 dsinfer 五个 inference 接口的 schema**，16 条告警里 15 条是「无法校验」 | 第 2 轮先补了最高信号的一条不变式：`check_acoustic_controls()` 校验 `exports.varianceControls`/`transitionControls ⊆ configuration.parameters`。实测抓到 zhibin 的 `energy`（告警 16→17，0 error） | 剩余：Acoustic/Duration/Pitch/Variance/Vocoder 的 exports 与 import-options JSON Schema（10 个文件）待补 |
| W2 | ✅ 已修 | `um` 漂移：`qixuan/jpn-romaji`、`yousa/cmn-pinyin`、`yousa/jpn-romaji` 三处 linguist 声明可产出 `um`，而对应模型表无此音素。加载期零校验 | 第 3 轮加 `check_phoneme_coverage()`，第 5 轮收紧为**按语言匹配**（声明语言的 `<lang>/<音素>` 或全表裸键）。严格规则实测**恰好复现首轮人工审计的三处**，告警 20 / 0 error，对 15 个 wolf 包无误报 | wolf `ff3ba60`。宿主侧拒绝式校验（S2）仍待定，因为把未知音素升为加载失败会打断现有声库 |
| W3 | P2 | `make-voicebank-fixture.py:166-169` 仍从包里读 `compatVersion`，与新增的固定版本夹具并存但未统一，A68 的反向实现仍在 | 源码现状 | 在该脚本头注明分工，或让其发现固定快照存在时改为对齐而非自生成 |
| W4 | ✅ 已修 | CI 仍只 lint 一个无依赖包（`ci.yml:92` → `wolf-lang-zxx`），`check_dependencies` 的依赖分支在 CI 从未执行 | 第 6 轮：lint 步骤加上 `scripts/fixtures/voicebank-pinned`，并新增一个「Linux 专用」步骤把固定快照与 vcpkg 装出的发行包同批传入，依赖下沿规则才真正被执行。两步均在本机复现：`3 declarations / 0 error`、`25 declarations / 0 error` | wolf `0e1e713` |
| W5 | ✅ 已修 | 语言包端口在 wolf 与 lite 各一份，`portfile.cmake` 已出现行为差异（wolf 的 `file(WRITE ... ${_copyright})` 未加引号，会把版权文本里的分号吞掉） | 第 6 轮给 wolf 那份补上引号并写明原因；跨仓哈希比对仍未做 | wolf `0e1e713`。**注意**：端口内容变了，`vcpkg install` 会重建该端口，需下次安装验证 |
| W6 | ✅ 已修 | `check-release-assets.py` 的 `DEFAULT_MANIFEST` 指向旧路径，是「2 个既有失败」误判的根源 | 改为 `newest_manifest()` 取最新 `build/lang-archives-*/manifest.json`；不带 `--manifest` 跑实测 `35 passed / 0 failed / 0 skipped` | wolf `7e5df27` |
| W7 | ✅ 已修 | `compatVersion > version` 不被 lint（spec `:183` 称清单无效） | `check_desc` 补大小比较；造 `compatVersion 9.9.9.9` + `version 1.0.0.0` 的负例实测报 `compatVersion 9.9.9.9 is greater than version 1.0.0.0` | wolf `7e5df27` |

### 2.2 synthrt

| # | 级别 | 缺陷 | 证据 | 收敛动作 |
| :-- | :-- | :-- | :-- | :-- |
| S1 | **P1** | 候选被拒原因仍被静默吞掉：`$version` 不支持、`runtimeLevel` 过高、`compatVersion > version`、类别未注册、desc 损坏全部退化成「不存在的候选」 | `PackageLoader.cpp:629-632` 的 `if (!packageResult) { continue; }` | catalog 保留 per-directory 拒绝原因，失败时按 id 汇总进消息 |
| S2 | **P1** | `validateReservedPhonemes` 只覆盖 `reservedPhonemes`，从不看 linguist `exports.phonemes`，与 W2 同源 | `DiffSingerProvider.cpp:147-183` | 与 W2 一并做，源头在此 |
| S3 | P2 | 目录枚举错误不检查：`while (!error && ...)` 因 `increment` 出错退出后不再看 `error`，对部分列表继续排序 | `PackageLoader.cpp:613-619` | 退出循环后补 `if (error)` 分支 |
| S4 | 上游，不在四仓 | 版本比较非任意精度，spec `:177` 要求任意精度 | `stdc::VersionNumber` 是 `std::array<int,4>`，该类定义在**独立的 `stdcorelib` 仓**（`include/stdcorelib/support/versionnumber.h:62`），synthrt 只是通过 vcpkg 依赖它（`PackageLoader.cpp:446` 调 `fromString`） | 需在 `stdcorelib` 立项。第 24 轮归正：此前误记为 synthrt 项 |
| S5 | 上游，不在四仓 | `toString()` 省略尾部零分量，非四段规范化（spec `:179`） | 同 S4，`versionnumber.cpp` 在 `stdcorelib` 仓；行为被 synthrt 的 `test_SynthUnit.cpp:706` 断言锁定 | 同上。改它会影响 synthrt 的既有断言，需要跨仓协调 |
| S6 | P3 | 区间语义四类用例仍缺：下沿拒绝、目标高于已装、多版本共存、遮蔽 | `test_SynthUnit.cpp` 仅有失败消息的新用例 | 补四类，锁住 spec `:186`/`:402`/`:404` |

### 2.3 ds-editor-lite

| # | 级别 | 缺陷 | 证据 | 收敛动作 |
| :-- | :-- | :-- | :-- | :-- |
| L1 | **P1** | 声库声明的 `defaultLyric` 读而不用 | `VoicebankCatalog.cpp:459-462` 读入，`DefaultLyricRules.h:15-18` 明说 resolution deliberately does not consume it | 在 `resolve()` 插入「查当前声库声明」一级，需先定优先级 |
| L2 | **P1** | 保留音素语义三方不一致：lite 用**并集**（`withForcedReservedPhonemes`），wolf `SingerContrib` 用**覆盖**，spec 说缺省为空 | 三处源码 | 定语义后统一，属设计决策 |
| L3 | P2 | `LanguageBridge::setReservedMarkers` 无生产调用方，只有测试调用 | 全仓 grep | 接入或删除 |
| L8 | ✅ 已修 | 注副本夹具改写错了 `"id"`：`desc.json` 键为字母序，首个 `"id"` 是**依赖项**的 id（6 空格缩进），包自己的 id 在 2 空格缩进处。旧代码用裸 `"id": "` 命中依赖，导致模块的 `wolf/lang-cmn:inference/g2p` 变成未声明依赖，`validateReservedPhonemes` 根本没执行 | 第 10 轮把 `doctor()` 的锚点改为 `"\n  \"id\": \""`（换行 + 根级两空格）。重建后：6 个失败降到 **1 个**，拒绝原因变为 `DiffSinger singer declares reserved phonemes that are missing from its models (4 errors found)` 并逐条点名 duration/pitch/variance/acoustic | lite 告警：**这同时证实保留音素校验在产品路径上确实有效**，round-1 的结论成立 |

**遗留**：修好后仍剩 1 个失败 —— `FAIL the singer declaration lists its speakers`，输出里 `declares` 与 `model has` 两列都是空的。

第 11–12 轮查证后的定性结论：**是测试的读取器与这套声库的字段形状不匹配，叠加断言强度不足**，不是产品缺陷。

- `tst_voicebank_audit.cpp:507-510`：`declaration` 读 `characters/<contributionId>/config.json`，`modelSpeakers` 读 `inferences/acoustic/config.json` 的 `exports.speakers`。
- 但实测这两个文件里**都有数据**：`characters/yousa/config.json` 有 `speakers`，`exports.speakers` 是 5 个**纯字符串**（`["Yousa_Bright", ...]`），`configuration.speakers` 是 5 项映射。
- 两个读取器却都返回空 → 它们找的形状与这套声库不一致（推测期望对象数组或不同键名，与 `convert-voicebank.py` 产出的布局一致，而不是 OpenUtau 这套）。
- 更值得修的是：`expect(offered == declared, ...)`（`:521`）在两个空集上**平凡通过**，所以「speakers 全丢了」这类回归对这条断言是免疫的。

修法建议：让两个读取器在遇到 `speakers` 键但解析不出条目时**报错而非返回空**，并把空集从「通过」改成「跳过并明示」。这属于测试强度问题，与产品无关。

### 2.5 产品路径实测基线（第 29 轮）

`TestModelResources.exe --editor <DsEditorLite.exe>` 全量跑：

```
Totals: 18 passed, 2 failed, 2 skipped   (71.8 秒)
```

**16 条 `voicebankInferenceAndWaveExport` 端到端用例全部通过**（普通话/英语演唱、修复模型、运行期模型故障恢复、DirectML 设备），CUDA 一条按预期 skip（本机构建无 CUDA）。这是「用 2.4 声库实测」最完整的一环：从加载声库到推理到导出波形。

**调用契约**：该二进制用自定义 `main`（`tst_model_resources.cpp:1189-1198`）解析 `--editor <路径>`，不传则 16 条端到端用例全部报 `QFileInfo::exists(editorPath) returned FALSE`。这不是缺陷，是调用方式（第 28 轮曾误记为缺陷，已撤销）。

| L10 | **P1 元凶已定位** | **测试之间存在状态污染**：`defaultLyricDeclaration` 单独跑绿，全量跑红 | 第 36 轮用组合测试逐条试出元凶：单独 `defaultLyricDeclaration` ✅ 3 passed；`runtimePluginFailurePreservesEditing` + 它 ✅ 5 passed；**`publishedVoicebankAudit` + 它 ❌ 两条都失败**。5 秒即可复现，不必跑全量：<br>`TestModelResources.exe --editor <DsEditorLite.exe> publishedVoicebankAudit defaultLyricDeclaration`<br>已排除：插件根被前置用例清空（组合测试推翻）、`mirror()` 硬链接污染真实声库（三套声库全 JSON 扫描无 `zz-not-a-phoneme`/`doctored-` 痕迹，原始数据干净）、磁盘残留目录（第 35 轮单跑时残留目录已存在仍绿 → **污染是进程内状态，不是磁盘**） | 在 `publishedVoicebankAudit` 内部逐段做最小复现，找出留下进程级状态的那一段；修复方向是清理该段的静态缓存而非临时文件 |
| L7 | 【部分】机理已定 | 语言包 staging 不覆盖陈旧文件，`out/bin/wolf/packages` 出现混合版本 | 第 8 轮：删除目标后 `ninja: no work to do`（带戳复制，删输出不重跑）；`qm_add_copy_command` 只支持 `VERBOSE/SKIP_BUILD/SKIP_INSTALL`，**无 FORCE**。重新 configure 后 15 个包全部恢复为 `1.0.0.0`，产品路径随即从「三套全灭」推进到最后一段 | 永久修法：换成能先清空目标的自定义命令，或把 bundle 版本编进目标路径并在 configure 期清理旧目录 |
| L4 | P2 | `tst_voicebank_audit` 默认 `QSKIP` | 第 7 轮补充：它不仅默认 skip，而且 `LITE_AUDIT_VOICEBANK`/`LITE_AUDIT_LANGUAGE_PACKAGES` 是 **CMake 缓存变量**（configure 时烧进编译期宏），**设环境变量完全无效**；默认值是 `.tmp/audit-singers/yousa-2.4@1.65.1.0` 这个转换产物 | 接 CI 时必须用 `-D` 传缓存变量，不能靠 env |
| L5 | 检测已具备 | zhibin `energy`：`varianceControls` 含 `energy` 而 `configuration.parameters` 不含，`AcousticTask` 不写 energy tensor，但 UI 打开 Energy 轨 | 正文仍成立；第 2 轮起 wolf lint 能自动报出该矛盾 | 数据侧要么从 `varianceControls` 去掉 `energy`，要么在 `configuration.parameters` 补上——取决于模型是否真消费 energy tensor，需实测 ONNX 输入 |
| L6 | P3 | `desc.json` 根级 `name` 是死数据，`PackageInfo` 也没有 name 字段 | `PackageLoader.cpp:1140` 用 id 当 name | 要么接上，要么在 spec 里删字段 |

### 2.4 otter

| # | 级别 | 缺陷 | 证据 | 收敛动作 |
| :-- | :-- | :-- | :-- | :-- |
| O1 | 已查清主体 | otter 在 spec 2.4 里的位置。派出的只读审计子代理连续两轮无产出已中断，改由主代理侦察 | 第 3–4 轮事实：otter 在 `packages/` 下确实有 **4 个真实 2.4 包**——`otter/game`、`otter/hfa`、`otter/rmvpe`、`otter/tifa`，各含 `desc.json`（`$version 1.0`、`version 0.1.0.0`、`compatVersion 0.1.0.0`、`runtimeLevel 1`、贡献到标准 `inference` 类别）+ `inferences/*/inference.json`。模块接口是**自有命名空间** `org.openvpi.otter.inference.{Align,F,Note}`。`desc.json` 无 `dependencies`，`src/`+`include/` 全量 grep `reservedPhonemes`/`exports.phonemes`/`compatVersion`/`dependencies` 零命中 | 待办：`docs/packages.md`、`otter-design.md`（6 处提到 spec 2.4）与 4 个包的 `exports` 逐字段对照规范；otter 包 floor == version 是单点区间，若将来重打包会踩 A68 那个坑 |
| O2 | P3 | lite 的 otter 端口副本与 otter 仓的端口是否漂移未核 | — | 与 W5 同一类问题，一起做跨仓比对 |

## 3. 建议的收敛顺序

1. **W1 + L5**：给 lint 补 dsinfer 五个接口的 schema。这是把「只能靠人工读 JSON 发现」的一整类缺陷变成可自动检查，投入产出最高，且能立刻验证 zhibin 的 energy 矛盾会被抓到。
2. **W2 + S2**：把 `exports.phonemes` 纳入校验，源头在 synthrt 的 provider，wolf 侧补静态交叉。`um` 漂移是现成实例。
3. **S1**：候选被拒原因进诊断。这是所有兼容性失效场景的共同出口。
4. **L1 + L2**：两个都需要先定设计（默认歌词优先级、保留音素语义），走选项式确认后实施。
5. **W4 + L4**：把已有门禁接进 CI，这一步只是接线，不改逻辑。
6. **W3 / W5 / W6 / W7 / S3 / S4 / S5 / S6 / L3 / L6 / O2**：逐条清理。
7. **O1**：otter 单独一轮审计。

## 4. 本轮未做

- lite 的 **CMake 构建**未跑（只跑到 vcpkg install 这一层）。`cmake --preset` + 编译才能真正确认 P1 的 `find_package` 移除无副作用。
- `tst_voicebank_audit` 未实跑（它需要完整构建）。
- otter 未审计。
- 所有缺陷**均未开始修复**，本轮只做分析与实测取证。

### L10 处置决定（第 42 轮，主代理建议 2+3）

**已定位**：毒源唯一，是 `publishedVoicebankAudit`。5 秒复现：
`TestModelResources.exe --editor <DsEditorLite.exe> publishedVoicebankAudit defaultLyricDeclaration`
顺序颠倒后前者绿、后者红，污染单向。

**已排除的机制（均为实验推翻，不是推理）**：
1. 插件根被前置用例清空 —— `runtimePluginFailurePreservesEditing` + 目标 = 绿
2. `mirror()` 硬链接污染真实声库 —— 三套声库全 JSON 扫描无注入痕迹
3. 磁盘残留目录 —— 单跑时有残留仍绿，污染在进程内
4. 包路径经单例共享 —— `SynthrtBootstrap.cpp:69-77` 每次新建 unit 与私有路径
5. ONNX 运行时单次初始化 —— 重度 ONNX 前置（14 条端到端、56 秒）后仍绿
6. 失败 `openPackage` 残留 —— `SynthUnit.cpp:202-210` 包表是 `_impl` 私有成员，不跨 unit

**剩余方向**：进程级注册表（`wolf::linkLinguistCategory()` 与 dsinfer 插件注册表，见 `SynthrtBootstrap.cpp:63-67` 注释「a unit reads the registry only once, at construction」）。

**处置**：L10 性质是测试顺序依赖，不影响用户与产品行为，已投入 12 轮、6 次假设全部推翻。**暂缓追问题机制，转回四仓内 7 项真实缺陷**。全量门禁前如需可信结果，把 `defaultLyricDeclaration` 置于 `publishedVoicebankAudit` 之前或独立进程运行。

## 3. 第 44–52 轮的修正与收口

### otter：当前无已确认缺陷（第 49–52 轮实测）

四个真实 2.4 包与其契约逐一核对，**全部合规**：

| 包 | interface | `languages` 形态 | 契约要求 | 判定 |
|---|---|---|---|---|
| `game` | Note | 字符串数组 | `note-1`：`items: string` | ✅ |
| `hfa` | Align | 对象数组 | `align-1`：`items: $ref language` | ✅ |
| `tifa` | Align | 对象数组 | `align-1`：`items: $ref language` | ✅ |
| `rmvpe` | F0 | 无此字段 | `f0-1`：无 | ✅ |

**撤销第 44 / 46 轮的误报**：当时把 `game`（Note）与 `hfa`/`tifa`（Align）当同类横向比较，判成「形状不一致 / `game` 违反 align schema」。实为分属两种契约，本就不该一致。教训：**先确认对象类别，再拿对应规则判**。

**O1b 降级为 P3**：`check-declarations.py` 用手写镜像表（`EXPORTS_KEYS`、`ALIGN_LANGUAGE_KEYS`、`LANGUAGE_NUMBERING`）校验 exports，**从不读取 `docs/schemas/*.json`**。第 52 轮逐契约比对，三份契约的键集合与 schema `properties` **当前完全一致，无漂移**。故不是当前缺陷，只是维护风险：将来改 schema 而忘改表，lint 仍报绿。`jsonschema` 4.26.0 已安装，可将镜像表换成 schema 驱动校验。

### lite L3 收窄（第 43 轮）

`setReservedMarkers` 只有测试调用，但**声库声明的保留音素主路径是通的**：`PackageManager.cpp:87` → `SingerInfo.h:50` → `Note.cpp:201` 实际在用。故不是「整条路径未接线」，仅是「宿主额外追加标记」这条 API 无生产用途，性质为**待定的冗余 API**，优先级下调。

### 四仓当前未修清单（均需用户定语义）

| 项 | 待定决策 |
|---|---|
| synthrt S2 | `exports.phonemes` 与模型表不匹配：拒绝加载（会打破现有 3 套声库）还是告警 |
| lite L1 | `defaultLyric` 优先级链顺序 |
| lite L2 | 保留音素三处语义以哪处为准 |
| lite L3 | `setReservedMarkers` 删除还是接上（倾向删除：语言会话已能从 singer 类别读到） |
| lite L6 | `name` 死数据删除还是接入 |
