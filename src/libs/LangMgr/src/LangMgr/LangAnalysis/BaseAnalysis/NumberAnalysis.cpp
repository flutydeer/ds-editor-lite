#include "NumberAnalysis.h"

namespace LangMgr {
    bool isNumber(const QChar &c) {
        return c.isDigit();
    }

    bool NumberAnalysis::contains(const QChar &c) const {
        return isNumber(c);
    }

    bool NumberAnalysis::contains(const QString &input) const {
        for (const QChar &ch : input) {
            if (!isNumber(ch)) {
                return false;
            }
        }
        return true;
    }
} // LangMgr