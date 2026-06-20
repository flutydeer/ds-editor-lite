# Speaker Mix Dialog (声线混合对话框)

## 概述

SpeakerMixDialog 用于配置同一歌手模型内多个 speaker 的固定混合比例。对话框位于 `src/app/UI/Dialogs/SpeakerMix/`，包含 3 个文件：

- `SpeakerMixDialog.h/.cpp` — 继承 `Dialog`，组合 tag 选择区 + 列表 + 比例条
- `SpeakerMixList.h/.cpp` — QListWidget，管理 speaker 行（拖拽排序、ComboBox、位置标签）
- `SpeakerMixBar.h/.cpp` — 自绘比例条（拖拽分割点调比例、彩色 segment、标签）

新增通用控件：

- `UI/Controls/FlowLayout.h/.cpp` — 来自 Qt 官方 example，自动换行布局
- `UI/Controls/TagButton.h/.cpp` — 可选中的 tag 按钮（继承 QPushButton，QSS 在 controls.qss）

## 对话框布局（从上到下）

1. **Tag 选择区** — FlowLayout + TagButton，每个 speaker 一个 tag；checked = 参与混合
2. **SpeakerMixList** — 仅显示已勾选的 speaker
3. **SpeakerMixBar** — 比例条，拖拽分割点调整比例

```
┌───────────────────────────────────────┐
│  [朱] [樱] [琪] [梨] [珏]            │  ← Tag 选择区（FlowLayout 自动换行）
├───────────────────────────────────────┤
│  = ● 声线: [朱 ▼]  位置: 0%          │  ← 列表行（可拖拽排序）
│  = ● 声线: [樱 ▼]  位置: 20%         │
│  = ● 声线: [琪 ▼]  位置: 40%         │
│  = ● 声线: [梨 ▼]  位置: 60%         │
│  = ● 声线: [珏 ▼]  位置: 80%         │
├───────────────────────────────────────┤
│  ██朱██  ██樱██  ██琪██  ██梨██ ██珏██│  ← 比例条
└───────────────────────────────────────┘
```

## 关键设计决策

- Speaker 集合由歌手包决定（固定），不允许添加不存在的或重复的 speaker
- Tag 选择区控制成员资格，取代了原有的 +/- 按钮
- 所有 speaker 默认全选
- 最少保留 1 个 speaker（最后一个 tag 不可取消）
- Tag ↔ 列表双向同步：tag toggle → addSpeaker/removeSpeaker；行内 ComboBox 切换 → emit `speakerChanged` → Dialog 更新 tag 状态
- 行内 ComboBox 用于替换当前槽位 speaker：当前 speaker 和未使用 speaker 可选；其它已使用 speaker 禁用并显示 `（已使用）`
- 行内替换只改变 speaker 身份，保留该行当前比例，不做自动交换

## SpeakerMixList 公开接口

| 方法 / 信号 | 说明 |
|------------|------|
| `addSpeaker(const QString &name)` | 添加行，保留原有 speaker 比例，新 speaker 从当前比例中分配份额 |
| `removeSpeaker(const QString &name)` | 移除行，被移除 speaker 的比例按剩余项原比例重新分配 |
| `speakerChanged(oldName, newName)` 信号 | 行内 ComboBox 切换时发射 |
| `defaultColors()` | public static，Dialog 也用于 tag 颜色 |
| `getMixBar()` | 返回 SpeakerMixBar 指针 |
| `getValues() / getDoubleValues() / getLabels()` | 获取当前整数显示比例、内部 double 比例和标签 |

## SpeakerMixBar 实现要点

- 内部使用 double 比例，避免反复增删、拖动时整数误差累积；public `getValues()` 仍返回总和为 100 的整数百分比
- 显示与拖动吸附到整数百分比：拖动分割线时按 1% 粒度吸附，segment 文本显示整数百分比
- 默认拖动分割线只调整相邻两个 speaker 的边界，其余 speaker 比例不变
- 按住 `Alt` 拖动时，分割线左侧组与右侧组按组内原比例等比例压缩/扩展
- 拖动计算基于按下时的比例快照，拖动过程中切换 `Alt` 不会基于中间结果继续累计误差
- 允许相邻块压缩到 0%，但整体比例始终 clamp 到非负并保持总和为 100
- `m_dragOffset` 模式：按下时记录光标到分割点中心的偏移，拖动时补偿（无抖动）
- `m_margin = 1`：四周 1px 留白，防止抗锯齿圆角裁切
- `m_handleWidth = 6`，`m_handleTouchMargin = 3`：热区匹配视觉分割线宽度
- 所有 segment 统一 3px 圆角；首段右缩 1px，末段左缩 1px，中间段两侧各缩 1px

## SpeakerMixList 架构要点

- QListWidget + `setItemWidget()` 方案：嵌入控件吞噬鼠标事件，需 eventFilter 转发拖拽手柄事件到 viewport
- 拖放排序时按 speaker 名称携带原比例，重排后比例跟随 speaker，而不是按新行号重新等分
- 行内 ComboBox 切换 speaker 时保留该行当前比例，只更新标签与可选项
- ComboBox 的业务值以 `row.speakerName` / `Qt::UserRole` 原始 speaker 名为准，不依赖带 `（已使用）` 后缀的展示文本
- 行内 ComboBox 固定宽度 120px；下拉弹窗按最长选项设置最小宽度，避免 `（已使用）` 被截断；禁用项使用半透明文字
- `resizeEvent` 中调用 `doItemsLayout()` 强制同步布局（QListWidget 的 `scheduleDelayedItemsLayout` 在快速 resize 时延迟）
- 全局 QSS `QListWidget::item { padding: 4px 8px }` 增加 8px 垂直间距 → sizeHint 必须为 36px（28 内容 + 8 padding）
- FlowLayout 容器需要 `setSizePolicy(Expanding, Preferred)` 才能在 QVBoxLayout 中正确计算 heightForWidth

## 共享比例计算

- 新增 `UI/Utils/SpeakerMixUtils.h/.cpp`，供 `SpeakerMixBar` 和 `SpeakerMixEditorView` 共用
- 负责 N-1 stored weights ↔ N full weights 转换、full weights 归一化、整数百分比显示、1% 吸附
- 负责默认相邻拖动和 `Alt` 左右两组等比例拖动，避免比例条和关键帧编辑器行为分叉

## SpeakerMixEditorView 对齐情况

- 关键帧分割点已对齐 `SpeakerMixBar`：整数百分比显示/吸附、默认相邻调整、`Alt` 等比例压缩/扩展
- 拖动计算基于 drag-start 快照，允许相邻块压缩到 0%，不再保留 1% 最小宽度
- hover 和 drag 分割点 tooltip 都使用自定义 `ToolTip`，不再混用原生 `QToolTip`
- 第一个关键帧仍只锁水平位置，不锁权重编辑

## TagButton QSS（controls.qss）

| 状态 | 样式 |
|------|------|
| 默认 | transparent 背景，`#363B46` 描边，4px 圆角，28px 高，12px 水平 padding |
| hover | `#2E3444` 背景 |
| checked | `#9BBAFF` 文字 + 描边，transparent 背景 |
| checked:hover | 12% 透明度 `#9BBAFF` 背景 |

## Commits

- `c45ae64b` — Add speaker tag selection area with TagButton control
- `324168da` — Add FlowLayout from Qt official example
- `dde3f61f` — Tone down QListWidget item hover/selected colors to match track list
- `b95ed2c9` — Move SpeakerMix controls into main app as debug dialog
- `2b6ac398` — Improve speaker mix ratio editing
- `bf8eb7ed` — Align speaker mix split dragging
- `d63c0b81` — Use custom tooltip for speaker mix hover
- `fc377933` — Disable used speaker mix options

## 未提交的本地改动

- `main.cpp` — 启动时弹出对话框的调试代码（不提交）
- `docs/speaker-mix-dialog.md` — 当前文档仍是未跟踪文件，需在合适时机加入版本控制

## 下一步

- 把 `SpeakerMixDialog` 从 debug/demo 收口成正式组件接口：设置初始 sources/weights，确认后返回当前配置
- 接入真实歌手包 speaker 列表，替换当前硬编码 `{"朱","樱","琪","梨","珏"}` 和 `test_package`
- 明确 OK/Cancel 或即时应用语义，以及后续如何接 `ReplaceSpeakerMixAction`
- 完成固定模式与关键帧模式的权限切换：Fixed Mix 允许增删/替换/排序，关键帧模式锁定 speaker 列表，只允许拖比例
- 后续再做 Fixed Mix / Dynamic Mix 模式切换和固定混合预设
