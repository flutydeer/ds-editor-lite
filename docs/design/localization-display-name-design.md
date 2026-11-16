# 声库多语言显示名

## 数据契约

声库元数据的多语言字段遵循 ds-spec 2.4。字段可用单个字符串表示默认文本，也可以用对象表示不同语言的文本。对象必须包含默认键 `"_"`，其他键推荐使用 BCP 47 书写，如 `zh-CN`、`zh-Hans` 和 `ja-JP`。

synthrt 的 `srt::core::DisplayText` 保留原始键。`locales()` 枚举键，`text(key)` 区分大小写精确读取，缺键返回 `nullptr`。`text()` 读取默认文本。[声库包管理器](../../src/libs/PackageManager/PackageManager.cpp)同时保留默认文本和本地化表。工程与剪贴板在需要稳定名称时使用默认字段。界面和自动化接口的显示字段使用本地化结果，资源详情保留原始键和文本。

## 语言匹配

[`UiLanguageManager`](../../src/app/Utils/UiLanguageManager.cpp) 根据有效界面语言获取 `QLocale::uiLanguages()`，按 Qt 给出的顺序尝试候选。以 `zh_CN` 为例，候选可包含 `zh-Hans-CN`、`zh-CN`、`zh-Hans` 和 `zh`。仅用 `QLocale::bcp47Name()` 得到的最短标签会遗漏脚本或地区候选。

[`lookupLocalizedText()`](../../src/libs/Support/LocalizedTextUtils.cpp) 对每个候选调用 [`IcuWrapper::bestMatch()`](../../src/3rdparty/icu-wrapper/README.md)。匹配沿显式父链从具体到一般比较规范化标签，保留命中键的原始拼写。POSIX 键如 `zh_CN` 可经规范化匹配。兄弟标签不会互相匹配，例如使用 `zh-TW` 请求时，候选条目只有 `zh-Hant` 和 `zh` 就会命中 `zh`。候选全部落空时使用默认文本。

显式父链避免将平台库的语言距离排序当作回退规则。Windows 和 Linux 由 ICU 提供标签规范化与父链处理，macOS 使用 Foundation 字符串原语实现相同的比对顺序。

## 界面语言切换

多语言文本保留在资源快照中，显示名按有效界面语言重新解析，切换语言无需重扫声库。窗口和控件可通过 `QEvent::LanguageChange` 刷新。依赖控件先刷新显示名的更新可接在 `UiLanguageManager::languageChanged` 信号上。应用先更新有效语言、安装或移除翻译器，再发出该信号。
