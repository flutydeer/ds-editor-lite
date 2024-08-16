//
// Created by fluty on 24-2-20.
//

#ifndef MENU_H
#define MENU_H

#include "UI/Utils/AcrylicBrush.h"

#include <QMenu>

class Menu : public QMenu {
public:
    explicit Menu(QWidget *parent = nullptr);
    explicit Menu(const QString &title, QWidget *parent = nullptr);

private:
    void applyEffects();
    void paintEvent(QPaintEvent *event) override;
    AcrylicBrush m_brush = AcrylicBrush(this, 64, QColor(32, 32, 32, 200), QColor(0, 0, 0, 0));
    QPainterPath clipPath() const;

protected:
    void showEvent(QShowEvent *event) override;
};



#endif // MENU_H
