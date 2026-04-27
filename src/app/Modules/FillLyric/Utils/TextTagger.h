#ifndef LYRIC_TAB_UTILS_TEXT_TAGGER_H
#define LYRIC_TAB_UTILS_TEXT_TAGGER_H

#include <filesystem>
#include <string>
#include <vector>

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

struct CustomTaggerRule;

namespace FillLyric
{
    struct TaggerResult {
        std::string lyric;
        std::string language = "unknown";
        std::string tag = "unknown";
        bool discard = false;

        explicit TaggerResult(std::string lyric) : lyric(std::move(lyric)) {}
        TaggerResult(std::string lyric, std::string language, std::string tag) :
            lyric(std::move(lyric)), language(std::move(language)), tag(std::move(tag)) {}
    };

    struct TaggerEntryInfo {
        QString type;   // "regex", "array", "dict"
        QString tag;
        QStringList values;
        bool discard = false;
    };

    struct TaggerRuleInfo {
        QString language;
        QList<TaggerEntryInfo> entries;
        bool builtin = true;
        bool enabled = true;
    };

    class TextTagger {
    public:
        static bool init(const std::filesystem::path &configDir, const std::filesystem::path &dictRootDir);
        static std::vector<TaggerResult> tag(const std::vector<std::string> &input, bool discard = false,
                                             const std::vector<std::string> &priorityLanguages = {});

        // Enable/disable built-in tagger configs by language name
        static void setBuiltinEnabled(const QMap<QString, bool> &enabledMap);
        // Set user custom tagger rules (replaces previous custom rules)
        static void setCustomRules(const QList<CustomTaggerRule> &rules);
        // Set the execution order of all rules (by language name)
        static void setRuleOrder(const QStringList &order);
        // Return list of built-in tagger language names
        static QStringList builtinLanguages();
        // Return info for all loaded rules (builtin + custom)
        static QList<TaggerRuleInfo> ruleInfoList();
    };
}

#endif // LYRIC_TAB_UTILS_TEXT_TAGGER_H
