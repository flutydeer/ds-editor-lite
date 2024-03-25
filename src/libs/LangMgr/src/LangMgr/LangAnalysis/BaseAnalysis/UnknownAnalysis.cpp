#include "UnknownAnalysis.h"

#include <qrandom.h>

namespace LangMgr {
    bool UnknownAnalysis::contains(const QString &input) const {
        return true;
    }

    QList<LangNote> UnknownAnalysis::split(const QString &input) const {
        return {LangNote(input)};
    }

    QString UnknownAnalysis::randString() const {
        const int unicode = QRandomGenerator::global()->bounded(0x2200, 0x22ff + 1);
        return {QChar(unicode)};
    }

} // LangMgr