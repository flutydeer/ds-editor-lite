//
// Created by fluty on 2023/8/17.
//

#include "SwitchButton.h"

#include <QPainter>
#include <QDebug>
#include <QEvent>
#include <QPropertyAnimation>

SwitchButton::SwitchButton(QWidget *parent) : QAbstractButton(parent) {
    initUi();
}

SwitchButton::SwitchButton(bool on, QWidget *parent) : QAbstractButton(parent) {
    m_apparentValue = on ? 255 : 0;
    initUi();
    setChecked(on);
}

SwitchButton::~SwitchButton() = default;

bool SwitchButton::value() const {
    // return m_value;
    return isChecked();
}

void SwitchButton::setValue(bool value) {
    // m_value = value;
    setChecked(value);
    m_valueAnimation->stop();
    m_valueAnimation->setStartValue(m_apparentValue);
    m_valueAnimation->setEndValue(isChecked() ? 255 : 0);
    m_valueAnimation->start();
}

void SwitchButton::initUi() {
    setCheckable(true);
    setAttribute(Qt::WA_Hover, true);
    installEventFilter(this);

    m_valueAnimation = new QPropertyAnimation;
    m_valueAnimation->setTargetObject(this);
    m_valueAnimation->setPropertyName("apparentValue");
    m_valueAnimation->setDuration(250);
    m_valueAnimation->setEasingCurve(QEasingCurve::InOutCubic);

    m_thumbHoverAnimation = new QPropertyAnimation;
    m_thumbHoverAnimation->setTargetObject(this);
    m_thumbHoverAnimation->setPropertyName("thumbScaleRatio");
    m_thumbHoverAnimation->setDuration(200);
    m_thumbHoverAnimation->setEasingCurve(QEasingCurve::OutCubic);

    //    setMinimumSize(32, 16);
    //    setMaximumSize(32, 16);
    setMinimumSize(40, 20);
    setMaximumSize(40, 20);
    calculateParams();
    connect(this, &QAbstractButton::clicked, this, &SwitchButton::setValue);
}

void SwitchButton::calculateParams() {
    m_rect = rect();
    //    m_penWidth = qRound(rect().height() / 16.0);
    m_halfRectHeight = rect().height() / 2;
    m_thumbRadius = qRound(m_halfRectHeight / 2.0);
    m_trackStart.setX(m_rect.left() + m_halfRectHeight);
    m_trackStart.setY(m_halfRectHeight);
    m_trackEnd.setX(m_rect.right() - m_halfRectHeight);
    m_trackEnd.setY(m_halfRectHeight);
    m_trackLength = m_trackEnd.x() - m_trackStart.x();
}

void SwitchButton::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen;

    // Draw inactive background
    pen.setWidth(m_rect.height());
    pen.setColor(QColor(255, 255, 255, m_apparentValue == 255 ? 0 : 10));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(m_trackStart, m_trackEnd);

    // Draw active background
    pen.setColor(QColor(155, 186, 255, m_apparentValue));
    painter.setPen(pen);
    painter.drawLine(m_trackStart, m_trackEnd);

    // Draw thumb
    auto left = m_apparentValue * m_trackLength / 255.0 + m_halfRectHeight;
    //    qDebug() << m_apparentValue;
    auto handlePos = QPointF(left, m_halfRectHeight);
    auto thumbRadius = m_thumbRadius * m_thumbScaleRatio / 100.0;

    painter.setPen(Qt::NoPen);
    painter.setBrush(isChecked() ? QColor(0, 0, 0) : QColor(255, 255, 255));
    painter.drawEllipse(handlePos, thumbRadius, thumbRadius);
}

void SwitchButton::resizeEvent(QResizeEvent *event) {
    calculateParams();
}

int SwitchButton::apparentValue() const {
    return m_apparentValue;
}

void SwitchButton::setApparentValue(int x) {
    m_apparentValue = x;
    repaint();
}

int SwitchButton::thumbScaleRatio() const {
    return m_thumbScaleRatio;
}

void SwitchButton::setThumbScaleRatio(int ratio) {
    m_thumbScaleRatio = ratio;
    repaint();
}
bool SwitchButton::eventFilter(QObject *object, QEvent *event) {
    auto type = event->type();
    if (type == QEvent::HoverEnter || type == QEvent::MouseButtonRelease) {
        //        qDebug() << "Hover Enter";
        m_thumbHoverAnimation->stop();
        m_thumbHoverAnimation->setStartValue(m_thumbScaleRatio);
        m_thumbHoverAnimation->setEndValue(125);
        m_thumbHoverAnimation->start();
    } else if (type == QEvent::HoverLeave) {
        //        qDebug() << "Hover Leave";
        m_thumbHoverAnimation->stop();
        m_thumbHoverAnimation->setStartValue(m_thumbScaleRatio);
        m_thumbHoverAnimation->setEndValue(100);
        m_thumbHoverAnimation->start();
    } else if (type == QEvent::MouseButtonPress) {
        m_thumbHoverAnimation->stop();
        m_thumbHoverAnimation->setStartValue(m_thumbScaleRatio);
        m_thumbHoverAnimation->setEndValue(85);
        m_thumbHoverAnimation->start();
    }
    return QObject::eventFilter(object, event);
}
