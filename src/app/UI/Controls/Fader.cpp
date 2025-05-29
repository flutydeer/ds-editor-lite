//
// Created by FlutyDeer on 2025/3/26.
//

#include "Fader.h"

#include <cmath>

#include <QAccessible>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QVariantAnimation>

class FaderPrivate : public QObject {
    Q_DECLARE_PUBLIC(Fader)

public:
    Fader* q_ptr;

    double decibelMinimum = -54;
    double decibelMaximum = 6;
    double decibelDefaultValue = 0;
    double scalePower = 0.4;

    double paddingVertical = 0;
    double trackPenWidth = 2;
    double trackGraduatePenWidth = 1;
    QPointF trackStartPoint;
    QPointF trackEndPoint;
    double actualLength = 0;
    QPointF activeStartPoint;
    QPointF activeEndPoint;
    int valuePos = 0;
    double zeroGraduateY = 0;
    double thumbRadius = 2;
    double thumbGraduateWidth = 1;

    QSizeF thumbSize = {16, 24};
    QPointF thumbPos = {0, 0};

    bool hasTracking = true;
    bool isSliderDown = false;

    bool resetOnDoubleClick = true;

    double decibelValue = 0;
    double decibelSliderValue = 0;

    double linearMaximum = 1;
    double linearMinimum = 0;
    double interval = 0;
    double singleStep = 1;
    double pageStep = 3;
    double trackActiveStartValue = 0;

    bool mouseMoveBarrier = false;
    bool canMoveThumb = false;

    bool mouseOnThumb(const QPoint&mousePos) const;

    QTimer* timer;
    bool doubleClickWindow = false;
    QVariantAnimation* thumbHoverAnimation;

    QColor trackInactiveColor = {22, 22, 22};
    QColor trackActiveColor = {155, 186, 255};
    QColor thumbFillColor = {211, 214, 224};
    QColor thumbGraduateColor{22, 22, 22};
    int animationDuration = 200;

    int thumbBorderRatio = 102; // ratio max = 255;
    void setThumbBorderRatio(const QVariant&ratio);

    double boundAndRound(double value) const;

    void setSliderPosition(double value);

    void setDecibelValue(double value);

    void calculateParams();

    double gainToSliderValue(double gain) const;

    double gainFromSliderValue(double value) const;

    std::function<double(double)> displayValueConverter = [](double v) { return v; };
};

bool FaderPrivate::mouseOnThumb(const QPoint&mousePos) const {
    auto thumbRect = QRectF(thumbPos, thumbSize);
    return thumbRect.contains(mousePos);
}

void FaderPrivate::setThumbBorderRatio(const QVariant&ratio) {
    Q_Q(Fader);
    thumbBorderRatio = ratio.toInt();
    q->update();
}

double FaderPrivate::boundAndRound(double value_) const {
    value_ = qBound(decibelMinimum, value_, decibelMaximum);
    // if (!qFuzzyIsNull(interval)) {
    //     value_ = linearMinimum + interval * std::round((value_ - linearMinimum) / interval);
    // }
    return value_;
}

void FaderPrivate::setSliderPosition(double value_) {
    Q_Q(Fader);
    value_ = boundAndRound(value_);
    if (!qFuzzyCompare(value_, decibelSliderValue)) {
        decibelSliderValue = value_;
        if (isSliderDown)
            Q_EMIT q->sliderMoved(value_);
        q->update();
    }
    if (hasTracking && !qFuzzyCompare(value_, decibelSliderValue)) {
        decibelSliderValue = value_;
        QAccessibleValueChangeEvent event(q, decibelSliderValue);
        QAccessible::updateAccessibility(&event);
        Q_EMIT q->valueChanged(value_);
    }
}

void FaderPrivate::setDecibelValue(double value_) {
    Q_Q(Fader);
    value_ = boundAndRound(value_);
    if (!qFuzzyCompare(value_, decibelSliderValue)) {
        decibelSliderValue = value_;
        if (isSliderDown)
            Q_EMIT q->sliderMoved(value_);
    }
    if (!qFuzzyCompare(value_, decibelValue)) {
        decibelValue = value_;
        QAccessibleValueChangeEvent event(q, displayValueConverter(value_));
        QAccessible::updateAccessibility(&event);
        Q_EMIT q->valueChanged(value_);
    }
    q->update();
}

void FaderPrivate::calculateParams() {
    Q_Q(Fader);
    auto centerX = q->rect().width() / 2.0;
    paddingVertical = thumbSize.height() / 2.0 + 8;

    // Calculate slider track
    auto actualStart = q->rect().top() + paddingVertical;
    auto actualEnd = q->rect().bottom() - paddingVertical;
    actualLength = actualEnd - actualStart;
    trackStartPoint = {centerX, actualStart};
    trackEndPoint = {centerX, actualEnd};

    // Calculate slider track active
    activeStartPoint.setX(centerX);
    activeStartPoint.setY(q->rect().bottom() - paddingVertical);
    auto linearSliderValue = gainToSliderValue(decibelSliderValue);
    auto valueLength = (actualLength * (linearSliderValue - linearMinimum) / (linearMaximum - linearMinimum));
    activeEndPoint.setX(centerX);
    activeEndPoint.setY(q->rect().bottom() - paddingVertical - valueLength);

    // Calculate 0 dB graduate
    auto zeroLength = (actualLength * (gainToSliderValue(0) - linearMinimum) / (linearMaximum - linearMinimum)); //0dB
    zeroGraduateY = q->rect().bottom() - paddingVertical - zeroLength;

    // Calculate thumb
    auto thumbX = centerX - (thumbSize.width() / 2.0);
    auto thumbY = activeEndPoint.y() - (thumbSize.height() / 2.0);
    thumbPos = {thumbX, thumbY};
}

Fader::Fader(QWidget* parent) : Fader(parent, *new FaderPrivate) {
    Q_D(Fader);
    setAttribute(Qt::WA_StyledBackground);
    d->q_ptr = this;
    d->timer = new QTimer(this);
    d->timer->setInterval(400);
    QObject::connect(d->timer, &QTimer::timeout, this, [=]() {
        d->timer->stop();
        d->doubleClickWindow = false;
    });
    resetValue();
    this->setMinimumWidth(32);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);
    d->thumbHoverAnimation = new QVariantAnimation(this);
    d->thumbHoverAnimation->setDuration(d->animationDuration);
    d->thumbHoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(d->thumbHoverAnimation, &QVariantAnimation::valueChanged, d,
            &FaderPrivate::setThumbBorderRatio);
}

Fader::~Fader() = default;

bool Fader::hasTracking() const {
    Q_D(const Fader);
    return d->hasTracking;
}

void Fader::setTracking(bool enable) {
    Q_D(Fader);
    d->hasTracking = enable;
}

bool Fader::isSliderDown() const {
    Q_D(const Fader);
    return d->isSliderDown;
}

void Fader::setSliderDown(bool down) {
    Q_D(Fader);
    d->isSliderDown = down;
}

double Fader::sliderPosition() const {
    Q_D(const Fader);
    return d->decibelSliderValue;
}

void Fader::setSliderPosition(double position) {
    Q_D(Fader);
    position = d->boundAndRound(position);
    if (qFuzzyCompare(position, d->decibelSliderValue))
        return;
    d->decibelSliderValue = position;
    if (!d->hasTracking)
        update();
    if (d->isSliderDown)
        emit sliderMoved(d->gainFromSliderValue(position));
    if (d->hasTracking)
        setValue(d->decibelSliderValue);
}

double Fader::value() const {
    Q_D(const Fader);
    return d->decibelSliderValue;
}

void Fader::setValue(double dB) {
    Q_D(Fader);
    d->setDecibelValue(dB);
}

void Fader::resetValue() {
    Q_D(Fader);
    d->setDecibelValue(d->decibelDefaultValue);
}

bool Fader::eventFilter(QObject* object, QEvent* event) {
    Q_D(Fader);
    if (event->type() == QEvent::HoverEnter) {
        d->thumbHoverAnimation->stop();
        d->thumbHoverAnimation->setStartValue(d->thumbBorderRatio);
        d->thumbHoverAnimation->setEndValue(77);
        d->thumbHoverAnimation->start();
    }
    else if (event->type() == QEvent::HoverLeave) {
        d->thumbHoverAnimation->stop();
        d->thumbHoverAnimation->setStartValue(d->thumbBorderRatio);
        d->thumbHoverAnimation->setEndValue(102);
        d->thumbHoverAnimation->start();
    }
    return QObject::eventFilter(object, event);
}

void Fader::paintEvent(QPaintEvent* event) {
    Q_D(Fader);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    d->calculateParams();
    QPen pen;

    auto drawSliderTrackInactive = [&] {
        pen.setColor(d->trackInactiveColor);
        pen.setCapStyle(Qt::RoundCap);
        pen.setWidthF(d->trackPenWidth);
        painter.setPen(pen);
        painter.drawLine(d->trackStartPoint, d->trackEndPoint);
    };

    auto drawZeroGraduate = [&] {
        pen.setColor(d->trackInactiveColor);
        pen.setWidthF(d->trackGraduatePenWidth);
        painter.setPen(pen);

        auto padding = 2.0;
        auto x1 = rect().left() + padding;
        auto x2 = (rect().width() / 2.0) - padding;
        auto graduateLeftStartPoint = QPointF(x1, d->zeroGraduateY);
        auto graduateLeftEndPoint = QPointF(x2, d->zeroGraduateY);
        painter.drawLine(graduateLeftStartPoint, graduateLeftEndPoint);

        auto x3 = (rect().width() / 2.0) + padding;
        auto x4 = rect().right() - padding;
        auto graduateRightStartPoint = QPointF(x3, d->zeroGraduateY);
        auto graduateRightEndPoint = QPointF(x4, d->zeroGraduateY);
        painter.drawLine(graduateRightStartPoint, graduateRightEndPoint);
    };

    auto drawSliderTrackActive = [&] {
        pen.setColor(d->trackActiveColor);
        pen.setWidthF(d->trackPenWidth);
        painter.setPen(pen);
        painter.drawLine(d->activeStartPoint, d->activeEndPoint);
    };

    auto drawThumbBackground = [&] {
        painter.setPen(Qt::NoPen);
        painter.setBrush(d->thumbFillColor);
        painter.drawRoundedRect({d->thumbPos, d->thumbSize}, d->thumbRadius, d->thumbRadius);
    };

    auto drawThumbGraduate = [&] {
        pen.setColor(d->trackInactiveColor);
        pen.setWidthF(d->thumbGraduateWidth);
        painter.setPen(pen);
        auto thumbRect = QRectF(d->thumbPos, d->thumbSize);
        auto thumbPadding = 2.0;
        auto thumbGraduateStartPoint = QPointF(thumbRect.left() + thumbPadding, d->activeEndPoint.y());
        auto thumbGraduateEndPoint = QPointF(thumbRect.right() - thumbPadding, d->activeEndPoint.y());
        painter.drawLine(thumbGraduateStartPoint, thumbGraduateEndPoint);
    };

    drawSliderTrackInactive();
    drawZeroGraduate();
    drawSliderTrackActive();
    drawThumbBackground();
    drawThumbGraduate();
}

void Fader::resizeEvent(QResizeEvent* event) {
    Q_D(Fader);
    d->calculateParams();
    update();
}

void Fader::mouseMoveEvent(QMouseEvent* event) {
    Q_D(Fader);
    if (d->mouseMoveBarrier || !d->canMoveThumb)
        return;

    auto pos = event->pos();
    auto posValue =
            ((d->actualLength + d->paddingVertical - pos.y()) * (d->linearMaximum - d->linearMinimum) / d->actualLength)
            + d->
            linearMinimum;
    d->setSliderPosition(d->gainFromSliderValue(posValue));
    QWidget::mouseMoveEvent(event);
}

void Fader::mouseDoubleClickEvent(QMouseEvent* event) {
    Q_D(Fader);
    // auto pos = event->pos();
    // if (d->resetOnDoubleClick && d->mouseOnThumb(pos))
    //     resetValue();
    QWidget::mouseDoubleClickEvent(event);
}

void Fader::mousePressEvent(QMouseEvent* event) {
    Q_D(Fader);
    d->thumbHoverAnimation->stop();
    d->thumbHoverAnimation->setStartValue(d->thumbBorderRatio);
    d->thumbHoverAnimation->setEndValue(114);
    d->thumbHoverAnimation->start();

    auto pos = event->pos();

    // Move cursor to the center of thumb
    if (d->mouseOnThumb(pos)) {
        auto thumbRect = QRectF(d->thumbPos, d->thumbSize);
        d->mouseMoveBarrier = true;
        QCursor::setPos(mapToGlobal(thumbRect.center().toPoint()));
        d->mouseMoveBarrier = false;
        setSliderDown(true);
        d->canMoveThumb = true;

        if (d->doubleClickWindow) {
            resetValue();
            d->doubleClickWindow = false;
        }
        else {
            d->doubleClickWindow = true;
            d->timer->start();
        }
    }
    else
        d->canMoveThumb = false;

    // Ignore if mouse not on thumb
    // else if (!d->doubleClickLocked) {
    //     auto posValue =
    //             ((d->actualLength + d->paddingVertical - pos.y()) * (d->maximum - d->minimum) / d->actualLength) + d->
    //             minimum;
    //     d->setValue(posValue);
    // }
    // setSliderDown(true);
    // d->doubleClickLocked = true;
}

void Fader::mouseReleaseEvent(QMouseEvent* event) {
    Q_D(Fader);
    d->thumbHoverAnimation->stop();
    d->thumbHoverAnimation->setStartValue(d->thumbBorderRatio);
    d->thumbHoverAnimation->setEndValue(77);
    d->thumbHoverAnimation->start();
    setSliderDown(false);
    d->setDecibelValue(d->decibelSliderValue);
    QWidget::mouseReleaseEvent(event);
}

void Fader::keyPressEvent(QKeyEvent* event) {
    Q_D(Fader);
    switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Down:
            d->setDecibelValue(d->decibelSliderValue - d->singleStep);
            break;
        case Qt::Key_Right:
        case Qt::Key_Up:
            d->setDecibelValue(d->decibelSliderValue + d->singleStep);
            break;
        case Qt::Key_PageDown:
            d->setDecibelValue(d->decibelSliderValue - d->pageStep);
            break;
        case Qt::Key_PageUp:
            d->setDecibelValue(d->decibelSliderValue + d->pageStep);
            break;
        case Qt::Key_Home:
            d->setDecibelValue(d->linearMinimum);
            break;
        case Qt::Key_End:
            d->setDecibelValue(d->linearMaximum);
            break;
        case Qt::Key_Return:
        case Qt::Key_Space:
            if (event->isAutoRepeat())
                d->setDecibelValue(d->gainToSliderValue(d->decibelDefaultValue));
            break;
        default:
            event->ignore();
            break;
    }
}

double Fader::displayValue() const {
    Q_D(const Fader);
    return d->displayValueConverter(d->decibelSliderValue);
}

void Fader::setDisplayValueConverter(const std::function<double(double)>&converter) {
    Q_D(Fader);
    d->displayValueConverter = converter;
}

Fader::Fader(QWidget* parent, FaderPrivate&d) : QWidget(parent), d_ptr(&d) {
}

QColor Fader::trackInactiveColor() const {
    Q_D(const Fader);
    return d->trackInactiveColor;
}

void Fader::setTrackInactiveColor(const QColor&color) {
    Q_D(Fader);
    d->trackInactiveColor = color;
    update();
}

QColor Fader::trackActiveColor() const {
    Q_D(const Fader);
    return d->trackActiveColor;
}

void Fader::setTrackActiveColor(const QColor&color) {
    Q_D(Fader);
    d->trackActiveColor = color;
    update();
}

QColor Fader::thumbFillColor() const {
    Q_D(const Fader);
    return d->thumbFillColor;
}

void Fader::setThumbFillColor(const QColor&color) {
    Q_D(Fader);
    d->thumbFillColor = color;
    update();
}

QColor Fader::thumbBorderColor() const {
    Q_D(const Fader);
    return {};
    // return d->thumbBorderColor;
}

void Fader::setThumbBorderColor(const QColor&color) {
    Q_D(Fader);
    // d->thumbBorderColor = color;
    update();
}

int Fader::animationDuration() const {
    Q_D(const Fader);
    return d->animationDuration;
}

void Fader::setAnimationDuration(int dur) {
    Q_D(Fader);
    d->animationDuration = dur;
    d->thumbHoverAnimation->setDuration(d->animationDuration);
}

bool Fader::resetOnDoubleClick() const {
    Q_D(const Fader);
    return d->resetOnDoubleClick;
}

void Fader::setResetOnDoubleClick(bool a) {
    Q_D(Fader);
    d->resetOnDoubleClick = a;
}

double FaderPrivate::gainToSliderValue(double gain) const {
    return std::pow((gain - decibelMinimum) / (decibelMaximum - decibelMinimum), 1.0 / scalePower);
}

double FaderPrivate::gainFromSliderValue(double value) const {
    return ((decibelMaximum - decibelMinimum) * std::pow(value, scalePower)) + decibelMinimum;
}
