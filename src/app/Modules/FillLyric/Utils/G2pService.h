#ifndef LYRIC_TAB_UTILS_G2P_SERVICE_H
#define LYRIC_TAB_UTILS_G2P_SERVICE_H

#include <QList>
#include <QString>
#include <QStringList>
#include <string>
#include <vector>

#include "Modules/FillLyric/LangCommon.h"
#include <lite/ProjectModel/AppModel/SingerIdentifier.h>

namespace srt::g2p {
    class LanguageService;
}

namespace FillLyric {
    struct G2pResult {
        QString language;
        QString g2pId;
        QString pronunciation;
        QStringList candidates;
    };

    class G2pService {
    public:
        /// Conversion uses the active synthrt session's language service.
        G2pService(SingerIdentifier singer, const srt::g2p::LanguageService &languageService);

        /// Returns one result per note in input order and preserves lyrics on failure.
        QList<G2pResult> convert(const QList<LangNote> &notes,
                                 const std::vector<std::string> &priorityLanguages = {}) const;

    private:
        SingerIdentifier m_singer;
    };
} // namespace FillLyric

#endif // LYRIC_TAB_UTILS_G2P_SERVICE_H
