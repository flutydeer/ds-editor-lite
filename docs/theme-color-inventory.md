# 主题颜色基线盘点

> 状态：迁移后审计。统计对象为 `src/app/Resources/theme/lite-dark/*.qss`。数字保留为历史
> 快照，用于衡量迁移规模。Phase 3 已将领域 QSS 中的普通颜色和 `qproperty-*` 颜色属性
> 迁移到 semantic token；FillLyric 的复合字符串属性由独立 QssParser 管理，自 Phase 4 起
> 支持 `${token}` 占位符与旧 `r,g,b,a[,width]` 格式。Phase 4 另完成生产代码（`src/app` 与
> `src/libs/GUI`）硬编码颜色复核，结果见下文「生产代码硬编码颜色审计」。

## 规模

按颜色字面量做词法统计（包含注释中的历史值、`transparent`、RGB/RGBA 和十六进制的
不同写法）共有 **588 处、169 种写法**。这些数字用于衡量迁移规模，不代表最终需要
169 个 token；同值可能有不同语义，同语义也可能因为历史原因存在多个近似值。

| 文件 | 出现次数 | 不同写法 | 主要职责 |
|---|---:|---:|---|
| `base.qss` | 2 | 2 | 应用基础表面 |
| `controls.qss` | 367 | 120 | 通用控件、时间轴、状态控件 |
| `popups.qss` | 10 | 7 | Toast、ToolTip、弹出层 |
| `title-bar.qss` | 43 | 23 | 标题栏、菜单、播放控制 |
| `track-editor.qss` | 31 | 26 | 轨道编辑器 |
| `clip-editor.qss` | 91 | 48 | 钢琴卷帘、参数、歌词与音素编辑 |
| `mix-console.qss` | 22 | 15 | 混音台 |
| `windows.qss` | 1 | 1 | 独立窗口 |
| `dialogs.qss` | 19 | 13 | 对话框与说话人混合 |
| `lyricwrapview.qss` | 2 | 2 | FillLyric 独立样式通道 |

## 现状归类

颜色使用可归为三类：

1. **通用 UI 语义**：表面、文本、图标、边框、控件状态、选择、反馈、阴影；
2. **编辑器领域语义**：网格、播放头、循环、钢琴键、曲线、电平、任务片段；
3. **业务调色板**：轨道、音符、说话人等 12 个可选色，继续由
   `app-color-palette.json` 管理，不进入 UI token。

`docs/theme-qproperty-checklist.md` 已覆盖 C++ 视图暴露出的颜色属性。本次 token map 将这些
属性按视觉职责归并，而不是按控件类名逐项复制。

## 迁移约束

- token 名称描述“用途”，不描述具体色相、亮度或当前值；
- primitive palette 仅供 token 引用，QSS 只能使用 `${token.name}`；
- 同一颜色值如果承担不同职责，应保留不同 token；反之，历史上略有差异但职责相同的值，
  是否合并留到视觉调色阶段决定；
- 标在 12 色业务色块之上的选择边框仍属于过渡问题，最终应结合业务色做对比度校验，不能
  仅凭深色主题中的白色决定浅色主题方案；
- 第一轮不引入自动亮度变体。所有状态色均为显式 token，避免把深色主题的变体规则错误地
  套到浅色主题。
- Phase 3 后，主题 QSS 中除 `transparent` 等与主题无关的关键字外，不再保留普通十六进制
  或 `rgb/rgba` 颜色字面量；FillLyric 的 `cell*`、`handle*`、`splitterPen` 复合值暂不在此
  规则内（Phase 4 已全部改为 `${token}` 或单值字面量）。

## 生产代码硬编码颜色审计（Phase 4）

对 `src/app` 与 `src/libs/GUI` 生产代码中的颜色字面量逐项复核，剩余结果全部落入以下类别：

### 本批次修复的主题绕过

| 位置 | 处理 |
|---|---|
| `UI/Dialogs/PackageManager/PackageItemDelegate` | 删除固定 palette；普通标题/说明取 `text.primary`/`text.secondary`，选中前景取 `selection.text`，token 无效回退 `QStyleOptionViewItem::palette`；主题变化刷新列表 viewport |
| `UI/Window/TaskWindow.cpp` | 删除局部 `setStyleSheet()`，内联边框/hover/selected 迁入两套 `windows.qss`（`border.subtle`/`control.fill.hover`/`selection.fill`） |
| `UI/Window/LogWindow.cpp` | Debug/Info/Warning/Error/Fatal 改查 `status.success/info/warning/error`；`themeChanged` 后重绘既有行 |
| `UI/Dialogs/SpeakerMix/SpeakerMixList.cpp` | 移除禁用下拉项的固定半透明白前景，改回主题 disabled palette |
| `Modules/FillLyric/{Utils,QssParser,Controls}` | 复合解析器兼容 `${token}` 与旧格式；cell/handle/splitter 与整行选中底色全部 token 化；热切换重解析并重绘既有项 |
| `MainTitleBar`/`TabPanelTitleBar`/`DialogTitleBar` | 非 Windows 分支用 `IconUtils` 按 `icon.primary`/`icon.disabled` 蒙版着色系统按钮 SVG；主题与最大化/还原变化时重建；Windows 字体字形路径不变 |

### 明确不做修改的类别

- **QRhi 控件**及其 shader/geometry/clear/overlay 颜色：由独立渲染主题工作处理；
- **AppColorPalette**、两套 `app-color-palette.json`、轨道/音符/说话人 12 色，以及基于业务色
  计算的对比前景（含 Timeline 调试叠层的四个业务分类色）；
- **构造期回退值**：`SeekBar`/`SwitchButton`/`ProgressIndicator`/`LevelMeter`/`TimelineView`
  正常状态、钢琴卷帘/参数编辑器等已在深浅 QSS 中逐项覆盖的 Q_PROPERTY 默认值，保留为
  无主题时的安全回退；
- **与主题无关**：`Qt::transparent`、字形 alpha 蒙版、图像缓存清空、`shadow.popup` 已覆盖的
  阴影默认值、未使用的 AcrylicBrush 默认参数、测试/工具/第三方/品牌图标资源；
- `DiscoverDiffScopeDialog` 的探索性样式与富文本颜色按本次修订明确不迁移。

