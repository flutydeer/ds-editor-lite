#ifndef LYRIC_TAB_UTILS_TEXT_SPLITTER_H
#define LYRIC_TAB_UTILS_TEXT_SPLITTER_H

#include <filesystem>
#include <string>
#include <vector>

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

struct CustomSplitterRule;

namespace FillLyric
{
    struct SplitterRuleInfo {
        QString name;
        QStringList regexes;
        bool builtin = true;
        bool enabled = true;
    };

    class TextSplitter {
    public:
        static bool init(const std::filesystem::path &configDir);
        static std::vector<std::string> split(const std::string &input);
        static std::vector<std::string> split(const std::vector<std::string> &input);

        // Enable/disable built-in splitter configs by name
        static void setBuiltinEnabled(const QMap<QString, bool> &enabledMap);
        // Set user custom splitter rules (replaces previous custom rules)
        static void setCustomRules(const QList<CustomSplitterRule> &rules);
        // Set the execution order of all rules (by name)
        static void setRuleOrder(const QStringList &order);
        // Return list of built-in config names
        static QStringList builtinNames();
        // Return info for all loaded rules (builtin + custom)
        static QList<SplitterRuleInfo> ruleInfoList();
    };
}

#endif // LYRIC_TAB_UTILS_TEXT_SPLITTER_H
