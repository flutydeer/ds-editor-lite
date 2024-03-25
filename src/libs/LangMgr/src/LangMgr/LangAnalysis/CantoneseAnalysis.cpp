#include "CantoneseAnalysis.h"

namespace LangMgr {
    bool CantoneseAnalysis::contains(const QChar &c) const {
        return MandarinAnalysis::contains(c);
    }

    QString CantoneseAnalysis::randString() const {
        return MandarinAnalysis::randString();
    }
} // LangMgr