#include "TapTempoButton.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyle>

TapTempoButton::TapTempoButton(QWidget *parent) : Button(parent) {
    m_progressAnimation.setTargetObject(this);
    m_progressAnimation.setPropertyName("apparentProgress");
    m_progressAnimation.setDuration(150);
    m_progressAnimation.setEasingCurve(QEasingCurve::OutCubic);
}

double TapTempoButton::progress() const {
    return m_progress;
}

void TapTempoButton::setProgress(double progress) {
    progress = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(m_progressAnimation.endValue().toDouble(), progress))
        return;

    m_progressAnimation.stop();
    m_progressAnimation.setStartValue(m_progress);
    m_progressAnimation.setEndValue(progress);
    m_progressAnimation.start();
}

void TapTempoButton::setProgressImmediately(double progress) {
    progress = qBound(0.0, progress, 1.0);
    m_progressAnimation.stop();
    m_progressAnimation.setStartValue(progress);
    m_progressAnimation.setEndValue(progress);
    setApparentProgress(progress);
}

QColor TapTempoButton::progressColor() const {
    return m_progressColor;
}

void TapTempoButton::setProgressColor(const QColor &color) {
    if (m_progressColor == color)
        return;

    m_progressColor = color;
    update();
}

bool TapTempoButton::isStable() const {
    return m_stable;
}

void TapTempoButton::setStable(bool stable) {
    if (m_stable == stable)
        return;

    m_stable = stable;
    style()->unpolish(this);
    style()->polish(this);
    updateGeometry();
    update();
}

double TapTempoButton::apparentProgress() const {
    return m_progress;
}

void TapTempoButton::setApparentProgress(double progress) {
    if (qFuzzyCompare(m_progress, progress))
        return;

    m_progress = progress;
    update();
}

void TapTempoButton::paintEvent(QPaintEvent *event) {
    if (m_progress > 0.0 && m_progressColor.alpha() > 0) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF buttonRect(rect());
        QPainterPath clipPath;
        clipPath.addRoundedRect(buttonRect, 4.0, 4.0);
        painter.setClipPath(clipPath);

        QRectF progressRect(buttonRect);
        progressRect.setWidth(buttonRect.width() * m_progress);
        painter.fillRect(progressRect, m_progressColor);
    }

    Button::paintEvent(event);
}
