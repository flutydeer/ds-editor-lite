//
// Created by fluty on 2023/8/17.
//

#include "SwitchButton.h"

#include <QPainter>
#include <QEvent>
#include <QPropertyAnimation>

SwitchButton::SwitchButton(QWidget *parent) : QAbstractButton(parent) {
    initUi();
}

SwitchButton::SwitchButton(const bool on, QWidget *parent) : QAbstractButton(parent) {
    m_apparentValue = on ? 255 : 0;
    initUi();
    setChecked(on);
}

SwitchButton::~SwitchButton() = default;

bool SwitchButton::value() const {
    return isChecked();
}

void SwitchButton::setValue(const bool value) {
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
    m_valueAnimation.setEasingCurve(QEasingCurve::OutBack);

    m_thumbHoverAnimation.setTargetObject(this);
    m_thumbHoverAnimation.setPropertyName("thumbScaleRatio");
    m_thumbHoverAnimation.setEasingCurve(QEasingCurve::OutCubic);

    setMinimumSize(40, 28);
    setMaximumSize(40, 28);
    connect(this, &QAbstractButton::clicked, this, &SwitchButton::setValue);

    initializeAnimation();
}

void SwitchButton::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen;


    // Calculate params
    const auto m_halfRectHeight = rect().height() / 2;
    const auto m_thumbRadius = (m_halfRectHeight - m_vPadding) / 2.0;
    QPointF m_trackStart;
    QPointF m_trackEnd;
    m_trackStart.setX(rect().left() + m_vPadding + m_thumbRadius + 1); // Avoid clipping
    m_trackStart.setY(m_halfRectHeight);
    m_trackEnd.setX(rect().right() - m_vPadding - m_thumbRadius);
    m_trackEnd.setY(m_halfRectHeight);
    const auto trackLength = m_trackEnd.x() - m_trackStart.x();

    // Draw inactive background
    pen.setWidthF(rect().height() - m_vPadding * 2);
    auto trackOff = m_trackOffColor;
    if (m_apparentValue == 255)
        trackOff.setAlpha(0);
    pen.setColor(trackOff);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(m_trackStart, m_trackEnd);

    // Draw active background
    auto alpha = m_apparentValue;
    if (alpha > 255)
        alpha = 255;
    if (alpha < 0)
        alpha = 0;
    auto trackOn = m_trackOnColor;
    trackOn.setAlpha(alpha * trackOn.alpha() / 255);
    pen.setColor(trackOn);
    painter.setPen(pen);
    painter.drawLine(m_trackStart, m_trackEnd);

    // Draw thumb
    const auto left = m_apparentValue * trackLength / 255.0 + m_trackStart.x();
    const auto handlePos = QPointF(left, m_halfRectHeight);
    const auto thumbRadius = m_thumbRadius * m_thumbScaleRatio / 100.0;

    painter.setPen(Qt::NoPen);
    auto t = m_apparentValue;
    if (t > 255)
        t = 255;
    if (t < 0)
        t = 0;
    // Interpolate thumb color between off and on states
    const auto lerp = [t](const int from, const int to) { return from + (to - from) * t / 255; };
    painter.setBrush(QColor(lerp(m_thumbOffColor.red(), m_thumbOnColor.red()),
                            lerp(m_thumbOffColor.green(), m_thumbOnColor.green()),
                            lerp(m_thumbOffColor.blue(), m_thumbOnColor.blue())));
    painter.drawEllipse(handlePos, thumbRadius, thumbRadius);
}

int SwitchButton::apparentValue() const {
    return m_apparentValue;
}

void SwitchButton::setApparentValue(const int x) {
    m_apparentValue = x;
    repaint();
}

int SwitchButton::thumbScaleRatio() const {
    return m_thumbScaleRatio;
}

void SwitchButton::setThumbScaleRatio(const int ratio) {
    m_thumbScaleRatio = ratio;
    repaint();
}

QColor SwitchButton::trackOffColor() const {
    return m_trackOffColor;
}

void SwitchButton::setTrackOffColor(const QColor &color) {
    if (m_trackOffColor == color)
        return;
    m_trackOffColor = color;
    update();
}

QColor SwitchButton::trackOnColor() const {
    return m_trackOnColor;
}

void SwitchButton::setTrackOnColor(const QColor &color) {
    if (m_trackOnColor == color)
        return;
    m_trackOnColor = color;
    update();
}

QColor SwitchButton::thumbOffColor() const {
    return m_thumbOffColor;
}

void SwitchButton::setThumbOffColor(const QColor &color) {
    if (m_thumbOffColor == color)
        return;
    m_thumbOffColor = color;
    update();
}

QColor SwitchButton::thumbOnColor() const {
    return m_thumbOnColor;
}

void SwitchButton::setThumbOnColor(const QColor &color) {
    if (m_thumbOnColor == color)
        return;
    m_thumbOnColor = color;
    update();
}

void SwitchButton::updateAnimationDuration() {
    int valueDuration = 0;
    int hoverDuration = 0;
    if (animationLevel() == AnimationGlobal::Full) {
        valueDuration = 400;
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
    const auto type = event->type();
    if (type == QEvent::HoverEnter || type == QEvent::MouseButtonRelease) {
        m_thumbHoverAnimation.stop();
        m_thumbHoverAnimation.setStartValue(m_thumbScaleRatio);
        m_thumbHoverAnimation.setEndValue(125);
        m_thumbHoverAnimation.start();
    } else if (type == QEvent::HoverLeave) {
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
