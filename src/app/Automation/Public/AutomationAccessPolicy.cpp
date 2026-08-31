#include "AutomationAccessPolicy.h"

#include <QReadLocker>
#include <QWriteLocker>

namespace Automation {

    AutomationAccessPolicy::AutomationAccessPolicy(const AutomationWire::ControlLevel controlLevel,
                                                   QSet<QString> customEnabled)
        : m_controlLevel(controlLevel), m_customEnabled(std::move(customEnabled)) {
    }

    void AutomationAccessPolicy::update(const AutomationWire::ControlLevel controlLevel,
                                        QSet<QString> customEnabled) {
        QWriteLocker locker(&m_lock);
        m_controlLevel = controlLevel;
        m_customEnabled = std::move(customEnabled);
    }

    bool AutomationAccessPolicy::isAllowed(const AutomationWire::ToolContract &contract) const {
        QReadLocker locker(&m_lock);
        if (contract.minimumControlLevel == AutomationWire::ControlLevel::L0)
            return true;
        if (m_controlLevel == AutomationWire::ControlLevel::Custom)
            return m_customEnabled.contains(contract.operationId);
        return AutomationWire::presetIncludes(m_controlLevel, contract.minimumControlLevel);
    }

    bool AutomationAccessPolicy::isAllowed(const QString &operationId) const {
        const auto *contract = AutomationWire::findPublicTool(operationId);
        return contract && isAllowed(*contract);
    }

    QList<AutomationWire::ToolContract> AutomationAccessPolicy::enabledContracts() const {
        QReadLocker locker(&m_lock);
        return AutomationWire::toolsForControlLevel(m_controlLevel, m_customEnabled);
    }

    AutomationAccessPolicySnapshot AutomationAccessPolicy::snapshot() const {
        QReadLocker locker(&m_lock);
        return {m_controlLevel, m_customEnabled};
    }

} // namespace Automation
