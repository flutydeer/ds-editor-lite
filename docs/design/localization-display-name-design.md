# 声库多语言显示名与本地化取词机制 — 设计文档

> 状态：✅ 实施完成（2026-08-25 透传化迭代：`text(key)` 契约 + ICU 匹配内核对拍通过，ctest 38/38）
> 修订：2026-08-26 PR#164 审阅修复——ICU ≥67 的 `uloc_acceptLanguage` 改走 LocaleMatcher 距离匹配，
> 兄弟 locale（zh_TW↔zh_Hant）会互相命中；匹配内核改为「显式父链 + 规范 ID 精确比对」（见 §4）。
> 上游契约：synthrt `localization/passthrough-keys`（透传化提交 `217862d`，见
> `synthrt/docs/api-changes-2026-08-25.md`）+ ds-spec 2.4 多语言文本规则。
> 历史：首版实现（2026-08-23，自研 Lookup）已被本节 §1/§4 的记录取代。

## 背景与目标

synthrt 将声库元数据中的多语言文本收敛为单一类型 `srt::core::DisplayText`，并升级到 ds-spec 2.4 规则：

- 一个 JSON 值可以是**字符串**（只有默认文本的短形式），或**对象**（必含默认键 `"_"`，其余键为内容作者与前端约定的语言代码，推荐 BCP 47）
- **Runtime 不做任何匹配**（ds-spec 2.4 透传模型）：`text(key)` 是精确、区分大小写的按键直取，缺键返回 `nullptr`，不 Lookup、不折叠大小写、不回退默认文本；`locales()` 原样暴露全部键
- 语言匹配是**前端**的责任：候选键生成、截断/归一化、何时取默认文本由宿主自决；**切 UI 语言时对已缓存对象直接重新取词**（无需重扫声库）

此前 ds-editor-lite 只把 `DisplayText` 拍扁成默认文本（`QString`），翻译全部丢弃，切语言不生效。

**目标**：声库/歌手/声线/语言/包名等展示名携带全翻译，按当前 UI 语言（含 BCP 47 候选链）即时解析，且与 synthrt 的匹配语义逐例一致。

## 数据流全景

```
synthrt 扫描
  VoicebankSnapshot (singerSnapshot.name 等, srt::core::DisplayText)
        │  PackageManager.cpp::toLocalizedTextMap() 遍历 locales() 拷全翻译
        ▼
lite 模型（值类型, QSharedData）
  SingerInfo / LanguageInfo / SpeakerInfo .localizedNames: QMap<QString,QString>
  PackageInfo .localizedVendor / .localizedDescription / .localizedLicense
        │  主字段 name/vendor/... 保持 默认文本(_) 语义
        ▼
UI 取词
  displayName(UiLanguageManager::currentBcp47Candidates())
  → lite::Support::lookupLocalizedText(localized, defaultText, candidates)
  → 逐候选 IcuWrapper::bestMatch（ICU 归一化大小写/POSIX 分隔符，命中键按原样拼写精确取值）
        ▼
  歌手 combo / 轨道头 / 剪辑标题 / 语言 combo / 声库管理器 / 参数编辑器
```

序列化/剪贴板路径（`ClipsInfo`、`SpeakerMixPreset`）继续消费主字段 `name()`——即默认文本，符合"无语言上下文用 `.text()`"的契约，向后兼容。

## 关键设计决策

### 1. 匹配上交前端，宿主匹配内核用 ICU（vendored icu-wrapper）

synthrt 的 `DisplayText` 自 2026-08-25 透传化后**不再有匹配语义**：键是不透明、区分大小写的 map key，`text(key)` 只做精确直取。宿主需自承载匹配，内核选择为 ICU：

- `lite::Support::lookupLocalizedText()` 保持既有 API 不变（UI 消费点零改动），实现改为逐候选调 `IcuWrapper::bestMatch(candidate, keys)`，首个非空命中即按**原样拼写**回查 map
- icu-wrapper 以源码 vendored 于 `src/3rdparty/icu-wrapper`（静态库，Windows 链系统 SDK `icu.lib` → 系统 `icuuc.dll`，零新增运行时依赖；macOS 用 `NSBundle`/`NSLocale`，Linux 用 `ICU::uc`）
- 顺带收益：POSIX 键老包（`zh_CN`）经归一化后**恢复显示**；大小写差异键可互命中
- 旧决策「不用 ICU」被推翻，理由与新依据见下文 §4

### 2. 取词候选用 `QLocale::uiLanguages()`，而不是 `bcp47Name()`

Qt 实测（Qt 6.11.2）`QLocale("zh_CN")`：

| API | 结果 | 说明 |
|---|---|---|
| `name()` | `zh_CN` | POSIX 下划线，匹配前需归一化（ICU 内核已处理 `_`→`-`），不要直接喂给匹配器当唯一候选 |
| `bcp47Name()` | `zh` | **最短形式**（只留语言子标签）——单独用它命中率远低于完整候选链，命中不了 `zh-Hans` / `zh-CN` 键 |
| `uiLanguages()` | `zh-Hans-CN | zh-CN | zh-Hans | zh` | 完整偏好候选链（从最完整到最简），逐候选右截可覆盖 `zh-Hans`、`zh-CN`、`zh` 各种数据形态 |

结论：`UiLanguageManager::effectiveBcp47Candidates()` 返回 `effectiveLocale().uiLanguages()`；`effectiveBcp47Name()` 定义为候选链首位（完整标签）。UI 一律传候选列表。

### 3. 数据侧键的规范（ds-spec 2.4 透传模型）

- 多语言对象必须含 `"_"` 默认项；其余键推荐 **BCP 47**（`zh-CN`、`ja-JP`、`zh-Hans`…），Runtime 不验证语法；synthrt `PackageValidator` 对 POSIX 下划线键仅报 Warning（引导迁移），缺 `_` 仍报 Error
- 历史遗留 `zh_CN` 等 POSIX 键在 Runtime 侧原样保留；经 ICU 归一化匹配后**恢复显示**（相对首版实现的语义翻转）。存量数据改写补丁仍建议应用（BCP 47 是命中面最广的写法）
- 实测：已装 14 个声库包中 29/34 个多语言字段是 POSIX 键；修正后全 BCP 47。语言名/音色名同样存在于 `characters/*/config.json`，一并覆盖

### 4. 为何改用 ICU（2026-08-25，推翻首版「不用 ICU」决策）

首版决策的前提是「synthrt 内嵌自研 RFC 4647 Lookup，宿主必须与它对拍」。ds-spec 2.4 改为透传模型后此前提失效——匹配本就归前端，「与 synthrt 契约冲突」的论点不复存在。重新评估：

- **匹配内核**：仅用 `uloc_forLanguageTag`（归一化）+ `uloc_getParent`（父链展开）做**规范 ID 精确比对**，不使用 `uloc_acceptLanguage` 当匹配器（2026-08-26 修订）。后者两头都不满足需求：Windows SDK umbrella ICU（64.2）对 script+region 组合不做内部回退；而自 ICU 67 起它改委托 `LocaleMatcher` 距离匹配，likely-subtag 最大化会让 zh_TW↔zh_Hant 兄弟 locale 互相命中（ICU 67.1/68.2/70.1/74.2 源码对拍确认；`IcuWrapperTests::rejectsSiblingScriptRegionCrossMatch` 锁定；`TestLocalizedText` 含同义断言）。注：ICU ≤66 的手写实现恰好就是「父链 + 精确 strcmp」，本修复等于把该语义统一固化到全平台全版本。父链式匹配语义逐候选确定、跨平台/跨 ICU 版本一致。
- **运行时面**：Windows IBM 系统组件（Win10 1903+ 自带 `icuuc.dll`），零新增依赖；包体零变化（静态库）
- **跨平台**：三后端各用平台原生能力（Win：SDK umbrella ICU；macOS：Foundation `NSBundle`；Linux：`ICU::uc`），统一 `IcuWrapper::bestMatch` API + 共享单测
- **fetch 契约闭环**：bestMatch 返回**原样拼写**的命中键，宿主再经 synthrt `text(key)` 精确直取——归一化只发生在匹配阶段，不污染数据面

仍成立的注意点：macOS 路径未随本波实测（P3）：返回键拼写与原样的等价性、zh-Hant/zh-TW 边界行为以 ICU 路径测试为准。

### 5. 切语言刷新链路

- Qt 语言切换广播 `QEvent::LanguageChange` → 各 `QWidget::changeEvent`（`TrackControlView` / `ClipEditorToolBarView` / `LanguageComboBox` 已有 retranslate 链路，重建 combo 时经 `displayName` 取新语言）
- `UiLanguageManager::languageChanged` 信号 → 非 QWidget 场景（`SpeakerMixEditorView` 参数编辑器声线名、`ParamEditorView` 工具栏），以及 RHI 剪辑标题（`TracksRhiWidget::event` 里 `scheduleSnapshot()` 重建快照）
- **legacy 后端剪辑卡片标题**：`TrackEditorView::changeEvent` 的 `LanguageChange` 分支遍历 `m_viewModel.tracks[*].clips`，对所有 `SingingClipView` 重新调 `setSingerName(displayName(candidates))` + `setSpeakerName`（`updateSingingClipDisplay`），即时跟随 UI 语言；RHI 后端由自身快照机制接管
- 顺序约定：`installTranslator` 造成的 `LanguageChange` 事件先于 `languageChanged` 信号触发 → 依赖"编辑器已重建名字，再刷工具栏"的 refresh 挂在**信号**上（`ParamEditorView` 构造函数 connect languageChanged → `refreshSpeakerMixToolBar`）
- 数据模型不缓存显示名 → 无重扫需求，天然满足"已缓存对象重新取词"

## 模块清单

| 文件 | 职责 |
|---|---|
| `src/3rdparty/icu-wrapper/` | vendored 匹配内核：`IcuWrapper::bestMatch`（平台后端 ×3，静态库）+ `IcuWrapperTests` |
| `src/libs/Support/LocalizedTextUtils.{h,cpp}` | `lite::Support::lookupLocalizedText`（单标签 + 候选列表两个重载，内核走 IcuWrapper）、`hasLocalizedTexts` |
| `src/app/Utils/UiLanguageManager.{h,cpp}` | `effectiveBcp47Candidates()` = `uiLanguages()`；`effectiveBcp47Name()` = 候选首位；同名静态 `current*` |
| `src/libs/ProjectModel/Voice/{Singer,Language,Speaker}Info.{h,cpp}` | 新增 `localizedNames` + `displayName(String/StringList)` |
| `src/libs/PackageManager/Models/PackageInfo.{h,cpp}` | 新增 `localizedVendor/Description/License` + `display*` 重载 |
| `src/libs/PackageManager/PackageManager.cpp` | `toLocalizedTextMap(const srt::core::DisplayText&)` 拷全翻译；构建各模型时 `setLocalized*` |
| UI 消费点 | `SpeakerMixDisplayUtils`、`TwoLevelComboBox`、`LanguageComboBox`、`SpeakerMixDialog`、`Package{DetailsHeader,DetailsContent,ItemDelegate,FilterProxyModel}`、`ParamEditorView`/`SpeakerMixEditorView`、`{TrackEditor,TracksGraphics,TracksRhi}View` |

模型 API 保持**主字段(默认文本) + 本地化表 + display(locale)** 的组合，`operator==`/拷贝构造同步本地化表（`SingerInfoData` 等），`isEmpty()` 语义不变。

## 数据侧修正补丁（交付物）

- 修正脚本 + 按包补丁 zip 已就绪（2026-08-23），覆盖已装 `DiffScope_packages` 中 7 个包的多语言键
- 补丁仅改写 `DisplayText` 对象内的键格式（`zh_CN→zh-CN` 等），不动其他字段；并注入 RAW 原始包（OpenUtau `character.txt`）提取的翻译：`liliko` zh「琉璃」、`shisakune` ja「試作音デモ」
- 回滚：原始文件已单独备份；用户**按需下载单个包的补丁压缩包**解压覆盖即可，无需重装声库

## 验证

- `TestLocalizedText`（`src/tests/TestLocalizedText/main.cpp`）对拍前端匹配语义：候选链命中 `zh-Hans`/`zh-CN`、POSIX 键 `zh_CN` 经归一化命中、大小写不敏感、空表回退、`zh-Hant`/`zh-TW` 边界互不命中、`hasLocalizedTexts` 判定；`IcuWrapperTests` 覆盖 ICU 内核（含 script+region 回退回归）
- 构建：2026-08-25 经 GitHub pin（非本地 setdir）的 synthrt `0.1.0.13#5` debug preset 全量编译 + `ctest` 38/38 通过
- GUI 冒烟：切 UI 语言 → 声库列表/歌手 combo/语言名单词/声库管理应显示本地化名（`junninghua`：君凝华/君凝華/うろこ音凝華；`qixuan` 绮萱、`zhibin` 挚彬 CE、`liliko` 琉璃、`shisakune` 試作音デモ）

## 已知限制

- 被修正的包若将来重新安装/升级会覆盖修复，发布方需在打包链路里采用 BCP 47 键
