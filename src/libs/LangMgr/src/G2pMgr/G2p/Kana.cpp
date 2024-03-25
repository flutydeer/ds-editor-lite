#include "Kana.h"

namespace G2pMgr {
    Kana::Kana(QObject *parent) : IG2pFactory("Kana", parent) {
        setAuthor(tr("Xiao Lang"));
        setDisplayName(tr("Kana"));
        setDescription(tr("Kana to Romanization converter."));
    }

    Kana::~Kana() = default;

    bool Kana::initialize(QString &errMsg) {
        m_kana = new IKg2p::JpG2p();
        if (m_kana->kanaToRomaji("かな").isEmpty()) {
            errMsg = tr("Failed to initialize Kana");
            return false;
        }
        return true;
    }

    QList<LangNote> Kana::convert(const QStringList &input, const QJsonObject *config) const {
        Q_UNUSED(config);

        QList<LangNote> result;
        auto g2pRes = m_kana->kanaToRomaji(input);
        for (int i = 0; i < g2pRes.size(); i++) {
            LangNote langNote;
            langNote.lyric = input[i];
            langNote.syllable = g2pRes[i];
            langNote.candidates = QStringList() << g2pRes[i];
            result.append(langNote);
        }
        return result;
    }

    QJsonObject Kana::config() {
        return {};
    }

    QWidget *Kana::configWidget(QJsonObject *config) {
        Q_UNUSED(config);
        return new QWidget();
    }
} // G2pMgr