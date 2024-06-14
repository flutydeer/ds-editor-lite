#include "Kana.h"

namespace G2pMgr {
    Kana::Kana(QObject *parent) : IG2pFactory("Kana", parent) {
        setAuthor(tr("Xiao Lang"));
        setDisplayName(tr("Kana"));
        setDescription(tr("Kana to Romanization converter."));
    }

    Kana::~Kana() = default;

    bool Kana::initialize(QString &errMsg) {
        m_kana = new IKg2p::JapaneseG2p();
        if (m_kana->kanaToRomaji("かな").empty()) {
            errMsg = tr("Failed to initialize Kana");
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

    QList<LangNote> Kana::convert(const QStringList &input, const QJsonObject *config) const {
        Q_UNUSED(config);

        QList<LangNote> result;
        const auto g2pRes = m_kana->kanaToRomaji(toStdVector(input));
        for (int i = 0; i < g2pRes.size(); i++) {
            LangNote langNote;
            langNote.lyric = input[i];
            langNote.syllable = QString::fromUtf8(g2pRes[i]);
            langNote.candidates = QStringList() << langNote.syllable;
            result.append(langNote);
        }
        return result;
    }

    QJsonObject Kana::config() {
        return {};
    }
} // G2pMgr