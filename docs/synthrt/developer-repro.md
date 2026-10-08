# 外部开发者复现：从零取 wolf 语言包到打包并加载 2.4 声库

适用对象：**本地没有任何声库、没有 wolf 构建产物**的开发者。只需要一个 lite 检出、一个能跑的编辑器
构建、Python 3 与网络。本文命令按顺序执行即可复现，路径一律用占位符，替换成你自己的路径。

占位符约定：`<lite 检出>`、`<编辑器构建树>`（例如某个 CMake 预设的构建目录）、`<归档目录>`、
`<语言包目录>`、`<2.3 声库目录>`、`<目标目录>`、`<声库搜索根>`。

## 0. 前置条件

- 一个 lite 检出，至少包含 `scripts/convert-package.py`、`scripts/convert-voicebank.py` 与
  `scripts/vcpkg-ports/wolf-lang-packages/`。
- 一个已构建的编辑器：`<编辑器构建树>/out/bin/DsEditorLite.exe`。
- Python 3（转换与打包脚本只用标准库，无需安装第三方包）。
- 能访问 `github.com`（wolf 的发布件只从 GitHub Release 分发）。

## 1. 取依赖：按 `assets.cmake` 下载归档并逐条校验 SHA512

**权威清单只有一处**：`<lite 检出>/scripts/vcpkg-ports/wolf-lang-packages/assets.cmake`，它给出
bundle 版本、每个归档的文件名、SHA512 与解包目录名。**不要从本文或别处抄包名与哈希**，照该文件取。

- release 地址：`https://github.com/diffscope/wolf/releases/download/lang-v<bundle 版本>/<FILE>`
- 当前 bundle 为 `0.1.2.0`，共 15 个归档，总计约 17 MiB（2026-10-07 实测 18,113,900 字节）。
- 两种等价做法，任选其一：
  - **交给包管理器**：构建时由 vcpkg 端口 `wolf-lang-packages` 下载并逐条校验 SHA512，安装到
    `share/wolf/packages`，编辑器构建再从那里取用。
  - **自己下载**：逐个归档下载后校验 SHA512，再解包到 `<语言包目录>`。

```powershell
# 取依赖：清单与哈希都从 assets.cmake 读，脚本自己解析，不在别处再抄一份
python <lite 检出>/docs/synthrt/tools/fetch-wolf-release.py `
  --assets <lite 检出>/scripts/vcpkg-ports/wolf-lang-packages/assets.cmake `
  --out <归档目录> `
  --report <归档目录>/fetch-report.json

# 解包（tar 在 Windows 10+ 自带，能直接处理 zip）
foreach ($a in Get-ChildItem <归档目录> -Filter *.zip) { tar -xf $a.FullName -C <语言包目录> }
```

脚本先跑一次自证（把探针文件哈希两次，一次不改、一次改一字节，改过的那次必须判不一致），
自证不过直接退出码 2。它的退出码 0 表示 15 个归档逐个都匹配哈希，1 表示至少一个不匹配并请立刻停下。

预期：`<语言包目录>` 下有 15 个目录，每个目录内的 `desc.json` 是包的身份证（`id`、`version`、
`compatVersion`、`$version`、`runtimeLevel`）。**校验不通过必须停下**，否则后面所有结论都建立在
不可信的字节上。

## 2. 携带给编辑器：语言包放到插件根下的 `wolf/packages`

运行期落点由代码决定，不由文档约定：`src/libs/SynthrtEngine/DeployLayout.h` 的
`LANGUAGE_PACKAGES_DIR = "wolf/packages"`，拼在插件根之后，于是就是
`<编辑器构建树>/out/bin/wolf/packages`。

搜索链（按优先级）：cache 变量 `LITE_WOLF_LANG_PACKAGES` → 环境变量 `WOLF_LANG_PACKAGES_SOURCE`
→ wolf 包自带的安装目录（包管理器装出来的 `share/wolf/packages`）→ 同级目录兜底。该链要求目标
目录里存在 `*/desc.json`，否则配置阶段直接报错而不是静默产出空目录。

```powershell
# 让构建把下载来的语言包落地进运行树（只构建主目标即可，落地是挂在它上面的复制命令）
cmake -S <lite 检出> -B <编辑器构建树> -DLITE_WOLF_LANG_PACKAGES=<语言包目录>
cmake --build <编辑器构建树> --target DsEditorLite
```

预期日志：

```
-- [lang-packages] Staged 15 wolf language package(s) from '<语言包目录>' into '<编辑器构建树>/out/bin/...'
```

落地之后构建还会跑一次 `LitePackagingLayoutCheck`，按**包名**核对落点（数量对但名字不对、或
"复制了空集而旧包残留"都会被它抓出来）。发行版做法是把 `wolf/packages` 直接随安装包带上，
编辑器离线即可用。

## 3. 打包 2.4 声库并把依赖绑上

转换规则只有一份实现（`scripts/convert-voicebank.py`，参考转换器），`convert-package.py` 只是
在它外面加护栏与读回校验，两者不要各存一套规则。

```powershell
python <lite 检出>/scripts/convert-package.py <2.3 声库目录> --packages <语言包目录> --output <目标目录>
```

三个要点：

1. `--packages` 决定每个声明语言的 G2P 来自哪个语言包。缺包的语种会被**告警并丢弃**，不会静默
   产出一个自称支持却加载不了的包。
2. `--output` 是**确切的目标目录**，不是父目录（参考转换器的默认值才是"源包同级 + `-2.4` 后缀"）。
3. 目标目录不能已存在、不能是源包、不能包含源包或位于源包内，护栏会在复制之前拒绝。

预期输出（2026-10-07 实测样例）：

```
  converting a copy at <目标目录>
    cmn: built cmn-pinyin on wolf/lang-cmn:inference/g2p, 64 phoneme(s), 2 own stage(s)
    eng: built eng-arpabet on wolf/lang-eng:inference/g2p, 42 phoneme(s), 2 own stage(s)
    jpn: built jpn-romaji on wolf/lang-jpn:inference/g2p, 40 phoneme(s), 2 own stage(s)
    yue: built yue-jyutping on wolf/lang-yue:inference/g2p, 74 phoneme(s), 2 own stage(s)
    reserved phonemes, confirmed against every model: AP EP GS SP
  0 error(s), 3 warning(s)
    <目标目录>: 18 contribution(s) verified
  convert-package: 0 error(s), 0 warning(s)
```

- 依赖会按语言包的实际目标点写成 `wolf/lang-<lang>@<compatVersion>`（上例为 4 个语言）。
- 常见 warning：某些音素（如 `um`、`cl`）"有保留标记的形状且模型里有，但没有模型把它标为语言无关"，
  脚本会提示用 `--reserved` 显式声明。确定它们是标记时再传，不传就保留告警。
- 退出码 0 才代表产物可用。非 0 时目标目录里没有可用的 2.4 包。

## 4. 加载验证（要真加载，不是只看文件在不在）

把**产物所在目录**加进编辑器的声库搜索路径，然后让无头实例真的加载并合成：

```powershell
# 无头实例（端口自选，l3 是自动化控制级别）
<编辑器构建树>/out/bin/DsEditorLite.exe --headless --no-mcp --control-level l3 --control-port <port>
```

搜索路径写在编辑器的 `appConfig.json` 的 `general.packageSearchPaths`。随后按这个顺序调用
`http://127.0.0.1:<port>/automation/v1`（JSON-RPC）：

1. `application.get_status` 确认实例就绪。
2. `voices.list` 看包有没有被认出来，返回里应含 `package_id`、`package_version`、`singer_id`。
3. `documents.new` → `tracks.insert` → `tracks.set_voice` 指定歌手（`speaker` 按包内声明传，没有
   speaker 的包必须显式传 `null`，传空字符串会被参数校验拒绝）。
4. 插 `clips.insert` 与 `notes.insert`（音符带歌词与语言），等引擎渲染。
   **`rendered with phonemes: True` 才是 G2P 与音素层真的跑通**，只看到"包被认出来"不够。
5. `exports.audio.start` 导出波形并检查字节数大于 0。

预期：编辑器日志出现每个模型会话的推理耗时行（`Session [model.onnx] - Finished inference in ...`），
导出文件非空。这一步是"2.4 形状确实能被加载器接受"的唯一证据。

### 4.1 验证词典兼容性（提供方更新语言包时必须做）

背景：语言包可能**只改词典、不动任何版本号**，而加载器不会也无法发现这一点（实测见
[packaging-voicebank-essentials.md](packaging-voicebank-essentials.md) 第 4 节）。所以换语言包或怀疑词典变动时，
必须自己比一次 G2P 输出：

```
# 顶点 A：更新之前
pwsh -File tools/measure-g2p-output.ps1 -EditorExe <DsEditorLite.exe> -SingersRoot <Singers 目录> `
     -OutRoot <临时目录 A> -Out before.txt -Words "你 好" -Speaker <说话人> `
     -SingerPackage yousa -SingerPackageVersion 1.65.1 -SingerId yousa

# 换掉提供方包（或按 assets.cmake 取新版落地）之后
pwsh -File tools/measure-g2p-output.ps1 -EditorExe <DsEditorLite.exe> -SingersRoot <Singers 目录> `
     -OutRoot <临时目录 B> -Out after.txt -Words "你 好" -Speaker <说话人> `
     -SingerPackage yousa -SingerPackageVersion 1.65.1 -SingerId yousa

# 判读
Compare-Object (Get-Content before.txt) (Get-Content after.txt)
```

判读要求：歌词集要覆盖声库真正支持的语言与常见词，且**必须选词典命中词**（同输入重复运行须给出相同音素）。
**不要把 OOV／模型回退词纳入门禁**：实测见过同一 OOV 词跨运行给出不同音素（韩文 `zzq` 三次运行出现过两种结果），
这类路径本身不确定，差异不能归因于词典变更。
差异非空即说明这次更新会改变演唱内容。此时二选一：把声库的目标点改到新版本并接受新读音，
或者要求提供方抬 `compatVersion` 让老目标点失效（见
[conformance-yousa-2.4.md](conformance-yousa-2.4.md) 第 2.7 节）。

## 5. 分发：打成 zip

```powershell
python <lite 检出>/docs/synthrt/tools/pack-voicebank-zip.py <声库目录> <目标 zip>
```

- 形状是**扁平**的：`desc.json` 与顶层目录直接在归档根，与上一轮已发布产物一致。
- 注意：声库搜索根里那两个 `qixuan@2.7.0.0.zip`、`zhibin@26.7.16.0.zip` 是 **2.3 源**、带外层
  同名目录，不能当作 2.4 分发约定。
- 该脚本打完后会逐成员校验：成员名安全（拒绝绝对路径、盘符、反斜杠、`.`/`..` 段、重名、非 ASCII）、
  成员与磁盘内容按 SHA256 与长度一致、不多不少。脚本自带负控，先证明校验器不是空转。

## 6. 换机复现与排查

| 症状 | 原因 | 处理 |
| :-- | :-- | :-- |
| 配置阶段报"语言包目录里没有包" | 目录里缺 `*/desc.json`，或指向了归档目录而不是解包目录 | 指向解包后的 15 个包目录的**父目录** |
| `voices.list` 返回空 | 声库搜索路径没加、目录层级多了一层、或包根本不是 2.4 形状 | 搜索路径应指向**包含包目录的目录** |
| 渲染没有音素 | 该语言的 G2P 语言包没被携带进运行树 | 回第 2 步，检查 `<编辑器构建树>/out/bin/wolf/packages` 是否含该语言包 |
| `tracks.set_voice` 报参数错误 | 包的 speaker 是空的却传了字符串 | 传 `null` |
| 转换报缺语言包 | `--packages` 未传或指向空目录 | 传解包后的语言包目录 |
| 包能加载但某个语言不可用 | 该语言的卡拍层/音素层没有提供方 | 查语言包侧是否提供 onset 与 g2p，缺层必须显式登记为已知限制 |
| 链接时报 `LNK1168 无法打开 …dll 进行写入` | 有编辑器实例在跑，占住输出目录里的 DLL | 先关掉所有编辑器实例再构建 |
| 取依赖时部分归档 `not-downloaded` | 链路抖动，直连与代理都失败 | 脚本按 `--attempts` 重试（默认 3 轮，每轮先直连后代理），重跑即可。脚本不会把哈希不符的文件留在目录里 |
| 审计报 `declared 3, reached 4` 且多出 `yue` | 给审计工具传了整组语言包，`wolf-lang-yue` 让 `yue` 被视为可达 | 传该声库的依赖闭包，见 [conformance-yousa-2.4.md](conformance-yousa-2.4.md) 第 3 节 |

## 7. 与规范的关系

- 格式权威：`synthrt/docs/ds-spec-2.4.md`（wolf 检出内有内容等价的同名副本，仅措辞与两处小节标题
  不同，行号可互换引用）。
- 语言包分发与版本兼容：`wolf/docs/linguist-distribution.md`（依赖方写目标点、提供方给区间、
  第四位是打包修订号）。
- 卡拍层归属与检测：`wolf/docs/linguist-domain-contract.md`（对每个声明语言查 `maxDepth`，
  报 `Depth::Onsets` 才达标，缺层不得静默）。
