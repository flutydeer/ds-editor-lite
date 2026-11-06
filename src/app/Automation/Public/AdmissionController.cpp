#include "AdmissionController.h"

#include <QMutexLocker>

#include <algorithm>

namespace Automation {
    namespace {
        AutomationError admissionError(const AutomationErrorCode code, QString message) {
            AutomationError error;
            error.code = code;
            error.message = std::move(message);
            return error;
        }
    }

    struct AdmissionSharedState {
        mutable QMutex mutex;
        AdmissionLimits limits;
        bool accepting = true;
        AdmissionController::AdmissionGate gate;
        int globalInFlight = 0;
        int backgroundTasks = 0;
    };

    struct AdmissionLease::Record {
        std::shared_ptr<AdmissionSharedState> state;
        bool backgroundTask = false;

        ~Record() {
            if (!state)
                return;
            const QMutexLocker locker(&state->mutex);
            state->globalInFlight = std::max(0, state->globalInFlight - 1);
            if (backgroundTask)
                state->backgroundTasks = std::max(0, state->backgroundTasks - 1);
        }
    };

    AdmissionLease::AdmissionLease(std::shared_ptr<Record> record) : m_record(std::move(record)) {
    }

    bool AdmissionLease::isValid() const {
        return bool(m_record);
    }

    AdmissionController::AdmissionController(AdmissionLimits limits)
        : m_state(std::make_shared<AdmissionSharedState>()) {
        limits.maximumGlobalInFlight = std::max(1, limits.maximumGlobalInFlight);
        limits.maximumBackgroundTasks = std::max(1, limits.maximumBackgroundTasks);
        m_state->limits = limits;
    }

    AutomationResult<AdmissionLease> AdmissionController::tryAcquire(const bool backgroundTask) {
        const QMutexLocker locker(&m_state->mutex);
        // The host closes admission by committing its exit, and it does that without
        // waiting for the owner thread to run a server stop handshake. The gate is the
        // host's own state, so nothing here needs to know where that state lives.
        if (!m_state->accepting || (m_state->gate && !m_state->gate())) {
            return admissionError(AutomationErrorCode::OperationUnavailable,
                                  QStringLiteral("Automation is not accepting new requests"));
        }
        if (m_state->globalInFlight >= m_state->limits.maximumGlobalInFlight) {
            return admissionError(
                AutomationErrorCode::Busy,
                QStringLiteral("Global automation concurrency limit was reached"));
        }
        if (backgroundTask && m_state->backgroundTasks >= m_state->limits.maximumBackgroundTasks) {
            return admissionError(AutomationErrorCode::Busy,
                                  QStringLiteral("Automation background task limit was reached"));
        }

        ++m_state->globalInFlight;
        if (backgroundTask)
            ++m_state->backgroundTasks;

        auto record = std::make_shared<AdmissionLease::Record>();
        record->state = m_state;
        record->backgroundTask = backgroundTask;
        return AdmissionLease(std::move(record));
    }

    void AdmissionController::setAccepting(const bool accepting) {
        const QMutexLocker locker(&m_state->mutex);
        m_state->accepting = accepting;
    }

    void AdmissionController::setAdmissionGate(AdmissionGate gate) {
        const QMutexLocker locker(&m_state->mutex);
        m_state->gate = std::move(gate);
    }

    AdmissionSnapshot AdmissionController::snapshot() const {
        const QMutexLocker locker(&m_state->mutex);
        // The published state has to agree with tryAcquire: a closed gate means the
        // controller is not accepting, even when someone re-enabled the flag.
        return {
            .accepting = m_state->accepting && (!m_state->gate || m_state->gate()),
            .globalInFlight = m_state->globalInFlight,
            .backgroundTasks = m_state->backgroundTasks,
        };
    }

} // namespace Automation
