//
// Created by fluty on 24-2-20.
//

#ifndef MENU_H
#define MENU_H

#include <QMenu>

class Menu : public QMenu {
public:
    explicit Menu(QWidget *parent = nullptr);
    explicit Menu(const QString &title, QWidget *parent = nullptr);

private:
    void applyEffects();
};



#endif // MENU_H
