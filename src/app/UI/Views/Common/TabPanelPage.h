//
// Created by FlutyDeer on 2025/7/13.
//

#ifndef TABPANELPAGE_H
#define TABPANELPAGE_H

#include "Global/AppGlobal.h"

#include <QObject>
#include <QWidget>

class TabPanelPage : public QWidget {
    Q_OBJECT

public:
    explicit TabPanelPage(QWidget *parent = nullptr) : QWidget(parent) {
    }

    virtual ~TabPanelPage() = default;

    virtual QString tabId() const = 0;
    virtual QString tabName() const = 0;
    virtual AppGlobal::PanelType panelType() const = 0;
    virtual QWidget *toolBar() = 0;
    virtual QWidget *content() = 0;
    virtual bool isToolBarVisible() const = 0;

signals:
    void toolBarVisibilityChanged();
};

#endif // TABPANELPAGE_H
