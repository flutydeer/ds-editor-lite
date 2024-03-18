#include "Unknown.h"

namespace G2pMgr {
    Unknown::Unknown(QObject *parent) : IG2pFactory("Unknown", parent) {
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

    QJsonObject Unknown::config() {
        return {};
    }

    QWidget *Unknown::configWidget(QJsonObject *config) {
        Q_UNUSED(config);
        return new QWidget();
    }
} // G2pMgr