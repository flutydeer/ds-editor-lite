#ifndef DS_EDITOR_LITE_CLEANLYRIC_H
#define DS_EDITOR_LITE_CLEANLYRIC_H

#include <QList>
#include <QString>

#include "../Model/PhonicCommon.h"

namespace FillLyric {
    class CleanLyric {
    public:
        static QList<Phonic> splitAuto(const QString &input, const bool &excludeSpace = true,
                                       const QString &fermata = "-");

        static QList<Phonic> splitByChar(const QString &input, const bool &excludeSpace = true,
                                         const QString &fermata = "-");

        static QList<Phonic> splitCustom(const QString &input, const QStringList &splitter,
                                         const bool &excludeSpace = true,
                                         const QString &fermata = "-");

        static TextType lyricType(const QString &lyric, const QString &fermata = "-");

        static bool isLetter(const QChar &c);

        static bool isHanzi(const QChar &c);

        static bool isKana(const QChar &c);

        static bool isSpecialKana(const QChar &c);

        static bool isLineBreak(const QChar &c);

        static bool isEnglishWord(const QString &word);
    };
}



#endif // DS_EDITOR_LITE_CLEANLYRIC_H
