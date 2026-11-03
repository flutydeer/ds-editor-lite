# 连接器契约变更：spec 2.4 迁移

本文记录 spec 2.4 迁移对**已发布自动化契约**的破坏性变更。这些改动没有对应的兼容层，按旧契约编写的连接器客户端会静默失效或超时，因此单独成文。

适用范围：`docs/automation/02-mcp-server-and-connector/public-tool-matrix.md` 描述的公开工具面。

## 设置字段改名

| 旧字段 | 新字段 | 状态 |
|---|---|---|
| `gameDirectory` | — | **已移除**（全仓 0 命中） |
| `pitchModelPath` | — | **已移除**（全仓 0 命中） |
| — | `noteAnalyzer` | 新增（全仓 26 处） |
| — | `pitchAnalyzer` | 新增（全仓 28 处） |

分析器不再由固定路径指定，而是由「已安装分析器」推导。旧客户端读取或写入旧字段不会报错，但也不会生效。

## 提取能力的 `models[]`

旧契约：固定返回 `rmvpe`（音高）与 `game`（音符）两项。

新契约：**每个已安装分析器一项**，`id` 形如 `包:inference/贡献`。

影响：按数组下标或按固定 `id` 取值的客户端会取错项或取不到。应按 `id` 前缀匹配，或先枚举再按能力筛选。

## `packages.refresh` 的终态

旧契约：任务失败以 `failed` 终态表示。

新契约：**失败也返回 `succeeded`**，具体失败项走 `application_result.failures`。

影响：**按旧语义轮询 `failed` 终态的客户端会一直等到超时**。这是本次变更中最容易造成实际故障的一条。

## 错误字段路径

`fieldPath` 由 `language` 改为 `language_id`。

影响：按字符串精确匹配 `fieldPath` 做错误提示的客户端会匹配不到。

## `g2p_id`

事实上已被弃用，但**尚未完全移除**（全仓仍有 3 处引用）。

影响：客户端不应再依赖该字段。它是否会在后续版本中移除尚未确定。

## 尚未核实的两项

以下两条在本次核对中未能确认，需要与实现方对账后再补入本文：

1. `models[]` 的精确 JSON 形状（本文只确认了「每分析器一项」与 `id` 的构造方式，未确认其余字段）。
2. `application_result.failures` 的条目标结构。

## 核对依据

- 字段命中统计：对 `src` 全目录 *.cpp/*.h 计数（`gameDirectory` 0、`pitchModelPath` 0、`noteAnalyzer` 26、`pitchAnalyzer` 28、`g2p_id` 3、`language_id` 47）。
- `public-tool-matrix.md` 当前仅有 `packages.refresh` 的两条描述，上述变更均未记录。
- 基线：`ds-editor-lite` 分支 `synthrt/spec2.4-uptake`，提交 `76d64402d`。
