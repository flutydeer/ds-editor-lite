# 音符语种模型重构：意图、能力与解析

> 状态：✅ 已实施，待验收（2026-11-22）。
> 关联工单：rKgePB「切换剪辑语种导致原有音符不发声」、rXtAd3「语言切换（建议）」

## 0. 摘要

这次改的不是一个 bug，是音符语种的模型。

现状把「用户想用什么语种」和「系统实际解析成什么语种」压在同一个字段上，再用一条活引用把音符绑到片段默认语种。于是片段语种一变，一批从未对语种表达过意图的音符被静默改判，推理失败。与此同时，另一批用户从外部工程导入音符，希望工具栏的语种能批量改到已有音符上，却被「只影响新音符」的规则挡住。两个诉求表面相反，其实是同一个模型缺陷的两面。

重构后的模型：

- 音符语种只有两态，未指定、已指定。未指定只由**当前歌手的可用转换路由**解析，优先歌手默认，否则取路由列表首项，不读片段、轨道或应用设置。
- 应用级「默认演唱语种」降级为**交互创建时的偏好**，在歌手的转换路由能力内兑现，兑现不了就静默留空。语种与默认歌词由同一次创建决策产生。
- 片段级语种改名为**录入语种**，只做填词对话框的预选和工具栏显示，不参与推理。
- 轨道级语种删除。
- 转换路由冲突与模型音素不兼容分别诊断，不静默改写声明，也不保证保留的手工音素一定可合成。
- 填词语种选择器改为「优先级 + 兜底」，所选语种实际参与 tagger 排序，不做「强制全部」。
- 词级语种声明或解析语种变化时，清除失效的手工发音、保留手工音素。片段歌手下拉在发生冲突或将清除发音时汇总确认。
- 换歌手、必要级联和可选语种修复是**一个撤销步骤**，包括切回轨道继承。

贯穿全部决策的不变量：

> 偏好可以静默让位，显式声明必须可见。

## 1. 为什么要做这次改动

### 1.1 两个工单指向同一个缺陷

**工单 rKgePB**：切换剪辑语种后，原有音符不发声。

**工单 rXtAd3**：切换语言时，希望已有音符跟着变。

如果只按第一个工单修，结论是「语种变化后要重新推理，音符继续跟随片段」。这正好和第二个工单的诉求相反。如果只按第二个工单修，结论是「切语种时把已有音符全改掉」，那第一批用户手工调整过的多语种音符就被摧毁了。

关键在于，这两个用户群描述的不是同一个操作。

- 第一个用户群是**创作型**。他们在一个片段里逐句写词、逐句调语种，音符是他们主动标注的。片段语种只是「我接下来画的那批音符的默认值」。
- 第二个用户群是**导入型**。他们从 DAW 或别的工程把音符和歌词搬过来，语种是一次性批量声明的。工具栏对他们来说就是「这一整段的语言」。

两类用户共用了一个控件，而这个控件的语义从来没有被定义清楚。所以问题不是「该不该跟随」，而是「谁有权决定一个音符的语种」。

### 1.2 一个字段承担了两个职责

`Note::m_language` 同时表示三件事：

| 角色 | 表现 |
|---|---|
| 用户意图 | 「这个音符是粤语」 |
| 解析结果 | 「它现在实际按普通话合成」 |
| 继承标记 | 空串表示「跟随片段」 |

[Note::effectiveLanguage](../../src/libs/ProjectModel/AppModel/Note.cpp#L153) 在读取时把第三种含义展开成第二种，于是三个消费者各取所需：

| 消费者 | 它以为自己在读什么 |
|---|---|
| 推理 | 这个音符该走哪个 G2P |
| UI | 这个音符显示什么语种 |
| 导入导出 | 这个音符的语种是什么 |

三个消费者没有共享「这个值是谁决定的」这个概念。一旦片段语种变化，有效值漂移，三个消费者会同时看到不同的东西。

### 1.3 四次局部修复叠加成现在的行为

这不是某一次写错，是四层补丁叠出来的。

**第一层，活引用继承。** 为了让新画的音符自动跟随片段语种，把空串定义为 auto，解析时读 `m_clip->effectiveDefaultLanguage()`。这解决了「新音符要自动跟随」，代价是音符的有效值不再是它自己的属性，而是随片段变化的时间函数。

**第二层，latch 回写。** 为了让工具栏的语种下拉显示解析结果，[ClipEditorToolBarView::refreshLanguageComboPresentation](../../src/app/UI/Views/ClipEditor/ToolBar/ClipEditorToolBarView.cpp#L639) 在歌手已解析时把歌手默认语种写回片段字段。轨道侧还有一份更激进的版本，[TrackControlView::refreshLanguageComboPresentation](../../src/app/UI/Views/TrackEditor/TrackControlView.cpp#L379) 连解析状态都不检查，直接写。于是「跟随歌手」这个状态在用户什么都没做的情况下被固化成了具体值。之后换歌手，片段字段不再是空的，音符不会跟随新歌手，而是继续按旧歌手钉住的语种解析。这就是工单 A 的时间炸弹，写入发生在很久以前，爆发发生在换语种或换歌手的那一刻。

**第三层，导入固化。** MIDI 转换会将传入的导入语种写给音符，交互入口传入应用默认语种（[MidiConverter.cpp#L24](../../src/libs/ProjectConverters/MidiConverter.cpp#L24)、[MidiConverterUi.cpp#L36](../../src/app/Modules/ProjectConverters/MidiConverterUi.cpp#L36)）。DSPX 转换则保留文件里的音符语种，不应混同为应用默认值覆盖。两条路径都可能带来一批已有明确语种的音符。用户希望工具栏能批量修改它们，而「只影响新音符」的规则无法满足，于是有了工单 B。

**第四层，没有推理钩子。** 片段语种变化只 bump 了 revision，没有订阅者触发重新推理（[SingingClip::setDefaultLanguage](../../src/libs/ProjectModel/AppModel/SingingClip.cpp#L212)）。所以前两层造成的错误不会当场暴露，要等到导出或重新打开工程才变成「音符不发声」。这解释了为什么这个 bug 的内测反馈来得这么晚。

早先的方案文档里也留着这个矛盾的书面证据。它写的是「只影响**新音符**以及**仍处 auto 的音符**」。前半句是多余的，因为新音符一出生就是 auto，已经被后半句覆盖。而它隐含的「存在一批不再是 auto 的音符」，指的其实是导入时被写死或用户手改过的音符，不是 latch 的产物。

### 1.4 社区工作流改变了产品定位

内测反馈里最集中的一类用户，工作流是这样的：

1. 从 DAW 或其他编辑器导入工程。
2. 音符、歌词、语种一起进来。
3. 试听，发现某些句子发音不对。
4. 改语种，再听。

他们的语种大多**不是在本编辑器里产生的**。所以语言控件对他们的意义是「批量声明 + 批量修复」，不是「新音符默认值」。这决定了三件事：

- 语种必须能在已有音符上批量生效，而且必须是显式动作，不能靠「新建时继承」这种隐式机制。
- 冲突必须能看见。他们要判断「是歌手不支持，还是我选错了」。
- 编辑器不能假装自己知道用户想要什么。它能做的是解析和暴露。

### 1.5 歌手能力是这次重构的约束核心

SynthV 可以让所有歌手支持所有语种，因为音素空间统一，语种只是 G2P 的选择。我们的歌手是异质的：

- 歌手的词级转换语种取决于其可寻址的 G2P / S2P 路由与包内语种资源。
- `SingerCapabilitySummary::effectiveLanguages` 是各模型音素语言表键的交集，不是转换路由集合。`effectivePhonemes` 则表达模型音素表的共同部分。
- 词级转换语种与输出音素的字典语种可以不同。模型语言表既不能为歌手新增转换路由，也不能直接按同名交集裁掉合法的转换路由。
- 同一个工程里的两个歌手，转换路由与模型音素能力都可能不同。

于是「应用设置里的默认演唱语种」这个选项缺少一个前提：**歌手**。没有歌手的语种偏好是一个悬空的声明。它可能在用户选了一个不支持该语种的歌手后变成一条永远无法兑现的承诺，而用户从未被告知。

解决方案不是删掉偏好，而是把它降级为**创建时的建议**，并且明确它可能被能力静默否决。同时把「能力是否满足意图」变成可见的诊断。

### 1.6 术语

后文出现的语种概念严格区分如下：

| 术语 | 含义 | 作用域 |
|---|---|---|
| 转换语种 | 音符声明的语种，决定 G2P / S2P 路由 | 音符 |
| 合成语种 | 单个音素的字典语种，决定 SP token 路由 | 音素 |
| 录入语种 | 用户填词时选择的语种，只影响填词预选和显示 | 片段 |
| 歌手默认语种 | 声库包声明的首选转换语种，不可路由时按有序路由回退 | 歌手 |
| 默认演唱语种 | 应用设置里的用户偏好，只在新音符创建时兑现 | 应用 |

## 2. 设计公理

以下六条是后续所有决策的依据。实现细节与它们冲突时，以它们为准。

1. **意图与有效值分离。** 文档只存用户意图。有效值每次由意图加当前环境算出来，不落盘。
2. **解析只依赖两类输入。** 音符声明的语种，当前歌手的默认语种与能力。不读片段语种、轨道语种、应用设置。
3. **能力优先于偏好。** 偏好只在创建时兑现，不参与解析。兑现不了就静默留空。
4. **显式声明可见，不静默改写。** 用户明确指定的东西，系统只能提示冲突，不能替他改。
5. **语种分两层。** 词级语种是转换意图，音素级语种是合成属性，两者可以不同，且都合法。
6. **文档不存派生值。** `unknown`、latch 回写的有效值、`defaultG2pId` 都是派生值，不写入文档。

## 3. 决策记录

| 编号 | 决策 | 依据 | 被否掉的备选 |
|---|---|---|---|
| D1 | 音符语种只保留两态，未指定与已指定 | `unknown` 是检测失败的产物，不是用户意图。检测失败应该表现为未指定 | 保留三态。否，它让「未指定」和「检测失败」在 UI 和推理里都要分叉 |
| D2 | 未指定只按歌手转换路由解析，优先默认，否则取首项 | 不向片段、轨道语种或应用偏好继续回退，解析结果来源唯一可解释 | 保留音符到片段到轨道到应用的四级链路。否，它正是本次缺陷的来源 |
| D3 | 删除轨道级语种 | 轨道不参与合成，它的语种没有消费者，存在只让用户以为「设了就会生效」 | 保留作为批量默认。否，批量应该是显式动作 |
| D4 | 片段语种改名录入语种，只做填词预选 | 需要记住用户填词时选的语种，避免每次重选，但它不能影响推理 | 直接删除。否，社区会反复填词 |
| D5 | 应用级默认演唱语种降级为创建时偏好 | 它缺少歌手这个前提。作为偏好仍有价值，但不能参与解析 | 删除。否，用户仍希望新建音符默认用某种语言 |
| D6 | 默认歌词按新音符的实际解析语种查表 | 已有 `defaultLyrics` 表可复用，关键是歌词与音符语种来自同一次创建决策 | 无条件按歌手默认语种查表。否，会与受支持的应用偏好错配 |
| D7 | 冲突用音符级红叉，暂不做汇总徽标 | 现有 `noteInferenceErrors` 通道已经能表达，汇总徽标要新造 UI 且信息密度低 | 汇总徽标。暂缓，不是永久放弃 |
| D8 | 片段歌手下拉切换前汇总语种冲突和将清除的手工发音 | 切回轨道歌手同样是有效歌手变化。即使没有声明冲突，也可能使旧语种发音失效 | 只统计不支持的声明。否，会漏掉未指定音符的发音损失 |
| D9 | 按目标 voice context 构造复合动作 | `clips.set_voice` 与 `clips.use_track_voice` 共用修复与级联逻辑。History 无需通用事务，`ActionSequence` 嵌套已有范式，见 [MoveClipsWithNewTrackActions](../../src/app/Controller/Actions/AppModel/Clip/MoveClipsWithNewTrackActions.cpp#L5) | 用指定歌手命令代替继承。否，会丢失轨道继承关系 |
| D10 | 改词级语种清派生音素、保留手工音素，清手工发音 | 声明变化或解析语种变化都触发词级级联。手工音素可跨语种，手工发音不能无提示复用于新语种。清除与原操作同一步撤销 | 只看存储的 `language`。否，会把旧拼音送入新语种路由 |
| D11 | 填词选择器用「优先级加兜底」 | 英日混杂需要保留 tagger 的明确判断，同时给未识别项一个确定归属 | 强制全部。否，会把已识别正确的英文词也改成日语 |
| D12 | 不做老工程兼容迁移 | 内测期用户量小，迁移规则本身可能制造新错误 | 写迁移器。否，成本高于收益 |
| D13 | 不改 slicer 的整句丢弃 | 这是算力保护策略，不是缺陷。用户应该修好那一句 | 拆句逐音符失败。否，违背设计意图 |
| D14 | 手工音素的音符计入转换语种冲突统计 | 词级语种仍是声明。保留手工音素不代表新歌手能接受其 token 与字典语种 | 排除或保证正常合成。否，都会掩盖不同层的兼容性问题 |

## 4. 目标模型

### 4.1 概念与载体

| 概念 | 载体 | 持久化 | 参与推理 |
|---|---|---|---|
| 音符语种 | `Note::m_language` | opendspx `Note.language` | 是，转换路由 |
| 音素语种 | `PhonemeName::language` | 音符 workspace `phoneme` | 是，合成路由 |
| 录入语种 | `SingingClip::m_inputLanguage` | 片段 workspace `inputLanguage` | 否 |
| 歌手默认语种 | `SingerInfo::defaultLanguage` | 声库包 | 是，未指定时的解析 |
| 转换语种能力 | 歌手上下文的转换路由元数据 | 声库包 | 是，解析与词级冲突校验 |
| 模型音素能力 | `SingerCapabilitySummary` | 声库包 | 是，音素兼容性诊断，不直接决定词级语种 |
| 默认演唱语种 | `GeneralOption` | 应用设置 | 否，仅创建时 |
| 默认歌词 | `GeneralOption::defaultLyrics` | 应用设置 | 否，交互新建和无歌词导入时的占位 |

### 4.2 音符语种

```cpp
enum class NoteLanguageState {
    Unspecified,
    Specified,
};
```

- 空串表示未指定，`setLanguage("")` 回到未指定。
- 读入时 `"unknown"` 归一为未指定。写出时未指定写空串。
- `Note::effectiveLanguage()` 删除，换成 `languageState()` 与 `resolvedLanguage()`。
- `Note::resolvedLanguage()` 返回词级解析语种。已指定时仍返回声明值，不能把返回非空视为歌手已解析或可推理。未指定且无可用转换路由时返回空串。
- [Note::serialize](../../src/libs/ProjectModel/AppModel/Note.cpp#L248) 保持「空则不写」的行为。

UI 展示规则：

| 状态 | 有歌手 | 显示 |
|---|---|---|
| 未指定 | 是 | 跟唱歌手（普通话） |
| 未指定 | 否 | 跟唱歌手（未选择歌手） |
| 已指定 | 任意 | 语种名，不支持时附警示 |

### 4.3 解析算法

词级能力由与语言服务同源的路由元数据投影提供，不从模型语言表推导：

```text
conversionSupport(singer) -> Known(ordered routes) | Unresolved(reason)

    resolve exact packageId/version/singerId metadata
    if metadata cannot be resolved: return Unresolved(reason)
    candidates = singer manifest languages if nonempty else package languages
    routes = candidates addressable by the language service metadata rules
    return Known(stable unique routes by languageId)

supported(singer):
    support = conversionSupport(singer)
    return support.languageIds if support is Known else []
```

契约：

- 非空歌手声明限定候选，但声明的语种仍须有对应包级资源。歌手声明为空时遵循语言服务已有的包级语种兜底，不能误判为禁止全部语种。
- 补充与 `resolveLanguageRoute` 同源的元数据查询或投影，精确指定歌手与包版本。现有 `SingerInfo::languages()` 不足以完整表达包级兜底，不在 UI 再实现一套近似路由规则。
- `Known([])` 表示确定没有可寻址转换路由，不回退到模型语言表、其他歌手或全局 G2P。`Unresolved` 表示尚未选择歌手、元数据未完成或包/歌手解析失败，不能伪装成已知空集合。
- 可寻址只代表元数据支持。插件、模型、字典的运行时加载失败仍是独立的 G2P / S2P 错误，不能因一次 `ensureLanguageReady` 失败改写支持集合。也不为填充选择器提前加载所有模型。
- `SingerCapabilitySummary::effectiveLanguages`、`effectivePhonemes` 只用于最终音素与模型约束的诊断，不能直接与词级转换语种求交集。模型能力为空时要区分未能分析的 `Degraded` 与已确认不一致的 `Inconsistent`，不能把两者都回退为可合成。

解析：

```text
fallbackLanguage(singer):
    routes = supported(singer)
    if routes.isEmpty(): return ""
    d = singer.defaultLanguage()
    return d if d in routes else routes.first()

resolve(note, singer):
    if note.state == Specified: return note.language
    return fallbackLanguage(singer)
```

- 显式声明始终保留。若没有该转换路由，按该语种请求转换会产生路由错误，不能偷偷改用歌手默认语种。没有结果音素时仍按既有 slicer 策略整句丢弃。
- 不把模型不兼容等同于转换失败。有转换路由时，G2P / S2P 可能成功，但结果音素仍不被模型接受，必须通过独立的音素兼容性校验或后端错误暴露，不能假定一定在 slicer 前失败。
- 歌手默认语种不可路由时，回退到有序路由列表的首项。声明顺序或包级兜底顺序属于公开契约，不用哈希容器遍历次序代替。
- 无歌手时未指定返回空串，已指定仍可返回声明供 UI 展示，但推理不会启动，也不产生无歌手的转换冲突。已选歌手却解析失败时沿用声源解析错误，不伪报为某个语种不支持。

### 4.4 冲突

```text
conflicts(clip, candidateSinger):
    support = conversionSupport(candidateSinger)
    if support is Unresolved: return []      # pending/source errors are separate
    return group notes by note.language
        where note.state == Specified and note.language not in support.languageIds
```

`Known([])` 时所有显式声明均冲突，而不是全部通过。未指定且没有可用路由时，显示歌手无可用转换语种的上下文诊断，不虚构一个声明冲突。`Unresolved` 不代表兼容，UI 单独表达未解析或失败状态。

产出按语种分组，每组带音符 id 列表，每条记录附带该音符是否使用手工音素。手工音素的音符计入统计（D14），诊断文案不同。这份词级冲突集合不包含最终音素的模型兼容性结论。

### 4.5 语种的两层

| 层 | 字段 | 粒度 | 谁写 | 用途 |
|---|---|---|---|---|
| 转换语种 | `Note::m_language` | 词 | 用户、导入、填词 | G2P 与 S2P 路由 |
| 合成语种 | `PhonemeName::language` | 音素 | S2P 默认写入，用户可改 | SP token 路由 |

桥接点在 S2P 输出。[GetPhonemeNameTask.cpp#L184](../../src/app/Modules/Inference/Tasks/GetPhonemeNameTask.cpp#L184) 当前把输入的词级语种写给每个音素。用户在音素编辑器里逐行改语种，就形成混合语种。混合本身合法，但最终每个音素的字典语种与 token 必须被当前模型接受。`GenericInferModel` 构造输入对象不等于完成了兼容性校验。

这条公理支持 D10 对**音素**的规则：换语种时 `original` 必须重算，音素 `edited` 必须保留。词级的 `pronunciation.edited` 则按第 9 节清除，不属于手工音素保留承诺。

### 4.6 录入语种

- 片段级字段，`""` 表示「跟随歌手」。
- 只做两件事，一是填词对话框的默认选择，二是工具栏的显示。
- 绝不参与解析，绝不因为改变它而修改任何音符。
- 默认值为 `""`。它不用歌手默认语种初始化，因为它是用户的选择，不是解析结果。
- 用户选择了歌手不支持的语种时，控件显示警示图标，不改写任何东西。

## 5. 持久化与序列化

### 5.1 opendspx

- `Note.language` 保持官方字段，未指定写空串。
- 片段 `inputLanguage` 写入 `ds-editor-lite` workspace。原 `defaultLanguage` 键改名，读取时先找新键，找不到再看旧键。这是一行兼容，不是迁移器。
- 删除片段 workspace 的 `defaultLanguage` 与 `defaultG2pId`。
- 删除轨道 workspace 的 `defaultLanguage` 与 `defaultG2pId`。
- 删除模型里的 `m_defaultG2pId`。它从来没被读过，而且 [SingingClip::defaultG2pId](../../src/libs/ProjectModel/AppModel/SingingClip.cpp#L245) 用的是 own singer 而不是 effective singer，属于死代码加错误来源。
- 音符 `phoneme` 结构不变，`original` 与 `edited` 都保留。

### 5.2 读入归一

| 输入 | 归一为 |
|---|---|
| 无 `language` 字段 | 未指定 |
| `language` 为空串 | 未指定 |
| `language` 为 `"unknown"` | 未指定 |
| 其他 | 已指定该值 |

### 5.3 不做迁移

内测期不做工程级迁移。用户打开旧工程时，音符上写过语种的保留成已指定，空着的变成未指定并按歌手的有序转换路由解析。片段上被 latch 过的值会作为录入语种保留，但不再影响音符。如果结果不符合预期，用户可以批量修，而批量修现在是显式且一次撤销的。

## 6. 交互

### 6.1 片段工具栏

- 语种下拉的标签从「语言」改为「录入语种」。
- 选项为「跟随歌手」加当前歌手可用的转换路由语种。歌手未解析时只显示「跟随歌手」并禁用。已有非空录入值即使不再可用也保留显示并警示，不在刷新控件时抹掉。
- 改变它只写片段字段，不动音符。
- 它的值作为填词对话框的默认选择。

### 6.2 音符右键

现有语种子菜单保留。追加两个条目，作用于选中音符或光标所在音符：

- 「应用到本片段全部音符」
- 「仅应用到未指定的音符」

这两个是显式批量操作，一次撤销。它们解决社区工单 B 的批量诉求，同时不把工具栏变成隐式批量的开关。

### 6.3 冲突徽标

- 复用现有 `noteInferenceErrors` 通道和带圈叉号。
- 新增原因 `UnsupportedLanguage`，标题「转换语种不受支持」。
- 枚举名 `SliceExclusionReason` 现在不再只描述 slicer 的排除原因。建议同期改名为 `NoteInferenceIssueReason`，或者至少加注释说明扩展来源。
- 正文格式：`该歌手未提供普通话转换路由，可用语种为英语、日语。` 已知路由为空时明确显示「无可用转换语种」，能力未解析时不伪报为不支持。
- 手工音素的音符追加：`手工音素已保留，能否合成取决于当前歌手是否支持这些音素。`
- 同一音符既有转换语种冲突又有缺音素时，转换语种冲突优先作为摘要显示，同时保留可查看的其他原因。
- 最终结果音素的 token 或字典语种与模型不兼容时，给出独立的音素兼容性诊断，不能拿词级冲突替代。保留音素不保证合成成功。已有后端验证失败必须传达给用户，静态校验无法确定时不显示成功保证。

数据合流：`SingingClip` 新增一份转换语种冲突缓存，与 slicer 和音素兼容性诊断合并后由 `noteInferenceErrors()` 统一返回。转换冲突在音符语种、有效歌手或路由元数据变化、片段装载和重新分段时刷新。音素兼容性还随最终音素、模型能力变化刷新。几何变化本身不改变这两种兼容性。

### 6.4 换歌手弹窗

- 只在用户主动切换歌手时弹，包括片段歌手下拉里的「使用轨道歌手」、轨道歌手下拉与声源预设。撤销、重做、导入、自动化都不弹。没有转换冲突且不会清除手工发音时直接提交。
- 提交前预检目标 voice context，分别统计不支持的语种声明和因词级语种变化而将清除的手工发音。手工发音按实际清除的音符去重计数，不能用冲突音符数代替。
- 内容为按语种分组的冲突计数，例如「普通话 20 个，粤语 17 个」，以及每种提交选项将清除多少个手工发音。明确说明「手工音素保留，清除可随本次操作一起撤销」。
- 三个动作：
  - 取消：不改变 voice context，不改任何音符。
  - 保留语种声明：切换 voice context，不修复 `Note.language`，冲突留给红叉。解析语种变化所必需的发音级联仍执行，不承诺音符全部字段不变。
  - 修复为目标语种：按钮标出实际目标语种，将冲突声明改为 `fallbackLanguage(targetSinger)`，切换 voice context 并执行级联，一次撤销。目标没有可用转换语种时禁用此项。
- 即使只有未指定音符、没有声明冲突，只要其解析语种变化会清除手工发音，也必须显示确认。
- 轨道级切换复用同一份预检与确认，统计范围只含跟随轨道的片段。轨道参数没有修复选项，因此只提供取消与继续，继续时保留声明、只清除解析语种变化所必需的数据，并且不能跳过第 9 节的正确性级联。

### 6.5 设置页

- 「默认演唱语种」保留，标签改为「新建音符的默认语种」。
- 说明文字：`交互新建音符会优先按此语种标记。若当前歌手不支持或尚未解析，音符保持未指定。默认歌词与新音符实际使用的语种一致。`
- 下拉不再包含 `unknown`。
- 建议工厂默认值为空，避免预先假定普通话，仍待第 13 节确认，实施前不据此修改现有默认值。
- 「默认歌词」设置按已有的 `defaultLyrics` 表提供编辑，每行一个语种，一列歌词。行来自已知语种与已配置语种的并集。查询复用 `defaultLyricForLanguage`，空值兜底保持 `la`。

创建规则由同一个无副作用策略同时返回存储语种与占位歌词，不能分别在画音符 helper 和 DTO 构造层猜测：

```text
newNoteDefaults(singer, preference, defaultLyrics):
    storedLanguage = preference if preference in supported(singer) else ""
    lyricLanguage = resolve(note with storedLanguage, singer)
    lyric = defaultLyricForLanguage(lyricLanguage, defaultLyrics)  # existing lookup semantics
    return {language: storedLanguage, lyric: lyric}
```

- 偏好受支持时，标记该语种并查询它的歌词。偏好为空、不支持或能力未解析时，保持未指定，再按当前歌手可解析出的语种查歌词。仍无语种时使用 `la`，不重新读应用偏好。
- 钢琴卷帘等交互创建入口用该策略填好 `NoteDraftDto.language` 和 `lyric`。不能只修改 `defaultLyricForNewNote` 的查询来源。
- 通用自动化创建与 `buildNote` 只兑现 draft，不隐式读取 GUI 设置。缺省或显式空语种均为未指定。需要与交互创建一致的上层调用者须显式使用同一策略后再提交。
- 导入时保留源文件的显式语种与歌词。确需为缺失内容生成默认值的交互入口使用该策略，无法确定目标歌手时不强写应用偏好。不在项目装载后遍历填充语种。

### 6.6 填词对话框

- 在文本输入页的拆分选项旁增加「语种」下拉。
- 选项为「自动识别」加当前歌手可用的转换路由语种。没有歌手时只有「自动识别」。
- 初值取 `clip->inputLanguage()`。已有值不再可用时保留警示，要求重选或明确改为自动识别，不能悄悄当作有效兜底。
- 改变语种会按当前拆分模式重新拆分，因为 tagger 的优先级顺序影响分词结果。沿用已有的预览覆盖确认，拒绝时保留原选择和预览。接受后更新同一份上下文，片段字段在最终确认填词时写回。

## 7. 填词链路改造

### 7.1 现状

[ClipController::onFillLyric](../../src/app/Controller/ClipController.cpp#L613) 构造 `LyricDialog`，经 `LyricTab`、`LyricBaseWidget`、`LyricSplitter`、`G2pService::convert`，最后回写音符。

关键事实：

- 优先级列表的第一个元素是 `clip->defaultLanguage()`（[ClipController.cpp#L634](../../src/app/Controller/ClipController.cpp#L634)）。一个被 latch 过的片段语种会偏置 tagger。
- [TextTagger::tag](../../src/app/Modules/FillLyric/Utils/TextTagger.cpp#L266) 的优先级只重排 tagger 的询问顺序，规则只改 `language == "unknown"` 的 token，先命中者胜。
- [G2pService::convert](../../src/app/Modules/FillLyric/Utils/G2pService.cpp#L61) 再次 tag，未识别项落回 tagger 结果，按语种分组后转换。
- 回写时 `edit.language = noteResult.language`（[ClipController.cpp#L683](../../src/app/Controller/ClipController.cpp#L683)）。

### 7.2 新规则

选择器同时决定 tagger 优先级与兜底。空选择表示自动识别，不把片段字段直接放回音符解析链。所有列表保序去重，歌手默认语种只有在可用转换路由集合内才加入。

```text
supported = supported(singer)        # conversion routes, not model language-table keys
choice = selector choice            # empty means auto detect
require choice is empty or choice in supported

base = valid singer default + supported
priority = unique(base) if choice is empty else unique([choice] + base)
tagged = tag(text, priority)         # TextTagger still appends other registered taggers

for each token in tagged:
    lang = token.language
    if choice is not empty and (lang == unknown or lang not in supported):
        lang = choice
    write Unspecified if lang == unknown else Specified(lang)
```

- **自动识别模式**保留 tagger 的判断，包括歌手未提供转换路由的语种，也保留与歌手默认语种相同的识别结果。识别到的语种一律写入声明，用户没有声明兜底，冲突应可见。
- **指定语种模式**先让所选语种的 tagger 尝试，再让其他语种识别。已识别且有转换路由的其他语种保留，未知或无转换路由的结果落到所选语种。不强制把全部词改成所选语种，也不保证最终音素一定可被模型合成。
- 普通话与粤语都用汉字规则，因此选择粤语必须使粤语 tagger 先于普通话。歌手默认普通话、同时支持两者时，选择粤语后汉字应标记 `yue`。选择英语或日语时，其他 tagger 已识别的异语种仍可保留。
- 拆分、预览 G2P、确认回写共用同一份选择与优先级上下文。`G2pService` 的第二次 tag 不能重新退回歌手默认优先，且不得覆盖已有明确语种的 token。
- `inputLanguage` 是初始选择来源，不是有效选择的保证。换歌手后若保存值已不在转换路由集合中，展示警示并停用导入按钮，要求用户重选或明确选择自动识别，不把无效值用作兜底，也不静默覆写保存值。
- 预览里已有手工修改的音节时，切换语种先确认再重建预览。取消时把下拉框与上下文选择一起还原，修改原样保留。

### 7.3 代码触点

- `ClipController::onFillLyric`：构建转换路由上下文、持久化用户选择的 `inputLanguage`。
- `LyricDialog`、`LyricTab`、`LyricWrapView` 与 `LyricBaseWidget`：传递同一份选择、优先级与可用转换语种，在选择变化后重新拆分。
- `G2pService::convert`：接收同一份填词上下文，统一 tagger 优先级与兜底，不另建一套规则。
- 回写处：未知结果映射为未指定，其他结果显式存储，提交前重新校验选择仍可用。
- `FillLyricOption::exportLanguage`：确认无消费者后删除。它是持久化字段，但只有写没有读。

## 8. 复合命令与撤销

### 8.1 现状

`HistoryManager` 是一个平栈，[record](../../src/libs/History/HistoryManager.cpp#L44) 一次压一条。每个命令走 [CommandCommitter::commit](../../src/app/Automation/CommandCommitter.cpp#L8)，固定 `execute` 后 `record`。没有跨命令事务。

### 8.2 方案

扩展 `clips.set_voice` 与 `clips.use_track_voice`，增加同一个可选参数：

```text
language_repair: "none" | "map_unsupported_to_default"     # default "none"
```

二者共用面向目标 voice context 的动作构造，不能把恢复继承转换为一次普通的指定歌手。目标同时包含继承状态、片段自有上下文和解析后的有效歌手。

命令内部：

1. 解析目标上下文与受影响片段。恢复继承时，以轨道的有效歌手预检，但保留片段的继承标记及自有配置。
2. 捕获每个音符的旧声明、旧解析语种及可撤销数据，根据目标上下文计算新解析语种。
3. 若 `language_repair != none`，用目标转换路由集合计算冲突，先确定新声明，再计算最终解析语种。目标取歌手可用默认语种，否则取有序转换路由的首项。无合法目标时拒绝修复，不写入空串冒充成功。
4. 对声明变化或解析语种变化的音符应用第 9 节共享级联。`language_repair == none` 仅表示不修复语种声明，不能关闭必要的手工发音清除。
5. 构造复合动作，`SpeakerMixActions` 改变上下文，音符动作应用预先算好的声明与级联结果，一次 `commit`。不得在动作执行中根据已经改变的上下文重新推导旧值。

复合动作参考 `MoveClipsWithNewTrackActions` 的写法，把子序列作为 `IAction` 加入，并把 focus transition 上提到复合序列。

性质与入口边界：

- 单 History、单 revision。资源与参数校验在执行前完成，动作执行阶段不新增可能失败的资源加载，不依赖 `ActionSequence` 提供失败回滚。
- 撤销一次同时恢复继承关系、自有与有效上下文、音符声明以及被清除的手工发音。重做使用相同的动作数据，不重新弹窗。
- 自动化侧可使用同一修复参数，不需要自己拼两步。自动化不弹窗，但遵循相同的级联和撤销契约。
- 声源预设、混合配置等其他会改变有效歌手的片段入口也复用语种变化级联，不要求它们都新增修复选项或弹窗。
- 轨道上下文变化必须在同一次轨道命令内处理所有实际跟随它的片段，清除解析语种变化导致失效的手工发音。独立声源片段不受影响。轨道级显式语种修复与汇总弹窗仍可暂缓，但正确性级联不能暂缓。
- 继承片段跨轨移动也可能改变有效歌手。[MoveClipToTrackAction](../../src/app/Controller/Actions/AppModel/Clip/MoveClipToTrackAction.cpp#L55-L70) 及移动到新轨道的复合路径，必须将必要级联纳入原移动动作，一次撤销恢复轨道归属和手工发音。不为移动新增弹窗或声明修复选项。

### 8.3 预检与提交的一致性

UI 按目标 voice context 预检两种提交选项，显示冲突集合与实际将清除的手工发音数量。预检绑定当前文档 revision 和目标路由元数据版本，不能只比较歌手显示名。

命令内部重新计算冲突、最终解析语种与级联集合，不信任 UI 传来的音符列表。提交时若预检依据已变化，返回过期结果，由 UI 重新展示，不在用户确认的数量之外静默多清除一批发音。自动化不依赖弹窗，但同样执行命令级校验。

### 8.4 快照字段

`ClipSnapshotDto::effectiveDefaultLanguage` 换成 `inputLanguage`。公开 schema 的 `default_language` 同步改名为 `input_language`。这是破坏性变更，按 D12 不做兼容别名。

## 9. 音素级联规则

### 9.1 规则

| 变化 | `nameSeq.original` | `nameSeq.edited` | `offsetSeq.original` | `offsetSeq.edited` | `pronunciation.edited` |
|---|---|---|---|---|---|
| 歌词 | 清空 | 清空 | 清空 | 清空 | 清空 |
| 语种声明变化，或 `resolvedLanguage` 变化 | 清空 | 保留 | 清空 | 保留 | 清空 |
| 换歌手但声明和解析语种均不变 | 重新推理 | 保留 | 重新推理 | 保留 | 保留 |
| 仅发音 | 保留 | 保留 | 保留 | 保留 | 按选项 |

- 语种单独变化时保留音素 `edited`，因为手工音素可能是跨语种构造的。`pronunciation.edited` 是词级输入，必须清除，不能将旧语言注音无提示地送入新路由。
- 清除词级手工发音与导致变化的文档操作一起提交、一起撤销。不能只依靠换歌手重跑 G2P，现有 G2P 只写 `pronunciation.original`，结果读取仍优先采用非空 `edited`。
- `pronunciation.original` 和候选发音也是派生值，在歌词或词级语种变化时失效并重算。异步回写必须验证任务使用的文档与声源上下文仍有效，不能把旧语种结果写回新上下文。
- 声源操作前捕获旧解析语种，再按最终 voice context 和修复后的声明计算新解析语种。变化判断不依赖 `Note.language` 是否被写入，同样覆盖未指定音符从 `cmn` 跟随到 `yue` 的情况。
- 初次装载只建立当前上下文和诊断，不伪造一次切歌手历史，也不推测旧手工发音的来源语种。后续文档操作执行上述级联。无来源信息的历史发音不做兼容迁移（D12）。

### 9.2 必须先合并的重复

级联逻辑现在有两份：

- [EditNoteWordPropertiesAction::applyCascadeResets](../../src/app/Controller/Actions/AppModel/Note/EditNoteWordPropertiesAction.cpp#L6)
- [NoteAutomationFacade::normalizedWordProperties](../../src/app/Automation/NoteAutomationFacade.cpp#L179)

两份内容基本重合，`setWordProperties` 会先后经过两者。改规则前先合并成一个共享函数，否则必然只改一处。

共享函数显式接收旧/新词属性与旧/新解析语种，输出待应用的级联结果，不在内部读取已改变的全局声源。音符属性编辑、片段或轨道上下文切换、继承片段跨轨移动都复用它。命令将计算结果纳入自身动作，不靠模型通知回调另起命令或修改手工数据。与已有音素偏移归一逻辑协调，不能在另一条路径无提示地覆盖本节的手工数据保留规则。

### 9.3 音素编辑器

`original` 为空时禁用「Reset Phones」。它现在只判断 `m_isPhoneEdited`，在 S2P 未跑或失败时会出现空白重置。

## 10. 分期

| 阶段 | 内容 | 验收 |
|---|---|---|
| P0 | 合并级联逻辑，补现有行为回归用例 | 现有行为不变，共享规则成为唯一维护点 |
| P1 | 两态模型、同源转换路由投影、`LanguageCompatibility`、解析改造 | 路由元数据与模型能力分离，已知空与未解析可区分 |
| P2 | 删除写回点，片段字段改名，清理轨道语种与 `defaultG2pId`，持久化、设置与创建策略 | 新建语种和歌词匹配，导入保留源意图，保存重开符合模型 |
| P3 | 声明及解析语种级联，覆盖片段/轨道声源变化、跨轨移动，修复重置按钮 | 旧语种手工发音不会进入新路由，手工音素保留，原操作一次撤销 |
| P4 | 转换冲突与音素兼容性诊断 | 已知无路由、未解析和音素不兼容不混淆，不错误承诺可合成 |
| P5 | 两个片段命令增加 `language_repair`，共享复合动作与换歌手弹窗 | 保持继承语义，预检过期不提交，取消无写入，一次撤销全部恢复 |
| P6 | 填词选择器与批量动作 | 粤语汉字及英日混杂按规则落位，链路优先级一致，批量一次撤销 |
| P7 | 清理 `exportLanguage`、枚举改名、旧文档标记 | 无死字段，无重复规则 |

依赖关系：P1 是 P2、P4、P6 的前提。P3 依赖 P0 和 P1，P5 依赖 P3 与 P4。填词交互和创建入口对接在 P2 后完成，不将未接入级联的换歌手弹窗单独作为可交付功能。

## 11. 测试

### 11.1 单元

- 转换能力，同一元数据下投影与语言服务可寻址路由一致。覆盖非空声明、空声明使用包级路由、声明缺少包资源、包版本未解析、`Known([])` 与 `Unresolved`。运行时模型加载失败不改写元数据支持集合。
- 模型语言表多于声明时，不提供未声明的转换路由。合法转换路由与输出音素语言不同名时，不按模型语言表误删路由。模型能力分析失败与已知不一致分别保留其状态。
- 解析矩阵，已指定支持/不支持、未指定有默认、默认不可路由时稳定回退首项、无歌手、已知无路由、元数据未解析。
- 冲突分组，部分支持、全部不支持、已知无路由时全部显式声明冲突、未解析时不伪报不支持、手工音素标记。
- 读入归一，缺字段、空串、`"unknown"`、合法值。
- 创建策略，偏好受支持、不支持、为空及歌手未解析。歌手默认日语且支持英语、应用偏好英语时，断言创建为英语并取英语歌词 `la`，而不是日语歌词 `ら`。
- 级联规则，歌词变化、声明变化、声明不变但解析语种变化、换歌手但语种不变。断言手工音素的保留与手工发音清除符合第 9 节，组合变化以歌词规则优先。

### 11.2 集成

- 指定新歌手并修复语种，一次撤销恢复上下文、声明、手工发音和手工音素，文档 revision 每次提交只涨一次。重做不重新弹窗。
- 片段独立中文歌手切回轨道英文歌手并修复，提交后仍处继承状态。撤销恢复独立上下文与原音符数据，后续轨道切换可验证继承没有被变成独立指定。
- 保留语种声明时，显式不支持声明不变且产生冲突。未指定音符从普通话跟随到粤语，旧 `ni3` 手工发音被清除，之后采用新原始发音。撤销同时恢复歌手与 `ni3`。
- 声源预设、自动化与轨道切换复用级联。轨道切换只处理跟随片段，不修改独立片段。自动化默认不修复声明，但仍清除失效的词级发音且可一次撤销。
- 继承片段从普通话轨道移到粤语轨道，清除旧 `ni3` 手工发音、保留手工音素，撤销移动同时恢复发音与归属。覆盖移动到新轨道的复合路径，独立声源片段移动时不因轨道歌手不同而误清除。
- 初次装载不伪造清除历史。装载后切换上下文、保存与重开，验证已提交的清除结果和保留的手工音素。
- 预检后文档或目标路由元数据变化时，不按旧确认静默提交。修复目标无可用路由时拒绝，无部分修改。
- 交互创建同时传入语种与歌词。自动化 draft 的显式值和空语种不被 GUI 偏好覆盖。导入保留源声明，切换片段和修改录入语种不再 latch 回写。
- 默认普通话且支持普通话/粤语，选择粤语输入汉字，拆分、预览转换与提交均为 `yue`。重选普通话重新拆分后为 `cmn`，验证 G2P 二次 tag 没有恢复旧优先级。
- 自动模式与指定模式分别覆盖英日混杂、未知 token、无转换路由的 tagger 结果。指定模式保留已识别且可路由的其他语种，自动模式暴露不支持声明。
- 保留的手工音素含新模型未知 token 或字典语种时，出现音素兼容性错误，不显示「仍会正常合成」。测试用可控离线表验证错误传播，不下载真实模型。

### 11.3 GUI

- 工具栏显示录入语种，改变它只影响填词预选，不影响已有音符。
- 转换冲突与音素兼容性错误可见，手工音素附加说明不承诺成功合成。
- 选择具体歌手与切回轨道歌手均覆盖取消、保留声明、修复三种选择。取消不写入，无可用目标时修复禁用。
- 只有未指定音符的手工发音将失效、没有声明冲突时，也显示清除数量并允许取消。撤销后控件与手工发音同时恢复。
- 换歌手后保存的录入语种已不可用时，显示警示，不能作为填词兜底直接提交。
- 音素编辑器重置按钮在 `original` 为空时禁用。

## 12. 明确不做

- 不做通用跨命令事务。片段指定与恢复继承复用 voice context 复合动作，其他声源入口只接入必要的解析语种级联。
- 不做整句级冲突提示，保留音符级。
- 不做汇总徽标，暂缓。
- 不改 slicer 的整句丢弃策略。
- 不做老工程迁移器。
- 不保留 `unknown` 第三态。
- 不保留轨道级语种。
- 不在工具栏语种上做隐式批量，批量必须是显式菜单动作。

## 13. 待确认与实施检查

1. 「默认演唱语种」的工厂默认值是否改为空。改为空更中立，但新用户首次创建音符不再预置显式语种标记，仍可按歌手解析。
2. 手工音素音符的转换冲突是否要区分颜色或图标，还是同一红叉加说明即可。不论视觉选择如何，都不承诺手工音素一定可合成。
3. 填词选择变化导致重新拆分时沿用已有覆盖确认，保护用户手工修改的预览。拒绝覆盖时保留原预览及其选择上下文，不能把旧预览配上新语种直接提交。
4. `clips.set_voice` 和 `clips.use_track_voice` 增加参数属于公开 schema 变更。同步修改公开契约定义、执行绑定和自动化契约文档，不只修改 registry。
5. opendspx 的 `Note.language` 空值写法需要在实施时核对。当前转换器无条件写入（[DspxProjectConverter.cpp#L1201](../../src/libs/ProjectConverters/DspxProjectConverter.cpp#L1201)），以校验规则允许空串还是要求省略为准，不改变未指定语义。
6. 第一版暂缓轨道级汇总弹窗与显式语种批量修复。轨道切换仍必须执行解析语种变化级联，并在原命令内支持撤销。片段下拉的「使用轨道歌手」不属于暂缓范围。

## 附录 A：改动清单

按层列出主要改动点。行号只作为定位参考，实施时以符号为准。

### 路由与模型层

- 新增 `src/libs/ProjectModel/Voice/LanguageCompatibility.h/.cpp`，消费只读路由投影，提供 `conversionSupport`、`supportedLanguages`、`fallbackLanguage`、`conflicts`。保持 `Known` / `Unresolved` 区分，不依赖 GUI 设置或模型加载状态。
- [PackageManager.cpp](../../src/libs/PackageManager/PackageManager.cpp)、[SynthrtEngine.cpp](../../src/libs/SynthrtEngine/SynthrtEngine.cpp)：接通与语言服务同源、精确到包版本和歌手的路由元数据投影，补齐空歌手声明时的包级兜底。模型层仅消费数据，不直接调用引擎。
- [Note.cpp](../../src/libs/ProjectModel/AppModel/Note.cpp)：新增 `NoteLanguageState`、`languageState()`、`resolvedLanguage()`，删除 `effectiveLanguage()`，读入时把 `"unknown"` 归一为未指定。
- [SingingClip.cpp](../../src/libs/ProjectModel/AppModel/SingingClip.cpp)：`m_defaultLanguage` 改为 `m_inputLanguage`，删除 `effectiveDefaultLanguage()`、`defaultG2pId()`、`m_defaultG2pId`、`updateDefaultG2pId()`，信号改名。
- [Track.cpp](../../src/libs/ProjectModel/AppModel/Track.cpp)：删除 `defaultLanguage()`、`setDefaultLanguage()`、`defaultLanguageChanged`、`defaultG2pId()`、`updateDefaultG2pId()`。
- [AppModel.cpp](../../src/libs/ProjectModel/AppModel/AppModel.cpp)：删除 `m_defaultSingingLanguage`、`setDefaultSingingLanguage()`、`setData` 的语种填充、`newProject` 的轨道语种写入。

### 应用层

- `AppContext`：删除 `pushModelDefaults` 及其连接。
- `TrackController`：删除新建轨道与新建片段的语种播种，删除 `changeTrackDefaultLanguage`。
- [ClipController.cpp](../../src/app/Controller/ClipController.cpp)：`onFillLyric` 的上下文与 `inputLanguage` 持久化，回写时把未知结果映射为未指定。
- [ClipEditorToolBarView.cpp](../../src/app/UI/Views/ClipEditor/ToolBar/ClipEditorToolBarView.cpp)：语种下拉改为录入语种，删除 latch。指定歌手与使用轨道歌手共用预检及确认逻辑。
- `TrackControlView`：删除语种下拉与 latch。
- [PianoRollGraphicsViewHelper.cpp](../../src/app/UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsViewHelper.cpp)：使用共享创建策略同时填充 draft 的语种与默认歌词，不无条件读取歌手默认语种。音符诊断展示转换冲突与独立的音素错误。
- `PhonemeView`：`noteLanguage` 改用 `resolvedLanguage()`。
- `PhonemeEditorDialog`：`original` 为空时禁用重置。
- `PianoRollContextMenuController`：语种子菜单增加两个批量动作。
- `GeneralPage`、[GeneralOption.cpp](../../src/app/Model/AppOptions/Options/GeneralOption.cpp)：调整偏好语义，复用已有默认歌词表与 `defaultLyricForLanguage`。
- `AppGlobal`：已知语种列表去掉 `unknown`。

### 级联与推理层

- [EditNoteWordPropertiesAction.cpp](../../src/app/Controller/Actions/AppModel/Note/EditNoteWordPropertiesAction.cpp)、[NoteAutomationFacade.cpp](../../src/app/Automation/NoteAutomationFacade.cpp)：合并级联逻辑，显式传入旧/新词属性和解析语种，声源动作复用同一策略。
- [NoteInferenceSnapshot.cpp](../../src/app/Modules/Inference/Models/NoteInferenceSnapshot.cpp)、`InferInputNote`：改用 `resolvedLanguage()`，不能将已失效的旧语种手工发音送入新路由。
- [SingingClip.cpp](../../src/libs/ProjectModel/AppModel/SingingClip.cpp)：新增转换冲突缓存，与 slicer 及音素兼容性诊断合并。
- [InferController.cpp](../../src/app/Modules/Inference/InferController.cpp)：在语种、有效歌手、路由元数据或最终音素变化后刷新对应诊断，保证旧异步结果不会覆盖新上下文。沿用合并排程，不把手工发音清除放进推理回调另行执行。

### 自动化层

- `ProjectAutomationFacade`、[ProjectAutomationDtos.cpp](../../src/app/Automation/ProjectAutomationDtos.cpp)：`ClipSnapshotDto` 与 `ClipDraftDto` 的语种字段改名，删除轨道语种相关入口。`buildNote` 不读取应用偏好，创建默认值在上层填入 draft。
- [ParameterAutomationFacade.cpp](../../src/app/Automation/ParameterAutomationFacade.cpp)：`selectClipSingleSpeaker` 和 `useTrackVoiceContext` 增加 `languageRepair` 参数，共用目标 voice context 复合构造。其他片段和轨道声源入口接入必要的语种变化级联。
- [SpeakerMixActions.cpp](../../src/app/Controller/Actions/AppModel/SpeakerMix/SpeakerMixActions.cpp)：保留继承与独立上下文动作的区别，复合动作保存可撤销的全部词级变化。
- [MoveClipToTrackAction.cpp](../../src/app/Controller/Actions/AppModel/Clip/MoveClipToTrackAction.cpp)、[MoveClipsWithNewTrackActions.cpp](../../src/app/Controller/Actions/AppModel/Clip/MoveClipsWithNewTrackActions.cpp)：跨轨移动时按实际有效歌手变化应用级联，与移动操作同一撤销步骤。
- [PublicToolContract.cpp](../../src/libs/AutomationWire/PublicToolContract.cpp)：两个片段命令的公开 schema 增加 `language_repair`，快照字段改名。
- [PublicAutomationRegistry.cpp](../../src/app/Automation/Public/PublicAutomationRegistry.cpp)：同步两个命令的执行绑定，不将 registry 当作唯一 schema 定义。
- `SettingsAutomationFacade`、`AppOptionsAutomationAdapter`：偏好字段及说明对齐。

### 转换器

- [DspxProjectConverter.cpp](../../src/libs/ProjectConverters/DspxProjectConverter.cpp)：音符语种空值处理，片段 workspace 写 `inputLanguage`，删除轨道语种与 `defaultG2pId` 的读写，保留源文件的显式音符语种。
- [MidiConverter.cpp](../../src/libs/ProjectConverters/MidiConverter.cpp)、[MidiConverterUi.cpp](../../src/app/Modules/ProjectConverters/MidiConverterUi.cpp)、[DocumentImportController.cpp](../../src/app/Modules/Import/DocumentImportController.cpp)：需要补默认值的入口对齐创建策略，不在工程装载后用应用偏好覆盖已有音符。

### 填词

- `LyricBaseWidget`：新增语种下拉与重新拆分，沿用预览覆盖确认。
- [LyricDialog.cpp](../../src/app/UI/Dialogs/FillLyric/LyricDialog.cpp)、[LyricTab.cpp](../../src/app/Modules/FillLyric/LyricTab.cpp)、[LyricWrapView.cpp](../../src/app/Modules/FillLyric/Controls/LyricWrapView.cpp)：传递选择、优先级、可用路由与 `inputLanguage`，避免中途重新构造不同优先级。
- [G2pService.cpp](../../src/app/Modules/FillLyric/Utils/G2pService.cpp)：接收共享填词上下文，统一二次 tag 和兜底规则。
- `FillLyricOption`：确认后删除 `exportLanguage`。
