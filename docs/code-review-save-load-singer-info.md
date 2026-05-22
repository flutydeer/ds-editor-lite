# 代码审查：Implement save/load track and clip singer info in dspx projects

**提交**：`ac0b34ee` - Implement save/load track and clip singer info in dspx projects  
**审查日期**：2026-05-23

## 概述

该提交实现了 dspx 项目文件中轨道和片段歌手/说话人信息的保存与加载功能。整体实现质量不错，向后兼容性处理得当。以下是发现的问题。

## Bug 1（中等严重）：保存时将轨道歌手冗余写入片段的 `sources.singers[]`

**位置**：`DspxProjectConverter.cpp` save 路径，片段保存部分

```cpp
// Write official sources.singers[]
const auto effectiveSinger = singingClip->singerInfo();
if (!effectiveSinger.identifier().singerId.isEmpty()) {
    opendspx::Sources sources;
    sources.category = "singing";
    sources.singers.push_back(encodeSingleSingerRef(effectiveSinger));
    singClip->sources = sources;
}
```

`singingClip->singerInfo()` 在 `useTrackSingerInfo == true` 时返回的是**轨道的歌手**，而不是片段自己的歌手。这意味着即使片段设置了"继承轨道歌手"，保存时仍然会把轨道歌手写入片段的 `sources.singers[]`。

**影响**：
- 功能上不会导致加载错误（因为 `useTrackSingerInfo` 标志也被保存了，加载时优先使用标志）
- 但会导致文件中存在冗余/语义不正确的数据
- 第三方工具读取 dspx 文件时会误以为片段有独立歌手
- 用户修改轨道歌手后保存，片段中会残留旧的轨道歌手数据

**修复建议**：

```cpp
// Write official sources.singers[] only if clip has its own singer
if (!singingClip->useTrackSingerInfo.get()) {
    const auto effectiveSinger = singingClip->singerInfo();
    if (!effectiveSinger.identifier().singerId.isEmpty()) {
        opendspx::Sources sources;
        sources.category = "singing";
        sources.singers.push_back(encodeSingleSingerRef(effectiveSinger));
        singClip->sources = sources;
    }
}
```

## Bug 2（低严重）：`setSpeakerInfo` / `setSingerInfo` 未同步更新 `useTrack*` 标志

**位置**：`SingingClip.cpp`

当调用 `setSpeakerInfo` 或 `setSingerInfo` 时，没有自动将对应的 `useTrackSpeakerInfo` / `useTrackSingerInfo` 设为 `false`。

目前不会触发问题（这些方法只在 converter 加载时调用，加载代码会单独设置标志），但如果未来有 UI 代码直接调用这些方法来设置片段独立歌手/说话人，就会出现"设置了值但不生效"的 bug。

**建议**：在 `setSingerInfo` 和 `setSpeakerInfo` 中加入标志更新，或者在注释中明确说明调用者需要同时设置标志。

## 潜在问题 3（低风险）：`InferController` 中新增的即时推理触发

**位置**：`InferController.cpp`，`handleSingingClipInserted` 方法

```cpp
// Trigger inference if language module is already ready and clip has a valid singer
if (appStatus->languageModuleStatus == AppStatus::ModuleStatus::Ready &&
    !clip->singerInfo().isEmpty()) {
    createAndRunGetPronTask(*clip);
}
```

该代码在片段插入时立即触发推理。而 `handleLanguageModuleStatusChanged` 在语言模块就绪时也会遍历所有片段触发推理。正常流程中文件加载发生在语言模块就绪之后，此代码是正确且必要的。但如果存在竞态条件导致两者都触发，可能会产生重复任务。

**风险**：低。正常流程中不会同时触发两次。

## 其他观察（无问题）

- **向后兼容性**：旧文件（没有 DS workspace 数据）加载时，`useTrackSingerInfo` 和 `useTrackSpeakerInfo` 默认为 `true`，与旧行为一致。
- **ValidationController 的修改**：正确地避免了覆盖从文件加载的语言设置。
- **TrackControlView 的修改**：正确地在构造时初始化歌手显示，并连接了后续变更信号。
- **AudioClip 加载代码**：未受影响，保持完整。
- **workspace 数据保存顺序**：先复制旧 workspace 键，再用 `writeDsWorkspace` 覆盖 `"ds-editor-lite"` 键，逻辑正确。
