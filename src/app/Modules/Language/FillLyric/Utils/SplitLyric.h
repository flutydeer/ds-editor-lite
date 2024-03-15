#ifndef DS_EDITOR_LITE_CLEANLYRIC_H
#define DS_EDITOR_LITE_CLEANLYRIC_H

#include <QList>
#include "../Model/PhonicCommon.h"

namespace FillLyric {
    class CleanLyric {
    public:
        static QList<Phonic> splitAuto(const QString &input);

        static QList<Phonic> splitByChar(const QString &input);

        static QList<Phonic> splitCustom(const QString &input, const QStringList &splitter);
    };
}



#endif // DS_EDITOR_LITE_CLEANLYRIC_H
