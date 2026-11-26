#ifndef LYRIC_TAB_UTILS_G2P_SERVICE_H
#define LYRIC_TAB_UTILS_G2P_SERVICE_H

#include <QList>
#include <QString>
#include <QStringList>
#include <string>
#include <vector>

#include "Modules/FillLyric/LangCommon.h"
#include "Modules/FillLyric/Utils/LyricLanguageContext.h"
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
        /// The shared engine supplies the voicebank's language service for conversion.
        G2pService(SingerIdentifier singer, const srt::g2p::LanguageService &languageService);

        /// Each language uses its singer-specific route and preserves the lyric on failure.
        QList<G2pResult> convert(const QList<LangNote> &notes,
                                 const LyricLanguageContext &context) const;

    private:
        SingerIdentifier m_singer;
    };
} // namespace FillLyric

#endif // LYRIC_TAB_UTILS_G2P_SERVICE_H
