#include "Kana.h"

namespace G2pMgr {
    Kana::Kana(QObject *parent) : IG2pFactory("Kana", parent), m_kana(new IKg2p::JpG2p()) {
    }

    Kana::~Kana() = default;

    QList<Phonic> Kana::convert(QStringList &input) const {
        QList<Phonic> result;
        auto g2pRes = m_kana->kanaToRomaji(input);
        for (int i = 0; i < g2pRes.size(); i++) {
            Phonic phonic;
            phonic.text = input[i];
            phonic.pronunciation = Pronunciation(g2pRes[i], QString());
            phonic.candidates = QStringList() << g2pRes[i];
            result.append(phonic);
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