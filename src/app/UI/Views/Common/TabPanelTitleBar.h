//
// Created by FlutyDeer on 2025/7/13.
//

#ifndef TABPANELTITLEBAR_H
#define TABPANELTITLEBAR_H

#include <QWidget>

class Button;
class QTabBar;
class QStackedWidget;

class TabPanelTitleBar : public QWidget {
    Q_OBJECT

public:
    explicit TabPanelTitleBar(QWidget *parent = nullptr);

    [[nodiscard]] QTabBar *const &tabBar() const;

    [[nodiscard]] QStackedWidget *const &toolBar() const;

private:
    QTabBar *m_tabBar;
    QStackedWidget *m_toolBar;
    Button *m_btnMaximize;
    Button *m_btnHide;
};

#endif //TABPANELTITLEBAR_H