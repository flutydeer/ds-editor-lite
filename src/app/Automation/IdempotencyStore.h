#ifndef IDEMPOTENCYSTORE_H
#define IDEMPOTENCYSTORE_H

#include "AutomationTypes.h"

#include <QHash>
#include <QMutex>

#include <any>
#include <optional>
#include <typeindex>

namespace Automation {

    class IdempotencyStore final {
    public:
        template <typename T>
        AutomationResult<std::optional<T>> replay(const OperationId &operationId,
                                                  const QString &key,
                                                  const QByteArray &fingerprint) const {
            if (key.isEmpty())
                return std::optional<T>{};

            const QMutexLocker locker(&m_mutex);
            const auto it = m_entries.constFind(key);
            if (it == m_entries.cend())
                return std::optional<T>{};

            if (it->operationId != operationId || it->fingerprint != fingerprint) {
                AutomationError error;
                error.code = AutomationErrorCode::IdempotencyConflict;
                error.operationId = operationId;
                error.fieldPath = QStringLiteral("idempotency_key");
                error.message = QStringLiteral("Idempotency key was already used with another request");
                return error;
            }
            if (it->resultType != std::type_index(typeid(T))) {
                AutomationError error;
                error.code = AutomationErrorCode::InternalError;
                error.operationId = operationId;
                error.message = QStringLiteral("Cached idempotency result has an incompatible type");
                return error;
            }

            try {
                return std::optional<T>(std::any_cast<T>(it->result));
            } catch (const std::bad_any_cast &) {
                AutomationError error;
                error.code = AutomationErrorCode::InternalError;
                error.operationId = operationId;
                error.message = QStringLiteral("Cached idempotency result cannot be decoded");
                return error;
            }
        }

        template <typename T>
        AutomationResult<AutomationUnit> store(const OperationId &operationId,
                                               const QString &key,
                                               QByteArray fingerprint,
                                               T result) {
            if (key.isEmpty())
                return AutomationUnit{};

            const QMutexLocker locker(&m_mutex);
            const auto it = m_entries.constFind(key);
            if (it != m_entries.cend()) {
                if (it->operationId == operationId && it->fingerprint == fingerprint &&
                    it->resultType == std::type_index(typeid(T))) {
                    return AutomationUnit{};
                }
                AutomationError error;
                error.code = AutomationErrorCode::IdempotencyConflict;
                error.operationId = operationId;
                error.fieldPath = QStringLiteral("idempotency_key");
                error.message = QStringLiteral("Idempotency key was already used with another request");
                return error;
            }

            Entry entry;
            entry.operationId = operationId;
            entry.fingerprint = std::move(fingerprint);
            entry.resultType = std::type_index(typeid(T));
            entry.result = std::move(result);
            m_entries.insert(key, std::move(entry));
            return AutomationUnit{};
        }

        void clear();
        [[nodiscard]] qsizetype size() const;

    private:
        struct Entry {
            OperationId operationId;
            QByteArray fingerprint;
            std::type_index resultType{typeid(void)};
            std::any result;
        };
        mutable QMutex m_mutex;
        QHash<QString, Entry> m_entries;
    };

} // namespace Automation

#endif // IDEMPOTENCYSTORE_H
