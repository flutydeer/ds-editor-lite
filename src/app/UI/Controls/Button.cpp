//
// Created by fluty on 24-2-20.
//

#include <QPainter>
#include <QEvent>

#include "Button.h"
Button::Button(QWidget *parent) : QPushButton(parent) {
    initUi();
}
Button::Button(const QString &text, QWidget *parent)
    : QPushButton(text, parent) {
    initUi();
}
Button::Button(const QIcon &icon, const QString &text, QWidget *parent)
    : QPushButton(icon, text, parent){
    initUi();
}
void Button::initUi() {
    setAttribute(Qt::WA_StyledBackground);
    setMinimumWidth(80);
}