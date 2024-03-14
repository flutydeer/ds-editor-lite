#include "ChineseAnalysis.h"

namespace LangMgr {
    bool isHanzi(const QChar &c) {
        return c >= QChar(0x4e00) && c <= QChar(0x9fa5);
    }

    bool ChineseAnalysis::contains(const QChar &c) const {
        return isHanzi(c);
    }
} // LangMgr