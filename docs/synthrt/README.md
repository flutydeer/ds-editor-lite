# synthrt 交接材料索引

本目录存放迁移到 synthrt 的交接文档与随附工具。面向的读者是**本地没有任何声库、没有 wolf 构建产物**
的开发者，以及接手 synthrt 侧集成的人。本目录不改动任何上游原文档。

## 阅读顺序

| 文档 | 内容 |
| :-- | :-- |
| 本文件 | 索引与一页结论 |
| [developer-repro.md](developer-repro.md) | 从零取 wolf 语言包到打包并加载 2.4 声库的完整配方（可照做） |
| [conformance-yousa-2.4.md](conformance-yousa-2.4.md) | `yousa-2.4@1.65.1.0` 的合规核对、实测证据与未证实项 |
| [korean-via-wolf-kor.md](korean-via-wolf-kor.md) | 用 wolf 的 `wolf-lang-kor` 给 2.4 声库补韩语：做法、踩坑与实测记录（结论修正为：链路可用、音素不可用） |
| [packaging-voicebank-essentials.md](packaging-voicebank-essentials.md) | 打包声库要点：提供方版本字段怎么填、清单纪律、依赖声明与词典型内容变更的波及面 |
| [wolf-release-history.md](wolf-release-history.md) | wolf 两个语言包发行的真实比对：版本与兼容声明的实际纪律、词典差异、供给路径差异 |
| [PLAN.md](PLAN.md) | 本批工作的方案与决策台账（含每个决策的来源轮次） |
| `tools/` | 随附脚本：取依赖、打包声库、测 G2P 输出以便判断词典是否兼容、审计 wolf 发行的版本纪律 |
| `baselines/` | 音素基线快照（cmn 18 字、eng 9 词、jpn 15 音节）与在用词典文件哈希清单：判断语言包更新是否改变演唱内容 |

## 一页结论

**`yousa-2.4@1.65.1.0` 满足 2.4 标准格式要求，可分发。** 三层证据：静态核对（15 个贡献与全部引用实存）、
语义核对（三个声明语言的 `maxDepth` 实测为 `Onsets`，卡拍层由声库侧闭环）、真加载（无头编辑器认出包、
渲染出音素、导出 8 秒波形 1,411,280 字节）。审计工具以依赖闭包运行时退出码 0。

**从零复现这条路是通的。** 实测：按 `assets.cmake` 直连 GitHub Release 取全 15 个归档（总计 18,113,900 字节，
逐条 SHA512 通过），用构建变量把它们携带进编辑器运行树，转换出 2.4 声库并真加载成功。

## 交给 synthrt 侧的关键事实

1. **依赖要整组带。** 语言包之间有依赖边：`wolf/lang-cmn → wolf/g2p-pinyin`、
   `wolf/lang-eng → wolf/g2p-multi`、`wolf/lang-jpn → 无`。只带用到的语言包会直接解析失败，
   所以要按 `assets.cmake` 整组取。
2. **运行期落点是契约。** 编辑器在 `<插件根>/wolf/packages` 下找语言包
   （`src/libs/SynthrtEngine/DeployLayout.h` 的 `LANGUAGE_PACKAGES_DIR`）。把该目录抽走会让声库
   因依赖无法解析而整包打不开，日志给出 `no installed Package satisfies dependency wolf/lang-cmn`。
3. **搜索链有四个来源，顺序固定**：cache 变量 `LITE_WOLF_LANG_PACKAGES` → 环境变量
   `WOLF_LANG_PACKAGES_SOURCE` → wolf 包自带的安装目录 → 同级兜底，且要求目标目录含 `*/desc.json`。
4. **审计工具要传依赖闭包，别传整组。** 传整组时你会看到 `declared 3, reached 4` 与多出来的 `yue`，
   这在 yousa、junninghua、zzm-kl 三个互不相关的包上都能复现，是工具侧的假阳性而非声库缺陷。
5. **版本在接口层会裁掉末尾零段**：`1.65.1.0` 报 `1.65.1`，`2.7.0.0` 报 `2.7`。按接口返回值传参。
6. **分发 zip 是扁平结构**（`desc.json` 与四个顶层目录直接在归档根）。搜索根里那两个带外层同名目录的
   zip 是 2.3 源，不是 2.4 分发约定。
7. **语言句柄必须与模型音素表的前缀一致。** 引擎把 token 拼成 `<语言句柄>/<音素>` 去模型音素表里查
   （`dsinfer/util/inferutil/src/InputWord.cpp:34-47`），对不上就报 `unknown token <音素>`。
   规范推荐 639-3（`ds-spec-2.4.md:732`），wolf 用 `kor`，而 yousa 的模型对韩语用的是 639-1 的 `ko`，
   两者必须统一。详见 [korean-via-wolf-kor.md](korean-via-wolf-kor.md)。
8. **依赖版本写"目标点"，区间在提供方一侧。** `dependencies[].version` 的含义是"要求对方兼容到的版本"
   （`ds-spec-2.4.md:157-161`），能否加载取决于候选包的 `compatVersion <= target <= version`
   （`:183-186`、`:398`）。所以"防止将来不必要的升级不兼容"靠两件事：依赖方只写真正需要的最低目标点，
   提供方维持低位 `compatVersion`。**不要在该字段写 `1.0.0 <= x < 2.0.0` 这类范围表达式**，见
   [conformance-yousa-2.4.md](conformance-yousa-2.4.md) 第 2.5 节。

## 随附工具

```powershell
# 取依赖：按 assets.cmake 取全组归档并逐条校验 SHA512
python docs/synthrt/tools/fetch-wolf-release.py --assets scripts/vcpkg-ports/wolf-lang-packages/assets.cmake --out <归档目录>

# 打包声库：扁平 zip，并逐成员校验与磁盘内容一致
python docs/synthrt/tools/pack-voicebank-zip.py <声库目录> <目标 zip>

# 测 G2P 输出：判断语言包（词典）更新是否改变演唱内容，两次运行直接 diff
pwsh -File docs/synthrt/tools/measure-g2p-output.ps1 -EditorExe <DsEditorLite.exe> `
     -SingersRoot <Singers 目录> -OutRoot <临时目录> -Out before.txt -Words "你 好" `
     -Speaker <说话人> -SingerPackage yousa -SingerPackageVersion 1.65.1 -SingerId yousa
```

Python 侧三个脚本都只用标准库，其中取依赖与打包两个各自**先跑一次自证**（取依赖脚本用负控证明
哈希校验不是空转，打包脚本用负控证明成员校验能抓出被篡改的字节），自证失败即中止。

## 未证实与待办

- 语言包 rev3（本地构建）与 rev4（发布件）的差异已量化（67→70 文件、15 个文件内容不同、rev4 多出
  `wolf-lang-zxx`），但两侧语义完全等价未逐字段证实。
- 审计假阳性已定位到 `wolf-lang-yue`：依赖闭包 + 它一个包即复现"多出 yue"，
  闭包 + `wolf-lang-zxx` 不复现（退出码 0）。机制（该包让 `yue` 语言被视为可达）未读源码证实。
- 歌手声明里的 `configuration.dict` 在 2.4 下是否仍被引擎消费未证实。
- 本文档只覆盖 Windows + 本仓构建树的落地方式，安装包侧的分发路径未实测。
