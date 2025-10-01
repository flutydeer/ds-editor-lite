//
// Created by FlutyDeer on 2025/7/13.
//

#ifndef ITABPANELPAGE_H
#define ITABPANELPAGE_H

#include "Global/AppGlobal.h"
#include "Utils/Macros.h"

#include <QWidget>

LITE_INTERFACE ITabPanelPage {
    I_DECL(ITabPanelPage)
    I_NODSCD(QString tabId() const);
    I_NODSCD(QString tabName() const);
    I_NODSCD(AppGlobal::PanelType panelType() const);
    I_NODSCD(QWidget *toolBar());
    I_NODSCD(QWidget *content());
};



#endif //ITABPANELPAGE_H
