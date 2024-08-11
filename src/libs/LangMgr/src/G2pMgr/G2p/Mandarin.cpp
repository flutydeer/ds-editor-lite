#include "Mandarin.h"

namespace G2pMgr {
    Mandarin::Mandarin(QObject *parent) : IG2pFactory("cmn", parent) {
        setAuthor(tr("Xiao Lang"));
        setDisplayName(tr("Mandarin"));
        setDescription(tr("Using Pinyin as the phonetic notation method."));
    }

    Mandarin::~Mandarin() = default;

    bool Mandarin::initialize(QString &errMsg) {
        m_mandarin = new IKg2p::MandarinG2p();
        if (!m_mandarin->initialized()) {
            errMsg = tr("Failed to initialize Mandarin G2P");
            return false;
        }
        return true;
    }

    static std::vector<std::string> toStdVector(const QStringList &input) {
        std::vector<std::string> result;
        result.reserve(input.size());
        for (const auto &str : input)
            result.push_back(str.toUtf8().toStdString());
        return result;
    }

    static QStringList fromStdVector(const std::vector<std::string> &input) {
        QStringList result;
        for (const auto &str : input)
            result.append(QString::fromUtf8(str));
        return result;
    }

    QList<LangNote> Mandarin::convert(const QStringList &input, const QJsonObject *config) const {
        const auto tone = config && config->keys().contains("tone")
                              ? config->value("tone").toBool()
                              : this->m_tone;
        const auto convertNum = config && config->keys().contains("convertNum")
                                    ? config->value("convertNum").toBool()
                                    : this->m_convertNum;

        QList<LangNote> result;
        const auto g2pRes = m_mandarin->hanziToPinyin(toStdVector(input), tone, convertNum);
        for (int i = 0; i < g2pRes.size(); i++) {
            LangNote langNote;
            langNote.lyric = input[i];
            langNote.syllable = QString::fromUtf8(g2pRes[i].syllable);
            langNote.candidates = fromStdVector(g2pRes[i].candidates);
            langNote.error = g2pRes[i].error;
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