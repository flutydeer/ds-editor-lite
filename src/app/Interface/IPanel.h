//
// Created by fluty on 2024/7/10.
//

#ifndef IPANEL_H
#define IPANEL_H

#include "Global/AppGlobal.h"

class IPanel {
public:
    explicit IPanel(AppGlobal::PanelType type = AppGlobal::Generic) : m_type(type) {
    }
    virtual ~IPanel() = default;
    [[nodiscard]] AppGlobal::PanelType panelType() const {
        return m_type;
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
    AppGlobal::PanelType m_type;
    bool m_activated = false;
};

#endif // IPANEL_H
