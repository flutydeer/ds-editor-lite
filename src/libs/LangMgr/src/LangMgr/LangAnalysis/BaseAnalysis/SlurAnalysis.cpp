#include "SlurAnalysis.h"

namespace LangMgr {
    bool SlurAnalysis::contains(const QChar &c) const {
        return c == '-';
    }
} // LangMgr