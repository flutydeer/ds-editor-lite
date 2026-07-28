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
        /// `languageService` is preserved for API stability during B1b migration
        /// (LyricTab/LyricDialog construct G2pService with SynthrtEngine::
        /// languageService()). B1b-3 routes G2P conversion through
        /// SynthrtEngine::session().convertG2p() which uses the LanguageService
        /// injected via SessionResources; this parameter is no longer used
        /// internally and will be removed together with the legacy SynthrtEngine
        /// API in B1c.
        G2pService(SingerIdentifier singer, const srt::g2p::LanguageService &languageService);

        /// Each call invokes session().convertG2p per language; on failure
        /// that language keeps the original lyric (ds-session.md §206).
        QList<G2pResult> convert(const QList<LangNote> &notes,
                                 const std::vector<std::string> &priorityLanguages = {}) const;

    private:
        SingerIdentifier m_singer;
    };
} // namespace FillLyric

#endif // LYRIC_TAB_UTILS_G2P_SERVICE_H
