#include "ChineseAnalysis.h"

namespace LangMgr {
    bool isHanzi(const QChar &c) {
        return c >= QChar(0x4e00) && c <= QChar(0x9fa5);
    }

    bool ChineseAnalysis::contains(const QChar &c) const {
        return isHanzi(c);
    }

    bool ChineseAnalysis::contains(const QString &input) const {
        for (const auto &c : input) {
            if (!isHanzi(c)) {
                return false;
            }
        }
        return true;
    }
} // LangMgr