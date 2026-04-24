//
// Created by FlutyDeer on 2026/4/25.
//

#ifndef MENU_H
#define MENU_H

#include <QMWidgets/cmenu.h>

class Menu : public CMenu {
    Q_OBJECT
public:
    explicit Menu(QWidget *parent = nullptr);
    explicit Menu(const QString &title, QWidget *parent = nullptr);

private:
    void initUi();
};

#endif // MENU_H
