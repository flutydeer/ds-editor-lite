# 编辑器 Experimental QRhi 渲染后端

TrackEditor 与 PianoRoll 提供默认的 Legacy（QGraphicsView）后端和实验性的 QRhi
后端。渲染器在进程启动时从“首选项 → 开发者 → Experimental → Editor
renderer”读取；修改选项只会保存设置并显示重启提示，不会在运行期热切换。

## 边界与回退

- 外层视图只依赖 `ITrackEditorCanvas`、`IPianoRollCanvas` 和纯值 viewport/snapshot
  类型。Legacy scene/item 与 QRhi 资源分别封装在各自 adapter 中。
- 工厂每次只创建所选后端。QRhi 初始化或渲染失败后，宿主先捕获 viewport，销毁失败
  canvas/widget，再创建 Legacy adapter；data context、selection 和 viewport 从模型与纯值
  状态恢复，不保留隐藏的 QGraphicsScene。
- Windows QRhi canvas 在加入窗口层级前固定选择 Direct3D 11。初始化日志记录 Qt
  版本、QRhi backend、adapter、vendor/device ID 和 resource generation。

可使用以下环境变量执行诊断：

- `DS_EDITOR_RHI_FAIL=initialize`：注入初始化失败。
- `DS_EDITOR_RHI_FAIL=render`：注入首帧渲染失败。
- `DS_EDITOR_RHI_MSAA=4`：以 4x MSAA 作为解析抗锯齿的对照；其他值使用 1x。

## 资源与性能指标

`RhiEditorCanvasWidget` 在 `initialize()`、`releaseResources()`、render target/QRhi
变化、resize 和 DPR 变化时重建对应资源。camera-only 滚动复用 snapshot 与几何
buffer，仅更新 camera uniform；数据、样式、文本、波形和 overlay 由 dirty domain
合并到下一次事件循环。

日志每 120 帧输出以下指标：

- frame interval、update 到 `frameSubmitted`；
- snapshot build、stroke tessellation、command encoding；
- draw calls、vertices、upload bytes；
- glyph atlas hits/misses；
- dynamic/cached frame 数。

性能结论只应取 Release、本地显示器。远程桌面结果必须单独标注，不得与本地基准混合。

## Qt private API 风险

实现使用 Qt Gui/Widgets private RHI API、`QRhiWidget`、ShaderTools 和构建期 qsb。
这些接口不提供补丁版本间的 ABI/API 稳定保证，因此发布环境必须使用 Qt 6.11.1
或更高的同系列补丁版本，并用该版本重新完成 Debug/Release 构建及 GUI 验证。升级 Qt
时必须重点检查：

- D3D11 初始化、render target/QRhi 变化与 `releaseResources()`；
- 最小化/恢复、最大化、窗口 resize 和 100%/非 100% DPR；
- glyph shaping、fallback font、atlas eviction 与 DPR 后重建；
- 急转弯、短段、断点、miter/bevel/round join、butt/round cap 的像素对齐；
- initialize/render 失败注入后的单次、可重入 Legacy 回退。

验收时应分别执行针对改动的 Legacy/RHI 对照和基本功能回归冒烟测试，不把两类门合并。
