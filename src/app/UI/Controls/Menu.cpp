//
// Created by fluty on 24-2-20.
//

#include "Menu.h"

#include <QGraphicsDropShadowEffect>
#include <QLayout>
#include <QPainter>

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
void Menu::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    // m_brush.setClipPath(clipPath());
    m_brush.paint();
    QMenu::paintEvent(event);
}
QPainterPath Menu::clipPath() const {
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0, 0, -2.5, -2.5), 8, 8);
    return path;
}
void Menu::showEvent(QShowEvent *event) {
    QMenu::showEvent(event);
    auto size = sizeHint();
    auto topLeft = mapToGlobal(rect().topLeft());
    m_brush.grabImage(QRect(topLeft, size));
}