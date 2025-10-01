//
// Created by FlutyDeer on 2025/7/13.
//

#include "TabPanelView.h"

#include "UI/Views/Common/ITabPanelPage.h"
#include "UI/Views/Common/TabPanelTitleBar.h"

#include <QTabBar>
#include <QStackedWidget>
#include <QVBoxLayout>

TabPanelView::TabPanelView(AppGlobal::PanelType type, QWidget *parent) : PanelView(type, parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_tabPanelTitleBar = new TabPanelTitleBar;

    m_pageContent = new QStackedWidget;

    const auto layout = new QVBoxLayout;
    layout->addWidget(m_tabPanelTitleBar);
    layout->addWidget(m_pageContent);
    layout->setContentsMargins({});
    layout->setSpacing({});
    setLayout(layout);

    setContentsMargins({});

    connect(m_tabPanelTitleBar->tabBar(), &QTabBar::currentChanged, this,
            &TabPanelView::onSelectionChanged);
}

void TabPanelView::registerPage(ITabPanelPage *page) {
    QSignalBlocker blocker(m_tabPanelTitleBar->tabBar());

    m_pages.append(page);
    m_tabPanelTitleBar->tabBar()->addTab(page->tabName());
    m_tabPanelTitleBar->toolBar()->addWidget(page->toolBar());
    m_pageContent->addWidget(page->content());

    if (m_pages.size() == 1)
        onSelectionChanged(0);
}

void TabPanelView::onSelectionChanged(const int index) const {
    m_tabPanelTitleBar->toolBar()->setCurrentWidget(m_pages.at(index)->toolBar());
    m_pageContent->setCurrentIndex(index);
}