//
// Created by FlutyDeer on 2025/7/13.
//

#ifndef TABPANELVIEW_H
#define TABPANELVIEW_H

#include "UI/Views/Common/PanelView.h"

class QStackedWidget;
class ITabPanelPage;
class TabPanelTitleBar;

class TabPanelView : public PanelView {
    Q_OBJECT

public:
    explicit TabPanelView(AppGlobal::PanelType type = AppGlobal::Generic,
                          QWidget *parent = nullptr);

    void registerPage(ITabPanelPage *page);

private slots:
    void onSelectionChanged(int index);

private:
    TabPanelTitleBar *m_tabPanelTitleBar;
    QStackedWidget *m_pageContent;

    QList<ITabPanelPage *> m_pages;
};


#endif //TABPANELVIEW_H