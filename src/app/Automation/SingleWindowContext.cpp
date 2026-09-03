#include "SingleWindowContext.h"

namespace Automation {

    SingleWindowContext::SingleWindowContext() : SingleWindowContext(WindowId::create()) {
    }

    SingleWindowContext::SingleWindowContext(std::optional<WindowId> windowId)
        : m_windowId(std::move(windowId)) {
        if (m_windowId && m_windowId->isNull())
            m_windowId.reset();
    }

    const std::optional<WindowId> &SingleWindowContext::windowId() const {
        return m_windowId;
    }

    AutomationResult<AutomationUnit>
        SingleWindowContext::validateWindow(const WindowId &requested) const {
        if (!m_windowId) {
            AutomationError error;
            error.code = AutomationErrorCode::HostCapabilityUnavailable;
            error.fieldPath = QStringLiteral("window_id");
            error.message = QStringLiteral("This host does not provide a window");
            return error;
        }

        if (requested == *m_windowId)
            return AutomationUnit{};

        AutomationError error;
        error.code = AutomationErrorCode::HostCapabilityUnavailable;
        error.fieldPath = QStringLiteral("window_id");
        error.message = QStringLiteral("The requested window is not available in this host");
        return error;
    }

} // namespace Automation
