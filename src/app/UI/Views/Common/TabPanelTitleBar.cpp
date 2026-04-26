//
// Created by FlutyDeer on 2025/7/13.
//

#include "TabPanelTitleBar.h"

#include "Controller/AppController.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ToolTipFilter.h"

#include <QHBoxLayout>
#include <QStackedWidget>
#include <QTabBar>

TabPanelTitleBar::TabPanelTitleBar(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_tabBar = new QTabBar;
    m_tabBar->setDrawBase(false);
    m_tabBar->setExpanding(false);
    m_tabBar->setMinimumWidth(12);

    m_toolBar = new QStackedWidget;

    const int btnSize = 28;

    m_btnMaximize = new Button;
    m_btnMaximize->setObjectName("btnPanelMaximize");
    m_btnMaximize->setFixedSize(btnSize, btnSize);
    m_btnMaximize->setCheckable(true);
    m_btnMaximize->setToolTip(tr("Maximize or restore"));
    m_btnMaximize->installEventFilter(new ToolTipFilter(m_btnMaximize, 500, false, true));
    connect(m_btnMaximize, &Button::clicked, this, [=] {
        if (appStatus->trackPanelCollapsed)
            appController->setTrackAndClipPanelCollapsed(false, false);
        else
            appController->setTrackAndClipPanelCollapsed(true, false);
    });
    connect(appStatus, &AppStatus::trackPanelCollapseStateChanged, m_btnMaximize,
            &Button::setChecked);

    m_btnHide = new Button;
    m_btnHide->setObjectName("btnPanelHide");
    m_btnHide->setFixedSize(btnSize, btnSize);
    m_btnHide->setToolTip(tr("Hide"));
    m_btnHide->installEventFilter(new ToolTipFilter(m_btnHide, 500, false, true));
    connect(m_btnHide, &Button::clicked, this,
            [=] { appController->setTrackAndClipPanelCollapsed(false, true); });

    auto btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins({});
    btnLayout->setSpacing(6);
    btnLayout->addWidget(m_btnMaximize);
    btnLayout->addWidget(m_btnHide);

    auto layout = new QHBoxLayout;
    layout->addWidget(m_tabBar);
    layout->addWidget(m_toolBar);
    layout->addStretch();
    layout->addLayout(btnLayout);
    layout->setSpacing(0);
    layout->setContentsMargins(6, 6, 9, 6);
    setLayout(layout);

    setContentsMargins({});
}

QTabBar *const &TabPanelTitleBar::tabBar() const {
    return m_tabBar;
}

QStackedWidget *const &TabPanelTitleBar::toolBar() const {
    return m_toolBar;
}