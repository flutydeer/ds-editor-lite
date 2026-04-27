//
// Created by FlutyDeer on 2025/7/13.
//

#ifndef TABPANELVIEW_H
#define TABPANELVIEW_H

#include "UI/Views/Common/PanelView.h"

class QStackedWidget;
class TabPanelPage;
class TabPanelTitleBar;

class TabPanelView : public PanelView {
    Q_OBJECT

public:
    explicit TabPanelView(AppGlobal::PanelType type = AppGlobal::Generic,
                          QWidget *parent = nullptr);

    void registerPage(TabPanelPage *page);

    [[nodiscard]] TabPanelTitleBar *titleBar() const;

signals:
    void detachRequested();

private slots:
    void onSelectionChanged(int index);
    void onToolBarVisibilityChanged();

private:
    void updateToolBarVisibility(TabPanelPage *page);

    TabPanelTitleBar *m_tabPanelTitleBar;
    QStackedWidget *m_pageContent;

    QList<TabPanelPage *> m_pages;
    int m_currentIndex = -1;
};


#endif //TABPANELVIEW_H