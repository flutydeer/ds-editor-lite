#include "Punctuation.h"

namespace LangMgr {
    bool Punctuation::contains(const QChar &c) const {
        return c.isPunct() && c != '-';
    }
} // LangMgr