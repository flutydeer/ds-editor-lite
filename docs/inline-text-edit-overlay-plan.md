# QWidget 覆盖层就地编辑 — 行为约定

> 状态：✅ 六个实施阶段全部完成（2026-07 各阶段独立提交并通过 Debug 构建；旧 `EditLabel/EditDialog` popup 实现已删除）
> 本文只保留长期有效的**固定行为约定**；实施过程记录已移除。

## 背景

编辑器内所有就地编辑（轨道名、剪辑名、钢琴卷帘歌词/发音、速度/拍号/播放位置/声像/增益数值入口）统一由 `InlineTextEditOverlay`（普通子 `QWidget` 覆盖层 + 专用 `QLineEdit`）+ `InlineEditLabel` 实现，不使用顶层窗口标志、不使用 `QGraphicsProxyWidget`。

## 固定行为约定

- 编辑器内部文本操作、输入法和自身右键菜单**不属于**"外部事件"。
- 滚动、缩放、窗口或面板尺寸变化、选择/工具/数据上下文变化、目标移动或删除、窗口失活都属于**外部事件**：立即提交并退出编辑。
- 普通重绘和 QSS 刷新不触发提交。
- 目标已失效且无法提交时安全取消。
- 提交前执行 `trimmed()`；无变化不提交；一次有效修改只产生一次 action。
- **轨道名和剪辑名允许为空**；空歌词恢复语言默认歌词；空发音恢复原始发音。
- 非法或不完整输入关闭覆盖层并保持当前模型值，不转换为 `0`，不发送业务修改。
- Enter 提交、Esc 取消、FocusOut 提交、重复结束保护（`m_submitted`）。
- 编辑会话绑定开始编辑时的目标 id（如 clip id / note id）；切换活动剪辑和外部属性刷新前先提交，禁止写入错误目标。
- 数值入口验证规则：速度仅接受有限正数；拍号接受正整数 `分子/分母` 且分母为 2 的幂；播放位置验证合法 `小节:拍:tick` 范围；声像支持 `Lxx/Rxx/C` 和数值百分比；增益支持有限数值及 `-∞`，提交后沿用滑块范围截断并规范化显示。

## 关键实现

- `InlineTextEditOverlay` / `InlineEditLabel`：`src/app/UI/Controls/`
- 钢琴卷帘共享覆盖层宿主：`PianoRollGraphicsView`（viewport 覆盖编辑器 + 稳定目标 note id）
- 编辑态 QSS：`clip-editor.qss` 等主题文件中的 `InlineEditLabel` / overlay 角色样式
- FillLyric 模块自己的同名 `EditLabel` 控件保留，不在统一范围内
