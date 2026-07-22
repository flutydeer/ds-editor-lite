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
    ~TabPanelView() override;

    void registerPage(TabPanelPage *page);
    [[nodiscard]] bool hasPage(const QString &pageId) const;
    [[nodiscard]] QString currentPageId() const;
    [[nodiscard]] AppGlobal::PanelType currentPagePanelType() const;
    bool setCurrentPageId(const QString &pageId);

    [[nodiscard]] TabPanelTitleBar *titleBar() const;

signals:
    void detachRequested();
    void currentPageChanged(const QString &pageId, AppGlobal::PanelType panelType);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void onSelectionChanged(int index);
    void onToolBarVisibilityChanged();

private:
    void retranslateUi();
    void updateToolBarVisibility(TabPanelPage *page);

    TabPanelTitleBar *m_tabPanelTitleBar;
    QStackedWidget *m_pageContent;

    QList<TabPanelPage *> m_pages;
    int m_currentIndex = -1;
};


#endif // TABPANELVIEW_H
