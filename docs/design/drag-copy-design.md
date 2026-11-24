# 拖动复制（Ctrl 拖出副本）

## 背景

剪辑与音符的拖动只有「移动」一种语义，想要副本必须先复制再粘贴。办公软件的习惯做法是按住 Ctrl 拖动：原件留在原位，指针下方画一个半透明的副本，松开就落在那里。本文描述轨道窗剪辑与钢琴卷帘音符的这套复制拖动，覆盖 `Legacy`（默认，QGraphics）与 `RhiExperimental` 两个渲染后端。

## 行为定义

### 手势

- **Ctrl 在按下之前或拖动中途按下**都算复制，松开 Ctrl 立即退回移动。Ctrl 点按是加选 / 减选，但减选的作用点被推迟到松手：按下时目标仍留在选择集里，因此按住 Ctrl 再按下并拖动可以直接复制，只有「按下后原地松手」才把目标移出选择集。
- 复制期间原件**完全不动**：几何、轨道、颜色、底部面板都停在手势开始时的地方。指针下方是一个半透明副本，样式与粘贴预览一致（0.35 不透明度，不参与命中测试、不可选、无标题改名）。
- 松开鼠标提交一次复制，选择集随即换成新副本，`activeClipId` 指向副本。下一次拖动抓的是副本，而不是原件。
- Esc、窗口失活、面板隐藏、触摸取消都走丢弃路径：原件不动，副本预览消失，模型不变。
- 复制与 Alt 临时关闭量化正交：Ctrl+Alt 拖动会把副本停在未吸附的位置。

### 轨道窗

- 复制拖动整块选择集：每个剪辑落到「当前指针的相对轨道偏移 + 吸附后的 tick 偏移」处，与移动拖动共用同一份 `ClipSelectionDrag::Session` 计划结果。
- 复制不新建轨道：指针越到最后一条轨道下方时，目标轨道钳制在最后一条真实轨道，虚拟追加行不是复制目标。
- 副本落在其它轨道时沿用目标轨道的颜色，落在原轨道时保持原色。
- Ctrl 期间原件所在轨道不再显示「活动剪辑轨道色」预览，因为原件没有离开它。

### 钢琴卷帘

- 复制拖动作用于 `selectedNotes`，delta 与移动拖动完全一致（同一个吸附步长、同一套键位钳制、同一个左沿钳制），只是不写回 NoteView。
- 音符副本跟随歌词与音素，和粘贴预览一样带发音提示。
- Ctrl 期间不出现缩放把手帧：那副把手承诺的是「改变原件边界」。

### 轨道缩略图

`AppStatus::pianoRollNoteEditPreview` 在复制期间发布负数 id 的临时条目，轨道侧缩略图因此显示副本会落在哪里，而原件保持静止。

## 实现

### 命令层

两条自动化入口各自把一份手势计划变成一次事务，历史记录里只留一条：

- `ProjectAutomationFacade::duplicateClipsAt(context, targets)`：每个目标携带源剪辑 id、可选目标轨道与目标几何。`ClipDuplicateTargetDto` 逐条校验，`duplicateClipDraft` 深拷贝源剪辑后再覆盖 `start / length / clipStart / clipLen / trimStartMs / playLengthMs / materialLengthMs`，同时刷新 `ownSpeakerMixData` 关键帧 id 与参数曲线节点 id，因此副本拥有自己的音符、参数、声线与音频素材。落地走 `ClipActions::insertClips`，一次提交。
- `NoteAutomationFacade::duplicateNotes(context, sourceClipId, noteIds, targetClipId, targetStart, deltaKey)`：在原有 `targetStart` 平移的基础上增加 `deltaKey` 转调，键位越界时整条命令以 `delta_key` 拒绝且不写入。音符 id 指纹包含 `deltaKey`，重试同一手势不会产生第二份副本。

控制器入口把这些接到视图：`TrackController::onDuplicateClips` 返回按 `edits` 顺序排列的新剪辑 id，`ClipController::onDuplicateNotes` 返回新音符 id 并触发插入后的显影。

### 修饰键检测

Qt 只把键盘事件交给焦点控件，而拖动期间画布未必持焦点。`EditorModifierWatcher` 因此挂在 `qApp` 上过滤 Control / Shift / Alt / Meta 的按下与松开，只观察不吞事件，回调里带上按下的键算出的修饰键状态。两个编辑器在拖动开始时启用它、结束或丢弃时关闭，回调把最后一次已知的指针位置交给常规的拖动更新函数重算预览。指针因自动滚动而重绘时，改用这次缓存下来的修饰键状态，不再查询键盘。

## 验证

- 命令层：`TestProjectEditing` 覆盖多目标复制（跨轨、实时锚点毫秒真值、id 不共享、一次撤销）与 `deltaKey` 转调、越界拒绝。
- 轨道窗：`trackCtrlDragDuplicatesTheSelection`（Legacy）与 `rhiCtrlDragCopiesTheClipAndSelectsTheCopy`（RHI，用真实 Ctrl 键事件覆盖修饰键监听）验证原件不动、副本落点、选择集换成副本、一次撤销。
- 钢琴卷帘：`ctrlDraggingANoteCopiesTheSelectionAndLeavesTheSources`（Legacy）与 `rhiCtrlDraggingANoteCopiesTheSelection`（RHI）验证同一组契约与缩略图预览条目。

## 非目标

- 不提供 Alt 拖动复制。Alt 已被「临时关闭量化」占用，两个手势会互相冲突。
- 复制不新建轨道，也没有「拖到追加行即复制到新轨道」。
- 不支持复制锚点、参数曲线片段或整条轨道。
