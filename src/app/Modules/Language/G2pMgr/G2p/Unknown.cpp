#include "Unknown.h"

namespace G2pMgr {
    Unknown::Unknown(QObject *parent) : IG2pFactory("Unknown", parent) {
        setCategory("ja");
        setDisplayName("Kana");
    }

    Unknown::~Unknown() = default;

    QList<Phonic> Unknown::convert(QStringList &input) const {
        QList<Phonic> result;
        for (const auto &i : input) {
            Phonic phonic;
            phonic.text = i;
            phonic.pronunciation = Pronunciation(i, QString());
            phonic.candidates = QStringList() << i;
            result.append(phonic);
        }
        return result;
    }
} // G2pMgr