//
// Created by fluty on 24-2-20.
//

#include <QMouseEvent>

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
    // setMinimumWidth(80);
}
void Button::mousePressEvent(QMouseEvent *event) {
    QPushButton::mousePressEvent(event);
    event->ignore();
}