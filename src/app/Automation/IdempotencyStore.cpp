#include "IdempotencyStore.h"

namespace Automation {

    void IdempotencyStore::clear() {
        const QMutexLocker locker(&m_mutex);
        m_entries.clear();
    }

    qsizetype IdempotencyStore::size() const {
        const QMutexLocker locker(&m_mutex);
        return m_entries.size();
    }

} // namespace Automation
