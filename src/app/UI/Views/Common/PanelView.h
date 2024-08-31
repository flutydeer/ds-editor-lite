//
// Created by fluty on 24-7-25.
//

#ifndef PANELVIEW_H
#define PANELVIEW_H

#include "Global/AppGlobal.h"
#include "Interface/IPanel.h"

#include <QStyle>
#include <QWidget>

class PanelView : public QWidget, public IPanel {
    Q_OBJECT
public:
    explicit PanelView(AppGlobal::PanelType type = AppGlobal::Generic, QWidget *parent = nullptr)
        : QWidget(parent), IPanel(type) {
        setAcceptDrops(true);
    }

private:
    void afterSetActive() override {
        setProperty("activePanel", panelActive());
        style()->unpolish(this);
        style()->polish(this);
    }
};

#endif // PANELVIEW_H
