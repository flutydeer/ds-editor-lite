#include "Mandarin.h"

namespace G2pMgr {
    Mandarin::Mandarin(QObject *parent)
        : IG2pFactory("Mandarin", parent), m_mandarin(new IKg2p::Mandarin()) {
        setCategory("zh");
        setDisplayName("Mandarin");
    }

    Mandarin::~Mandarin() = default;

    QList<Phonic> Mandarin::convert(QStringList &input) const {
        QList<Phonic> result;
        auto g2pRes = m_mandarin->hanziToPinyin(input, false, false);
        for (int i = 0; i < g2pRes.size(); i++) {
            Phonic phonic;
            phonic.text = input[i];
            phonic.pronunciation = Pronunciation(g2pRes[i], QString());
            phonic.candidates = m_mandarin->getDefaultPinyin(input[i], false);
            result.append(phonic);
        }
        return result;
    }
} // G2pMgr