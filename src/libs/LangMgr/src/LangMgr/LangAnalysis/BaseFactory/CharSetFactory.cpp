#include "CharSetFactory.h"

#include <qrandom.h>

namespace LangMgr {
    void CharSetFactory::loadDict() {
    }

    bool CharSetFactory::contains(const QChar &c) const {
        return m_charset.contains(c);
    }

    QString CharSetFactory::randString() const {
        if (m_charset.isEmpty()) {
            return {};
        }

        const int randomIndex =
            static_cast<int>(QRandomGenerator::global()->bounded(m_charset.size()));
        auto it = m_charset.begin();
        std::advance(it, randomIndex);

        return *it;
    }

} // LangMgr