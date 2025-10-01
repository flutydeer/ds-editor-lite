//
// Created by FlutyDeer on 2025/7/13.
//

#include "TabPanelTitleBar.h"

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

    const auto layout = new QHBoxLayout;
    layout->addWidget(m_tabBar);
    layout->addWidget(m_toolBar);
    // TODO: Add btnMaximize and btnHide
    layout->setSpacing(0);
    layout->setContentsMargins(6, 6, 0, 6);
    setLayout(layout);

    setContentsMargins({});
}

QTabBar *const &TabPanelTitleBar::tabBar() const {
    return m_tabBar;
}

QStackedWidget *const &TabPanelTitleBar::toolBar() const {
    return m_toolBar;
}