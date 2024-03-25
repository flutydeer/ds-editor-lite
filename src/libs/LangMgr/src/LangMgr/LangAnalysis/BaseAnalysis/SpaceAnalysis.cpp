#include "SpaceAnalysis.h"

namespace LangMgr {
    bool SpaceAnalysis::contains(const QChar &c) const {
        return c == ' ';
    }

    QString SpaceAnalysis::randString() const {
        return " ";
    }

} // LangMgr