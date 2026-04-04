//
// Created by FlutyDeer on 2025/7/13.
//

#include "TabPanelView.h"

#include "UI/Views/Common/TabPanelPage.h"
#include "UI/Views/Common/TabPanelTitleBar.h"

#include <QTabBar>
#include <QStackedWidget>
#include <QVBoxLayout>

TabPanelView::TabPanelView(AppGlobal::PanelType type, QWidget *parent): PanelView(type, parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_tabPanelTitleBar = new TabPanelTitleBar;

    m_pageContent = new QStackedWidget;

    auto layout = new QVBoxLayout;
    layout->addWidget(m_tabPanelTitleBar);
    layout->addWidget(m_pageContent);
    layout->setContentsMargins({});
    layout->setSpacing({});
    setLayout(layout);

    setContentsMargins({});

    connect(m_tabPanelTitleBar->tabBar(), &QTabBar::currentChanged, this,
            &TabPanelView::onSelectionChanged);
}

void TabPanelView::registerPage(TabPanelPage *page) {
    QSignalBlocker blocker(m_tabPanelTitleBar->tabBar());

    m_pages.append(page);
    m_tabPanelTitleBar->tabBar()->addTab(page->tabName());
    m_tabPanelTitleBar->toolBar()->addWidget(page->toolBar());
    m_pageContent->addWidget(page->content());

    connect(page, &TabPanelPage::toolBarVisibilityChanged,
            this, &TabPanelView::onToolBarVisibilityChanged);

    if (m_pages.size() == 1)
        onSelectionChanged(0);
}

void TabPanelView::onSelectionChanged(int index) {
    m_currentIndex = index;
    auto *page = m_pages.at(index);
    m_tabPanelTitleBar->toolBar()->setCurrentWidget(page->toolBar());
    updateToolBarVisibility(page);
    m_pageContent->setCurrentIndex(index);
}

void TabPanelView::onToolBarVisibilityChanged() {
    if (m_currentIndex < 0 || m_currentIndex >= m_pages.size())
        return;

    auto senderPage = qobject_cast<TabPanelPage *>(sender());
    auto currentPage = m_pages.at(m_currentIndex);
    if (senderPage == currentPage) {
        updateToolBarVisibility(currentPage);
    }
}

void TabPanelView::updateToolBarVisibility(TabPanelPage *page) {
    page->toolBar()->setVisible(page->isToolBarVisible());
}