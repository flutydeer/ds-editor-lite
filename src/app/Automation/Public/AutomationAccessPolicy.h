#ifndef AUTOMATIONACCESSPOLICY_H
#define AUTOMATIONACCESSPOLICY_H

#include <lite/AutomationWire/ControlLevel.h>
#include <lite/AutomationWire/PublicToolContract.h>

#include <QReadWriteLock>
#include <QSet>

namespace Automation {

    struct AutomationAccessPolicySnapshot {
        AutomationWire::ControlLevel controlLevel = AutomationWire::ControlLevel::L1;
        QSet<QString> customEnabled;

        friend bool operator==(const AutomationAccessPolicySnapshot &,
                               const AutomationAccessPolicySnapshot &) = default;
    };

    class AutomationAccessPolicy final {
    public:
        explicit AutomationAccessPolicy(
            AutomationWire::ControlLevel controlLevel = AutomationWire::ControlLevel::L1,
            QSet<QString> customEnabled = {});

        void update(AutomationWire::ControlLevel controlLevel, QSet<QString> customEnabled = {});

        [[nodiscard]] bool isAllowed(const AutomationWire::ToolContract &contract) const;
        [[nodiscard]] bool isAllowed(const QString &operationId) const;
        [[nodiscard]] QList<AutomationWire::ToolContract> enabledContracts() const;
        [[nodiscard]] AutomationAccessPolicySnapshot snapshot() const;

    private:
        mutable QReadWriteLock m_lock;
        AutomationWire::ControlLevel m_controlLevel;
        QSet<QString> m_customEnabled;
    };

} // namespace Automation

#endif // AUTOMATIONACCESSPOLICY_H
