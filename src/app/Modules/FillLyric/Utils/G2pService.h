#ifndef LYRIC_TAB_UTILS_G2P_SERVICE_H
#define LYRIC_TAB_UTILS_G2P_SERVICE_H

#include <QList>
#include <QString>
#include <QStringList>
#include <string>
#include <vector>

#include "Modules/FillLyric/LangCommon.h"
#include <lite/ProjectModel/AppModel/SingerIdentifier.h>

namespace FillLyric {
    struct G2pResult {
        QString language;
        /// Deprecated G2P identifier, always empty.
        QString g2pId;
        QString pronunciation;
        QStringList candidates;
    };

    class G2pService {
    public:
        explicit G2pService(SingerIdentifier singer);

        /// Failed conversions preserve the original lyrics.
        QList<G2pResult> convert(const QList<LangNote> &notes,
                                 const std::vector<std::string> &priorityLanguages = {}) const;

    private:
        SingerIdentifier m_singer;
    };
} // namespace FillLyric

#endif // LYRIC_TAB_UTILS_G2P_SERVICE_H
