#include "CantoneseAnalysis.h"

namespace LangMgr {
    bool CantoneseAnalysis::contains(const QChar &c) const {
        return MandarinAnalysis::contains(c);
    }
} // LangMgr