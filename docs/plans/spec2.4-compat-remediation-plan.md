# 兼容性收口实施方案（spec 2.4 版本纪律 / cpp-pinyin2 单线化）

> 来源：两轮 spec 2.4 审计（[通用](spec2.4-audit-report.md) / [兼容性](spec2.4-compat-audit-report.md)）+ 本轮选择式提问的四个裁决。
> 状态：**已全部实施并验证。**实际落地过程与结果见文末「6. 实施结果」。

## 0. 决策台账（本轮你已拍板）

| # | 决策 | 你的选择 | 来源 |
|---|---|---|---|
| D1 | 引擎版本源怎么修 | **只保留 cpp-pinyin2**（把 1.x 从版本源与构建里摘掉） | 本轮提问 #1（自定义答复） |
| D2 | 「老声库 + 新语言包」回归怎么补 | **固定版本声库夹具快照 + CI 加 convert→fixture→ctest 作业** | 本轮提问 #2 |
| D3 | 要不要给 `dependencies` 加区间语法 | **不加，维持单点目标** | 本轮提问 #3 |
| D4 | 「产出内容变化」算不算兼容面 | **不动 spec**，把 `PACKAGING_REVISION` 判据从「声明形状变」扩到「产出变」，并补决策记录 | 本轮提问 #4 |

## 1. 需人工确认的关键点（**先看这一节**）

### K1【最重要】D1 会让下一次语言包发布变成破坏性发布——要不要接受

只读摸底算出的依赖链（全部实测自安装树）：

```
声库 zhibin/qixuan/yousa  →  wolf/lang-cmn  目标 1.0.1.0   （下沿 1.0.1.0）
wolf/lang-cmn 1.0.1.4     →  wolf/g2p-pinyin 目标 1.0.2.0
wolf/lang-yue 1.0.1.4     →  wolf/g2p-pinyin 目标 1.0.2.0
wolf/g2p-pinyin 1.0.2.4      （下沿 1.0.2.0，词典与 cpp-pinyin 1.0.2 字节相同）
```

`pinyin_engine_version()` 把端口版本的前三段当包版本（`convert-g2p-packages.py:148-150, 350-365`）。改成读 cpp-pinyin2 之后，包版本会变成 **`2.0.0.4`**。于是有两条路，爆炸半径差一个数量级：

- **A（下沿不动，推荐）**：`wolf/g2p-pinyin 2.0.0.4` + `compatVersion` 仍取 `1.0.2.0`。
  - 后果：`wolf/lang-cmn`/`yue` 的依赖目标 `1.0.2.0` 仍满足；**所有已发布声库继续可用**，无需重发。
  - 前提：2.x 引擎在 `(cmn,pinyin)` / `(yue,jyutping)` 上**契约面不变**（模块 id、`interface`/`level`、`exports` 语义、`options` 写法）。spec `:198` 明文允许在兼容区间内改「模型与算法」，A68 的论证（`convert-g2p-packages.py:162-167`）也正是这个方向。
  - 风险：词典内容确实变了（2.x 的 `word.txt` 与 1.x 字节不同，且多 `dialect.json`/`format.txt`），所以**逐词读音可能变化**——这正是 D4 里「产出变化」那一类，按 D4 的裁决它是允许的。
- **B（抬下沿）**：`compatVersion` 抬到 `2.0.0.0`。
  - 后果：`wolf/lang-cmn` 与 `yue` 立刻加载失败（目标 `1.0.2.0` < 下沿 `2.0.0.0`）⇒ 必须重发这两个包并把它们的下沿抬到 `2.0.0.0` ⇒ **所有已发布声库失效**（它们声明的是 `1.0.1.0`），要求声库作者重发或用户升级。

**需要你定：走 A 还是 B。** 我倾向 A —— 契约面没变，抬下沿按 spec 是"不实声明"，且 B 要牺牲全部存量声库。但 A 的前提（2.x 契约面不变）我**没有验证过**，需要确认 cpp-pinyin2 的 `algo-pinyin` 变体是否仍是同一个 `(language, scheme)` 与同一份 `exports`。

> 附带提醒：**如果你想要的只是「编辑器二进制里不再躺 cpp-pinyin.dll」**，那属于另一个更窄的范围（见 P1 阶段），完全不涉及语言包重发。请明确要哪个范围。

### K2 D2 的夹具快照放哪、由谁维护

固定版本夹具是「写死 `1.0.1.0` 等下沿、永不随包重生成」的一份声库声明。两个落点：

- **A（推荐）**：放 wolf 仓 `scripts/fixtures/voicebank-pinned/`，只含 json/txt（不含 onnx），体积小，由 CI 的 `make-test-fixtures.py` 同目录机制装载。
- **B**：放 lite 仓 `src/tests/TestModelResources/`，与已有 `tst_voicebank_audit` 共用装载代码。

差别：放 wolf 能覆盖语言包侧的发布纪律（这正是 A68 的语境）；放 lite 能覆盖宿主侧。**我建议 A**，因为失效点在下沿，属于 wolf 的发布物。

### K3 是否允许我为本次方案跑一次转换器 / 构建

方案里 P2、P3 阶段的验证需要跑 `convert-g2p-packages.py` 与至少一次 `cmake` 构建。按全局协议我不会私自执行。请一并给个口径：只出代码不构建，还是允许我在本地验证。

## 2. 实施阶段（每阶段独立提交、可回滚）

### P1｜lite 侧摘掉 cpp-pinyin 1.x（窄范围、无语言包影响）

只读摸底确认：lite 自己的源码**没有任何** 1.x 的 `#include` 或符号引用，`cmake/LiteBuildApi.cmake:247-249` 的注释也自认「the editor itself links neither」。1.x 只被三处拖着：

| 位置 | 现状 | 改法 |
|---|---|---|
| `scripts/vcpkg-manifest/vcpkg.json:23` | 顶层依赖 `"cpp-pinyin"` | 删除该项 |
| `src/app/CMakeLists.txt:24` | `find_package(cpp-pinyin CONFIG REQUIRED)` | 删除该行 |
| `cmake/LiteBuildApi.cmake:242-246` | 复制 `cpp-pinyin::cpp-pinyin` 到部署目录 | 删除该 `qm_add_copy_command`，并把 `:247-249` 的注释改成只描述 2.x |

不动 `scripts/vcpkg/ports/cpp-pinyin/`（共享 overlay 子模块，且被该子模块里的 `synthrt` 端口依赖；lite 自己的 `scripts/vcpkg-ports/synthrt/` 会遮蔽它，实测其依赖表里没有 cpp-pinyin）。

验证：`vcpkg install` 后 `bin/` 下不再出现 `cpp-pinyin.dll`，同时 `cpp-pinyin2.dll` 仍在；GUI 冒烟确认 cmn/yue 演唱正常（需你允许构建与运行）。

### P2｜wolf 侧把版本源接到 cpp-pinyin2 + 发布前断言（D1 + A/B 由 K1 定）

| 文件 | 改法 |
|---|---|
| `scripts/convert-g2p-packages.py:350-365` `pinyin_engine_version()` | 端口路径从 `scripts/vcpkg/ports/cpp-pinyin` 改为 `scripts/vcpkg-ports/cpp-pinyin2`（wolf 自有，实测 `version: "2.0.0"`、`port-version: 1`），并把函数注释里「跟随端口版本」的说明改成「跟随 cpp-pinyin2」 |
| 同上 `:410-424` 的 desc 生成 | 若 K1 选 A：`compatVersion` 显式写 `1.0.2.0` 而不是 `compat_floor(engine_version)`（否则会算出 `2.0.0.0`，即 B） |
| `scripts/make-lang-release.py` | 新增断言：`wolf/g2p-pinyin` 的 `version` 前三段必须等于所选引擎端口的 `version`，不等则拒绝打包 |
| `docs/linguist-distribution.md:224, 341, 845` | 把 `share/cpp-pinyin/dict` 改为 `share/cpp-pinyin2/dict`（`:341` 是给操作者的命令行，不改正会直接找不到路径） |
| `docs/linguist-distribution.md:794, 809` | `libcpp-pinyin` → `libcpp-pinyin2`（插件依赖表） |

验证：`check-declarations.py` 对新包 0 error；`check-release-assets.py` 通过；若允许跑转换器，比对新旧包的 `desc.json` 三元组（identity / floor / dependency target）并确认符合 K1 的选择。

### P3｜固定版本夹具 + CI 回归作业（D2）

| 文件 | 改法 |
|---|---|
| `scripts/fixtures/voicebank-pinned/`（新建） | 提交一份**写死下沿**的声库声明快照：`dependencies[].version` 固定为 `1.0.1.0` / `1.0.0.0` / `0.0.1.0`，**不**由 `make-voicebank-fixture.py` 生成，永不随包重生成 |
| `scripts/make-voicebank-fixture.py:166-169, 227` | 保留现有的「从包读 compatVersion」用于动态夹具，但在文件头注明它与固定快照的分工，避免下次又被当成唯一夹具 |
| `.github/workflows/ci.yml` | 新增一个 job：`convert-g2p-packages.py`（用缓存）→ 动态夹具 → **用固定快照跑 ctest**；Linux 与 Windows 各一条（Windows 当前没有任何 lang-packages feature，见 `ci.yml:32`） |
| `ci.yml:92` | lint 从「只跑 `wolf-lang-zxx`」扩到「转换输出 + 固定快照一起传」，让 `check_dependencies` 的依赖分支真正执行 |

验证（这是本阶段的核心）：**故意把 `wolf/lang-cmn` 的下沿抬到 `1.0.1.3`，CI 必须转红**；还原后必须转绿。这是 A68 记录的那次失效的可复现版本。

### P4｜产出变化的判定与记录（D4）

| 文件 | 改法 |
|---|---|
| `scripts/convert-g2p-packages.py:129-145` | `PACKAGING_REVISION` 的注释判据从「声明形状是否变」扩成「声明形状 **或产出** 是否变」，并写明判定方法（对同一组固定输入跑旧/新转换输出、比对音素序列） |
| `scripts/convert-g2p-packages.py` | 新增一个可独立调用的产出比对子命令（输入：两份转换输出目录；输出：是否有差异），CI 里 `--verify-output-unchanged` 使用 |
| `docs/linguist-decisions.md` | 追加一条决策记录（A70/A71，编号以仓内现有最大值为准）：把 rev4 的片假名别名、以及本次 cpp-pinyin2 换引擎，按新判据归档，写明「产出变化 → 抬 `PACKAGING_REVISION`，不抬 `compatVersion`」 |
| `docs/linguist-distribution.md:511` | 该条现在只说「词典增补属非破坏性」，补一句指向新判据 |

### P5｜诊断补强（本轮审计的 P1 C2/C3，与 D1–D4 独立）

| 文件 | 改法 |
|---|---|
| `synthrt/lib/Core/PackageLoader.cpp:692-693` | 失败消息补 `dependency.version` 与所有已见候选的 `[compatVersion, version]` |
| `synthrt/lib/Core/PackageLoader.cpp:629-632` | catalog 保留 per-directory 拒绝原因，失败时按 id 汇总 |
| `synthrt/tests/auto/Core/test_SynthUnit.cpp` | 补 spec 明文强制但零覆盖的四类用例：下沿拒绝、目标高于已装版本、多版本共存、遮蔽 |

这一步动的是 synthrt，按项目约定属于**源头修复**，不在 lite 做补偿。

## 3. 明确不做的事

- **不给 `dependencies` 加区间语法**（D3）。若将来要同时支持 `1.x` 与 `2.x` 在线，再单独立项。
- **不改共享 overlay 子模块** `scripts/vcpkg/`（里面的 `cpp-pinyin` 端口仍被该子模块的 `synthrt` 端口引用，动它属于跨仓行为）。
- **不改 spec 2.4 正文**（D4）。
- **不重新生成三套测试声库**，除非 K1 选 B。

## 4. 风险与回滚

| 风险 | 处置 |
|---|---|
| P1 删掉 1.x 后某条插件加载路径仍然需要 `cpp-pinyin.dll` | P1 单独成一个提交，`git revert` 即可回滚；验收时检查 `bin/` 与插件加载日志 |
| P2 换引擎后 cmn/yue 读音变化，老声库听起来不同 | 属 D4 允许范围；用 P4 的产出比对给出差异量级，再决定是否随发布公告 |
| P3 的 CI 作业依赖网络下载语言包归档 | 复用 `assets.cmake` 的 SHA512 校验与 vcpkg 缓存；离线环境走 `WOLF_LANG_PACKAGES_SOURCE` |
| P5 改的是 synthrt 错误文本 | 只增信息不改码，不需要 ABI 变更 |

## 5. 建议的落地顺序

**P1 → P5 → P2（等 K1）→ P3 → P4。**

P1 与 P5 互不依赖、可并行、风险最低，先做能立刻消除两处债；P2 卡在 K1（下沿要不要抬）；P3 的夹具与 P2 的产物无关，可提前准备；P4 依赖 P3 的产出比对工具。

---

## 6. 实施结果

**K1 的裁决改变了收尾方式**：先按「彻底换 v2、更新全部声库与语言包」实施，随后用户进一步决定把 wolf
的 15 个包一次性重置为 `1.0.0.0`（`compatVersion` 同值），并撤回全部旧 release 重发。

| 阶段 | 结果 |
| :-- | :-- |
| P1 摘掉 cpp-pinyin 1.x | 已改 manifest、`find_package`、DLL 拷贝三处，未做构建验证 |
| P2 引擎接到 cpp-pinyin2 | 已完成。版本源改读 `scripts/vcpkg-ports/cpp-pinyin2`，`PACKAGING_REVISION` 到 5，后来又按重置决定改为常量 `RELEASE_BASELINE = 1.0.0.0`，随之删除了「包版本跟随引擎」的发布断言 |
| P3 固定版本夹具 | 已完成。`scripts/fixtures/voicebank-pinned/` 加 `scripts/test_pinned_voicebank_fixture.py`，走已有 unittest discover 进 CI，实测抬下沿会转红 |
| P4 产出变化判据 | 已完成。判据写进 `convert-g2p-packages.py`，决策记录见 wolf 的 A82 / A83 |
| P5 依赖诊断 | 已完成。失败消息带要求版本与已装候选清单，生产 TU 编译通过，测试 TU 因本机缺 Boost 未编译 |
| 发布 | 已完成。15 个包重置为 `1.0.0.0`，`lang-v1.0.0.0` 已上传，`check-release-assets.py` 用本次 manifest 跑 `35 passed / 0 failed` |

**实测结论**：换引擎后语言包下沿不动，三套声库原本仍可加载。重置到 `1.0.0.0` 后三套全部失效（实测报
`no installed Package satisfies dependency wolf/lang-cmn`），已把它们的 `dependencies[].version` 同步改为
`1.0.0.0`，改后三套全部 `Load` 通过。
