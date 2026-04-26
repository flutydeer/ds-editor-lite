#include "Modules/FillLyric/Utils/TextSplitter.h"

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

            if (obj.contains("regexes") && obj["regexes"].isArray()) {
                const QJsonArray regexes = obj["regexes"].toArray();
                for (const auto &item : regexes) {
                    if (!item.isString())
                        continue;
                    auto re = std::make_unique<RE2>(item.toString().toStdString(), options);
                    if (re->ok())
                        cfg.regexes.push_back(std::move(re));
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
}
