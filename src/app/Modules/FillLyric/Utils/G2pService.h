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
        /// Always empty on this line, and kept because the lyric model still carries the field.
        /// The older line named which G2P module answered; here a language *is* the module, and
        /// which one it is follows from the singer's own language map.
        QString g2pId;
        QString pronunciation;
        QStringList candidates;
    };

    class G2pService {
    public:
        explicit G2pService(SingerIdentifier singer);

        /// One conversion per language; on failure that language keeps the original lyric
        /// (ds-session.md §206).
        QList<G2pResult> convert(const QList<LangNote> &notes,
                                 const std::vector<std::string> &priorityLanguages = {}) const;

    private:
        SingerIdentifier m_singer;
    };
} // namespace FillLyric

#endif // LYRIC_TAB_UTILS_G2P_SERVICE_H
