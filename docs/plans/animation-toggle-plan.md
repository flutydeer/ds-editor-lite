# 动画三档改开关实现方案（Animation Toggle）

> 状态：✅ 已实施并提交（`f6090d5b`，构建+单测通过）。待用户验收。2026-08-20

## Goal

把"外观设置 → 动画 → 等级"（三档：完全 Full / 减弱 Decreased / 关闭 None）改成**两态开关（开/关）**，动画只有开和关，删除中间档。保留"时长缩放（Duration scale）"。旧配置文件无需兼容——解析不了（缺失/非法值）一律默认**动画开**。允许重构代码。

## 设计决策

| 项 | 决策 |
|----|------|
| 开（ON）语义 | = 原 Full：所有动画启用，时长按 `animationTimeScale` 缩放 |
| 关（OFF）语义 | = 原 None：所有动画即时完成（duration = 0） |
| Decreased 档 | 删除，并入 ON（不再有"轻量" vs "全面"之分） |
| 配置存储 | `animationLevel`（字符串） → `animationEnabled`（bool，默认 `true`）；`animationTimeScale` 保持 |
| load 解析 | 只用 `isBool()` 判断，绝不用 `toBool()` 兜底（旧字符串转 bool 恒为 false 会让动画全关） |
| 枚举层 | 删除 `AnimationGlobal::AnimationLevels` 枚举及 fromString/toString，bool 直通 ThemeManager → IAnimatable |
| 重命名落点 | `setAnimationLevel` → `setAnimationEnabled`；`afterSetAnimationLevel` → `afterSetAnimationEnabled`；`getEffectiveAnimationTime(ms, minimumLevel)` → `getEffectiveAnimationTime(ms)` |

**开/关语义对齐确认**：改前"Decreased"会让部分"重"动画（编辑器视口平滑滚轮、ProgressIndicator 条、ToolTip/Toast 位移动画、EditorViewportAnimation 等 minimumLevel=Full 的调用）变成即时；改后一切由开关决定——ON 全部走动画+缩放，OFF 全部即时。行为对齐旧 Full/None，无中间态。

## 现状（排查结论）

调用链：`AppearanceOption`（存字符串）→ `AppController.cpp:179` `fromString` 转枚举 → `ThemeManager::setAnimationSettings` → 广播给全部 `IAnimatable` 订阅者 → 各控件 `getEffectiveAnimationTime` 计算时长。

| 层 | 文件 | 现状 |
|----|------|------|
| 词汇 | `src/libs/GUI/Animation/AnimationGlobal.h` | `enum { Full, Decreased, None }` + fromString/toString，仅被 IAnimatable.h 引用 |
| 核心 | `src/libs/GUI/Animation/IAnimatable.h` | `m_level`, `setAnimationLevel`, `getEffectiveAnimationTime(ms, minimumLevel)` |
| 广播 | `src/libs/GUI/Theme/ThemeManager.{h,cpp}` | `m_animationLevel`, `setAnimationSettings(level, timeScale)` |
| 边界 | `src/app/Controller/AppController.cpp:179` | `fromString(appearance->animationLevel)` |
| 设置 | Model `AppearanceOption.{h,cpp}` | `QString animationLevel="full"`，键 `animationLevel` |
| UI | `src/app/UI/Dialogs/Options/Pages/AppearancePage.{h,cpp}` | ComboBox 三档，`animationLevelsName`；时长缩放 LineEdit |
| 测试 | `src/tests/TestAnimationSettings/main.cpp` | 三档策略断言（effectiveDuration / 运行时热更 / Progress+TapTempo / ToolTip） |

订阅者（15 个类全部只做 `updateAnimationSettings()`/`updateAnimationDuration()`，无特判分支）：

- App 层：`DialogTitleBar`、`MainTitleBar`、`TabPanelTitleBar`、`EditorViewportAnimation`、`TimeGraphicsView`
- GUI Controls：`WheelInputController`、`OverlayScrollBar`、`OverlaySplitter`、`ProgressIndicator`（inline）、`SeekBar`、`SvsSeekbar`、`SwitchButton`、`TapTempoButton`、`Toast`、`ToolTip`

`getEffectiveAnimationTime` 调用点（17 处）：

- 传了 `minimumLevel=Full` 需删第二参数：`TimeGraphicsView.cpp:549`、`EditorViewportAnimation.cpp:61`、`ProgressIndicator.cpp:530`、`WheelInputController.cpp:291,327,357`、`SwitchButton.cpp:172`、`TapTempoButton.cpp:117`、`Toast.cpp:197`
- 用默认参数不用动：`DialogTitleBar.cpp:131`、`MainTitleBar.cpp:228`、`TabPanelTitleBar.cpp:390`、`OverlaySplitter.cpp:250`、`SvsSeekbar.cpp:550`、`SeekBar.cpp:270`、`OverlayScrollBar.cpp:209,240,282`、`SwitchButton.cpp:173`、`Toast.cpp:195`、`ToolTip.cpp:222`
- 测试 `src/tests/TestAnimationSettings/main.cpp:37`

i18n 现状（translation_zh_CN.ts）：
- 删除串：`Full`→完全, `Decreased`→减弱, `None`→关闭（AppearancePage.h:33），`Level`→等级（AppearancePage.cpp:131）。注意 `Level` 在 other context 3005 行仍有用（勿动）
- 保留串：`Duration scale`→时长缩放, `Animation`→动画
- 新增串：`Enable animations` → 启用动画

---

## 任务分解（只要实施就会一步到位，这里按可验证粒度拆分）

### Task 1：AppearanceOption 改成 bool `animationEnabled`

**文件：**
- Modify `src/app/Model/AppOptions/Options/AppearanceOption.h`
- Modify `src/app/Model/AppOptions/Options/AppearanceOption.cpp`

**Step 1** — AppearanceOption.h 改动：

```cpp
// 删除：
//   // animationLevel and themeId are stored as opaque persisted strings; the
//   // UI/animation and theme systems own the AnimationLevels enum and the theme
//   // vocabulary respectively and convert at the boundary.
//   QString animationLevel = QStringLiteral("full");
// 新增（默认开）：
//   // Animation is a simple on/off toggle; a missing or unparseable value
//   // defaults to enabled.
//   bool animationEnabled = true;
```

键名改为：

```cpp
const QString animationEnabledKey = "animationEnabled";  // was animationLevelKey
```

**Step 2** — AppearanceOption.cpp 的 load/save：

```cpp
void AppearanceOption::load(const QJsonObject &object) {
    // ...
    // Animation default is ON; accept only a real bool (old configs stored
    // strings "full"/"decreased"/"none", which must not turn animations off).
    if (object.value(animationEnabledKey).isBool())
        animationEnabled = object.value(animationEnabledKey).toBool();
    // ...
}

void AppearanceOption::save(QJsonObject &object) {
    // ...
    object.insert(animationEnabledKey, animationEnabled);  // was animationLevelKey string
    // ...
}
```

关键点：`isBool()` 判断，不合法（旧字符串/其他类型）→ 保持默认 `true`。

### Task 2：删除 `AnimationGlobal.h` 词汇层 + IAnimatable 核心改造

**文件：**
- Delete `src/libs/GUI/Animation/AnimationGlobal.h`
- Modify `src/libs/GUI/Animation/IAnimatable.h`

**Step 1** — IAnimatable.h 头部 remove `#include <lite/GUI/Animation/AnimationGlobal.h>`。

**Step 2** — IAnimatable.h 全部替换（bool 直通，删 minimumLevel）：

```cpp
class IAnimatable {
public:
    IAnimatable();
    virtual ~IAnimatable();
    [[nodiscard]] bool animationEnabled() const;
    void setAnimationEnabled(bool enabled);
    [[nodiscard]] double animationTimeScale() const;
    void setTimeScale(double scale);

protected:
    [[nodiscard]] int getScaledAnimationTime(int ms) const;
    [[nodiscard]] int getEffectiveAnimationTime(int ms) const;

    virtual void afterSetAnimationEnabled(bool enabled) = 0;
    virtual void afterSetTimeScale(double scale) = 0;
    void initializeAnimation();

private:
    bool m_enabled = true;
    double m_scale = 1.0;
    bool m_initialized = false;
};

inline bool IAnimatable::animationEnabled() const { return m_enabled; }

inline void IAnimatable::setAnimationEnabled(bool enabled) {
    m_enabled = enabled;
    if (m_initialized)
        afterSetAnimationEnabled(enabled);
}

// ... getScaledAnimationTime unchanged ...

inline int IAnimatable::getEffectiveAnimationTime(int ms) const {
    if (!m_enabled)
        return 0;
    return getScaledAnimationTime(ms);
}

inline void IAnimatable::initializeAnimation() {
    m_initialized = true;
    afterSetAnimationEnabled(animationEnabled());
    afterSetTimeScale(animationTimeScale());
}
```

语义：关 → 全部 0；开 → ms × scale。无中间档。

**Step 3** — 全仓库 grep 确认 `AnimationGlobal` / `AnimationLevels` 无其它残留后 `git rm src/libs/GUI/Animation/AnimationGlobal.h`。

### Task 3：ThemeManager 改为 bool 广播

**文件：**
- Modify `src/libs/GUI/Theme/ThemeManager.h`（:59 setAnimationSettings 签名、:88 m_animationLevel）
- Modify `src/libs/GUI/Theme/ThemeManager.cpp`（:253-263）

```cpp
// ThemeManager.h
void setAnimationSettings(bool enabled, double timeScale);
// ...
bool m_animationEnabled = true;

// ThemeManager.cpp
void ThemeManager::applyAnimationSettings(IAnimatable *object) const {
    object->setAnimationEnabled(m_animationEnabled);
    object->setTimeScale(m_animationTimeScale);
}

void ThemeManager::setAnimationSettings(bool enabled, double timeScale) {
    m_animationEnabled = enabled;
    m_animationTimeScale = timeScale;
    for (auto *object : m_subscribers)
        applyAnimationSettings(object);
}
```

### Task 4：边界转换 AppController.cpp

**文件：** Modify `src/app/Controller/AppController.cpp:179`

```cpp
theme->setAnimationSettings(appearance->animationEnabled, appearance->animationTimeScale);
```

### Task 5：15 个订阅控件签名改造（机械替换）

**模式（h + cpp 各改一行签名/函数头）：**

| 文件 | 位置 |
|------|------|
| `src/app/UI/Dialogs/Base/DialogTitleBar.{h,cpp}` | h:27 / cpp:116 |
| `src/app/UI/Views/MainTitleBar/MainTitleBar.{h,cpp}` | h:38 / cpp:213 |
| `src/app/UI/Views/Common/TabPanelTitleBar.{h,cpp}` | h:41 / cpp:372 |
| `src/app/UI/Views/Common/EditorViewportAnimation.{h,cpp}` | h:24 / cpp:47 |
| `src/app/UI/Views/Common/TimeGraphicsView.{h,cpp}` | h:117 / cpp:579 |
| `src/libs/GUI/Controls/WheelInputController.{h,cpp}` | h:83 / cpp:209 |
| `src/libs/GUI/Controls/OverlayScrollBar.{h,cpp}` | h:44 / cpp:292 |
| `src/libs/GUI/Controls/OverlaySplitter.{h,cpp}` | h:61 / cpp:263 |
| `src/libs/GUI/Controls/ProgressIndicator.h` | inline 161 |
| `src/libs/GUI/Controls/SeekBar.{h,cpp}` | h:41 / cpp:245 |
| `src/libs/GUI/Controls/SvsSeekbar.{h,cpp}` | h:86 / cpp:523 |
| `src/libs/GUI/Controls/SwitchButton.{h,cpp}` | h:37 / cpp:219 |
| `src/libs/GUI/Controls/TapTempoButton.{h,cpp}` | h:31 / cpp:103 |
| `src/libs/GUI/Controls/Toast.{h,cpp}` | h:49 / cpp:61 |
| `src/libs/GUI/Controls/ToolTip.{h,cpp}` | h:42 / cpp:209 |

```cpp
// 改前（h/cpp):
void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
// 改后:
void afterSetAnimationEnabled(bool enabled) override;
// 函数体内 Q_UNUSED(level) → Q_UNUSED(enabled) 即可，其余 updateAnimationSettings() 调用不动
```

### Task 6：`getEffectiveAnimationTime` 调用点清理

- 显式传 `AnimationGlobal::Full` 的调用点（Task 表格第 2 列）删第二参数；
- 用默认参数的调用点无需改（签名变化不影响它们）；
- 全部遗留对 `AnimationGlobal` 的引用随 Task 2 的 git rm + grep 兜底检查。

### Task 7：设置页 UI 换成开关

**文件：**
- Modify `src/app/UI/Dialogs/Options/Pages/AppearancePage.h`
- Modify `src/app/UI/Dialogs/Options/Pages/AppearancePage.cpp`

**Step 1** — .h：删除 `m_cbxAnimationLevel` 与 `animationLevelsName` 列表，新增成员：

```cpp
SwitchButton *m_swAnimationEnabled;
// const QStringList animationLevelsName = {...};  // 删除
```

**Step 2** — .cpp 的 modifyOption()：

```cpp
option->animationEnabled = m_swAnimationEnabled->value();
option->animationTimeScale = QLocale().toDouble(m_leAnimationTimeScale->text());
appOptions->saveAndNotify(AppOptionsGlobal::Appearance);
```

（删除 `... = AnimationGlobal::toString(static_cast<...>(currentIndex()))` 一行）

**Step 3** — .cpp 的 createContentWidget() 动画卡片：

```cpp
m_swAnimationEnabled = new SwitchButton(option->animationEnabled);
connect(m_swAnimationEnabled, &SwitchButton::toggled, this, &AppearancePage::modifyOption);

m_leAnimationTimeScale = new LineEdit;   // 原样保留
// ...

const auto animationCard = new OptionListCard(tr("Animation"));
animationCard->addItem(tr("Enable animations"), m_swAnimationEnabled);
animationCard->addItem(tr("Duration scale"), m_leAnimationTimeScale);
```

移除对 `<lite/GUI/Animation/AnimationGlobal.h>` 的 include（AppearancePage.cpp:22）。

**Step 4** — i18n：新增串 `Enable animations`，删除旧串 `Full/Decreased/None/Level`（AppearancePage 上下文），执行 i18n 流水线（规范见 ds-editor-lite skill references/i18n-lupdate-workflow.md）：

```bash
cd /d/GitRepos/ds-editor-lite && lupdate src/app -ts src/app/Resources/translate/translation_zh_CN.ts
```

然后用 Python（native open）把 `<source>Enable animations</source>` 的 translation 填上 `启用动画` 标记 `type=""`。旧的 Full/Decreased/None/Level 条目会自动标为 obsolete（保留无害；如需彻底清理可用 `lupdate ... -no-obsolete`，谨慎——会连其它上下文一并清理）。

> 注意：改 `.ts` 的提交不含 `.qm`（编译产物 gitignore）。

### Task 8：更新 TestAnimationSettings

**文件：** Modify `src/tests/TestAnimationSettings/main.cpp`

两态断言替换三档断言：

```cpp
void testEffectiveDurationPolicy() {
    auto *theme = ThemeManager::instance();
    theme->setAnimationSettings(true, 2.0);
    AnimationProbe probe;

    expect(probe.effectiveDuration(100) == 200, "enabled animations should use the configured time scale");
    expect(probe.effectiveDuration(100, ...) == 200, "...");  // 已无 minimumLevel 参数 → 全部去掉第二个实参

    theme->setAnimationSettings(false, 2.0);
    expect(probe.effectiveDuration(100) == 0, "disabled animations should be immediate");
}
```

- `testDialogTitleBarRuntimeUpdate`、`testProgressAndTapTempoLevels`、`testToolTipImmediateCompletion` 把 `setAnimationSettings(AnimationGlobal::X, ...)` 改成 `setAnimationSettings(bool,...)`
- `AnimationProbe` 覆盖 `afterSetAnimationEnabled(bool)` 空实现
- 结尾 `setAnimationSettings(AnimationGlobal::Full, 1.0)` → `setAnimationSettings(true, 1.0)`

**Step 5** — 补充（实施中发现）：`src/tests/TestScrollBarInterplay/main.cpp` 也在用 `setAnimationLevel(AnimationGlobal::None/Full)`（对 WheelInputController / EditorViewportAnimation 设开关语义），需同步替换为 `setAnimationEnabled(false/true)`。方案初版遗漏，grep 兜底检查时发现并一并处理。

### Task 9：构建 + 单测验证

```bash
# 全量（新增头文件删除/改动需要 configure + build）
cd /d/GitRepos/ds-editor-lite && powershell -NoProfile -ExecutionPolicy Bypass \
  -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset debug
```

```bash
# 单测目标构建 + 运行
cmake --build --preset debug --target TestAnimationSettings
build/Debug/.../TestAnimationSettings.exe   # 期望 stdout: All animation settings tests passed
```

（若 Menu / 图标等无关报错忽略；LNK1168 先 `taskkill /f /im DsEditorLite.exe`）

### Task 10：格式 + 提交

```bash
git diff --name-only | sort -u | xargs clang-format -i
git diff --check   # 空白检查，应无输出
git status --short # 核对改动文件清单与预期一致
```

提交（一条，批处理 — 这是单个逻辑改动）：

```bash
git add -A && git commit -m "Replace animation level setting with on/off toggle"
```

> 提交信息遵循 ds-editor-lite Commit Message 规范：单行、英文、首字母大写、无句号、无前缀修饰词（本改动既非纯 bug fix 也非穷举，用无前缀描述性信息）。

### 冒烟验收

按 `references/regression-smoke-test.md`：启动 → 设置页切换动画开关 → 观察标题栏/滚动条动画（开=丝滑、关=即时跳变）→ 时长缩放改动生效。

---

## 验证清单（回归关注点）

1. `git diff --stat` 改动范围合理（约 30 文件，全部是动画相关）
2. 构建通过（ConfigureAndBuild debug preset）
3. TestAnimationSettings 通过
4. `grep -rn "AnimationLevels" src/` 零残留
5. `grep -rn "animationLevel" src/` 零残留（`animationEnabled` 应在）
6. UI 手测：设置页动画卡片显示开关 + 时长缩放；切换开关立即生效（不用重启，走 `saveAndNotify` → `optionsChanged` → pushAppearance）

## 风险 / 取舍 / 开放问题

- **删除枚举的波及面**：全部 15 类 + 测试 + 边界，改动面大但全部机械。风险低；若实施中不想动枚举，可保留 `enum { Full = true, None = false }` 两值将改动面缩到仅 UI+Model。已评估：直接 bool 更干净（本次是排查+方案，最终实施粒度可在动手前再和用户确认一次）。
- **旧配置读入**：`animationLevel` 键不存在 → 默认开（符合要求）；若旧文件里该值为字符串且键名改成新键，则 load 完全不触碰 → 默认开。行为即"解析不了默认开"。
- **Duration scale**：保留，开关打开时缩放时长含义不变（`getScaledAnimationTime`）。
- `docs/theme/* 两处命中"动画"为配色文档，无关，不改。

## 涉及文件清单（共 ~30 个）

新增：无（仅删除 AnimationGlobal.h）
修改：AppearanceOption.{h,cpp}、AppController.cpp、ThemeManager.{h,cpp}、IAnimatable.h、AppearancePage.{h,cpp}、translation_zh_CN.ts、TestAnimationSettings/main.cpp、上述 15 类控件 {h,cpp}（ProgressIndicator 只 h）
删除：src/libs/GUI/Animation/AnimationGlobal.h
