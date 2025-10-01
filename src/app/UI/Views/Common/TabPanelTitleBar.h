//
// Created by FlutyDeer on 2025/7/13.
//

#ifndef TABPANELTITLEBAR_H
#define TABPANELTITLEBAR_H

#include <QWidget>

class QTabBar;
class QStackedWidget;

class TabPanelTitleBar : public QWidget {
    Q_OBJECT

public:
    explicit TabPanelTitleBar(QWidget *parent = nullptr);

    QTabBar *const &tabBar() const;

    QStackedWidget *const &toolBar() const;

private:
    QTabBar *m_tabBar;
    QStackedWidget *m_toolBar;
};

#endif // TABPANELTITLEBAR_H