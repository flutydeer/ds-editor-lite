#ifndef DS_EDITOR_LITE_CLEANLYRIC_H
#define DS_EDITOR_LITE_CLEANLYRIC_H

#include <QList>
#include <QString>

#include "../Model/PhonicCommon.h"

namespace FillLyric {
    class CleanLyric {
    public:
        static QList<Phonic> splitAuto(const QString &input, bool excludeSpace = true, const QString &fermata = "-");

        static QList<Phonic> splitByChar(const QString &input, bool excludeSpace = true,
                                         const QString &fermata = "-");

        static QList<Phonic> splitCustom(const QString &input, const QStringList &splitter,
                                         bool excludeSpace = true, const QString &fermata = "-");

        static TextType lyricType(const QString &lyric, const QString &fermata = "-");

        static bool isLetter(QChar c);

        static bool isHanzi(QChar c);

        static bool isKana(QChar c);

        static bool isSpecialKana(QChar c);

        static bool isLineBreak(QChar c);
    };
}



#endif // DS_EDITOR_LITE_CLEANLYRIC_H
