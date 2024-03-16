#include "MandarinAnalysis.h"

namespace LangMgr {
    bool isHanzi(const QChar &c) {
        return c >= QChar(0x4e00) && c <= QChar(0x9fa5);
    }

    bool MandarinAnalysis::contains(const QChar &c) const {
        return isHanzi(c);
    }
} // LangMgr