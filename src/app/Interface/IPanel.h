//
// Created by fluty on 2024/7/10.
//

#ifndef IPANEL_H
#define IPANEL_H

#include "Global/AppGlobal.h"

class IPanel {
public:
    explicit IPanel(const AppGlobal::PanelType type = AppGlobal::Generic) : m_type(type) {
    }

    virtual ~IPanel() = default;

    AppGlobal::PanelType panelType() const {
        return m_type;
    }

    bool panelActive() const {
        return m_active;
    }

    void setPanelActive(const bool b) {
        m_active = b;
        afterSetActive();
    }

protected:
    virtual void afterSetActive() = 0;

private:
    AppGlobal::PanelType m_type;
    bool m_active = false;
};

#endif // IPANEL_H
