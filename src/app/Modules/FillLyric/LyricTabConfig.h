#ifndef LYRIC_TAB_LYRIC_TAB_CONFIG_H
#define LYRIC_TAB_LYRIC_TAB_CONFIG_H

namespace FillLyric
{
    struct LyricTabConfig {
        bool lyricBaseVisible = true;
        bool lyricExtVisible = false;

        double lyricBaseFontSize = 11;
        bool baseSkipSlur = false;
        int splitMode = 0;

        double lyricExtFontSize = 12;

        bool exportLanguage = false;
    };
} // namespace FillLyric
#endif // LYRIC_TAB_LYRIC_TAB_CONFIG_H
