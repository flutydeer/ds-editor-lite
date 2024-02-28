//
// Created by fluty on 24-2-20.
//

#include "Menu.h"

#include <QGraphicsDropShadowEffect>

Menu::Menu(QWidget *parent) : QMenu(parent) {
    applyEffects();
}
Menu::Menu(const QString &title, QWidget *parent) : QMenu(title, parent) {
    applyEffects();
}
void Menu::applyEffects() {
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    // auto shadowEffect = new QGraphicsDropShadowEffect(this);
    // shadowEffect->setBlurRadius(8);
    // shadowEffect->setColor(QColor(0, 0, 0, 32));
    // shadowEffect->setOffset(0, 4);
    // setGraphicsEffect(shadowEffect);
}