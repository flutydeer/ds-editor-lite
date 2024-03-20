#include "CharSetFactory.h"

namespace LangMgr {
    void CharSetFactory::loadDict() {
    }

    bool CharSetFactory::contains(const QChar &c) const {
        return m_charset.contains(c);
    }
} // LangMgr