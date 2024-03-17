//
// Created by fluty on 24-2-20.
//

#include <QPainter>
#include <QEvent>

#include "Button.h"
Button::Button(QWidget *parent, bool isPrimary) : QPushButton(parent), m_isPrimary(isPrimary) {
    initUi();
}
Button::Button(const QString &text, QWidget *parent, bool isPrimary)
    : QPushButton(text, parent), m_isPrimary(isPrimary) {
    initUi();
}
Button::Button(const QIcon &icon, const QString &text, QWidget *parent, bool isPrimary)
    : QPushButton(icon, text, parent), m_isPrimary(isPrimary) {
    initUi();
}
bool Button::isPrimary() const {
    return m_isPrimary;
}
void Button::setPrimary(bool on) {
    m_isPrimary = on;
    update();
}
void Button::initUi() {
    setAttribute(Qt::WA_StyledBackground);
    setMinimumWidth(80);
}