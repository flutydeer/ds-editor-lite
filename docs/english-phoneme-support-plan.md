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

## 待实现

### 阶段 2：基于规则 DSL 的通用 Onset Marker

参考 `MakeDiffSinger/variance-temp-solution/add_ph_num_advanced.py` 中的设计，实现通用的基于规则的卡拍标记引擎。

#### 核心概念

**音素类型分类**：每个音素归入一种类型：
- `vowel` — 元音
- `consonant` — 辅音
- `liquid` — 介音（如 r, l, w, y）

分类表以外部数据加载（每种语言一份），格式为音素名→类型的映射。

**卡拍规则 DSL**：规则由"模式 → 卡拍位置"组成。模式是音素类型序列，卡拍位置指出模式中第几个音素卡拍。

示例规则（对应 Python 版本）：

| 模式 | 卡拍位置 | 含义 |
|------|----------|------|
| `vowel` | `[0]` | 单元音：自己卡拍 |
| `consonant, liquid, vowel` | `[1]` | 辅+介+元：介音卡拍 |
| `liquid, liquid, vowel` | `[1]` | 介+介+元：第2个介音卡拍 |

#### 匹配机制

- **Trie 树**存储规则，支持两种匹配：精确匹配（按音素名）和通配符匹配（按音素类型）
- 对每个单词的音素序列，从左到右**贪心最长匹配**
- 优先级：最长匹配 > 精确匹配数量多 > 精确匹配位置靠前
- 未匹配的音素标为非卡拍，跳过1个继续

#### 实现方案

新建 `Modules/Language/OnsetMarker/RuleBasedOnsetMarker`：

- `PhonemeTypeMap` — 音素名→类型映射表，可从外部文件加载
- `OnsetRuleTrie` — Trie 树，存储模式→卡拍位置规则，支持通配符
- `RuleBasedOnsetMarker : IOnsetMarker` — 持有 TypeMap + RuleTrie，实现 `mark()` 方法

`OnsetMarkerMgr` 注册时为英语提供 `RuleBasedOnsetMarker` 实例（加载英语音素分类表 + 默认规则集）。

#### 可扩展性

- 规则和分类表均为数据，不硬编码，将来可支持自定义卡拍规则
- 精确匹配 + 通配符共存，允许对特定音素添加特例
- 其他语言（日语、粤语等）可复用同一套引擎，只需提供各自的分类表和规则

### 阶段 3：英语 pronunciation 解析

英语 G2P 返回空格分隔的音素名字符串（不经过 S2P 字典查找）。需要在 `GetPhonemeNameTask` 中按语言决定是否走 S2P：
- 中文：pronunciation → S2P 查字典 → 音素名列表
- 英语：pronunciation → 按空格拆分 → 音素名列表

### 阶段 4："+" 分配逻辑

在 onset 标记之后，按 `isOnset=true` 的位置切成音素组，再根据 "+" 规则分配到各音符。这部分逻辑是语言无关的。`GetPhonemeNameTask` 已有 `splitSyllables` 和 `checkTrailingPlus` 方法，需要在此基础上完善多音符分配。

## 设计原则

- onset 标记规则按语言注册，通过 `OnsetMarkerMgr` 查找
- 新增语言只需提供音素分类表和卡拍规则即可
- 规则引擎通用化，支持 Trie + 通配符 + 贪心匹配，将来可自定义
- "+" 分配逻辑是语言无关的，在 onset 标记之后统一处理
