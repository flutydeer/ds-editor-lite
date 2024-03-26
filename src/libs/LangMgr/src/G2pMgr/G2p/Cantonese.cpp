#include "Cantonese.h"

namespace G2pMgr {
    Cantonese::Cantonese(QObject *parent) : IG2pFactory("Cantonese", parent) {
        setAuthor(tr("Xiao Lang"));
        setDisplayName(tr("Cantonese"));
        setDescription(tr("Using Cantonese Pinyin as the phonetic notation method."));
    }

    Cantonese::~Cantonese() = default;

    bool Cantonese::initialize(QString &errMsg) {
        m_cantonese = new IKg2p::CantoneseG2p();
        if (m_cantonese->getDefaultPinyin("好").isEmpty()) {
            errMsg = tr("Failed to initialize Cantonese G2P");
            return false;
        }
        return true;
    }

    QList<LangNote> Cantonese::convert(const QStringList &input, const QJsonObject *config) const {
        const auto tone = config && config->keys().contains("tone") ? config->value("tone").toBool()
                                                                    : this->m_tone;
        const auto convertNum = config && config->keys().contains("convertNum")
                                    ? config->value("convertNum").toBool()
                                    : this->m_convertNum;

        QList<LangNote> result;
        auto g2pRes = m_cantonese->hanziToPinyin(input, tone, convertNum);
        for (int i = 0; i < g2pRes.size(); i++) {
            LangNote langNote;
            langNote.lyric = input[i];
            langNote.syllable = g2pRes[i];
            langNote.candidates = m_cantonese->getDefaultPinyin(input[i], false);
            if (input[i] == g2pRes[i])
                langNote.g2pError = true;
            result.append(langNote);
        }
        return result;
    }

    QJsonObject Cantonese::config() {
        QJsonObject config;
        config["tone"] = m_tone;
        config["convertNum"] = m_convertNum;
        return config;
    }

    bool Cantonese::tone() const {
        return m_tone;
    }

    void Cantonese::setTone(const bool &tone) {
        m_tone = tone;
    }

    bool Cantonese::convertNum() const {
        return m_convertNum;
    }

    void Cantonese::setConvetNum(const bool &convertNum) {
        m_convertNum = convertNum;
    }
} // G2pMgr