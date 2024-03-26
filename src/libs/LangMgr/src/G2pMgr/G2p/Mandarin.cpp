#include "Mandarin.h"

namespace G2pMgr {
    Mandarin::Mandarin(QObject *parent) : IG2pFactory("Mandarin", parent) {
        setAuthor(tr("Xiao Lang"));
        setDisplayName(tr("Mandarin"));
        setDescription(tr("Using Pinyin as the phonetic notation method."));
    }

    Mandarin::~Mandarin() = default;

    bool Mandarin::initialize(QString &errMsg) {
        m_mandarin = new IKg2p::MandarinG2p();
        if (m_mandarin->getDefaultPinyin("好").isEmpty()) {
            errMsg = tr("Failed to initialize Mandarin G2P");
            return false;
        }
        return true;
    }

    QList<LangNote> Mandarin::convert(const QStringList &input, const QJsonObject *config) const {
        const auto tone = config && config->keys().contains("tone") ? config->value("tone").toBool()
                                                                    : this->m_tone;
        const auto convertNum = config && config->keys().contains("convertNum")
                                    ? config->value("convertNum").toBool()
                                    : this->m_convertNum;

        QList<LangNote> result;
        auto g2pRes = m_mandarin->hanziToPinyin(input, tone, convertNum);
        for (int i = 0; i < g2pRes.size(); i++) {
            LangNote langNote;
            langNote.lyric = input[i];
            langNote.syllable = g2pRes[i];
            langNote.candidates = m_mandarin->getDefaultPinyin(input[i], false);
            if (input[i] == g2pRes[i])
                langNote.g2pError = true;
            result.append(langNote);
        }
        return result;
    }

    QJsonObject Mandarin::config() {
        QJsonObject config;
        config["tone"] = m_tone;
        config["convertNum"] = m_convertNum;
        return config;
    }

    bool Mandarin::tone() const {
        return m_tone;
    }

    void Mandarin::setTone(const bool &tone) {
        m_tone = tone;
    }

    bool Mandarin::convertNum() const {
        return m_convertNum;
    }

    void Mandarin::setConvetNum(const bool &convertNum) {
        m_convertNum = convertNum;
    }
} // G2pMgr