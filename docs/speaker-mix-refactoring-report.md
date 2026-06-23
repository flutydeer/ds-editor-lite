# 声线混合功能重构建议报告（调整版）

> 分析范围：SpeakerMix 全链路 — 数据模型、Action/Undo 系统、UI 编辑视图、推理管线、序列化。
>
> 2026-06-24 | 基于对 80+ 文件、15+ 核心模块的代码审查

---

## P0 — 数据完整性问题

### 1. SingerSourceMode 语义过载

**问题**：3 个枚举值表达 4 种语义状态，导致条件组合散布。

| 实际状态 | `mode` 值 | `dynamicKeyframes` | 判定位置数 |
|---------|-----------|-------------------|-----------|
| 纯 FixedMix | `FixedMix` | 空 | — |
| 纯 DynamicMix | `DynamicMix` | 非空 | — |
| Dynamic Bypassed | `FixedMix` | 非空 | ~5 处 |
| Single（降级） | `Single` | 空 | — |

各处判定条件如 `mode == FixedMix && !dynamicKeyframes.isEmpty()` 反复出现（Dialog、EditorView、ParamEditorView、`normalizeSpeakerMixData`、序列化），语义不自描述。

**建议**：新增正交字段 `dynamicBypassed`：

```cpp
struct SpeakerMixData {
    SingerSourceMode mode = SingerSourceMode::Single;
    bool dynamicBypassed = false;  // 新增
    // 约束: 只在 mode == DynamicMix 时有意义
    // bypassed = true 时生效使用 fixedWeights，但 keyframes 保留
};
```

改动后 `mode` 表达纯粹的模式语义，`dynamicBypassed` 表达旁路状态。旁路时 EditorView 仍显示关键帧但为只态（或编辑时自动解除旁路）。

---

### 2. SpeakerMixEditorView 数据架构：双重副本是正确的，但实现需要清理

> 此条已根据讨论调整。钢琴卷帘音符编辑模式（`PianoRollEditHandler`）提供了标准的 commit/discard 模式参考。

**现状**：EditorView 内部有三份重叠数据：

```cpp
SpeakerMixData m_data;                       // 模型快照 + 编辑数据混合
QList<SpeakerMixSpeaker> m_speakers;         // 从 referenceSpeakers 派生
QList<SpeakerMixKeyframe> m_keyframes;       // 编辑工作副本

// setSpeakerMixData → m_data = normalize(data); syncFromData()
//   → syncFromData 将 m_data.dynamicKeyframes 复制到 m_keyframes
//
// speakerMixData() → 从 m_data（固定数据）+ m_keyframes（编辑数据）拼凑返回值
//   → toList() / toVector() 类型转换
//   → 再做一次 normalize
```

**问题**不是"有两个副本"——钢琴卷帘的 `EditPitchAnchorHandler` 也有 `m_localCurves` 作为本地工作副本——而是：

1. **`m_data` 职责混乱**：既做模型快照又部分做编辑副本，`m_data.dynamicKeyframes` 在 `syncFromData()` 之后变成垃圾数据（被 `m_keyframes` 取代）
2. **没有 commit/discard 边界**：每次编辑直接发射 `speakerMixEdited` → 写模型，无法按 Escape 丢弃
3. **类型转换摩擦**：`QList<double>` ↔ `QVector<double>` 反复转换

**建议**：参考 `EditPitchAnchorHandler` 模式，重构为：

```cpp
class SpeakerMixEditorView : public TimeOverlayView {
public:
    void commit();           // 写本地工作到模型
    void discard();          // 从 committed 快照恢复
    void syncFromModel();    // loadFromModel 等价物
    
    // 当前编辑状态的只读查询
    SpeakerMixModel::SpeakerMixData workingMixData() const;
    
    void keyPressEvent() override {
        if (event->key() == Qt::Key_Escape) { discard(); return; }
    }

private:
    // 两份清晰分离的数据源
    SpeakerMixModel::SpeakerMixData m_committedData;  // 模型快照（discard 目标 / commit 后更新）
    // 没有 m_data —— m_committedData 取而代之
    QList<SpeakerMixKeyframe> m_keyframes;  // 仅动态关键帧的编辑工作副本
    QList<SpeakerMixSpeaker> m_speakers;    // 派生显示数据
    
    // syncFromModel: m_keyframes ← m_committedData.dynamicKeyframes（全量替换·重置）
    // speakerMixData(): 返回 m_committedData（与模型一致·纯查询）
    // workingMixData(): m_committedData 但 dynamicKeyframes 替换为 m_keyframes
    
    // commit():
    //   1. auto data = workingMixData()
    //   2. emit speakerMixEdited(data)  // 外部的 ParamEditorView 处理 action
    //   3. m_committedData = normalizeSpeakerMixData(data)
    //   4. endActiveTransaction(Commit)
    //   5. appStatus->currentEditObject = None
    
    // discard():
    //   1. m_keyframes ← m_committedData.dynamicKeyframes（重新 sync）
    //   2. clearInteractionState()
    //   3. update()
    //   4. endActiveTransaction(Discard)
    //   5. appStatus->currentEditObject = None
};
```

配套修改 `ParamEditorView`：

```cpp
// 当前：onSpeakerMixEdited 直接调用 action
// 改为：
void ParamEditorView::onSpeakerMixEdited(SpeakerMixData data) {
    // 只做幂等判断，不执行 action
    // 实际 action 由 commitSpeakerMixEdit() 执行
    commitSpeakerMixEdit(data);
}

void ParamEditorView::commitSpeakerMixEdit() {
    auto data = mixView->workingMixData();
    auto actions = new SpeakerMixActions;
    actions->replaceSpeakerMix(data, m_clip);
    actions->execute();
    historyManager->record(actions);
}

void ParamEditorView::discardSpeakerMixEdit() {
    mixView->discard();
}
```

---

## P1 — 架构耦合

### 3. `useTrackSingerInfo` 职责膨胀

**现状**：一个布尔值控制 singer/speaker/speaker mix 三个正交属性是否跟随轨道。

```cpp
Property<bool> useTrackSingerInfo{true};
Property<bool> useTrackSpeakerInfo{true};  // 已死字段，总是 == useTrackSingerInfo
```

`SingingClip::setSpeakerMixData()` 第 274-279 行：
```cpp
void SingingClip::setSpeakerMixData(const SpeakerMixData &data) {
    if (useTrackSingerInfo)
        return;  // 静默失败
    setOwnSpeakerMixData(data);
}
```

无法表达"跟随 singer/speaker 但自定义 mix"或"自定义 singer 但跟随 track mix"的组合。`useTrackSpeakerInfo` 完全多余。

**建议**：

1. **删除 `useTrackSpeakerInfo`**
2. **`useTrackSingerInfo = true` 语义明确为"三合一全跟随"**；关闭后 singer/speaker/mix 各自独立
3. `setSpeakerMixData()` 中 `useTrackSingerInfo` 为 true 时，要么 assert 报错，要么自动 `useTrackSingerInfo = false`（类似 DAW 中编辑 automation 时自动退出 Follow）

---

### 4. Action 类爆炸与重复

**现状**：5 个 Action，其中 3 个做本质相同的事，代码级重复。

| Action | 保存状态 | execute | 与 Replace 的区别 |
|--------|---------|---------|-----------------|
| `ReplaceSpeakerMixAction` | `m_oldData` | `clip->setSpeakerMixData(new)` | 基线 |
| `ReplaceTrackSpeakerMixAction` | `m_oldData` | `track->setSpeakerMixData(new)` | 不同目标 + `preservePresetSourceAsDirty` 重复 |
| `ApplyClipSpeakerMixPresetAction` | `oldUseTrack + ownSinger + ownSpeaker + ownMixData` | `setOwnSingerAndSpeaker + setOwnMixData` | 额外设 singer/speaker |
| `ApplyTrackSpeakerMixPresetAction` | `oldSinger + oldSpeaker + oldMix` | `setSingerAndSpeakerInfo + setMixData` | 同上，目标为 track |
| `EnableClipDynamicSpeakerMixAction` | 同 ApplyClip | 同 ApplyClip | **与 ApplyClip 完全相同** |

`ApplyClipSpeakerMixPresetAction` 和 `EnableClipDynamicSpeakerMixAction` 的构造函数、execute、undo **逐行相同**。

**建议**：

1. `preservePresetSourceAsDirty` 逻辑提到 `SpeakerMixData` 成员函数：

```cpp
SpeakerMixData SpeakerMixData::withPresetSourceAsDirty(const SpeakerMixData &oldData) const;
```

2. 合并 `ApplyClipSpeakerMixPresetAction` 和 `EnableClipDynamicSpeakerMixAction` 为 `SetClipSingerAndSpeakerMixAction`，用 flag 或不同工厂方法区分

3. Track 版同理合并

---

## P2 — 代码质量与可维护性

### 5. `normalizeSpeakerMixData` 职责过多

当前函数同时做：preset metadata 清洗 + 数据验证 + 权重规范化 + keyframe 排序 + 无效数据降级为空对象。

**问题**：
- 降级为空的语义隐式表达，调用方需要再次 normalize 才能判断
- 副作用（排序）混在查询函数中
- 被广泛调用（setter/getter/action 构造/序列化），每次做完整验证

**建议**：

```cpp
// 拆分职责
bool validateSpeakerMixData(const SpeakerMixData &data, QString *error = nullptr);
SpeakerMixData normalizeSpeakerMixData(const SpeakerMixData &data);
// 只做规范化 + 排序，不静默降级
// 调用方自行用 isSpeakerMixDataSingle() 判断
```

---

### 6. `Track::setSingerAndSpeakerInfo` 混淆了“普通 setter”和“选择单 speaker”的语义

```cpp
void Track::setSingerAndSpeakerInfo(...) {
    if (!singerInfoChanged && !speakerInfoChanged) {
        if (!isSpeakerMixDataSingle(m_speakerMixData))
            resetSpeakerMixToSingle();
        return;
    }
    // ...
    resetSpeakerMixToSingle();
}
```

如果把 `setSingerAndSpeakerInfo` 理解为普通 setter，那么 singer/speaker 都没变时应该幂等，不应重置 mix。

但当前 UI 还把“用户选择单 speaker”也复用了这条路径。此时即使 singer/speaker id 与当前 Fixed Mix
的 fallback speaker 相同，用户意图仍然是从 `FixedMix` / `DynamicMix` 切回 `Single`，所以 reset mix
是正确行为。之前 preset -> single 不触发推理的问题正是因为这层语义没有表达出来。

`SingingClip::setOwnSingerAndSpeaker()` 有同样问题：

```cpp
} else if (!isSpeakerMixDataSingle(m_ownSpeakerMixData)) {
    resetSpeakerMixToSingle();
}
```

**建议**：

不要简单删除 reset，而是拆清楚 API 语义：

1. 保留一个纯 setter / sync 路径，用于内部状态同步；此路径在 singer/speaker 没变时保持幂等。
2. 新增显式“选择单 speaker”或“设置 voice context”的路径，例如：

```cpp
void Track::selectSingleSpeaker(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo);
void SingingClip::selectOwnSingleSpeaker(const SingerInfo &singerInfo,
                                         const SpeakerInfo &speakerInfo);
```

3. 选择 single speaker 的路径必须把 speaker mix 重置为 `Single`，并根据 old/new effective voice context
统一发出变更事件。
4. Phase 2 前建议进一步收敛为 `EffectiveVoiceContext` mutation，避免继续依赖
`singerOrSpeakerChanged` / `speakerMixChanged` 的发射顺序。

---

### 7. `fixedSpeakerMixFromData` 对 DynamicMix 的处理不透明

`fixedSpeakerMixFromData()` 在 `DynamicMix` 模式下用 `fixedWeights` 或第一帧 keyframe 权重作为静态快照传给推理。时间维度的关键帧变化信息完全丢失。

这不是一个需要立即修复的问题，但需要**文档化**：

- `InferControllerPrivate::handleSpeakerMixChanged()` 中标注当前限制
- 当 DynamicMix 被底层引擎支持后，`fixedSpeakerMixFromData` 应改为 `dynamicSpeakerMixFromData`
- Duration 任务当前完全忽略 speaker mix（`staticSpeakerMix(piece.speaker)`），这是正确的（时长不受声线影响）

---

## 已撤回的条目

### ~~#7（原版）：`handleSpeakerMixChanged` 过于激进~~

**撤回原因**：数据分层理解有误。

- 用户编辑曲线存储在 `clip.params.*.curves(Edited)`，`InferPiece` 上的 `originalPitch`/`inputPitch` 只是缓存
- `resetPitch` 只清除缓存，不动用户编辑
- 声线混合确实影响 Pitch/Variance/Acoustic 所有阶段，全级联重推是正确行为
- Duration 不受声线影响，明确使用 `staticSpeakerMix`

当前实现 ✅：

```cpp
// handleSpeakerMixChanged 的行为
piece->speakerMix = speakerMix;           // 1. 更新 mix 数据
resetPitch(piece, cascadeReset=true);     // 2. 清除陈旧缓存（不损失用户编辑）
pipeline->onExpressivenessChanged();      // 3. 从 pitch 阶段重推
```

---

## P3 — 基础设施

### 8. 零测试覆盖

整个 speaker mix 模块没有任何单元测试。权重计算（N-1 编码/解码、clamp、归一化、增删 speaker 时的权重重分配）的数学正确性完全靠人工验证。`normalizeSpeakerMixData` 的复杂分支逻辑不可回归。

**建议**：为 `SpeakerMixData` 的纯粹数据层写单元测试：

- `normalizeSpeakerMixData`: 正常数据 / 无效 source / 权重长度不匹配 / 空数据 / Dynamic 无 keyframe
- `explicitWeightsFromFullWeights` ↔ `fullWeightsFromExplicitWeights` 互转一致性
- `normalizeSpeakerMixFullWeights`: 总和=1 / 总和=0（均分）/ clamp 到 [0,1]
- 序列化/反序列化往返测试

---

## 优先级总表

| 优先级 | 条目 | 改动量 | 影响范围 | 关键文件 |
|--------|------|--------|---------|---------|
| **P0** | SingerSourceMode + bypass 正交化 | 中等 (~10 files) | Dialog/Editor/ParamEditor/序列化/推理 | `SpeakerMixData.h/.cpp`, `ParamEditorView.cpp` |
| **P0** | EditorView commit/discard 模式 | 大 (~5 files) | Editor 交互+父级集成 | `SpeakerMixEditorView.cpp/.h`, `ParamEditorView.cpp` |
| **P1** | `useTrackSingerInfo` 清理 + `useTrackSpeakerInfo` 删除 | 小 (~5 files) | SingingClip/Actions/Controller | `SingingClip.h/.cpp` |
| **P1** | Action 合并消重 | 中 (~10 files) | Undo 系统 | `SpeakerMixActions.cpp`, 所有 Action 文件 |
| **P2** | `normalizeSpeakerMixData` 拆分 | 中 (~8 files) | 所有调用方 | `SpeakerMixData.cpp` |
| **P2** | Track/Clip 单 speaker 选择语义收敛 | 小-中（建议与 voice context 一起做） | Track/SingingClip/Actions | `Track.cpp`, `SingingClip.cpp` |
| **P2** | DynamicMix 推理路径文档化 | 仅文档 | — | `InferSpeakerMix.cpp`, `InferController.cpp` |
| **P3** | 单元测试 | 取决于选型 | — | `src/tests/` |

---

## 关键设计原则（当前已确认正确的）

以下设计经审查确认合理，重构时应保持：

1. ✅ **权重存储 N-1**，UI 显示 N — 与 opendspx `ratio` 对齐
2. ✅ **apply preset = 复制而非引用** — track/clip 修改后显示 dirty 状态
3. ✅ **预设只保存 fixedWeights，不保存 keyframes**
4. ✅ **颜色不进入数据模型** — 仅 UI 层通过 `SpeakerMixColorResolver` 派生
5. ✅ **`fixedWeights` 与 `dynamicKeyframes` 可同时保留** — 支持 Bypass
6. ✅ **`handleSpeakerMixChanged` 全级联重推** — 声线混合影响 pitch/variance/acoustic
7. ✅ **Duration 任务使用静态回退** — 时长不受声线影响
