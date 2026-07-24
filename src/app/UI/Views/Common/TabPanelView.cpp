//
// Created by FlutyDeer on 2025/7/13.
//

#include "TabPanelView.h"

#include "UI/Views/Common/TabPanelPage.h"
#include "UI/Views/Common/TabPanelTitleBar.h"

#include <QTabBar>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>

#include <algorithm>

TabPanelView::TabPanelView(AppGlobal::PanelType type, QWidget *parent) : PanelView(type, parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_tabPanelTitleBar = new TabPanelTitleBar;

    auto titleBarHost = new QWidget;
    auto titleBarLayout = new QHBoxLayout;
    titleBarLayout->addWidget(m_tabPanelTitleBar);
    titleBarLayout->setContentsMargins(1, 0, 1, 0);
    titleBarLayout->setSpacing(0);
    titleBarHost->setLayout(titleBarLayout);

    m_pageContent = new QStackedWidget;

    auto layout = new QVBoxLayout;
    layout->addWidget(titleBarHost);
    layout->addWidget(m_pageContent);
    layout->setContentsMargins({});
    layout->setSpacing({});
    setLayout(layout);

    setContentsMargins({});

    connect(m_tabPanelTitleBar->tabBar(), &QTabBar::currentChanged, this,
            &TabPanelView::onSelectionChanged);
    connect(m_tabPanelTitleBar, &TabPanelTitleBar::detachRequested, this,
            &TabPanelView::detachRequested);
}

TabPanelView::~TabPanelView() {
    disconnect(m_tabPanelTitleBar->tabBar(), nullptr, this, nullptr);
}

TabPanelTitleBar *TabPanelView::titleBar() const {
    return m_tabPanelTitleBar;
}

void TabPanelView::registerPage(TabPanelPage *page) {
    QSignalBlocker blocker(m_tabPanelTitleBar->tabBar());

    m_pages.append(page);
    m_tabPanelTitleBar->tabBar()->addTab(page->tabName());
    m_tabPanelTitleBar->toolBar()->addWidget(page->toolBar());
    m_pageContent->addWidget(page->content());

    connect(page, &TabPanelPage::toolBarVisibilityChanged, this,
            &TabPanelView::onToolBarVisibilityChanged);

    if (m_pages.size() == 1)
        onSelectionChanged(0);
}

bool TabPanelView::hasPage(const QString &pageId) const {
    return std::any_of(m_pages.cbegin(), m_pages.cend(),
                       [&pageId](const TabPanelPage *page) { return page->tabId() == pageId; });
}

QString TabPanelView::currentPageId() const {
    if (m_currentIndex < 0 || m_currentIndex >= m_pages.size())
        return {};
    return m_pages.at(m_currentIndex)->tabId();
}

AppGlobal::PanelType TabPanelView::currentPagePanelType() const {
    if (m_currentIndex < 0 || m_currentIndex >= m_pages.size())
        return AppGlobal::Generic;
    return m_pages.at(m_currentIndex)->panelType();
}

bool TabPanelView::setCurrentPageId(const QString &pageId) {
    for (int i = 0; i < m_pages.size(); ++i) {
        if (m_pages.at(i)->tabId() != pageId)
            continue;
        if (m_tabPanelTitleBar->tabBar()->currentIndex() == i)
            onSelectionChanged(i);
        else
            m_tabPanelTitleBar->tabBar()->setCurrentIndex(i);
        return true;
    }
    return false;
}

void TabPanelView::onSelectionChanged(int index) {
    if (index < 0 || index >= m_pages.size()) {
        m_currentIndex = -1;
        return;
    }

    m_currentIndex = index;
    auto *page = m_pages.at(index);
    m_tabPanelTitleBar->toolBar()->setCurrentWidget(page->toolBar());
    updateToolBarVisibility(page);
    m_pageContent->setCurrentIndex(index);
    emit currentPageChanged(page->tabId(), page->panelType());
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

void TabPanelView::changeEvent(QEvent *event) {
    PanelView::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void TabPanelView::retranslateUi() {
    for (int i = 0; i < m_pages.size(); ++i)
        m_tabPanelTitleBar->tabBar()->setTabText(i, m_pages.at(i)->tabName());
}
