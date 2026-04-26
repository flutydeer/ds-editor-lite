#include "Modules/FillLyric/Utils/TextTagger.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <re2/re2.h>

namespace FillLyric
{
    struct TaggerEntry {
        std::string type;
        std::vector<std::string> value;
        std::string tag;
        bool discard = false;
    };

    class ITaggerRule {
    public:
        virtual ~ITaggerRule() = default;
        virtual void apply(std::vector<TaggerResult> &input) = 0;
    };

    class RegexTaggerRule : public ITaggerRule {
    public:
        RegexTaggerRule(const std::string &language, const TaggerEntry &entry) : m_language(language), m_entry(entry) {
            RE2::Options options;
            options.set_encoding(RE2::Options::EncodingUTF8);
            options.set_log_errors(false);
            options.set_max_mem(8 << 20);

            std::string merged;
            for (size_t i = 0; i < entry.value.size(); ++i) {
                if (i > 0)
                    merged += "|";
                merged += entry.value[i];
            }
            m_regex = std::make_unique<RE2>(merged, options);
        }

        void apply(std::vector<TaggerResult> &input) override {
            if (!m_regex || !m_regex->ok())
                return;
            for (auto &item : input) {
                if (item.language == "unknown" && RE2::FullMatch(item.lyric, *m_regex)) {
                    item.language = m_language;
                    item.tag = m_entry.tag;
                    item.discard = m_entry.discard;
                }
            }
        }

    private:
        std::string m_language;
        TaggerEntry m_entry;
        std::unique_ptr<RE2> m_regex;
    };

    class ArrayTaggerRule : public ITaggerRule {
    public:
        ArrayTaggerRule(const std::string &language, const TaggerEntry &entry)
            : m_language(language), m_entry(entry) {
            m_set.reserve(entry.value.size());
            m_set.insert(entry.value.begin(), entry.value.end());
        }

        void apply(std::vector<TaggerResult> &input) override {
            for (auto &item : input) {
                if (item.language == "unknown" && m_set.count(item.lyric)) {
                    item.language = m_language;
                    item.tag = m_entry.tag;
                    item.discard = m_entry.discard;
                }
            }
        }

    protected:
        std::string m_language;
        TaggerEntry m_entry;
        std::unordered_set<std::string> m_set;
    };

    class DictTaggerRule : public ArrayTaggerRule {
    public:
        DictTaggerRule(const std::string &language, const TaggerEntry &entry,
                       const std::vector<std::string> &resolvedPaths)
            : ArrayTaggerRule(language, entry) {
            for (const auto &path : resolvedPaths) {
                std::ifstream file(path);
                if (!file.is_open())
                    continue;
                std::string line;
                while (std::getline(file, line)) {
                    if (line.empty())
                        continue;
                    if (const size_t tab_pos = line.find('\t'); tab_pos != std::string::npos) {
                        if (std::string word = line.substr(0, tab_pos); !word.empty())
                            m_set.insert(word);
                    }
                }
            }
        }
    };

    struct TaggerConfig {
        std::string language;
        std::vector<std::unique_ptr<ITaggerRule>> rules;
    };

    static std::vector<TaggerConfig> g_taggers;
    static bool g_taggerInitialized = false;

    static void ensureTaggerInitialized() {
        if (g_taggerInitialized)
            return;
        const auto basePath = std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()) / "configs";
        TextTagger::init(basePath / "tagger", basePath / "tagger");
        g_taggerInitialized = true;
    }

    static std::string findDictFile(const std::filesystem::path &dictRootDir, const std::string &filename) {
        for (const auto &entry : std::filesystem::recursive_directory_iterator(dictRootDir)) {
            if (entry.is_regular_file() && entry.path().filename().string() == filename) {
                return entry.path().string();
            }
        }
        return {};
    }

    bool TextTagger::init(const std::filesystem::path &configDir, const std::filesystem::path &dictRootDir) {
        g_taggers.clear();
        g_taggerInitialized = true;

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
            TaggerConfig cfg;

            auto langIt = obj.constFind("language");
            if (langIt == obj.constEnd() || !langIt->isString())
                continue;
            cfg.language = langIt->toString().toStdString();

            auto taggerIt = obj.constFind("tagger");
            if (taggerIt == obj.constEnd() || !taggerIt->isArray())
                continue;

            const QJsonArray taggerArray = taggerIt->toArray();
            for (const auto &item : taggerArray) {
                if (!item.isObject())
                    continue;

                const QJsonObject itemObj = item.toObject();
                TaggerEntry te;

                auto typeIt = itemObj.constFind("type");
                if (typeIt == itemObj.constEnd() || !typeIt->isString())
                    continue;
                te.type = typeIt->toString().toStdString();

                auto tagIt = itemObj.constFind("tag");
                if (tagIt == itemObj.constEnd() || !tagIt->isString())
                    continue;
                te.tag = tagIt->toString().toStdString();

                auto valueIt = itemObj.constFind("value");
                if (valueIt == itemObj.constEnd() || !valueIt->isArray())
                    continue;
                for (const auto &v : valueIt->toArray()) {
                    if (v.isString())
                        te.value.push_back(v.toString().toStdString());
                }

                auto discardIt = itemObj.constFind("discard");
                if (discardIt != itemObj.constEnd() && discardIt->isBool())
                    te.discard = discardIt->toBool();

                if (te.type == "regex") {
                    cfg.rules.push_back(std::make_unique<RegexTaggerRule>(cfg.language, te));
                } else if (te.type == "array") {
                    cfg.rules.push_back(std::make_unique<ArrayTaggerRule>(cfg.language, te));
                } else if (te.type == "dict") {
                    std::vector<std::string> resolvedPaths;
                    for (const auto &dictFile : te.value) {
                        auto resolved = findDictFile(dictRootDir, dictFile);
                        if (!resolved.empty())
                            resolvedPaths.push_back(resolved);
                    }
                    cfg.rules.push_back(std::make_unique<DictTaggerRule>(cfg.language, te, resolvedPaths));
                }
            }

            g_taggers.push_back(std::move(cfg));
        }

        return true;
    }

    std::vector<TaggerResult> TextTagger::tag(const std::vector<std::string> &input, bool discard,
                                               const std::vector<std::string> &priorityLanguages) {
        if (input.empty())
            return {};

        ensureTaggerInitialized();

        std::vector<TaggerResult> result;
        result.reserve(input.size());
        for (const auto &lyric : input)
            result.emplace_back(lyric);

        std::vector<size_t> order;
        order.reserve(g_taggers.size());

        std::unordered_set<size_t> added;
        for (const auto &lang : priorityLanguages) {
            for (size_t i = 0; i < g_taggers.size(); ++i) {
                if (g_taggers[i].language == lang && added.find(i) == added.end()) {
                    order.push_back(i);
                    added.insert(i);
                }
            }
        }
        for (size_t i = 0; i < g_taggers.size(); ++i) {
            if (added.find(i) == added.end())
                order.push_back(i);
        }

        for (const auto idx : order) {
            for (const auto &rule : g_taggers[idx].rules) {
                rule->apply(result);
            }
        }

        if (discard) {
            result.erase(
                std::remove_if(result.begin(), result.end(), [](const TaggerResult &r) { return r.discard; }),
                result.end());
        }

        return result;
    }
}
