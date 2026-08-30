#ifndef SINGLEWINDOWCONTEXT_H
#define SINGLEWINDOWCONTEXT_H

#include "AutomationTypes.h"

namespace Automation {

    class SingleWindowContext final {
    public:
        SingleWindowContext();

        [[nodiscard]] const WindowId &windowId() const;
        [[nodiscard]] AutomationResult<AutomationUnit>
            validateWindow(const WindowId &requested) const;

    private:
        WindowId m_windowId;
    };

} // namespace Automation

#endif // SINGLEWINDOWCONTEXT_H
