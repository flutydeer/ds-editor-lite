#ifndef SINGLEWINDOWCONTEXT_H
#define SINGLEWINDOWCONTEXT_H

#include "AutomationTypes.h"

namespace Automation {

    class SingleWindowContext final {
    public:
        SingleWindowContext();
        explicit SingleWindowContext(std::optional<WindowId> windowId);

        [[nodiscard]] const std::optional<WindowId> &windowId() const;
        [[nodiscard]] AutomationResult<AutomationUnit>
            validateWindow(const WindowId &requested) const;

    private:
        std::optional<WindowId> m_windowId;
    };

} // namespace Automation

#endif // SINGLEWINDOWCONTEXT_H
