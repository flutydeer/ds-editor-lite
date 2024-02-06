#ifndef DS_EDITOR_LITE_CLEANLYRIC_H
#define DS_EDITOR_LITE_CLEANLYRIC_H

#include <QList>
#include <QString>

namespace FillLyric {
    class CleanLyric {
    public:
        static QList<QStringList> cleanLyric(const QString &lyric);

        static bool isLetter(QChar c);

        static bool isHanzi(QChar c);

        static bool isKana(QChar c);

        static bool isSpecialKana(QChar c);
    };
}



#endif // DS_EDITOR_LITE_CLEANLYRIC_H
