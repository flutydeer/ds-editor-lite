#include "Modules/FillLyric/Utils/TextSplitter.h"

#include "Model/AppOptions/Options/FillLyricOption.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <re2/re2.h>

namespace FillLyric
{
    struct SplitterConfig {
        std::string name;
        std::vector<std::unique_ptr<RE2>> regexes;
        QStringList regexPatterns; // keep original patterns for ruleInfoList()
        bool builtin = true;
        bool enabled = true;
    };

    static std::vector<SplitterConfig> g_splitters;
    static bool g_splitterInitialized = false;

    static void ensureSplitterInitialized() {
        if (g_splitterInitialized)
            return;
        const auto basePath = std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()) / "configs";
        TextSplitter::init(basePath / "splitter");
        g_splitterInitialized = true;
    }

    static std::vector<std::string> splitWithPattern(const std::string &text, const RE2 &regex) {
        std::vector<std::string> result;

        re2::StringPiece textPiece(text);
        re2::StringPiece match;
        size_t last_end = 0;

        while (RE2::FindAndConsume(&textPiece, regex, &match)) {
            if (match.empty()) {
                result.emplace_back(text);
                return result;
            }

            const size_t match_start = match.data() - text.data();

            if (match_start > last_end) {
                result.emplace_back(text.data() + last_end, match_start - last_end);
            }

            result.emplace_back(match.data(), match.size());
            last_end = match_start + match.size();
        }

        if (last_end < text.size()) {
            result.emplace_back(text.data() + last_end, text.size() - last_end);
        }

        if (result.empty()) {
            result.emplace_back(text);
        }

        return result;
    }

    bool TextSplitter::init(const std::filesystem::path &configDir) {
        g_splitters.clear();
        g_splitterInitialized = true;

        RE2::Options options;
        options.set_encoding(RE2::Options::EncodingUTF8);
        options.set_log_errors(false);
        options.set_max_mem(8 << 20);

        std::vector<std::filesystem::path> entries;
        for (const auto &entry : std::filesystem::directory_iterator(configDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                entries.push_back(entry.path());
        }
        std::sort(entries.begin(), entries.end());

        for (const auto &path : entries) {
            QFile file(QString::fromStdString(path.string()));
            if (!file.open(QIODevice::ReadOnly))
                continue;

            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
            file.close();

            if (doc.isNull() || !doc.isObject())
                continue;

            const QJsonObject obj = doc.object();
            SplitterConfig cfg;
            cfg.name = path.stem().string();
            cfg.builtin = true;
            cfg.enabled = true;

            if (obj.contains("regexes") && obj["regexes"].isArray()) {
                const QJsonArray regexes = obj["regexes"].toArray();
                for (const auto &item : regexes) {
                    if (!item.isString())
                        continue;
                    const QString pattern = item.toString();
                    auto re = std::make_unique<RE2>(pattern.toStdString(), options);
                    if (re->ok()) {
                        cfg.regexes.push_back(std::move(re));
                        cfg.regexPatterns.append(pattern);
                    }
                }
            }

            if (!cfg.regexes.empty()) {
                g_splitters.push_back(std::move(cfg));
            }
        }

        return true;
    }

    std::vector<std::string> TextSplitter::split(const std::string &input) {
        if (input.empty())
            return {};
        return split(std::vector<std::string>{input});
    }

    std::vector<std::string> TextSplitter::split(const std::vector<std::string> &input) {
        if (input.empty())
            return {};

        ensureSplitterInitialized();

        std::vector<std::string> current = input;

        for (const auto &splitter : g_splitters) {
            if (!splitter.enabled)
                continue;

            for (const auto &regex : splitter.regexes) {
                std::vector<std::string> next;
                next.reserve(current.size() * 2);

                for (const auto &segment : current) {
                    auto parts = splitWithPattern(segment, *regex);
                    next.insert(next.end(), std::make_move_iterator(parts.begin()),
                                std::make_move_iterator(parts.end()));
                }

                current = std::move(next);
            }
        }

        return current;
    }

    void TextSplitter::setBuiltinEnabled(const QMap<QString, bool> &enabledMap) {
        ensureSplitterInitialized();
        for (auto &cfg : g_splitters) {
            if (!cfg.builtin)
                continue;
            const auto key = QString::fromStdString(cfg.name);
            // Missing key means enabled
            cfg.enabled = enabledMap.value(key, true);
        }
    }

    void TextSplitter::setCustomRules(const QList<CustomSplitterRule> &rules) {
        ensureSplitterInitialized();

        // Remove existing custom rules
        g_splitters.erase(
            std::remove_if(g_splitters.begin(), g_splitters.end(),
                           [](const SplitterConfig &c) { return !c.builtin; }),
            g_splitters.end());

        RE2::Options options;
        options.set_encoding(RE2::Options::EncodingUTF8);
        options.set_log_errors(false);
        options.set_max_mem(8 << 20);

        // Separate rules by order: negative order goes before builtins, non-negative after
        std::vector<SplitterConfig> prependRules;
        std::vector<SplitterConfig> appendRules;

        for (const auto &rule : rules) {
            SplitterConfig cfg;
            cfg.name = rule.name.toStdString();
            cfg.builtin = false;
            cfg.enabled = rule.enabled;

            for (const auto &pattern : rule.regexes) {
                auto re = std::make_unique<RE2>(pattern.toStdString(), options);
                if (re->ok()) {
                    cfg.regexes.push_back(std::move(re));
                    cfg.regexPatterns.append(pattern);
                }
            }

            if (cfg.regexes.empty())
                continue;

            if (rule.order < 0)
                prependRules.push_back(std::move(cfg));
            else
                appendRules.push_back(std::move(cfg));
        }

        // Insert prepend rules at the beginning
        if (!prependRules.empty()) {
            g_splitters.insert(g_splitters.begin(),
                               std::make_move_iterator(prependRules.begin()),
                               std::make_move_iterator(prependRules.end()));
        }

        // Append rules at the end
        for (auto &cfg : appendRules) {
            g_splitters.push_back(std::move(cfg));
        }
    }

    QStringList TextSplitter::builtinNames() {
        ensureSplitterInitialized();
        QStringList names;
        for (const auto &cfg : g_splitters) {
            if (cfg.builtin)
                names.append(QString::fromStdString(cfg.name));
        }
        return names;
    }

    void TextSplitter::setRuleOrder(const QStringList &order) {
        ensureSplitterInitialized();
        if (order.isEmpty())
            return;

        std::vector<SplitterConfig> reordered;
        reordered.reserve(g_splitters.size());

        // Add rules in the specified order
        for (const auto &name : order) {
            const auto nameStd = name.toStdString();
            for (auto &cfg : g_splitters) {
                if (cfg.name == nameStd) {
                    reordered.push_back(std::move(cfg));
                    break;
                }
            }
        }

        // Append any rules not in the order list (new builtins, etc.)
        for (auto &cfg : g_splitters) {
            if (!cfg.regexes.empty()) // not moved
                reordered.push_back(std::move(cfg));
        }

        g_splitters = std::move(reordered);
    }

    QList<SplitterRuleInfo> TextSplitter::ruleInfoList() {
        ensureSplitterInitialized();
        QList<SplitterRuleInfo> list;
        for (const auto &cfg : g_splitters) {
            SplitterRuleInfo info;
            info.name = QString::fromStdString(cfg.name);
            info.regexes = cfg.regexPatterns;
            info.builtin = cfg.builtin;
            info.enabled = cfg.enabled;
            list.append(info);
        }
        return list;
    }
}
