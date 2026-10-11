# 迁移到 synthrt 的交接与声库打包（方案 V1）

> 状态：**已完成**（2026-10-07 落盘）。本文是该目标的方案与决策台账，按你的指示落在新建的 `docs/synthrt/`。
> 9 个阶段全部完成（见第 5 节），唯一未闭合项在 wolf 侧：韩语词典键与链条不匹配，见
> [korean-via-wolf-kor.md](korean-via-wolf-kor.md)。本文刻意留在本目录而不归档到 `docs/archive/`，
> 因为它与同目录的交接材料构成一套，互相引用都在本目录内。
> 本目录的最终产物是**给外部开发者的交接材料**：让一个本地没有任何声库的人，只照文档就能下载 wolf 的
> G2P 包给编辑器携带、自己打包 2.4 版声库并接上依赖。

## 0. 需人工确认的关键点（集中置顶）

| 编号 | 决策点 | 选项与取舍 |
| :-- | :-- | :-- |
| D-5 | 真加载用哪棵构建树、如何证明"携带的是下载来的包" | **(a) 用已重建的调试树，先备份其 `wolf/packages`，再用下载解包目录重配并只跑 staging 目标**（推荐，最少的构建动作即可证明来源切换）／(b) 只读运行，不改树（无法证明携带来源）／(c) 新建独立构建树（最忠实，代价是数小时） |
| D-6 | 第三方复现里"自己打包 2.4 声库"这一腿是否实跑转换 | **(a) 用本机保留的 2.3 声库压缩包走一遍转换再加载**（推荐，能证明整条腿）／(b) 只证明"下载 + 携带 + 加载现有 yousa-2.4" |
| D-7 | zip 产物的落点 | **(a) 与同级先例一致，放本机声库搜索根下**（推荐）／(b) 只放本机临时目录／(c) 两处都放 |
| D-8 | 语言包（cpp-pinyin）词典更新造成不兼容时，**提供方**该怎么做。注意：G2P 接口保持 `level 1` 是规范事实（Level 是运行时能力，不是软件版本），不随词典变更而变，所以这不是要决策的点 | **已选定 (a)**（用户 2026-10-07 确认"按推荐继续"）。真实历史比对显示目前从未抬过，见 [wolf-release-history.md](wolf-release-history.md)。原选项：**(a) 提供方抬 `compatVersion`**（推荐）／(b) 只涨 `version` 不动 `compatVersion`（省事但声库静默变声）／(c) 把词典拆成独立包（表达力最好，需重构包划分） |
| D-9 | 是否允许联网取一份**真实历史** wolf 语言发行，用来定"音素差异率达到多少算不兼容"的阈值 | **已选定 (a)**（用户同轮确认）。已取两个真实发行逐包比对：拼音词典没有独立历史修订，两发行间仅 `trans_word.txt` +1 行且 `compatVersion` 未动 ⇒ **不按差异率定阈值**，改为"词典文件任何变化即视为可能不兼容，再用门禁测影响"，见打包要点 §6。原选项：**(a) 允许**／(b) 不允许（阈值暂缺，只保留仪器与人造样本的标定结果） |

## 1. 目标与验收口径

三件事，按你的四项选择执行：

1. 校验 `yousa-2.4@1.65.1.0` 是否满足 wolf 与 synthrt 的标准格式要求，**静态校验 + 真加载两层都做**，
   实测成功再打包。打包对象**只限现有 yousa-2.4@1.65.1.0**，产物是**纯 .zip**（非 `.dspk`）。
2. 新建 `docs/synthrt/` 存放迁移到 synthrt 的交接文档（含打包声库），**尽量不动上游原 doc**。
3. 复测外部开发者流程：**模拟本地无资源**，从网络直接下载 wolf release 起，完成打包 2.4 声库与依赖。
   下载范围按 `assets.cmake` **全 15 个归档**。

**第三方编辑器 = 另一台机器上的另一个开发者**（你本轮的澄清）。因此本目标的真正验收标准是
**文档驱动的可复现性**：文档必须让没有本机任何资产的人从零走通。

## 2. 已核实的现场事实

| 事实 | 证据 |
| :-- | :-- |
| 声库包是 2.3 → 2.4 转换产物，目录名后缀 `-2.4` 来自脚本默认值 | `scripts/convert-voicebank.py:61` `DEFAULT_OUTPUT_SUFFIX = "-2.4"` |
| 发布级转换走带读回校验的包装脚本 | [synthrt-main-migration.md](../plans/synthrt-main-migration.md) §7 转换脚本三件套与回归三层 |
| 真加载机制已固化：无头自动化 `voices.list` → `tracks.set_voice` → 导出 8 s 波形 | `docs/plans/synthrt-main-migration.md:260-266`（当轮的无头启动与合成脚本为本机产物，未入库） |
| wolf release 为 `lang-v0.1.2.0`，15 个归档，端口逐条校验 SHA512 且不含任何凭证处理 | `scripts/vcpkg-ports/wolf-lang-packages/portfile.cmake:1-30`、`assets.cmake:6-7` |
| 语言包的搜索链：cache 变量 → 环境变量 → wolf 包自带的安装目录 → 同级兜底，且要求目录内含 `*/desc.json` | `cmake/LiteBuildApi.cmake:327-449` |
| 运行期落点是插件根下的 `wolf/packages` | `src/libs/SynthrtEngine/DeployLayout.h:55`、`SynthrtEngine.cpp:352` |
| 上一轮已做过静态核对与打包，且**自己标注了未做真加载** | 当轮的增量报告 `four-repo-increment-report.md` 已于 2026-10-07 移出仓库，留档在仓库外（未入库） |

## 3. 本轮新增的实测证据（P1 网络取证，已完成）

- 全新目录（本机临时目录，未入库）下按 `assets.cmake` 下载 15 个归档，**直连**完成，未使用代理。
- 15/15 逐条 SHA512 通过，总 18,113,900 字节，当轮报告为本机临时产物（未入库）。
- 验证器先自证：同输入比对为真、篡改一字节判 MISMATCH，排除"全绿是静默漏判"。
- 解包为 15 个目录，**全部含 `desc.json`**。
- 交叉核对：全新下载结果与端口缓存安装结果**逐字节一致**（双方各 70 文件，仅一方有 0，内容不同 0），
  两条独立获取路径互证。

## 4. 外部开发者路径（交接文档要写成的形状）

1. **取依赖**：按 `assets.cmake` 从 `https://github.com/diffscope/wolf/releases/download/lang-v<bundle>/`
   取归档并校验 SHA512，或直接让构建系统的包管理器（vcpkg 端口）代取。
2. **携带给编辑器**：把解包后的 15 个包目录放到编辑器插件根下的 `wolf/packages`
   （或把构建变量指向解包目录，让构建阶段自动落地）。
3. **打包声库**：`convert-package.py <2.3 声库目录> --packages <语言包目录> --output <目标目录>`，
   语言绑定由 `--packages` 决定，缺包的语种会告警并被丢弃。
4. **加载验证**：把产物所在目录作为声库搜索路径，无头实例 `voices.list` 认出、`tracks.set_voice` 通过、
   导出波形。
5. **分发**：打成 zip，内部带顶层同名目录（与 `qixuan@2.7.0.0.zip`、`zhibin@26.7.16.0.zip` 先例一致）。

**文档纪律**：交接文档一律用占位符写路径（如 `<wolf 检出>`、`<编辑器构建树>`），不含本机绝对路径。
本机专属细节（实际路径、会话记录）另存不入库的文件。

## 5. 阶段划分

| 阶段 | 内容 | 状态 |
| :-- | :-- | :-- |
| P1 | 网络取证：全 15 个归档下载 + SHA512 + 解包 + 交叉核对 | **已完成** |
| P2 | yousa-2.4 对 wolf/synthrt 标准格式的逐项核对表（子代理产出 + 主代理抽验） | **已完成**：见 [conformance-yousa-2.4.md](conformance-yousa-2.4.md) |
| P3 | 真加载：无头实例加载并合成，取波形证据（D-5 已定为改调试树并先备份） | **已完成**：`voices.list` 认出包、`rendered with phonemes: True`、导出 `succeeded`、wav 1,411,280 B |
| P4 | 打包：yousa-2.4 打成 zip，往返比对 + 结构校验（D-7 已定为放本机声库搜索根下） | **已完成**：516.4 MB、60 成员、逐成员校验 0 问题，指纹见 [conformance-yousa-2.4.md](conformance-yousa-2.4.md) |
| P5 | 交接文档落盘（本目录），按第 4 节形状写成可执行配方 | **已完成**：文档与工具清单见 [README.md](README.md)（本表不重复计数） |
| P6 | 照文档从零复现一遍（本目标的最终验收） | **已完成**：下载 ok=15、0 不一致、4 个 linguist、qixuan 真加载与导出均通过 |
| P7 | 韩语（kor）补齐与实测 | **已完成（结论为当前不可用）**：链路跑通但音素不可用，见 [korean-via-wolf-kor.md](korean-via-wolf-kor.md) |
| P8 | 词典不兼容的判据：仪器、标定与流程演练 | **已完成**：见 [packaging-voicebank-essentials.md](packaging-voicebank-essentials.md) 第 4 节与 [tools/measure-g2p-output.ps1](tools/measure-g2p-output.ps1) |
| P9 | 提供方政策（D-8）与阈值（D-9）落盘 | **已完成**：政策写入 [packaging-voicebank-essentials.md](packaging-voicebank-essentials.md) 第 6 节，真实发行证据见 [wolf-release-history.md](wolf-release-history.md) |

## 6. 未证实与边界

- 真加载已在 P3 与 P6 完成（无头实例认出包、渲染出音素、导出 8 s 波形），证据见
  [README.md](README.md) 与 [conformance-yousa-2.4.md](conformance-yousa-2.4.md)。
- 静态核对表已并入 [conformance-yousa-2.4.md](conformance-yousa-2.4.md)，其中未证实项在该文单列。
- 上游 OpenUtau 不识别 2.4 包（其源码与工程文件内扫不到 wolf/dsinfer/2.4 引用），因此"第三方编辑器"
  只能是相对 wolf/synthrt 的第三方消费方，即编辑器本身，相关线索见第 2 节。
- 下载走的是直连，未验证代理路径。
