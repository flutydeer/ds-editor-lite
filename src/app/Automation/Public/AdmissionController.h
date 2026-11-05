#ifndef ADMISSIONCONTROLLER_H
#define ADMISSIONCONTROLLER_H

#include "../AutomationTypes.h"

#include <QMutex>

#include <functional>
#include <memory>

namespace Automation {

    struct AdmissionLimits {
        int maximumGlobalInFlight = 32;
        int maximumBackgroundTasks = 8;
    };

    struct AdmissionSnapshot {
        bool accepting = true;
        int globalInFlight = 0;
        int backgroundTasks = 0;
    };

    class AdmissionController;
    struct AdmissionSharedState;

    class AdmissionLease final {
    public:
        AdmissionLease() = default;

        [[nodiscard]] bool isValid() const;

    private:
        struct Record;
        explicit AdmissionLease(std::shared_ptr<Record> record);

        std::shared_ptr<Record> m_record;

        friend class AdmissionController;
    };

    class AdmissionController final {
    public:
        // Host-provided gate. The editor binds it to its own exit commit so admission
        // closes at the moment the host commits, without this policy object depending
        // on where that state lives. It must be cheap and must not re-enter admission.
        using AdmissionGate = std::function<bool()>;

        explicit AdmissionController(AdmissionLimits limits = {});

        [[nodiscard]] AutomationResult<AdmissionLease> tryAcquire(bool backgroundTask = false);

        void setAccepting(bool accepting);
        void setAdmissionGate(AdmissionGate gate);
        [[nodiscard]] AdmissionSnapshot snapshot() const;

    private:
        std::shared_ptr<AdmissionSharedState> m_state;
    };

} // namespace Automation

#endif // ADMISSIONCONTROLLER_H
