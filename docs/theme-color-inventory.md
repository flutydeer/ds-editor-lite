# 主题颜色基线盘点

> 状态：迁移前基线，统计对象为 `src/app/Resources/theme/lite-dark/*.qss`。数字保留为历史
> 快照，用于衡量迁移规模。Phase 3 已将领域 QSS 中的普通颜色和 `qproperty-*` 颜色属性
> 迁移到 semantic token；FillLyric 的复合字符串属性仍由独立 QssParser 管理。

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
  规则内。
