#ifndef ADMISSIONCONTROLLER_H
#define ADMISSIONCONTROLLER_H

#include "../AutomationTypes.h"

#include <QHash>
#include <QMutex>

#include <functional>
#include <memory>

namespace Automation {

    struct AdmissionLimits {
        int maximumGlobalInFlight = 32;
        int maximumClientInFlight = 32;
        int maximumBackgroundTasks = 8;
        int maximumPerDomain = 1;
        double tokenCapacity = 64.0;
        double tokensPerSecond = 10.0;
    };

    struct AdmissionSnapshot {
        bool accepting = true;
        int globalInFlight = 0;
        int backgroundTasks = 0;
        QHash<QString, int> clientInFlight;
        QHash<QString, int> domainInFlight;
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
        using MonotonicClock = std::function<qint64()>;

        explicit AdmissionController(AdmissionLimits limits = {}, MonotonicClock clock = {});

        [[nodiscard]] AutomationResult<AdmissionLease>
            tryAcquire(QString clientId, QString concurrencyDomain = {}, bool backgroundTask = false);

        void setAccepting(bool accepting);
        [[nodiscard]] AdmissionSnapshot snapshot() const;

    private:
        std::shared_ptr<AdmissionSharedState> m_state;
    };

} // namespace Automation

#endif // ADMISSIONCONTROLLER_H
