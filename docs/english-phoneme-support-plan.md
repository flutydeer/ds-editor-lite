# 英语音素支持 — 设计讨论记录

## 开始时间
2026-05-02

## 背景

当前系统是为中文设计的，需要改造以支持英语演唱。英语 G2P 直出音素序列，不需要经过"发音→S2P 字典查找"这个中间步骤。

## 当前流程

```
lyric → G2P → pronunciation → (按语言分支) → 音素名列表 → OnsetMarker → 带卡拍的音素
```

### 关键文件

- `InferController.cpp` — 顶层编排，维护 6 个任务队列
- `GetPronunciationTask.cpp` — G2P：lyric → pronunciation（调用 `LangCore::Manager::convert()`）
- `GetPhonemeNameTask.cpp` — S2P + onset marking：pronunciation → phoneme names
- `S2pMgr.cpp` — 音节→音素字典管理器（按歌手+语言查字典文件）
- `Phonemes.h` — 数据模型：`PhonemeName`（含 `isOnset` 字段）、`PhonemeNameSeq`、`Phonemes`
- `Modules/Language/OnsetMarker/` — onset 标记接口和实现

## "+" 分配规则（已确认）

以 "international" 为例，g2p 返回带卡拍标记的音素序列，卡拍音素标记每组音素的起点：

- 单音符 → 所有音素集中在一个音符上
- word + `+` → 第1个音符取第1组，`+` 取剩余全部
- word + `+` + `+` → 每个 `+` 依次取下一组，最后一个 `+` 贪婪取剩余全部
- `-` 是延音符，不影响配额（用于转音）
- `++` 取 2 组 → `+` 的数量对应消耗的组数
- 溢出 → 标记错误

## 已完成

### 阶段 1：Onset Marker 接口（d06a712a）

将 `GetPhonemeNameTask` 中硬编码的 isOnset 标记逻辑抽取为可扩展接口。

**新增文件**（`Modules/Language/OnsetMarker/`）：
- `IOnsetMarker.h` — 接口：`mark(QStringList phonemeNames, QString language) → QList<PhonemeName>`
- `MandarinOnsetMarker` — 普通话规则（1个→卡拍，2个→第2个卡拍）
- `DefaultOnsetMarker` — 默认规则（全部标卡拍，作为占位）
- `OnsetMarkerMgr` — 单例注册表，按语言 ID 查找 marker，未找到返回 DefaultOnsetMarker

**改动**：
- `GetPhonemeNameTask.cpp` — 用 `OnsetMarkerMgr::instance()->marker(language)->mark(...)` 替换硬编码逻辑，移除 2 音素硬限制和 TODO 注释

### 阶段 2：基于规则 DSL 的通用 Onset Marker（177c4af6）

参考 `MakeDiffSinger/variance-temp-solution/add_ph_num_advanced.py` 中的设计，实现通用的基于规则的卡拍标记引擎。

**核心设计**：
- 音素类型分类（vowel/consonant/liquid）+ Trie 树规则匹配引擎
- 从左到右贪心最长匹配，优先级：最长 > 精确匹配多 > 精确匹配靠前
- 规则和分类表均为外部 JSON 数据，不硬编码

**新增文件**：
- `OnsetRuleTrie.h/.cpp` — Trie 树规则引擎（通配符+精确匹配）
- `PhonemeTypeMap.h/.cpp` — 音素名→类型映射
- `RuleBasedOnsetMarker.h/.cpp` — 从 JSON 加载配置，实现 `IOnsetMarker::mark()`
- `Resources/phoneme/eng.json` — 英语音素分类表（44个音素）+ 3条卡拍规则
- `scripts/convert_phoneme_symbols.py` — YAML→JSON 转换工具

**改动**：
- `OnsetMarkerMgr.h/.cpp` — 新增 `loadRuleBasedMarker(language, configPath)`

### 阶段 3：英语 pronunciation 解析（6eb53e11）

英语 G2P 返回空格分隔的音素名字符串，跳过 S2P 字典查找。

**改动**：
- `GetPhonemeNameTask.cpp` — 英语（`"eng"`）按空格 split 获取音素列表，其他语言仍走 S2P
- `OnsetMarkerMgr.h/.cpp` — 构造时自动扫描 `Resources/phoneme/` 目录加载配置

**已知问题**：~~英文歌词后追加音符时崩溃~~（已在阶段4中修复）。

### 阶段 4："+" 音素分配逻辑（29b5f939）

在 `getPhonemeNames()` 之后插入后处理步骤 `distributePhonemes()`，识别 word+`+` 序列，按卡拍组拆分并分配音素。

**分配规则**：
- 单词音符默认取第1组；末尾 `+` 可增加（如 `word+` 取前2组）
- 纯 `+` 音符按 `+` 数量消耗对应组数，最后一个 `+` 贪婪取剩余全部
- `-` 延音符不消耗配额，跳过
- 溢出时标空

**改动**：
- `GetPhonemeNameTask.h/.cpp` — 新增 `distributePhonemes()`、`isPlusNote()`；在 `processNotes()` 末尾调用
- `GetPronunciationTask.cpp` — G2P 输入剥掉歌词尾部 `+`；纯 `+` 和 `-` 音符跳过不送入 G2P
- `InferControllerHelper.cpp` — `updatePhoneName` 中 names 数量变化时清空 offsets
- `PhonemeView.cpp` — 防御性边界检查：offsets 索引不超过 offsets 长度

## 设计原则

- onset 标记规则按语言注册，通过 `OnsetMarkerMgr` 查找
- 新增语言只需提供音素分类表和卡拍规则即可
- 规则引擎通用化，支持 Trie + 通配符 + 贪心匹配，将来可自定义
- "+" 分配逻辑是语言无关的，在 onset 标记之后统一处理

## 待做

### 编辑器英语适配

- 按语言提供默认歌词和默认发音（如有），新建音符时使用对应语言的默认值
- 英语音符的 pronunciation view 隐藏（音素序列对用户无参考价值，已在音符下方 phoneme view 展示）
- `+` 音素组溢出时，UI 上应有可视化的错误提示

### 架构改进

- 语言分支目前硬编码 `"eng"` 特判跳过 S2P，将来需要通用机制（如检查 S2P 字典是否存在，或在语言配置中标记）
- phoneme 配置目前在 `OnsetMarkerMgr` 构造时同步加载，将来需改为异步以缩短应用启动时间
