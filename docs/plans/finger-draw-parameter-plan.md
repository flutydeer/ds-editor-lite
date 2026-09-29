# 用手指绘制参数：单指触摸在参数编辑上的归属开关

> 状态：已实施（2026-09-29）。行为验收需真机确认，见文末「验证」。
> 配套契约：`docs/design/touch-and-pen-input-design.md` 第三节「手指只做导航」与第十节「设置项」。

## 目标

在通用页（`GeneralPage`）新增一张「笔和触控」卡片与其中的开关**用手指绘制参数**（`general.drawParamWithFinger`，默认关），把"手指能不能画曲线"从设计里写死的"能"改成用户可选项：

- **关闭**（默认）：参数面板（`ParamEditorGraphicsView`）与钢琴卷帘的五个音高工具把手指降级为纯导航指针——拖动平移、轻点不落点、长按不动仍出菜单。
- **打开**：维持既有行为，单指与鼠标一致，按当前工具执行。

笔与鼠标任何情况下都不受影响；双指导航一行不改。

## UI 文案（定稿）

| 位置 | 源串（英文，`tr()`） | 中文（`translation_zh_CN.ts`） |
| --- | --- | --- |
| 卡片标题 | `Pen and Touch` | 笔和触控 |
| 开关标题 | `Draw parameters with finger` | 用手指绘制参数 |
| 开关描述 | `When off, a finger on the parameter panel or the piano roll's pitch tools only scrolls the timeline. Pen and mouse are unaffected.` | 关闭时，单指在参数面板与钢琴卷帘的音高工具上只滚动时间轴；笔和鼠标不受影响。 |

描述只保留"适用范围 + 关闭后果 + 笔鼠不受影响"，不再复述"参数面板始终滚动""音高工具不再绘制"这两处开关名已经说清的内容。

## 设计决策

| 项 | 决策 |
| --- | --- |
| 设置归属 | `GeneralOption`（`appConfig.json` 的 `general.drawParamWithFinger`），卡片挂在通用页 |
| 默认值 | `false`（关）。旧配置没有这个键，读到的是默认值 |
| 参数编辑器范围 | 仅参数曲线编辑。`m_speakerMixMode`（说话人混音）不覆盖，那是另一套编辑面 |
| 钢琴卷帘范围 | 仅五个音高工具（`DrawPitch` / `EditPitchAnchor` / `ErasePitch` / `TracePitch` / `ModulatePitch`，取现成分类器 `EditorViewGlobal::isPitchEditMode()`）。音符类工具与 `IntervalSelect` 不变 |
| 轻点 | 关闭时**什么也不做**。这两处没有"先点选再拖动"的需求，轻点直通交互层就只是一次落点/锚点编辑 |
| 长按 | 关闭时仍是"不动=菜单、动了=平移"，与 `Pan` 领地同一条规则 |
| 参数面板水平平移 | **转发给钢琴卷帘**执行（参数面板的水平位置本来就归钢琴卷帘所有），参数面板再跟随；转发增量而非值 |
| 读取方式 | 每手势一次直读单例，不设缓存。进行中的手势保持起手时锁定的模式，下一次手势生效 |
| 双指 | 导航几何与 `dispatch` 的 `Navigation*` 分支零改动 |

## 为什么不能只改 `touchBlankDragAction()`

这是本次唯一有分量的实现结论，三处都会绕过它：

1. `EditorTouchController::onSingleBegin()` 里 **`tap` 与 `fromLongPress` 本来就跳过该查询**，直接走合成鼠标流。只改它，关闭后"轻点仍会落一个参数点""长按后拖动仍会画一条线"。而 `fromLongPress` 必须是合成鼠标是**刻意设计**——钢琴卷帘 Select 模式下长按是唯一能起框选的手势（设计文档第三节），不能一刀切。
2. 钢琴卷帘在**显式工具**下，`touchContentAt()` 对任何落在音符上的位置返回 `Selected`，而 `touchContentAt()` 返回 `Selected` 时同样不查 `touchBlankDragAction()`。
3. 参数编辑器 `touchContentAt()` 恒为 `None`，这一处倒是查得到，但只有它一处生效。

因此新增了更强的判据 `EditorTouchTarget::touchFingerEdits()`（默认 `true`），只有它为 `false` 才把**整条单指流**降级为导航；并把 `onSingleBegin()` 的分流抽成纯函数 `EditorTouchTarget::fingerStreamFor()`（五值决策表），让"旧策略"与"关闭策略"能被同一张表无头断言。

## 落地形态

| 文件 | 改动 |
| --- | --- |
| `src/app/UI/Views/Common/EditorTouchTarget.h` | `touchFingerEdits()`；`FingerStream` / `FingerContext` / `fingerStreamFor()`（pure，inline） |
| `src/app/UI/Views/Common/EditorTouchController.cpp` | `onSingleBegin()` 改用决策表；`onSingleMove()` 的补发按下加 `m_syntheticStreamActive` 守卫；`onSingleEnd()` 的 deferred 分支清 `m_panStreamActive`；`onSingleCancel()` 的 pan 分支清 `m_pressDeferred` |
| `src/app/UI/Views/ClipEditor/ParamEditor/ParamEditorGraphicsView.{h,cpp}` | `fingerEditingEnabled()`（单一真相）、`touchFingerEdits()`、`panTouchViewportBy()` 覆盖、信号 `horizontalPanRequested(double)` |
| `src/app/UI/Views/ClipEditor/PianoRollEditorView.cpp` | 把 `horizontalPanRequested` 增量转发给钢琴卷帘 |
| `src/app/UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.{h,cpp}` | `touchFingerEdits()`：非音高模式返回 `true` |
| `src/app/UI/Views/ClipEditor/PianoRoll/PianoRollRhiWidget.{h,cpp}` | 同上（RHI 后端） |
| `src/app/Model/AppOptions/Options/GeneralOption.{h,cpp}` | `LITE_OPTION_ITEM(bool, drawParamWithFinger, false)` + load/save |
| `src/app/Automation/SettingsAutomationFacade.h`、`AppOptionsAutomationAdapter.cpp` | DTO 字段与 capture/restore 映射（漏了设置不落盘） |
| `src/app/UI/Dialogs/Options/Pages/GeneralPage.{h,cpp}` | 「笔和触控」卡片 + `SwitchButton` |
| `src/app/Resources/translate/translation_zh_CN.ts` | 三条中文本地化 |
| `src/tests/TestTouchGestures/` | `fingerStreamFor` 真值表用例；`CMakeLists.txt` 补 `EditorTouchTarget.cpp` |

三处标志位补丁不是可选项：新出现的 `pan + deferred` 组合（关闭态长按）会暴露两个既有缺陷——

1. `onSingleEnd()` 的 deferred 分支原先不碰 `m_panStreamActive`，一旦它与 `m_pressDeferred` 同时为真，抬手后 pan 标志会留下，而 `touchOwnsContextMenu()` 见到它为真就恒返回 `true`，**真实鼠标右键会被一直吞掉**（远超 600 ms 归因窗口）。
2. `onSingleCancel()` 的 pan 分支原先不清 `m_pressDeferred`，双指打断一次长按会留下"欠一次菜单"的标志。

## 行为对照

| 手指动作 | 关闭 | 打开（=改动前） |
| --- | --- | --- |
| 参数面板：拖动 | 平移视口（水平转发钢琴卷帘，垂直由自己处理） | 当前工具 |
| 参数面板：轻点 / 双击 | 无动作 | 合成 press(+dbl)+release |
| 参数面板：长按不动 / 长按后拖动 | 菜单 / 平移 | 合成按下（绘制/框选） |
| 钢琴卷帘（音高工具）：拖动 / 轻点 | 平移视口 / 无动作 | 画、擦、描、拖锚点、调制 |
| 钢琴卷帘（`Select`、音符工具、区间选择） | 不变 | 不变 |
| 双指 | 不变 | 不变 |
| 笔 / 鼠标 | 不变 | 不变 |

## 验证

离线（已做）：

- `ctest -R TestTouchGestures` 覆盖 `fingerStreamFor` 真值表：旧策略（可编辑）与关闭策略各一组，含"钢琴卷帘音高工具把落点报成 `Selected` 也必须改道"这一条。
- 编译通过（`build/Debug`，`DsEditorLite` + `TestTouchGestures`）。

真机（待做）：

1. 关闭：参数面板单指任意位置拖动都滚时间轴，**曲线与钢琴卷帘音符不错位**；Ctrl+滚轮放大到内容超宽后仍能滚到结尾。
2. 关闭：轻点不改参数；长按不动出菜单；长按后拖动是平移而不是画线。
3. 关闭：真实鼠标右键不被吞；开关切换后**下一次**手势生效，手势进行中切换不打断当前手势。
4. 打开：参数编辑器六种工具（绘制/擦除/烘焙/形态/缩放/锚点）与改动前逐项一致。
5. 关闭：钢琴卷帘五个音高工具逐一确认只平移、不绘制；音符类工具与区间选择不受影响；**两套后端都过**（开发者选项切 Legacy / Experimental）。
6. 笔（含音高模式）与说话人混音模式不受影响；轨道编排区与双指缩放/平移回归一致。
