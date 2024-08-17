#ifndef DS_EDITOR_LITE_CLEANLYRIC_H
#define DS_EDITOR_LITE_CLEANLYRIC_H

#include <QList>
#include <LangMgr/LangCommon.h>

namespace FillLyric {
    class CleanLyric {
    public:
        static QList<QList<LangNote>> splitAuto(const QString &input);

        static QList<QList<LangNote>> splitByChar(const QString &input);

        static QList<QList<LangNote>> splitCustom(const QString &input,
                                                  const QStringList &splitter);
    };
}



#endif // DS_EDITOR_LITE_CLEANLYRIC_H
