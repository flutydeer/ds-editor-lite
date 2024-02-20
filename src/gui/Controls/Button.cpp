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
void Button::paintEvent(QPaintEvent *event) {
    // QPushButton::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (isEnabled()) {
        if (isChecked() || m_isPrimary) {
            if (m_pressed) {
                m_foregroundColor = AppGlobal::primaryforegroundColorPressed;
                m_backgroundColor = AppGlobal::primaryColorPressed;
            } else if (m_hover) {
                m_foregroundColor = AppGlobal::primaryforegroundColorHover;
                m_backgroundColor = AppGlobal::primaryColorHover;
            } else {
                m_foregroundColor = AppGlobal::primaryforegroundColorNormal;
                m_backgroundColor = AppGlobal::primaryColor;
            }
        } else {
            if (m_pressed) {
                m_foregroundColor = AppGlobal::controlForegroundColorPressed;
                m_backgroundColor = AppGlobal::controlBackgroundColorPressed;
            } else if (m_hover) {
                m_foregroundColor = AppGlobal::controlForegroundColorHover;
                m_backgroundColor = AppGlobal::controlBackgroundColorHover;
            } else {
                m_foregroundColor = AppGlobal::controlForegroundColorNormal;
                m_backgroundColor = AppGlobal::controlBackgroundColorNormal;
            }
        }
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(m_backgroundColor);
    painter.drawRoundedRect(rect(), 6, 6);

    painter.setPen(m_foregroundColor);
    auto f = font();
    f.setPointSize(10);
    painter.setFont(f);
    painter.drawText(rect(), text(), QTextOption(Qt::AlignCenter));
}
bool Button::eventFilter(QObject *watched, QEvent *event) {
    auto type = event->type();
    if (type == QEvent::HoverEnter) {
        m_hover = true;
    } else if (type == QEvent::HoverLeave) {
        m_hover = false;
    } else if (type == QEvent::MouseButtonPress) {
        m_pressed = true;
    } else if (type == QEvent::MouseButtonRelease) {
        m_pressed = false;
    }
    return QPushButton::eventFilter(watched, event);
}
void Button::initUi() {
    setAttribute(Qt::WA_Hover);
    installEventFilter(this);
    setMinimumWidth(80);
    setMinimumHeight(24);
}