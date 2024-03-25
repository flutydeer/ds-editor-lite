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
    m_valueAnimation.stop();
    m_valueAnimation.setStartValue(m_apparentValue);
    m_valueAnimation.setEndValue(isChecked() ? 255 : 0);
    m_valueAnimation.start();
}

void SwitchButton::initUi() {
    setCheckable(true);
    setAttribute(Qt::WA_Hover, true);
    installEventFilter(this);

    m_valueAnimation.setTargetObject(this);
    m_valueAnimation.setPropertyName("apparentValue");
    m_valueAnimation.setEasingCurve(QEasingCurve::InOutCubic);

    m_thumbHoverAnimation.setTargetObject(this);
    m_thumbHoverAnimation.setPropertyName("thumbScaleRatio");
    m_thumbHoverAnimation.setEasingCurve(QEasingCurve::OutCubic);

    //    setMinimumSize(32, 16);
    //    setMaximumSize(32, 16);
    setMinimumSize(40, 28);
    setMaximumSize(40, 28);
    connect(this, &QAbstractButton::clicked, this, &SwitchButton::setValue);

    initializeAnimation();
}

void SwitchButton::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen;

    // painter.fillRect(rect(), QColor(255, 0, 0, 30));

    // Calculate params
    auto m_halfRectHeight = rect().height() / 2;
    auto m_thumbRadius = (m_halfRectHeight - m_vPadding) / 2.0;
    QPointF m_trackStart;
    QPointF m_trackEnd;
    m_trackStart.setX(rect().left() + m_vPadding + m_thumbRadius + 1); // Avoid clipping
    m_trackStart.setY(m_halfRectHeight);
    m_trackEnd.setX(rect().right() - m_vPadding - m_thumbRadius);
    m_trackEnd.setY(m_halfRectHeight);
    auto trackLength = m_trackEnd.x() - m_trackStart.x();

    // Draw inactive background
    pen.setWidthF(rect().height() - m_vPadding * 2);
    pen.setColor(QColor(255, 255, 255, m_apparentValue == 255 ? 0 : 16));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(m_trackStart, m_trackEnd);

    // Draw active background
    pen.setColor(QColor(155, 186, 255, m_apparentValue));
    painter.setPen(pen);
    painter.drawLine(m_trackStart, m_trackEnd);

    // Draw thumb
    auto left = m_apparentValue * trackLength / 255.0 + m_trackStart.x();
    //    qDebug() << m_apparentValue;
    auto handlePos = QPointF(left, m_halfRectHeight);
    auto thumbRadius = m_thumbRadius * m_thumbScaleRatio / 100.0;

    painter.setPen(Qt::NoPen);
    auto b = 255 - m_apparentValue;
    painter.setBrush(QColor(b, b, b));
    painter.drawEllipse(handlePos, thumbRadius, thumbRadius);
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
void SwitchButton::updateAnimationDuration() {
    int valueDuration = 0;
    int hoverDuration = 0;
    if (animationLevel() == AnimationGlobal::Full) {
        valueDuration = 250;
        hoverDuration = 200;
    } else if (animationLevel() == AnimationGlobal::Decreased) {
        valueDuration = 0;
        hoverDuration = 200;
    } else if (animationLevel() == AnimationGlobal::None) {
        valueDuration = 0;
        hoverDuration = 0;
    }
    m_valueAnimation.setDuration(getScaledAnimationTime(valueDuration));
    m_thumbHoverAnimation.setDuration(getScaledAnimationTime(hoverDuration));
}
bool SwitchButton::eventFilter(QObject *object, QEvent *event) {
    auto type = event->type();
    if (type == QEvent::HoverEnter || type == QEvent::MouseButtonRelease) {
        //        qDebug() << "Hover Enter";
        m_thumbHoverAnimation.stop();
        m_thumbHoverAnimation.setStartValue(m_thumbScaleRatio);
        m_thumbHoverAnimation.setEndValue(125);
        m_thumbHoverAnimation.start();
    } else if (type == QEvent::HoverLeave) {
        //        qDebug() << "Hover Leave";
        m_thumbHoverAnimation.stop();
        m_thumbHoverAnimation.setStartValue(m_thumbScaleRatio);
        m_thumbHoverAnimation.setEndValue(100);
        m_thumbHoverAnimation.start();
    } else if (type == QEvent::MouseButtonPress) {
        m_thumbHoverAnimation.stop();
        m_thumbHoverAnimation.setStartValue(m_thumbScaleRatio);
        m_thumbHoverAnimation.setEndValue(85);
        m_thumbHoverAnimation.start();
    }
    return QObject::eventFilter(object, event);
}
void SwitchButton::afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) {
    updateAnimationDuration();
}
void SwitchButton::afterSetTimeScale(double scale) {
    updateAnimationDuration();
}
