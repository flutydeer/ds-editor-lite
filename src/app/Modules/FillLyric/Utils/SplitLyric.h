#ifndef LYRIC_TAB_UTILS_LYRIC_SPLITTER_H
#define LYRIC_TAB_UTILS_LYRIC_SPLITTER_H

#include "Modules/FillLyric/LangCommon.h"

namespace FillLyric
{
    class LyricSplitter {
    public:
        static QList<QList<LangNote>> splitAuto(const QString &input, const std::vector<std::string> &priorityG2pIds);

        static QList<QList<LangNote>> splitByChar(const QString &input);

        static QList<QList<LangNote>> splitCustom(const QString &input, const QStringList &splitter);
    };
} // namespace FillLyric


#endif // LYRIC_TAB_UTILS_LYRIC_SPLITTER_H
