#include "English.h"

namespace LangMgr {
    English::English(QObject *parent) : IG2pFactory("en", parent) {
        setAuthor(tr("Xiao Lang"));
        setDisplayName(tr("English"));
        setDescription(tr("Greedy matching of consecutive English letters."));
    }

    English::~English() = default;

    QList<LangNote> English::convert(const QStringList &input, const QJsonObject *config) const {
        const auto toLower = config && config->keys().contains("toLower")
                                 ? config->value("toLower").toBool()
                                 : this->m_toLower;

        QList<LangNote> result;
        for (auto &c : input) {
            const auto syllable = toLower ? c.toLower() : c;

            LangNote langNote;
            langNote.lyric = c;
            langNote.syllable = syllable;
            langNote.candidates = QStringList() << syllable;
            result.append(langNote);
        }
        return result;
    }

    QJsonObject English::config() {
        QJsonObject config;
        config["toLower"] = m_toLower;
        return config;
    }

    bool English::toLower() const {
        return m_toLower;
    }

    void English::setToLower(const bool &toLower) {
        m_toLower = toLower;
    }
} // LangMgr