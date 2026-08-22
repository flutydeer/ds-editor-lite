#ifndef IDEMPOTENCYSTORE_H
#define IDEMPOTENCYSTORE_H

#include "AutomationTypes.h"

#include <QHash>
#include <QMutex>

#include <any>
#include <functional>
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
                error.message =
                    QStringLiteral("Idempotency key was already used with another request");
                return error;
            }
            if (it->resultType != std::type_index(typeid(T))) {
                AutomationError error;
                error.code = AutomationErrorCode::InternalError;
                error.operationId = operationId;
                error.message =
                    QStringLiteral("Cached idempotency result has an incompatible type");
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
        AutomationResult<AutomationUnit> store(const OperationId &operationId, const QString &key,
                                               QByteArray fingerprint, T result) {
            if (key.isEmpty())
                return AutomationUnit{};

            const QMutexLocker locker(&m_mutex);
            const auto pending = m_pendingReleases.find(key);
            if (pending != m_pendingReleases.end()) {
                const bool suppressed = pending->operationId == operationId &&
                                        pending->fingerprint == fingerprint &&
                                        pending->resultType == std::type_index(typeid(T)) &&
                                        pending->matches(std::any(result));
                m_pendingReleases.erase(pending);
                if (suppressed)
                    return AutomationUnit{};
            }

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
                error.message =
                    QStringLiteral("Idempotency key was already used with another request");
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

        template <typename T>
        bool release(const OperationId &operationId, const QString &key, QByteArray fingerprint,
                     const T &result) {
            if (key.isEmpty())
                return false;

            const QMutexLocker locker(&m_mutex);
            const auto it = m_entries.find(key);
            if (it != m_entries.end()) {
                if (it->operationId != operationId || it->fingerprint != fingerprint ||
                    it->resultType != std::type_index(typeid(T))) {
                    return false;
                }
                const auto *cached = std::any_cast<T>(&it->result);
                if (!cached || *cached != result)
                    return false;
                m_entries.erase(it);
                return true;
            }

            PendingRelease pending;
            pending.operationId = operationId;
            pending.fingerprint = std::move(fingerprint);
            pending.resultType = std::type_index(typeid(T));
            pending.matches = [expected = result](const std::any &candidate) {
                const auto *value = std::any_cast<T>(&candidate);
                return value && *value == expected;
            };
            m_pendingReleases.insert(key, std::move(pending));
            return true;
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

        struct PendingRelease {
            OperationId operationId;
            QByteArray fingerprint;
            std::type_index resultType{typeid(void)};
            std::function<bool(const std::any &)> matches;
        };

        mutable QMutex m_mutex;
        QHash<QString, Entry> m_entries;
        QHash<QString, PendingRelease> m_pendingReleases;
    };

} // namespace Automation

#endif // IDEMPOTENCYSTORE_H
