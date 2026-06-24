# 声线混合重构交接报告（v3）

> 分析范围：SpeakerMix 全链路，包括数据模型、Action/Undo、Track/Clip voice context、
> UI 编辑视图、推理管线、序列化与测试。
>
> 更新时间：2026-06-25
>
> 本文档用于在新对话中直接继续声线混合重构。当前代码已经完成 Fixed Mix、Dynamic Mix
> 初步接入和推理链路，后续工作不应再从“接入动态推理”开始。
>
> 发布状态：程序尚未公开发布，不需要兼容旧版工程文件。后续重构可以直接调整
> workspace schema、删除临时字段、移除旧组合状态；当前开发期测试数据如有需要，可手动修复或重新生成。

---

## 当前基线

### 已完成

1. **Fixed Mix 已接入**
   - Track 和 SingingClip 都持有 `SpeakerMixData`。
   - 固定混合预设、应用预设、dirty 元数据、工程保存恢复已接入。
   - 选择单 speaker 会把 speaker mix 重置为 `Single`。

2. **Dynamic Mix 已初步可用**
   - `SpeakerMixEditorView` 已绑定真实 `SpeakerMixData`。
   - 关键帧编辑通过 action 写回模型。
   - Dynamic Active / Bypassed / Stop Dynamic 基本流程已接入。
   - 已修复顶部边缘分割点不可再次拖下来的 hit-test 问题。
   - hit-test 已按视觉 z 序处理：时间靠后的关键帧优先，speaker index 较大的分割点优先。

3. **动态推理已接入**
   - Duration 继续使用静态/fallback speaker mix。
   - Pitch / Variance / Acoustic 使用 `effectiveSpeakerMixFromData()`。
   - `dynamicSpeakerMixFromData()` 会根据 piece tick 范围和 timeline 将关键帧采样成
     `InferSpeakerMix` 的逐帧 proportions。
   - Dynamic Bypassed 和 Stop Dynamic 会回到 Fixed Mix 推理。

4. **voice context 已初步收敛**
   - 已引入 `EffectiveVoiceContext`。
   - `Track` 已提供 `voiceContext()`、`setVoiceContext()`、`selectSingleSpeaker()`。
   - `SingingClip` 已提供 `effectiveVoiceContext()`、`setOwnVoiceContext()`、
     `setTrackVoiceContext()`、`selectOwnSingleSpeaker()`、`useTrackVoiceContext()`。
   - `useTrackSpeakerInfo` 仍留在代码里，但已不需要为了旧版工程兼容继续保留。

5. **临时 Unknown 语义已清理**
   - `ParamInfo::SpeakerMix` 已存在。
   - ParamEditor 不再依赖 `ParamInfo::Unknown` 表示 Speaker Mix。

6. **Action/Undo 已收敛第一轮**
   - Speaker Mix 底层 undo 已统一到 `SetClipVoiceContextAction` 和
     `SetTrackVoiceContextAction`。
   - `SpeakerMixActions` 继续保留“应用预设”“启用动态混合”“编辑 mix”等业务工厂入口。
   - Clip 编辑 speaker mix 会通过 own voice context 快照退出 Follow Track，并支持 undo 恢复。
   - Track 单 speaker 选择也已进入 history，避免绕过 undo 栈造成非法状态。

### 仍需保持的设计约束

1. **Duration 不做动态混合**
   - duration 前无法知道实际音素/片段时间范围，所以必须使用 fixed/static speaker mix。

2. **N-1 权重存储**
   - 数据层保存 N-1 权重，最后一个 speaker 权重由 `1 - sum(explicitWeights)` 得到。

3. **Fixed weights 与 dynamic keyframes 可以共存**
   - 这是 Dynamic Bypassed 的基础。Bypass 时保留 keyframes，但推理使用 fixedWeights。

4. **预设只保存 fixed mix**
   - Speaker Mix preset 不保存 dynamic keyframes。

5. **颜色只属于 UI**
   - 颜色不进入 `SpeakerMixData`，由 UI 根据 speaker / palette 派生。

6. **Follow Track 是三合一语义**
   - 新代码应把 `useTrackSingerInfo == true` 视为 singer/speaker/speaker mix 全部跟随 Track。
   - 不要再引入“只跟随 speaker 但不跟随 mix”的新路径。

---

## 下一轮重构目标

这轮重构的目标不是补功能，而是降低状态机复杂度，让后续声线混合功能更可维护。

推荐目标：

1. 补 SpeakerMix 纯数据测试和推理转换测试。
2. 拆分 speaker mix 数据验证和规范化。
3. 删除 Follow Track 冗余字段和静默失败路径。
4. 处理 Follow Track 相关 UI 显示一致性。
5. 视风险决定是否立即做 bypass 正交化与 editor commit/discard。

---

## P0：Action/Undo 收敛（已完成）

### 原问题

Speaker Mix 相关 Action 数量偏多，且语义重复。

重构前 `SpeakerMixActions` 暴露这些入口：

```cpp
void replaceSpeakerMix(const SpeakerMixData &data, SingingClip *clip);
void enableClipDynamicSpeakerMix(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                 const SpeakerMixData &data, SingingClip *clip);
void applyClipSpeakerMixPreset(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                               const SpeakerMixData &data, SingingClip *clip);
void applyTrackSpeakerMixPreset(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                const SpeakerMixData &data, Track *track);
void replaceTrackSpeakerMix(const SpeakerMixData &data, Track *track);
```

`enableClipDynamicSpeakerMix` 和 `applyClipSpeakerMixPreset` 的最终效果都是设置 clip own
voice context。两者应保留不同工厂方法名称，但底层不需要两套 Action 类。

另外，Track 歌手/说话人下拉框曾直接调用 `Track::setSingerAndSpeakerInfo()`，绕过
history。按“轨道选择 mix 预设 -> clip 开启动态混合 -> 轨道选择另一个 spk -> 撤销”
操作时，撤销会跳过 track 单 speaker 选择，直接撤销 clip 动态混合，导致状态非法。

### 已完成方案

已保留当前统一 Action：

```cpp
SetClipVoiceContextAction
SetTrackVoiceContextAction
```

它们只做一件事：保存 old/new voice context 快照，并在 execute/undo 中调用统一语义入口。

Clip 版保存：

```cpp
bool oldUseTrackVoiceContext;
SingerInfo oldOwnSinger;
SpeakerInfo oldOwnSpeaker;
SpeakerMixData oldOwnMix;

bool newUseTrackVoiceContext;
SingerInfo newOwnSinger;
SpeakerInfo newOwnSpeaker;
SpeakerMixData newOwnMix;
```

Track 版保存：

```cpp
SingerInfo oldSinger;
SpeakerInfo oldSpeaker;
SpeakerMixData oldMix;

SingerInfo newSinger;
SpeakerInfo newSpeaker;
SpeakerMixData newMix;
```

保留 `SpeakerMixActions` 作为语义工厂层：

- `applyClipSpeakerMixPreset(...)` 内部创建 `SetClipVoiceContextAction`
- `enableClipDynamicSpeakerMix(...)` 内部创建 `SetClipVoiceContextAction`
- `applyTrackSpeakerMixPreset(...)` 内部创建 `SetTrackVoiceContextAction`
- `replaceSpeakerMix(...)` 如果会让 Follow Track clip 退出跟随，也应走 `SetClipVoiceContextAction`
- `replaceTrackSpeakerMix(...)` 内部创建 `SetTrackVoiceContextAction`
- `selectTrackSingleSpeaker(...)` 内部创建 `SetTrackVoiceContextAction`，用于 Track
  singer/speaker 下拉框选择单 speaker 并清空 mix

已删除旧重复 Action 类：

- `ApplyClipSpeakerMixPresetAction`
- `EnableClipDynamicSpeakerMixAction`
- `ReplaceSpeakerMixAction`
- `ApplyTrackSpeakerMixPresetAction`
- `ReplaceTrackSpeakerMixAction`

### 注意点 / 后续风险

- 不要删除工厂方法名称，UI 代码可以继续表达“应用预设”“启用动态混合”等业务语义。
- 消重的是底层 undo 快照和 execute/undo 逻辑，不是 UI 层语义。
- `preservePresetSourceAsDirty` 逻辑应集中，不要在 clip/track action 内复制。
- Clip 进入 Dynamic Mix 后，工具栏歌手下拉框仍可能显示 Follow Track 语义，属于
  Follow Track UI 表现一致性问题，建议在删除 `useTrackSpeakerInfo` / 统一
  `followTrackVoiceContext` 时一起处理。

---

## P1：拆分 `normalizeSpeakerMixData`

### 问题

`normalizeSpeakerMixData` 现在同时承担：

- preset metadata 清洗
- mode/source/weights/keyframes 合法性检查
- 权重 clamp / normalize
- keyframe 排序
- 无效数据降级为 `Single`

这会让调用方无法区分“数据被规范化”和“数据被判定无效并降级”。

### 建议方案

拆成更明确的函数：

```cpp
enum class SpeakerMixValidationError {
    None,
    TooFewSources,
    InvalidFixedWeights,
    InvalidDynamicKeyframes,
    UnsupportedMode,
};

SpeakerMixValidationError validateSpeakerMixData(const SpeakerMixData &data);
SpeakerMixData normalizeSpeakerMixData(const SpeakerMixData &data);
SpeakerMixData fallbackToSingleSpeakerMix();
```

第一步可以不引入完整 error enum，至少先做到：

- `normalizeSpeakerMixData` 只做规范化和排序。
- 降级到 Single 的逻辑集中在一个显式 helper 中。
- 推理路径遇到无效 dynamic 数据时明确 fallback 到 fixed/static，并保留注释。

### 不要做

- 不要在每个 UI / Action 调用方手写合法性判断。
- 不要让 getter 反复做复杂 normalize 并隐藏降级行为。

---

## P1：Follow Track 冗余字段清理

### 问题

`SingingClip` 中仍有两个跟随字段：

```cpp
Property<bool> useTrackSingerInfo{true};
Property<bool> useTrackSpeakerInfo{true};
```

当前语义已经收敛为三合一 Follow Track，`useTrackSpeakerInfo` 不再有独立业务意义。

另一个风险是 `setSpeakerMixData()` 在 follow track 状态下可能静默失败。对 UI 编辑来说，
编辑 clip 自己的 speaker mix 应自动退出 Follow Track，而不是悄悄不生效。

### 建议方案

1. 直接删除 `useTrackSpeakerInfo` 属性和相关同步逻辑。
2. 从 workspace / clipboard / project converter 序列化中移除 `useTrackSpeakerInfo` 字段。
3. 如果要改名，可以把 `useTrackSingerInfo` 同步改成更准确的 `followTrackVoiceContext`。
4. `setSpeakerMixData()` 如果目标是 clip own mix，应自动复制当前 effective voice context 并退出 Follow Track。
5. 所有新代码优先使用：
   - `effectiveVoiceContext()`
   - `setOwnVoiceContext(...)`
   - `useTrackVoiceContext()`
   - `selectOwnSingleSpeaker(...)`

### 验收场景

- Track preset -> Follow Track clip 同步变化。
- Follow Track clip -> 编辑 Dynamic Mix：clip 应退出 Follow Track，并复制当前 effective singer/speaker/mix 作为 own context。
- Track single / preset 切换：Follow Track clips 跟随，自定义 clips 不受影响。
- 保存重开后 follow 状态一致。

---

## P2：SingerSourceMode 与 bypass 正交化

### 当前状态

当前 `SpeakerMixData` 只有：

```cpp
SingerSourceMode mode; // Single / FixedMix / DynamicMix
QList<SpeakerMixKeyframe> dynamicKeyframes;
```

Dynamic Bypassed 由组合状态表示：

```cpp
mode == FixedMix && !dynamicKeyframes.isEmpty()
```

已有 helper：

```cpp
hasDynamicMixAutomation(data)
isDynamicMixActive(data)
isDynamicMixBypassed(data)
```

### 问题

helper 已经缓解了可读性，但数据结构仍把“source mode”和“dynamic bypass”混在一起。

### 建议方案

此项不必优先于 Action/测试。若要做，推荐引入正交字段：

```cpp
struct SpeakerMixData {
    SingerSourceMode mode = SingerSourceMode::Single;
    bool dynamicBypassed = false;
};
```

目标语义：

- `mode == Single`：单 speaker。
- `mode == FixedMix`：固定混合。
- `mode == DynamicMix && !dynamicBypassed`：动态混合生效。
- `mode == DynamicMix && dynamicBypassed`：保留 keyframes，但推理使用 fixedWeights。

### Schema 调整策略

- 直接把 `dynamicBypassed` 写入 `SpeakerMixData` 序列化结构。
- 直接移除 `mode == FixedMix && !dynamicKeyframes.isEmpty()` 表示 bypass 的旧组合语义。
- 当前开发期文件如需保留，可一次性手动或临时脚本转换为
  `mode = DynamicMix; dynamicBypassed = true`。
- UI 入口不变：Bypass / Resume Dynamic / Stop Dynamic 行为保持。

---

## P2：SpeakerMixEditorView commit/discard

### 当前状态

`SpeakerMixEditorView` 内部仍是：

```cpp
SpeakerMixData m_data;
QList<SpeakerMixSpeaker> m_speakers;
QList<SpeakerMixKeyframe> m_keyframes;
```

编辑过程中会较快地把数据发给 `ParamEditorView`，再通过 action 写回模型。

### 问题

- `m_data` 既像 committed snapshot，又像工作副本来源。
- Escape / discard 语义不清晰。
- 后续若要做更复杂的曲线编辑或批量操作，会继续增加状态机复杂度。

### 建议方案

参考参数编辑 handler 的 commit/discard 模式：

```cpp
class SpeakerMixEditorView : public TimeOverlayView {
public:
    SpeakerMixData committedMixData() const;
    SpeakerMixData workingMixData() const;
    void commit();
    void discard();

private:
    SpeakerMixData m_committedData;
    QList<SpeakerMixKeyframe> m_keyframes;
    QList<SpeakerMixSpeaker> m_speakers;
};
```

建议先完成 Action/数据层重构，再做这项。否则两套状态机同时变化，回归成本会偏高。

---

## P3：测试补齐

这是当前最缺的一块。建议优先补纯数据测试，成本低，收益高。

### SpeakerMixData 测试

覆盖：

- `normalizeSpeakerMixFullWeights`
- `explicitWeightsFromFullWeights`
- `fullWeightsFromExplicitWeights`
- `normalizeSpeakerMixData`
- `hasDynamicMixAutomation`
- `isDynamicMixActive`
- `isDynamicMixBypassed`

重点场景：

- 权重总和大于 1。
- 权重总和小于 1。
- 空 weights。
- source 数量不足。
- DynamicMix + dynamicBypassed 的旁路状态。
- DynamicMix + 空 keyframes 的降级策略。

### InferSpeakerMix 测试

覆盖：

- `staticSpeakerMix`
- `fixedSpeakerMixFromData`
- `effectiveSpeakerMixForFixedInference`
- `dynamicSpeakerMixFromData`
- `effectiveSpeakerMixFromData`
- `InferSpeakerMix::signature`

重点场景：

- Single 保持旧行为。
- Fixed Mix 输出比例范围为 `0.0~1.0`。
- Dynamic Active 按时间范围采样。
- Dynamic Bypassed 使用 fixedWeights。
- timing 不足或 dynamic 无效时 fallback 到 fixed/static。
- signature 对 fixed/dynamic 的变化敏感。

---

## 推荐执行顺序

1. **Action/Undo 收敛（已完成）**
   - 已减少 Speaker Mix 状态路径数量。
   - UI 入口仍保留业务语义，底层 action 已统一为 voice context 快照。
   - Track 单 speaker 选择已进入 history。

2. **补最小测试集**
   - 先补 `SpeakerMixData` 和 `InferSpeakerMix` 的纯函数测试。
   - 这一步能保护后续 normalize / bypass 重构。

3. **拆分 normalize / validate**
   - 在测试保护下拆，避免隐式降级行为改变后无人发现。

4. **删除 Follow Track 冗余字段**
   - 不需要旧工程读写兼容。
   - 新代码只走 effective voice context。

5. **决定是否做 bypass 正交化**
   - 如果 Dynamic Bypassed 相关 bug 继续出现，优先做。
   - 如果当前行为稳定，可后置。

6. **最后做 EditorView commit/discard**
   - 这是交互状态机重构，建议不要和数据模型迁移混在一个提交里。

---

## 回归清单

### 状态机

- preset -> single speaker：必须触发推理。
- single speaker -> preset：必须触发推理。
- preset -> Dynamic Active：必须触发推理。
- Dynamic Active -> Bypassed：保留 keyframes，推理使用 fixedWeights。
- Dynamic Bypassed -> Resume：恢复动态推理。
- Stop Dynamic：清空 keyframes，回到 Fixed Mix 推理。
- Track preset -> Follow Track clip：clip 刷新。
- Follow Track clip -> own Dynamic Mix：退出 Follow Track 并复制当前 effective context。
- Track single / preset 切换：自定义 clip 不受影响。
- undo/redo 覆盖 single、preset、Follow Track、Dynamic enable、bypass、stop。

### 推理

- Single：行为与旧版本一致。
- Fixed Mix：不同 preset 能听出不同混合效果。
- Dynamic Active：关键帧变化后 pitch/variance/acoustic 使用动态比例。
- Dynamic Bypassed：推理使用 fixedWeights。
- Duration：始终使用 fixed/static fallback。
- 保存重开后 Dynamic Active / Bypassed 状态推理一致。

### 构建

- 不使用 CLion MCP run configuration。
- Configure：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Configure -Preset debug
```

- Build：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Build -Preset debug
```
