#include "SingleWindowContext.h"

namespace Automation {

    SingleWindowContext::SingleWindowContext() : m_windowId(WindowId::create()) {
    }

    const WindowId &SingleWindowContext::windowId() const {
        return m_windowId;
    }

    AutomationResult<AutomationUnit>
        SingleWindowContext::validateWindow(const WindowId &requested) const {
        if (requested == m_windowId)
            return AutomationUnit{};

        AutomationError error;
        error.code = AutomationErrorCode::HostCapabilityUnavailable;
        error.fieldPath = QStringLiteral("window_id");
        error.message = QStringLiteral("The requested window is not available in this host");
        return error;
    }

} // namespace Automation
