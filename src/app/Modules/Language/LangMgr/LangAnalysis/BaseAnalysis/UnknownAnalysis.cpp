#include "UnknownAnalysis.h"

namespace LangMgr {
    bool UnknownAnalysis::contains(const QString &input) const {
        return true;
    }

    QList<LangNote> UnknownAnalysis::split(const QString &input) const {
        return {LangNote(input)};
    }
} // LangMgr