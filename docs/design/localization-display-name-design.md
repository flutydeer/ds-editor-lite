# 声库多语言显示名与本地化取词机制 — 设计文档

> 状态：✅ 实施完成（代码已构建 + `TestLocalizedText` 通过；提交待整理，2026-08-23）
> 上游契约：synthrt `refactor` 分支 2026-08-22 三笔提交（DisplayText 本地化收口 + 多音候选修复）+ ds-spec 2.4 多语言文本规则。

## 背景与目标

synthrt 将声库元数据中的多语言文本收敛为单一类型 `srt::core::DisplayText`，并升级到 ds-spec 2.4 规则：

- 一个 JSON 值可以是**字符串**（只有默认文本的短形式），或**对象**（必含默认键 `"_"`，其余键为 BCP 47 语言标签）
- `text(locale)` 执行 **RFC 4647 §3.4 Lookup**：先试完整标签，再从右端逐段去掉子标签（分隔符严格为 `-`），直到只剩语言子标签；仍不命中回退默认文本；**匹配大小写不敏感**；键含 `_`（POSIX 风格 `zh_CN`）**永不匹配**
- 宿主侧必须按同语义取词，且**切 UI 语言时对已缓存对象直接重新取词**（无需重扫声库）

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
  → 逐候选 RFC 4647 Lookup（跳过 _ 键、大小写不敏感、- 右截）
        ▼
  歌手 combo / 轨道头 / 剪辑标题 / 语言 combo / 声库管理器 / 参数编辑器
```

序列化/剪贴板路径（`ClipsInfo`、`SpeakerMixPreset`）继续消费主字段 `name()`——即默认文本，符合"无语言上下文用 `.text()`"的契约，向后兼容。

## 关键设计决策

### 1. 匹配算法复刻 synthrt，不引第三方匹配库

synthrt `DisplayText::text(locale)` 的实现（`lib/Core/Support/DisplayText.cpp`）是自研的 RFC 4647 Lookup——纯 ASCII 大小写不敏感 `==` + `-` 逐段右截 + 跳过含 `_` 的键，**没有任何 ICU / collation / normalization**。宿主侧必须与它同轴：

- `lite::Support::lookupLocalizedText()` 单标签版与其逐例等价
- **不用 ICU/ucol**（团队成员曾建议"用 Windows SDK 的 ICU i18n + ucol 做 match"），理由见下文"为何不用 ICU"

### 2. 取词候选用 `QLocale::uiLanguages()`，而不是 `bcp47Name()`

Qt 实测（Qt 6.11.2）`QLocale("zh_CN")`：

| API | 结果 | 说明 |
|---|---|---|
| `name()` | `zh_CN` | POSIX 下划线，**永不匹配**（文档明确禁止用） |
| `bcp47Name()` | `zh` | **最短形式**（只留语言子标签）——RFC 4647 Lookup 是"候选从长到短"，`zh` 命中不了 `zh-Hans` / `zh-CN` 键 |
| `uiLanguages()` | `zh-Hans-CN | zh-CN | zh-Hans | zh` | 完整偏好候选链（从最完整到最简），逐候选右截可覆盖 `zh-Hans`、`zh-CN`、`zh` 各种数据形态 |

结论：`UiLanguageManager::effectiveBcp47Candidates()` 返回 `effectiveLocale().uiLanguages()`；`effectiveBcp47Name()` 定义为候选链首位（完整标签）。UI 一律传候选列表。

### 3. 数据侧键的硬性规范（ds-spec 2.4）

- 多语言对象必须含 `"_"` 默认项；其余键为 **BCP 47**（`zh-CN`、`ja-JP`、`zh-Hans`…），**禁止 POSIX 下划线键**
- 历史遗留 `zh_CN` 等键会保留在翻译表里但从不匹配（synthrt 侧同样跳过）→ 存量数据需改写（`_`→`-` 规则转换即可）
- 实测：已装 14 个声库包中 29/34 个多语言字段是 POSIX 键；修正后全 BCP 47。语言名/音色名同样存在于 `characters/*/config.json`，一并覆盖

### 4. 为何不用 ICU（决策记录）

| 论证 | 事实依据 |
|---|---|
| ucol 语义错位 | `ucol_*` 是 collation（排序/字符串比较），不是语言**标签匹配**；正确对应物是 `uloc_acceptLanguage`（其语义仍是 RFC 4647 扩展），且非本次需求的载体 |
| 与 synthrt 契约冲突 | synthrt 匹配是自研确定规则；宿主引入 ICU 会插入 API 差异（canonicalize / likely-subtag / normalization），同一数据两测行为可能分叉 |
| Windows SDK ICU 是裁剪 C API | 微软官方文档：只暴露 C API、无 C++ API；`Not all data returned by ICU APIs will align with Windows OS`；运行依赖 `icu.dll`（Win10 1903+）、需 `CoInitializeEx`（<1903） |
| 跨平台破坏 | 项目有 macOS 支持；Windows 私有 ICU 无法在 mac/linux 编译，需双实现 |

**保持自研 Lookup，并用单元测试与 synthrt 语义对拍**（见"验证"）。

### 5. 切语言刷新链路

- Qt 语言切换广播 `QEvent::LanguageChange` → 各 `QWidget::changeEvent`（`TrackControlView` / `ClipEditorToolBarView` / `LanguageComboBox` 已有 retranslate 链路，重建 combo 时经 `displayName` 取新语言）
- `UiLanguageManager::languageChanged` 信号 → 非 QWidget 场景（`SpeakerMixEditorView` 参数编辑器声线名、`ParamEditorView` 工具栏），以及 RHI 剪辑标题（`TracksRhiWidget::event` 里 `scheduleSnapshot()` 重建快照）
- **legacy 后端剪辑卡片标题**：`TrackEditorView::changeEvent` 的 `LanguageChange` 分支遍历 `m_viewModel.tracks[*].clips`，对所有 `SingingClipView` 重新调 `setSingerName(displayName(candidates))` + `setSpeakerName`（`updateSingingClipDisplay`），即时跟随 UI 语言；RHI 后端由自身快照机制接管
- 顺序约定：`installTranslator` 造成的 `LanguageChange` 事件先于 `languageChanged` 信号触发 → 依赖"编辑器已重建名字，再刷工具栏"的 refresh 挂在**信号**上（`ParamEditorView` 构造函数 connect languageChanged → `refreshSpeakerMixToolBar`）
- 数据模型不缓存显示名 → 无重扫需求，天然满足"已缓存对象重新取词"

## 模块清单

| 文件 | 职责 |
|---|---|
| `src/libs/Support/LocalizedTextUtils.{h,cpp}` | `lite::Support::lookupLocalizedText`（单标签 + 候选列表两个重载）、`hasLocalizedTexts` |
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

- `TestLocalizedText`（`src/tests/TestLocalizedText/main.cpp`）对拍 synthrt 语义：候选链命中 `zh-Hans`/`zh-CN`、跳过 POSIX 键、大小写不敏感、右截链、空表回退、`hasLocalizedTexts` 判定；`ctest -R TestLocalizedText` 通过
- 构建：debug preset 全量编译 + `DsEditorLite.exe` 链接通过
- GUI 冒烟：切 UI 语言 → 声库列表/歌手 combo/语言名单词/声库管理应显示本地化名（`junninghua`：君凝华/君凝華/うろこ音凝華；`qixuan` 绮萱、`zhibin` 挚彬 CE、`liliko` 琉璃、`shisakune` 試作音デモ）

## 已知限制

- 被修正的包若将来重新安装/升级会覆盖修复，发布方需在打包链路里采用 BCP 47 键
