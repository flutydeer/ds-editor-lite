//
// Created by fluty on 2024/7/10.
//

#ifndef IPANEL_H
#define IPANEL_H

#include "Global/AppGlobal.h"

class IPanel {
public:
    virtual ~IPanel() = default;
    [[nodiscard]] virtual AppGlobal::PanelType panelType() const {
        return AppGlobal::Unknown;
    }
    [[nodiscard]] bool panelActivated() const {
        return m_activated;
    }
    void setPanelActivated(bool b) {
        m_activated = b;
        afterSetActivated();
    }

protected:
    virtual void afterSetActivated() = 0;

private:
    bool m_activated = false;
};

#endif // IPANEL_H
