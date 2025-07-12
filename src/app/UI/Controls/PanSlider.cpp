//
// Created by FlutyDeer on 2025/6/11.
//

#include "PanSlider.h"

#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

class PanSliderPrivate : public QObject {
    Q_DECLARE_PUBLIC(PanSlider)

public:
    PanSlider *q_ptr;

    double panMinimum = -1;
    double panMaximum = 1;
    double panDefaultValue = 0;
    double panStep = 5;

    double thumbWidth = 2;
    double paddingH = 5; // = padding + thumbWidth / 2
    double paddingV = 4;
    double trackPenWidth = 2;
    double trackGraduatePenWidth = 1;

    bool isSliderDown = false;

    double panValue = 0;
    double panSliderValue = 0;

    double linearMaximum = 1;
    double linearMinimum = 0;

    bool mouseMoveBarrier = false;
    bool canMoveThumb = false;

    QTimer timer;
    bool doubleClickWindow = false;
    QPoint mouseDownPos;

    QColor centerGraduateColor = {22, 22, 22};
    QColor trackActiveColor = {155, 186, 255, 64};
    QColor thumbColor = {155, 186, 255};
    int animationDuration = 200;

    double boundAndRound(double value) const;
    void setSliderPosition(double value);
    void setPanValue(double value);

    double panToX(double pan) const;
    double xToPan(double x) const;
};

double PanSliderPrivate::boundAndRound(double value_) const {
    value_ = qBound(panMinimum, value_, panMaximum);
    // if (!qFuzzyIsNull(interval)) {
    //     value_ = linearMinimum + interval * std::round((value_ - linearMinimum) / interval);
    // }
    return value_;
}

void PanSliderPrivate::setSliderPosition(double value_) {
    Q_Q(PanSlider);
    value_ = boundAndRound(value_);
    if (!qFuzzyCompare(value_, panSliderValue)) {
        panSliderValue = value_;
        if (isSliderDown)
            Q_EMIT q->sliderMoved(value_);
        q->update();
    }
    if (!qFuzzyCompare(value_, panSliderValue)) {
        panSliderValue = value_;
        Q_EMIT q->valueChanged(value_);
    }
}

void PanSliderPrivate::setPanValue(double value_) {
    Q_Q(PanSlider);
    value_ = boundAndRound(value_);
    if (!qFuzzyCompare(value_, panSliderValue)) {
        panSliderValue = value_;
        if (isSliderDown)
            Q_EMIT q->sliderMoved(value_);
    }
    if (!qFuzzyCompare(value_, panValue)) {
        panValue = value_;
        Q_EMIT q->valueChanged(value_);
    }
    q->update();
}

double PanSliderPrivate::panToX(double pan) const {
    Q_Q(const PanSlider);
    auto ratio = (pan - panMinimum) / (panMaximum - panMinimum);
    return paddingH + ratio * (q->rect().width() - 2 * paddingH);
}

double PanSliderPrivate::xToPan(double x) const {
    Q_Q(const PanSlider);
    auto ratio = (x - paddingH) / (q->rect().width() - 2 * paddingH);
    return panMinimum + ratio * (panMaximum - panMinimum);
}

PanSlider::PanSlider(QWidget *parent) : PanSlider(parent, *new PanSliderPrivate) {
    Q_D(PanSlider);
    setAttribute(Qt::WA_StyledBackground);
    d->q_ptr = this;
    d->timer.setInterval(400);
    QObject::connect(&d->timer, &QTimer::timeout, this, [=]() {
        d->timer.stop();
        d->doubleClickWindow = false;
    });
    resetValue();
    setMinimumWidth(32);
    setMinimumHeight(24);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);

    // d->thumbHoverAnimation.setDuration(d->animationDuration);
    // d->thumbHoverAnimation.setEasingCurve(QEasingCurve::OutCubic);
    // connect(&d->thumbHoverAnimation, &QVariantAnimation::valueChanged, d,
    //         &PanSliderPrivate::setThumbBorderRatio);
}

PanSlider::~PanSlider() = default;

double PanSlider::sliderPosition() const {
    Q_D(const PanSlider);
    return d->panSliderValue;
}

void PanSlider::setSliderPosition(double position) {
    Q_D(PanSlider);
    position = d->boundAndRound(position);
    if (qFuzzyCompare(position, d->panSliderValue))
        return;
    d->panSliderValue = position;
    if (d->isSliderDown)
        emit sliderMoved(d->xToPan(position));
    setValue(d->panSliderValue);
    update();
}

double PanSlider::value() const {
    Q_D(const PanSlider);
    return d->panSliderValue;
}

void PanSlider::setValue(double pan) {
    Q_D(PanSlider);
    d->setPanValue(pan);
}

void PanSlider::resetValue() {
    Q_D(PanSlider);
    d->setPanValue(d->panDefaultValue);
}

void PanSlider::paintEvent(QPaintEvent *event) {
    Q_D(PanSlider);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen;

    auto drawZeroGraduate = [&] {
        pen.setColor(d->centerGraduateColor);
        pen.setWidthF(d->trackGraduatePenWidth);
        painter.setPen(pen);

        auto x = rect().width() / 2.0;
        auto y1 = rect().top() + d->paddingH;
        auto y2 = rect().height() - d->paddingH;
        painter.drawLine(QPointF(x, y1), QPointF(x, y2));
    };

    auto drawSliderTrackActive = [&] {
        painter.setPen(Qt::NoPen);
        painter.setBrush(d->trackActiveColor);

        auto x1 = d->panToX(d->panSliderValue);
        auto x2 = d->panToX(d->panDefaultValue);
        auto y1 = rect().top() + d->paddingV;
        auto y2 = rect().height() - d->paddingV;

        auto left = qMin(x1, x2);
        auto right = qMax(x1, x2);

        auto width = right - left;
        auto height = y2 - y1;

        painter.drawRect(QRectF(left, y1, width, height));
    };

    auto drawThumb = [&] {
        painter.setPen(Qt::NoPen);
        painter.setBrush(d->thumbColor);

        auto x0 = d->panToX(d->panSliderValue);
        auto x1 = x0 - d->thumbWidth / 2.0;
        auto x2 = x0 + d->thumbWidth / 2.0;
        auto y1 = rect().top() + d->paddingV;
        auto y2 = rect().height() - d->paddingV;

        auto width = x2 - x1;
        auto height = y2 - y1;

        painter.drawRect(QRectF(x1, y1, width, height));
    };

    // drawZeroGraduate();
    drawSliderTrackActive();
    drawThumb();
}

void PanSlider::mouseMoveEvent(QMouseEvent *event) {
    Q_D(PanSlider);
    if (d->mouseMoveBarrier || !d->canMoveThumb) {
        d->mouseMoveBarrier = false;
        return;
    }

    auto pos = event->pos();
    d->setSliderPosition(d->xToPan(pos.x()));
    QWidget::mouseMoveEvent(event);
}

void PanSlider::mouseDoubleClickEvent(QMouseEvent *event) {
    Q_D(PanSlider);
    // auto pos = event->pos();
    // if (d->resetOnDoubleClick && d->mouseOnThumb(pos))
    //     resetValue();
    QWidget::mouseDoubleClickEvent(event);
}

void PanSlider::mousePressEvent(QMouseEvent *event) {
    Q_D(PanSlider);
    if (event->button() != Qt::LeftButton)
        return;

    auto pos = event->pos();
    d->mouseDownPos = pos;

    // Move cursor to the center of thumb
    d->mouseMoveBarrier = true; // 防止 QCursor::setPos 导致意外移动
    auto x = d->panToX(d->panValue);
    auto y = rect().height() / 2.0;
    QCursor::setPos(mapToGlobal(QPointF{x, y}).toPoint());
    d->isSliderDown = true;
    d->canMoveThumb = true;

    if (d->doubleClickWindow) {
        resetValue();
        d->canMoveThumb = false;
        d->doubleClickWindow = false;
    } else {
        d->doubleClickWindow = true;
        d->timer.start();
    }
}

void PanSlider::mouseReleaseEvent(QMouseEvent *event) {
    Q_D(PanSlider);
    if (event->button() != Qt::LeftButton)
        return;

    d->canMoveThumb = true;
    auto currentPos = event->pos();
    if (currentPos != d->mouseDownPos)
        d->setPanValue(d->panSliderValue);

    QWidget::mouseReleaseEvent(event);
}

PanSlider::PanSlider(QWidget *parent, PanSliderPrivate &d) : QWidget(parent), d_ptr(&d) {
}

QColor PanSlider::centerGraduateColor() const {
    Q_D(const PanSlider);
    return d->centerGraduateColor;
}

void PanSlider::setCenterGraduateColor(const QColor &color) {
    Q_D(PanSlider);
    d->centerGraduateColor = color;
    update();
}

QColor PanSlider::trackActiveColor() const {
    Q_D(const PanSlider);
    return d->trackActiveColor;
}

void PanSlider::setTrackActiveColor(const QColor &color) {
    Q_D(PanSlider);
    d->trackActiveColor = color;
    update();
}

QColor PanSlider::thumbFillColor() const {
    Q_D(const PanSlider);
    return d->thumbColor;
}

void PanSlider::setThumbFillColor(const QColor &color) {
    Q_D(PanSlider);
    d->thumbColor = color;
    update();
}

int PanSlider::animationDuration() const {
    Q_D(const PanSlider);
    return d->animationDuration;
}

void PanSlider::setAnimationDuration(int dur) {
    Q_D(PanSlider);
    d->animationDuration = dur;
    // d->thumbHoverAnimation.setDuration(d->animationDuration);
}