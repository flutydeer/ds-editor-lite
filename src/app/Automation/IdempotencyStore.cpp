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

    QString IdempotencyStore::compoundKey(const OperationId &operationId, const QString &key) {
        QString result;
        result.reserve(operationId.size() + key.size() + 1);
        result.append(operationId);
        result.append(QChar::Null);
        result.append(key);
        return result;
    }

} // namespace Automation
