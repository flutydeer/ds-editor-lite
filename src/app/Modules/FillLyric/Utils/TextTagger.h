#ifndef LYRIC_TAB_UTILS_TEXT_TAGGER_H
#define LYRIC_TAB_UTILS_TEXT_TAGGER_H

#include <filesystem>
#include <string>
#include <vector>

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

    class TextTagger {
    public:
        static bool init(const std::filesystem::path &configDir, const std::filesystem::path &dictRootDir);
        static std::vector<TaggerResult> tag(const std::vector<std::string> &input, bool discard = false,
                                             const std::vector<std::string> &priorityLanguages = {});
    };
}

#endif // LYRIC_TAB_UTILS_TEXT_TAGGER_H
