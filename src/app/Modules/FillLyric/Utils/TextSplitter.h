#ifndef LYRIC_TAB_UTILS_TEXT_SPLITTER_H
#define LYRIC_TAB_UTILS_TEXT_SPLITTER_H

#include <filesystem>
#include <string>
#include <vector>

namespace FillLyric
{
    class TextSplitter {
    public:
        static bool init(const std::filesystem::path &configDir);
        static std::vector<std::string> split(const std::string &input);
        static std::vector<std::string> split(const std::vector<std::string> &input);
    };
}

#endif // LYRIC_TAB_UTILS_TEXT_SPLITTER_H
