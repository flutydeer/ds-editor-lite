#include "AdmissionController.h"

#include <QMutexLocker>

#include <algorithm>
#include <chrono>

namespace Automation {
    namespace {
        AutomationError admissionError(const AutomationErrorCode code, QString message) {
            AutomationError error;
            error.code = code;
            error.message = std::move(message);
            return error;
        }

        qint64 defaultMonotonicMilliseconds() {
            using namespace std::chrono;
            return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        }

        QString normalizedClientId(QString clientId) {
            clientId = clientId.trimmed();
            return clientId.isEmpty() ? QStringLiteral("anonymous") : clientId;
        }
    }

    struct AdmissionSharedState {
        struct Bucket {
            double tokens = 0.0;
            qint64 updatedAt = 0;
            bool initialized = false;
        };

        mutable QMutex mutex;
        AdmissionLimits limits;
        AdmissionController::MonotonicClock clock;
        bool accepting = true;
        int globalInFlight = 0;
        int backgroundTasks = 0;
        QHash<QString, int> clientInFlight;
        QHash<QString, int> domainInFlight;
        QHash<QString, Bucket> buckets;
    };

    struct AdmissionLease::Record {
        std::shared_ptr<AdmissionSharedState> state;
        QString clientId;
        QString concurrencyDomain;
        bool backgroundTask = false;

        ~Record() {
            if (!state)
                return;
            const QMutexLocker locker(&state->mutex);
            state->globalInFlight = std::max(0, state->globalInFlight - 1);
            if (backgroundTask)
                state->backgroundTasks = std::max(0, state->backgroundTasks - 1);

            auto client = state->clientInFlight.find(clientId);
            if (client != state->clientInFlight.end()) {
                if (--client.value() <= 0)
                    state->clientInFlight.erase(client);
            }
            if (!concurrencyDomain.isEmpty()) {
                auto domain = state->domainInFlight.find(concurrencyDomain);
                if (domain != state->domainInFlight.end()) {
                    if (--domain.value() <= 0)
                        state->domainInFlight.erase(domain);
                }
            }
        }
    };

    AdmissionLease::AdmissionLease(std::shared_ptr<Record> record) : m_record(std::move(record)) {
    }

    bool AdmissionLease::isValid() const {
        return bool(m_record);
    }

    AdmissionController::AdmissionController(AdmissionLimits limits, MonotonicClock clock)
        : m_state(std::make_shared<AdmissionSharedState>()) {
        limits.maximumGlobalInFlight = std::max(1, limits.maximumGlobalInFlight);
        limits.maximumClientInFlight = std::max(1, limits.maximumClientInFlight);
        limits.maximumBackgroundTasks = std::max(1, limits.maximumBackgroundTasks);
        limits.maximumPerDomain = std::max(1, limits.maximumPerDomain);
        limits.tokenCapacity = std::max(1.0, limits.tokenCapacity);
        limits.tokensPerSecond = std::max(0.001, limits.tokensPerSecond);
        m_state->limits = limits;
        m_state->clock = clock ? std::move(clock) : MonotonicClock(defaultMonotonicMilliseconds);
    }

    AutomationResult<AdmissionLease>
        AdmissionController::tryAcquire(QString clientId, QString concurrencyDomain,
                                        const bool backgroundTask) {
        clientId = normalizedClientId(std::move(clientId));
        concurrencyDomain = concurrencyDomain.trimmed();

        const QMutexLocker locker(&m_state->mutex);
        if (!m_state->accepting) {
            return admissionError(AutomationErrorCode::OperationUnavailable,
                                  QStringLiteral("Automation server is not accepting new requests"));
        }
        if (m_state->globalInFlight >= m_state->limits.maximumGlobalInFlight) {
            return admissionError(AutomationErrorCode::Busy,
                                  QStringLiteral("Global automation concurrency limit was reached"));
        }
        if (m_state->clientInFlight.value(clientId) >=
            m_state->limits.maximumClientInFlight) {
            return admissionError(AutomationErrorCode::TooManyRequests,
                                  QStringLiteral("Client automation concurrency limit was reached"));
        }
        if (backgroundTask &&
            m_state->backgroundTasks >= m_state->limits.maximumBackgroundTasks) {
            return admissionError(AutomationErrorCode::Busy,
                                  QStringLiteral("Automation background task limit was reached"));
        }
        if (!concurrencyDomain.isEmpty() &&
            m_state->domainInFlight.value(concurrencyDomain) >=
                m_state->limits.maximumPerDomain) {
            return admissionError(AutomationErrorCode::Busy,
                                  QStringLiteral("Automation concurrency domain is busy"));
        }

        const auto now = m_state->clock();
        auto bucket = m_state->buckets.value(clientId);
        if (!bucket.initialized) {
            bucket.tokens = m_state->limits.tokenCapacity;
            bucket.updatedAt = now;
            bucket.initialized = true;
        } else {
            const auto elapsed = std::max<qint64>(0, now - bucket.updatedAt);
            bucket.tokens = std::min(
                m_state->limits.tokenCapacity,
                bucket.tokens + double(elapsed) * m_state->limits.tokensPerSecond / 1000.0);
            bucket.updatedAt = now;
        }
        if (bucket.tokens < 1.0) {
            m_state->buckets.insert(clientId, bucket);
            return admissionError(AutomationErrorCode::TooManyRequests,
                                  QStringLiteral("Client automation request rate was exceeded"));
        }
        bucket.tokens -= 1.0;
        m_state->buckets.insert(clientId, bucket);

        ++m_state->globalInFlight;
        ++m_state->clientInFlight[clientId];
        if (backgroundTask)
            ++m_state->backgroundTasks;
        if (!concurrencyDomain.isEmpty())
            ++m_state->domainInFlight[concurrencyDomain];

        auto record = std::make_shared<AdmissionLease::Record>();
        record->state = m_state;
        record->clientId = std::move(clientId);
        record->concurrencyDomain = std::move(concurrencyDomain);
        record->backgroundTask = backgroundTask;
        return AdmissionLease(std::move(record));
    }

    void AdmissionController::setAccepting(const bool accepting) {
        const QMutexLocker locker(&m_state->mutex);
        m_state->accepting = accepting;
    }

    AdmissionSnapshot AdmissionController::snapshot() const {
        const QMutexLocker locker(&m_state->mutex);
        return {
            .accepting = m_state->accepting,
            .globalInFlight = m_state->globalInFlight,
            .backgroundTasks = m_state->backgroundTasks,
            .clientInFlight = m_state->clientInFlight,
            .domainInFlight = m_state->domainInFlight,
        };
    }

} // namespace Automation
