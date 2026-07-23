#ifndef LYRIC_TAB_UTILS_G2P_SERVICE_H
#define LYRIC_TAB_UTILS_G2P_SERVICE_H

#include <QList>
#include <QString>
#include <QStringList>
#include <string>
#include <vector>

#include "Modules/FillLyric/LangCommon.h"
#include "Model/AppModel/SingerIdentifier.h"

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
        G2pService(SingerIdentifier singer, const srt::g2p::LanguageService &languageService);

        /// 每次调用按不同语言解析一次路由，失败也在本次调用内缓存。
        QList<G2pResult> convert(const QList<LangNote> &notes,
                                 const std::vector<std::string> &priorityLanguages = {}) const;

    private:
        SingerIdentifier m_singer;
        const srt::g2p::LanguageService &m_languageService;
    };
} // namespace FillLyric

#endif // LYRIC_TAB_UTILS_G2P_SERVICE_H
