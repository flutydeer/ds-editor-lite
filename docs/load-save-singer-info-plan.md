# 保存/读取轨道和剪辑歌手信息方案

## Context

本需求要让 DS Editor Lite 在保存/读取 `.dspx` 项目时保留轨道与歌唱剪辑的歌手、声线、默认语言以及“是否跟随轨道歌手/声线”的语义。项目目前通过 `src/app/Modules/ProjectConverters/DspxProjectConverter.cpp` 使用 opendspx 模型读写文件；现有 converter 已读写 track/clip/note/param/workspace，但没有处理 opendspx 的 singer/sources 模型，也没有保存 DS Lite 的 track singer/speaker 信息。

关键发现：

- opendspx 的 `Track` 只有 `name/control/clips/workspace`，没有官方 singer 字段。
- opendspx 的 `SingingClip` 有官方 `sources` 字段；`sources.singers[]` 承载剪辑歌手信息。
- opendspx 的 `SingleSinger` 有官方 `id` 字段；`Singer` 还有 `extra/workspace` 可保存扩展信息。
- DS Lite 已在 `Note::workspace()` 使用 `workspace["ds-editor-lite"]`，本方案沿用该命名空间保存 DS Lite 私有字段。

目标结果：

1. 剪辑的有效歌手写入 opendspx 官方字段 `SingingClip.sources.singers[]`，保证第三方 opendspx 工具能读到歌手 ID。
2. 轨道歌手、声线、useTrack 标志、package/version、默认语言等 opendspx 没有官方字段的内容写入 `workspace["ds-editor-lite"]`。
3. 读取旧文件或第三方 opendspx 文件时不失败，并尽量恢复可用的歌手 ID。

## 推荐数据映射

### 1. Track 级信息

opendspx 没有 track singer 官方字段，所以 Track 级 DS 信息全部进入：

```text
/content/tracks[i]/workspace/ds-editor-lite
```

建议结构：

```json
{
  "schemaVersion": 1,
  "singer": {
    "identifier": {
      "singerId": "singer-id",
      "packageId": "package-id",
      "packageVersion": "1.2.3"
    },
    "name": "Singer Name",
    "defaultLanguage": "cmn",
    "defaultDict": "dict-id"
  },
  "speaker": {
    "id": "speaker-id",
    "name": "Speaker Name",
    "toneMin": "C3",
    "toneMax": "C5"
  },
  "defaultLanguage": "cmn",
  "defaultG2pId": "g2p-id"
}
```

字段映射：

| DS 字段 | opendspx JSON 路径 | 说明 |
|---|---|---|
| `Track::singerInfo().identifier().singerId` | `tracks[i].workspace.ds-editor-lite.singer.identifier.singerId` | Track 无官方 singer，只能放 workspace。 |
| `packageId/packageVersion` | `tracks[i].workspace.ds-editor-lite.singer.identifier.packageId/packageVersion` | DS Lite 扩展身份信息。 |
| `Track::singerInfo().name()` | `tracks[i].workspace.ds-editor-lite.singer.name` | package 缺失时 fallback 显示。 |
| `Track::speakerInfo()` | `tracks[i].workspace.ds-editor-lite.speaker` | opendspx 无官方 speaker 字段。 |
| `Track::defaultLanguage()` | `tracks[i].workspace.ds-editor-lite.defaultLanguage` | Track 默认语言。 |
| `Track::defaultG2pId()` | `tracks[i].workspace.ds-editor-lite.defaultG2pId` | 派生信息，可保存但读取时不强依赖。 |

### 2. SingingClip 级信息

剪辑歌手使用 opendspx 官方字段：

```text
/content/tracks[i]/clips[j]/sources/singers[0]
```

单歌手建议结构：

```json
{
  "type": "single",
  "id": "singer-id",
  "extra": {},
  "workspace": {
    "ds-editor-lite": {
      "schemaVersion": 1,
      "identifier": {
        "packageId": "package-id",
        "packageVersion": "1.2.3"
      },
      "name": "Singer Name",
      "defaultLanguage": "cmn",
      "defaultDict": "dict-id"
    }
  }
}
```

Clip 自身的 DS Lite 行为语义进入：

```text
/content/tracks[i]/clips[j]/workspace/ds-editor-lite
```

建议结构：

```json
{
  "schemaVersion": 1,
  "useTrackSingerInfo": true,
  "useTrackSpeakerInfo": true,
  "speaker": {
    "id": "speaker-id",
    "name": "Speaker Name",
    "toneMin": "C3",
    "toneMax": "C5"
  },
  "defaultLanguage": "cmn",
  "defaultG2pId": "g2p-id"
}
```

字段映射：

| DS 字段 | opendspx JSON 路径 | 说明 |
|---|---|---|
| `SingingClip::singerInfo().identifier().singerId` | `clips[j].sources.singers[0].id` | 官方字段，必须优先使用。 |
| singer `type` | `clips[j].sources.singers[0].type` | 单歌手写 `single`。 |
| `packageId/packageVersion` | `clips[j].sources.singers[0].workspace.ds-editor-lite.identifier.packageId/packageVersion` | 官方 `id` 不承载 package 信息。 |
| `SingingClip::singerInfo().name()` | `clips[j].sources.singers[0].workspace.ds-editor-lite.name` | fallback 显示。 |
| `useTrackSingerInfo` | `clips[j].workspace.ds-editor-lite.useTrackSingerInfo` | DS Lite 私有继承语义。 |
| `useTrackSpeakerInfo` | `clips[j].workspace.ds-editor-lite.useTrackSpeakerInfo` | DS Lite 私有继承语义。 |
| `SingingClip::speakerInfo()` | `clips[j].workspace.ds-editor-lite.speaker` | opendspx 无官方 speaker 字段。 |
| `SingingClip::defaultLanguage()` | `clips[j].workspace.ds-editor-lite.defaultLanguage` | Clip 默认语言。 |
| `SingingClip::defaultG2pId()` | `clips[j].workspace.ds-editor-lite.defaultG2pId` | 派生信息，可保存但读取时不强依赖。 |

保存 clip singer 时建议写“有效歌手”：即 `singingClip->singerInfo()`。如果 `useTrackSingerInfo == true`，这个值来自 track；仍写入 official `sources.singers[0].id`，让非 DS Lite 的 opendspx 读取方也能知道该 clip 使用哪个 singer。DS Lite 自己再通过 `workspace.ds-editor-lite.useTrackSingerInfo` 恢复“这个 singer 是跟随轨道”的语义。

## 关键实现方案

### 1. 只在 `DspxProjectConverter.cpp` 做文件格式转换

不建议为了本需求先扩展 `Clip/Track/SingerInfo` 的通用 `serialize()/deserialize()`。当前实际 `.dspx` 保存/读取路径在 `DspxProjectConverter.cpp`，直接在 converter 内完成 DS 模型与 opendspx 模型映射，变更面更小，也更贴近 opendspx 官方结构。

### 2. 在 anonymous namespace 添加 helper

位置：`src/app/Modules/ProjectConverters/DspxProjectConverter.cpp` 现有 anonymous namespace 中，放在 `JsonNlohmann` 后。

建议常量：

```cpp
constexpr const char *kDsWorkspaceKey = "ds-editor-lite";
constexpr int kDsWorkspaceSchemaVersion = 1;
```

建议 helper 类型：

1. Workspace helper
   - `QJsonObject dsWorkspaceFrom(const opendspx::Workspace &workspace)`
   - `void writeDsWorkspace(opendspx::Workspace &workspace, const QJsonObject &obj)`
   - 只读写 `workspace["ds-editor-lite"]`，保留其他 workspace key。

2. Identifier helper
   - `QJsonObject encodeSingerIdentifier(const SingerIdentifier &identifier, bool includeSingerId)`
   - `SingerIdentifier decodeSingerIdentifier(const QJsonObject &obj, const QString &officialSingerId = {})`
   - `officialSingerId` 来自 `SingleSinger::id` 时优先覆盖 workspace 中的 singerId。
   - `packageVersion` 使用 `QVersionNumber::toString()` / `QVersionNumber::fromString()`。

3. SingerInfo helper
   - `QJsonObject encodeSingerInfoForWorkspace(const SingerInfo &singerInfo, bool includeSingerId)`
   - `SingerInfo decodeSingerInfoFromWorkspace(const QJsonObject &obj, const QString &officialSingerId = {})`
   - 读取后用 `packageManager->findSingerByIdentifier(identifier)` 尝试解析已安装完整 singer；找不到时构造 fallback `SingerInfo(identifier, name, {}, {}, defaultLanguage, defaultDict)`，避免 package 缺失导致信息丢失。

4. SpeakerInfo helper
   - `QJsonObject encodeSpeakerInfoForWorkspace(const SpeakerInfo &speakerInfo)`
   - `SpeakerInfo decodeSpeakerInfoFromWorkspace(const QJsonObject &obj)`
   - `SpeakerInfo resolveSpeakerInfo(const SingerInfo &singerInfo, const SpeakerInfo &fallback)`：优先用 id 在 `singerInfo.speakers()` 中找完整 speaker，找不到则保留 fallback。

5. opendspx singer helper
   - `opendspx::SingerRef encodeSingleSingerRef(const SingerInfo &singerInfo)`
   - `std::optional<SingerInfo> decodePrimarySingerInfo(const std::vector<opendspx::SingerRef> &singers)`
   - `SingleSinger`：读取 `id` + `workspace["ds-editor-lite"]`。
   - `MixedSinger`：当前 DS 模型只能表达一个 singer，先取其中第一个可解析的 `SingleSinger` 作为 primary singer；未来若要支持声线混合再扩展模型。

### 3. 修改 `save()` 中的 `encodeTracks`

位置：`DspxProjectConverter::save()` 内 `encodeTracks` lambda。

处理顺序：

1. 写基础字段：`name/control`。
2. 组装 track DS workspace：
   - `schemaVersion`
   - 非空 `singer`
   - 非空 `speaker`
   - 非空 `defaultLanguage`
   - `defaultG2pId`
3. `writeDsWorkspace(track.workspace, dsWorkspace)`。
4. 调用 `encodeClips(dsTrack, track)`。
5. push 到 `dspx.content.tracks`。

### 4. 修改 `save()` 中的 `encodeClips`

位置：`DspxProjectConverter::save()` 内 `encodeClips` lambda 的 Singing 分支。

处理顺序：

1. 保持现有 `name/time/control/notes/params` 写入逻辑。
2. 先保留现有 `clip->workspace()` 中所有 key，现有代码已经遍历复制。
3. 组装 clip DS workspace：
   - `schemaVersion`
   - `useTrackSingerInfo`
   - `useTrackSpeakerInfo`
   - 非空 `speaker`
   - 非空 `defaultLanguage`
   - `defaultG2pId`
4. `writeDsWorkspace(singClip->workspace, dsWorkspace)`。
5. 写 official `sources`：
   - `const auto singerInfo = singingClip->singerInfo();`
   - 如果 `singerInfo.identifier().singerId` 非空，创建 `opendspx::Sources`。
   - `sources.category` 建议写 `"singing"`，读取时不依赖这个值。
   - `sources.singers.push_back(encodeSingleSingerRef(singerInfo))`。
   - `singClip->sources = sources`。
   - 如果 singer 为空，不写空 `sources`，避免 opendspx serializer 的 `EmptySingerMixingError`。

### 5. 修改 `load()` 中的 `decodeTracks`

位置：`DspxProjectConverter::load()` 内 `decodeTracks` lambda。

处理顺序：

1. 创建 `Track`。
2. 读取基础字段：`name/control`。
3. 从 `dspxTrack.workspace["ds-editor-lite"]` 读取：
   - `singer`
   - `speaker`
   - `defaultLanguage`
4. 解析并 resolve：
   - `SingerInfo trackSingerInfo`
   - `SpeakerInfo trackSpeakerInfo`
5. 设置到 Track：
   - `track->setSingerAndSpeakerInfo(trackSingerInfo, trackSpeakerInfo)`
   - 如果有 `defaultLanguage`，调用 `track->setDefaultLanguage(defaultLanguage)`。
6. 再调用 `decodeClips(dspxTrack.clips, track)`。
7. `model_->insertTrack(track, i)`。

Track 先读 singer/speaker 再读 clips，这样 `Track::insertClip()` 绑定的继承信息和 `decodeClips` 中手动注入的信息都能拿到正确父级状态。

### 6. 修改 `load()` 中的 `decodeClips`

位置：`DspxProjectConverter::load()` 内 `decodeClips` lambda 的 Singing 分支。

处理顺序：

1. 创建 `SingingClip`。
2. 读取基础字段：`name/time/control/notes/params/workspace`，保留现有 workspace 复制逻辑。
3. 注入 track 信息：
   - `clip->setTrackSingerAndSpeakerInfo(track->singerInfo(), track->speakerInfo())`。
4. 从 official `castClip->sources` 读取 primary singer：
   - 有 `sources.singers[]` 且能解析出 singer，则 `clip->setSingerInfo(decodedSinger)`。
5. 从 `castClip->workspace["ds-editor-lite"]` 读取：
   - `useTrackSingerInfo`
   - `useTrackSpeakerInfo`
   - `speaker`
   - `defaultLanguage`
6. 设置 flags：
   - 如果 workspace 明确有 `useTrackSingerInfo`，使用该值。
   - 如果没有该字段但 official `sources.singers[]` 有 singer，默认 `useTrackSingerInfo = false`，兼容第三方 opendspx clip singer。
   - 如果没有该字段也没有 official singer，默认 `useTrackSingerInfo = true`，兼容旧 DS 文件。
   - `useTrackSpeakerInfo` 如果缺失，默认 `true`；如果有 local speaker 但没有 flag，可考虑默认 `false`，但为了少改旧行为建议先默认 `true`。
7. 设置 speaker：
   - workspace 有 `speaker` 时 `clip->setSpeakerInfo(resolveSpeakerInfo(clip->singerInfo(), fallbackSpeaker))`。
8. 设置 defaultLanguage：
   - workspace 有 clip `defaultLanguage` 时使用它。
   - 否则不主动覆盖，保持 `SingingClip::defaultLanguage()` 的父级 fallback 语义。
9. 插入 track：`track->insertClip(clip)`。

## 相关模型行为调整

### 1. 修正 `SingingClip::speakerInfo()` 的 useTrack 语义

当前 `src/app/Model/AppModel/SingingClip.cpp` 中：

```cpp
SpeakerInfo SingingClip::speakerInfo() const {
    if (m_speakerInfo.get().isEmpty()) {
        return m_trackSpeakerInfo.get();
    }
    return m_speakerInfo.get();
}
```

这会让 `useTrackSpeakerInfo` 对实际返回值不起决定作用。建议改为与 `singerInfo()` 一致：

```cpp
SpeakerInfo SingingClip::speakerInfo() const {
    if (useTrackSpeakerInfo) {
        return m_trackSpeakerInfo.get();
    }
    return m_speakerInfo.get();
}
```

兼容策略由 converter 保证：旧文件缺失 flag 时默认 `true`。

### 2. 调整 `ValidationController::onModelChanged()`，避免覆盖已加载语言

当前 `src/app/Controller/ValidationController.cpp` 在 `onModelChanged()` 中无条件：

```cpp
track->setDefaultLanguage(appOptions->general()->defaultSingingLanguage);
singingClip->setDefaultLanguage(track->defaultLanguage());
```

这会覆盖 converter 从文件读出的 track/clip defaultLanguage。建议改为：

- 只在 `track->defaultLanguage().isEmpty()` 或 `track->defaultLanguage() == "unknown"` 时设置 app 默认语言。
- 只在 clip 没有显式 defaultLanguage 时设置/依赖 track fallback；`SingingClip::defaultLanguage()` 已有空值跟随父级逻辑，所以 converter 读取不到 clip language 时可保持空值。
- 保留 `setTrackSingerInfo/setTrackSpeakerInfo` 或改用 `setTrackSingerAndSpeakerInfo` 注入父级歌手/声线，但不要覆盖 clip local singer/speaker 和 flags。

## 兼容策略

### 旧 DS Lite 文件

无 `sources`、无 `workspace["ds-editor-lite"]` 时：

- load 不失败。
- Track singer/speaker 保持空。
- Clip local singer/speaker 保持空。
- `useTrackSingerInfo = true`。
- `useTrackSpeakerInfo = true`。
- defaultLanguage 保持模型默认或由 ValidationController 只在 unknown/empty 时补 app 默认值。

### 第三方 opendspx 文件

如果只有 official `sources.singers[0].id`，没有 DS workspace：

- 读取 `singerId`，`packageId/packageVersion` 为空。
- 构造 fallback `SingerInfo` 或尝试在 package manager 中解析。
- 默认 `useTrackSingerInfo = false`，避免 clip singer 被空 track singer 覆盖。
- 再保存时继续写 official `sources.singers[0].id`。

### package 未安装

如果 JSON 中有完整 identifier 但本机 package manager 找不到：

- 保留 identifier、name、defaultLanguage、defaultDict 的 fallback `SingerInfo`。
- UI 至少能显示 fallback name 或 singerId。
- 再保存时不丢 packageId/packageVersion。

### 空 singer

如果有效 singer 为空：

- 不写 `SingingClip.sources` 或保持 `std::nullopt`。
- 不写空 `sources.singers`，避免 serializer 报 `EmptySingerMixingError`。

## 验证方案

1. 构造/保存一个 Track singer=A、Clip 跟随 track 的项目。
   - JSON 应包含 `tracks[0].workspace.ds-editor-lite.singer`。
   - JSON 应包含 `clips[0].sources.singers[0].id == A.singerId`。
   - JSON 应包含 `clips[0].workspace.ds-editor-lite.useTrackSingerInfo == true`。
   - 重新读取后 Track singer=A，Clip effective singer=A，flag=true。

2. 构造/保存一个 Track singer=A、Clip override singer=B 的项目。
   - JSON 应包含 `clips[0].sources.singers[0].id == B.singerId`。
   - JSON 应包含 `useTrackSingerInfo == false`。
   - 重新读取后 Track singer=A，Clip effective singer=B，flag=false。

3. 构造/保存声线覆盖场景。
   - Track speaker=A，Clip speaker=B，`useTrackSpeakerInfo=false`。
   - 重新读取后 `SingingClip::speakerInfo()` 返回 B。

4. 读取旧文件。
   - 无 `sources`/无 DS workspace 不报错。
   - flags 默认为 true。
   - 保存后仍能由 opendspx serializer 正常读取。

5. 读取第三方 opendspx 文件。
   - 只有 `sources.singers[0].id` 时，Clip singerId 不丢。
   - 默认 `useTrackSingerInfo=false`。

6. package 缺失测试。
   - 保存含 packageId/packageVersion 的项目。
   - 在 package manager 找不到对应 singer 时读取。
   - 确认 identifier/name 仍保留，再保存不丢失。

7. 构建验证。
   - 使用项目技能或 CMake preset 构建 debug target。
   - 至少运行一次保存/读取 round-trip 手工验证，检查 JSON 结构与 UI 状态。

## 实施阶段

### Phase 1：模型层修正（前置依赖）

**状态**：✅ 已完成

**目标**：修正 `SingingClip::speakerInfo()` 方法，让 `useTrackSpeakerInfo` 标志真正控制返回值。

**修改文件**：
- `src/app/Model/AppModel/SingingClip.cpp`

**具体工作**：
- 将 `speakerInfo()` 从"speaker 为空时 fallback 到 track"改为"根据 `useTrackSpeakerInfo` flag 决定返回值"。
- 与 `singerInfo()` 的 `useTrackSingerInfo` 语义保持一致。

**完成标准**：
- `useTrackSpeakerInfo == true` 时返回 track speaker。
- `useTrackSpeakerInfo == false` 时返回 clip local speaker（即使为空）。
- 编译通过，现有功能不回退（兼容由 Phase 3 converter 保证旧文件 flag 默认 true）。

---

### Phase 2：Controller 层调整

**状态**：✅ 已完成

**目标**：避免 `ValidationController::onModelChanged()` 覆盖 converter 从文件读出的 defaultLanguage。

**修改文件**：
- `src/app/Controller/ValidationController.cpp`

**具体工作**：
- `track->setDefaultLanguage(...)` 只在 `track->defaultLanguage().isEmpty()` 或 `== "unknown"` 时执行。
- `singingClip->setDefaultLanguage(...)` 只在 clip 没有显式 defaultLanguage 时执行（空值保持父级 fallback 语义）。
- 不覆盖 clip local singer/speaker 和 useTrack flags。

**完成标准**：
- 从文件读取的 track/clip defaultLanguage 在 `onModelChanged()` 后仍保留。
- 新建 track/clip（无文件来源）仍能获得 app 默认语言。
- 编译通过。

---

### Phase 3：Converter 核心实现

**状态**：✅ 已完成

**目标**：在 `DspxProjectConverter.cpp` 中实现 singer/speaker/language 的完整保存与读取。

**修改文件**：
- `src/app/Modules/ProjectConverters/DspxProjectConverter.cpp`

**具体工作**：

1. **添加 helper 函数**（anonymous namespace）：
   - Workspace 读写 helper（`dsWorkspaceFrom` / `writeDsWorkspace`）
   - Identifier 编解码（`encodeSingerIdentifier` / `decodeSingerIdentifier`）
   - SingerInfo 编解码（`encodeSingerInfoForWorkspace` / `decodeSingerInfoFromWorkspace`，含 package manager fallback）
   - SpeakerInfo 编解码（`encodeSpeakerInfoForWorkspace` / `decodeSpeakerInfoFromWorkspace` / `resolveSpeakerInfo`）
   - opendspx singer helper（`encodeSingleSingerRef` / `decodePrimarySingerInfo`）

2. **修改 `save()` 路径**：
   - `encodeTracks`：写 track workspace（singer/speaker/defaultLanguage/defaultG2pId）
   - `encodeClips` Singing 分支：写 clip workspace（useTrack flags/speaker/defaultLanguage）+ 写 official `sources.singers[0]`

3. **修改 `load()` 路径**：
   - `decodeTracks`：从 workspace 读 track singer/speaker/language，先于 clips 设置到 Track
   - `decodeClips` Singing 分支：从 official sources 读 singer，从 workspace 读 flags/speaker/language，处理兼容默认值

**完成标准**：
- 编译通过。
- 保存后 `.dspx` JSON 包含正确的 `workspace["ds-editor-lite"]` 和 `sources.singers[]` 结构。
- 读取含 singer 信息的文件后模型状态正确。

---

### Phase 4：兼容性与集成验证

**状态**：🔶 待手工验证（代码已完成，构建通过）

**目标**：验证所有兼容场景和 round-trip 正确性。

**验证项**：
- [ ] 旧 DS Lite 文件（无 sources/无 DS workspace）正常加载，flags 默认 true
- [ ] 第三方 opendspx 文件（只有 `sources.singers[0].id`）正确读取 singer ID，默认 `useTrackSingerInfo=false`
- [ ] Track singer=A + Clip 跟随 track：round-trip 后 flag=true，effective singer=A
- [ ] Track singer=A + Clip override singer=B：round-trip 后 flag=false，effective singer=B
- [ ] Speaker 覆盖场景：`useTrackSpeakerInfo=false` 时 clip 返回 local speaker
- [ ] Package 缺失时 fallback 信息不丢，再保存不丢 packageId/packageVersion
- [ ] 空 singer 不写 `sources`，避免 `EmptySingerMixingError`
- [ ] Debug 构建通过

**完成标准**：
- 所有验证项通过。
- 至少完成一次完整 save/load round-trip 手工验证。

---

## 关键文件

需要修改：

- `src/app/Modules/ProjectConverters/DspxProjectConverter.cpp`
  - 增加 helper。
  - 更新 `load()` 内 `decodeTracks` / `decodeClips`。
  - 更新 `save()` 内 `encodeTracks` / `encodeClips`。
- `src/app/Model/AppModel/SingingClip.cpp`
  - 修正 `speakerInfo()` 使 `useTrackSpeakerInfo` 生效。
- `src/app/Controller/ValidationController.cpp`
  - 避免 `onModelChanged()` 覆盖从文件读取的 defaultLanguage。

需要引用/复用：

- `src/app/Modules/PackageManager/PackageManager.h`
  - `packageManager->findSingerByIdentifier(...)`。
- `src/app/Modules/Inference/Models/SingerIdentifier.h`
  - `singerId/packageId/packageVersion`。
- `src/app/Modules/PackageManager/Models/SingerInfo.h`
  - fallback `SingerInfo` 构造与 getter/setter。
- `src/app/Modules/PackageManager/Models/SpeakerInfo.h`
  - speaker fallback 与 id/name/tone range。
- `src/app/Model/AppModel/Note.cpp`
  - 已有 `workspace["ds-editor-lite"]` 命名空间可沿用。
