#include "AutomationAccessPolicy.h"

#include <QReadLocker>
#include <QWriteLocker>

namespace Automation {

    AutomationAccessPolicy::AutomationAccessPolicy(const AutomationWire::AutomationProfile profile,
                                                   QSet<QString> customEnabled)
        : m_profile(profile), m_customEnabled(std::move(customEnabled)) {
    }

    void AutomationAccessPolicy::update(const AutomationWire::AutomationProfile profile,
                                        QSet<QString> customEnabled) {
        QWriteLocker locker(&m_lock);
        m_profile = profile;
        m_customEnabled = std::move(customEnabled);
    }

    bool AutomationAccessPolicy::isAllowed(const AutomationWire::ToolContract &contract) const {
        QReadLocker locker(&m_lock);
        if (contract.minimumProfile == AutomationWire::AutomationProfile::Meta)
            return true;
        if (m_profile == AutomationWire::AutomationProfile::Custom)
            return m_customEnabled.contains(contract.operationId);
        return AutomationWire::presetIncludes(m_profile, contract.minimumProfile);
    }

    bool AutomationAccessPolicy::isAllowed(const QString &operationId) const {
        const auto *contract = AutomationWire::findPublicTool(operationId);
        return contract && isAllowed(*contract);
    }

    QList<AutomationWire::ToolContract> AutomationAccessPolicy::enabledContracts() const {
        QReadLocker locker(&m_lock);
        return AutomationWire::toolsForProfile(m_profile, m_customEnabled);
    }

    AutomationAccessPolicySnapshot AutomationAccessPolicy::snapshot() const {
        QReadLocker locker(&m_lock);
        return {m_profile, m_customEnabled};
    }

} // namespace Automation
