#ifndef ADMISSIONCONTROLLER_H
#define ADMISSIONCONTROLLER_H

#include "../AutomationTypes.h"

#include <QMutex>

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
        explicit AdmissionController(AdmissionLimits limits = {});

        [[nodiscard]] AutomationResult<AdmissionLease> tryAcquire(bool backgroundTask = false);

        void setAccepting(bool accepting);
        [[nodiscard]] AdmissionSnapshot snapshot() const;

    private:
        std::shared_ptr<AdmissionSharedState> m_state;
    };

} // namespace Automation

#endif // ADMISSIONCONTROLLER_H
