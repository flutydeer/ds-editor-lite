#include "Cantonese.h"

namespace G2pMgr {
    Cantonese::Cantonese(QObject *parent)
        : IG2pFactory("cantonese", parent), m_cantonese(new IKg2p::Cantonese()) {
        setCategory("yue");
        setDisplayName("Cantonese");
    }

    Cantonese::~Cantonese() = default;

    QList<Phonic> Cantonese::convert(QStringList &input) const {
        QList<Phonic> result;
        auto g2pRes = m_cantonese->hanziToPinyin(input, false, false);
        for (int i = 0; i < g2pRes.size(); i++) {
            Phonic phonic;
            phonic.text = input[i];
            phonic.pronunciation = Pronunciation(g2pRes[i], QString());
            phonic.candidates = m_cantonese->hanziToPinyin(input[i]);
            result.append(phonic);
        }
        return result;
    }
} // G2pMgr