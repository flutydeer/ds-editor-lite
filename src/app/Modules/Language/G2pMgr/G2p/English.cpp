#include "English.h"

namespace G2pMgr {
    English::English(QObject *parent) : IG2pFactory("English", parent) {
        setCategory("en");
        setDisplayName("English");
    }

    English::~English() = default;

    QList<Phonic> English::convert(QStringList &input) const {
        QList<Phonic> result;
        for (auto &c : input) {
            Phonic phonic;
            phonic.text = c;
            phonic.pronunciation = Pronunciation(c, QString());
            phonic.candidates = QStringList() << c;
            result.append(phonic);
        }
        return result;
    }
} // G2pMgr