#include "SpaceAnalysis.h"

namespace LangMgr {
    bool SpaceAnalysis::contains(const QChar &c) const {
        return c == ' ';
    }
} // LangMgr