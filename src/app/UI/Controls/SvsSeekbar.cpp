#include "SvsSeekbar.h"

#include <cmath>

#include <QAccessible>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QVariantAnimation>

namespace SVS {

    class SeekBarPrivate : public QObject {
        Q_DECLARE_PUBLIC(SeekBar)
    public:
        SeekBar *q_ptr;
        void calculateParams();
        int padding = 0;
        int trackPenWidth = 0;
        QRect rect;
        int halfHeight;
        int actualStart = 0;
        int actualEnd = 0;
        int actualLength = 0;
        QPoint trackStartPoint;
        QPoint trackEndPoint;
        int activeStartPos = 0;
        QPointF activeStartPoint;
        QPointF activeEndPoint;
        int valuePos = 0;
        float handlePenWidth = 0;
        float handleRadius = 0;

        bool hasTracking = true;
        bool isSliderDown = false;

        bool resetOnDoubleClick = true;

        double sliderValue = 0;
        double value = 0;
        double defaultValue = 0;
        double maximum = 100;
        double minimum = -100;
        double interval = 0;
        double singleStep = 1;
        double pageStep = 10;
        double trackActiveStartValue = 0;
        bool mouseOnHandle(const QPoint &mousePos) const;
        QTimer *timer;
        bool doubleClickLocked = false;
        QVariantAnimation *thumbHoverAnimation;


        QColor trackInactiveColor = QColor(0, 0, 0, 32);
        QColor trackActiveColor = QColor(112, 156, 255);
        QColor thumbFillColor = QColor(112, 156, 255);
        QColor thumbBorderColor = QColor(255, 255, 255);
        int animationDuration = 200;


        int thumbBorderRatio = 102; // ratio max = 255;
        void setThumbBorderRatio(const QVariant &ratio);

        double boundAndRound(double value) const;
        void setSliderPosition(double value);
        void setValue(double value);

        std::function<double(double)> displayValueConverter = [](double v) { return v; };
    };

    void SeekBarPrivate::calculateParams() {
        Q_Q(const SeekBar);
        rect = q->rect();
        halfHeight = rect.height() / 2;
        padding = halfHeight;
        trackPenWidth = q->rect().height() / 3;

        // Calculate slider track
        actualStart = rect.left() + padding;
        actualEnd = rect.right() - padding;
        actualLength = rect.width() - 2 * padding;
        trackStartPoint.setX(actualStart);
        trackStartPoint.setY(halfHeight);
        trackEndPoint.setX(actualEnd);
        trackEndPoint.setY(halfHeight);

        // Calculate slider track active
        activeStartPos =
            int(actualLength * (trackActiveStartValue - minimum) / (maximum - minimum)) + padding;
        activeStartPoint.setX(activeStartPos);
        activeStartPoint.setY(halfHeight);
        valuePos = int(actualLength * (sliderValue - minimum) / (maximum - minimum)) + padding;
        activeEndPoint.setX(valuePos);
        activeEndPoint.setY(halfHeight);
    }

    bool SeekBarPrivate::mouseOnHandle(const QPoint &mousePos) const {
        auto pos = mousePos;
        auto valuePos_ = actualLength * (sliderValue - minimum) / (maximum - minimum) + padding;
        if (pos.x() >= valuePos_ - halfHeight && pos.x() <= valuePos_ + halfHeight) {
            return true;
        }
        return false;
    }

    void SeekBarPrivate::setThumbBorderRatio(const QVariant &ratio) {
        Q_Q(SeekBar);
        thumbBorderRatio = ratio.toInt();
        q->update();
    }

    double SeekBarPrivate::boundAndRound(double value_) const {
        value_ = qBound(minimum, value_, maximum);
        if (!qFuzzyIsNull(interval)) {
            value_ = minimum + interval * std::round((value_ - minimum) / interval);
        }
        return value_;
    }

    void SeekBarPrivate::setSliderPosition(double value_) {
        Q_Q(SeekBar);
        value_ = boundAndRound(value_);
        if (!qFuzzyCompare(value_, sliderValue)) {
            sliderValue = value_;
            if (isSliderDown)
                Q_EMIT q->sliderMoved(value_);
            q->update();
        }
        if (hasTracking && !qFuzzyCompare(value_, value)) {
            value = value_;
            QAccessibleValueChangeEvent event(q, value);
            QAccessible::updateAccessibility(&event);
            Q_EMIT q->valueChanged(value_);
        }
    }

    void SeekBarPrivate::setValue(double value_) {
        Q_Q(SeekBar);
        value_ = boundAndRound(value_);
        if (!qFuzzyCompare(value_, sliderValue)) {
            sliderValue = value_;
            if (isSliderDown)
                Q_EMIT q->sliderMoved(value_);
            q->update();
        }
        if (!qFuzzyCompare(value_, value)) {
            value = value_;
            QAccessibleValueChangeEvent event(q, displayValueConverter(value_));
            QAccessible::updateAccessibility(&event);
            Q_EMIT q->valueChanged(value_);
        }
    }

    SeekBar::SeekBar(QWidget *parent) : SeekBar(parent, *new SeekBarPrivate) {
        Q_D(SeekBar);
        d->q_ptr = this;
        d->timer = new QTimer(this);
        d->timer->setInterval(400);
        QObject::connect(d->timer, &QTimer::timeout, this, [=]() {
            d->timer->stop();
            d->doubleClickLocked = false;
        });
        this->setMinimumWidth(50);
        setMinimumHeight(20);
        setMaximumHeight(20);
        setAttribute(Qt::WA_Hover, true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFocusPolicy(Qt::StrongFocus);
        installEventFilter(this);
        d->thumbHoverAnimation = new QVariantAnimation(this);
        d->thumbHoverAnimation->setDuration(d->animationDuration);
        d->thumbHoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
        connect(d->thumbHoverAnimation, &QVariantAnimation::valueChanged, d,
                &SeekBarPrivate::setThumbBorderRatio);
        d->calculateParams();
    }

    SeekBar::~SeekBar() = default;

    double SeekBar::maximum() const {
        Q_D(const SeekBar);
        return d->maximum;
    }

    void SeekBar::setMaximum(double maximum) {
        Q_D(SeekBar);
        setRange(qMin(d->minimum, maximum), maximum);
    }

    double SeekBar::minimum() const {
        Q_D(const SeekBar);
        return d->minimum;
    }

    void SeekBar::setMinimum(double minimum) {
        Q_D(SeekBar);
        setRange(minimum, qMax(d->maximum, minimum));
    }

    void SeekBar::setRange(double minimum, double maximum) {
        Q_D(SeekBar);
        d->minimum = minimum;
        d->maximum = maximum;
        d->setValue(d->value);
        update();
    }

    double SeekBar::interval() const {
        Q_D(const SeekBar);
        return d->interval;
    }

    void SeekBar::setInterval(double interval) {
        Q_D(SeekBar);
        d->interval = interval;
        d->setValue(d->value);
        update();
    }

    double SeekBar::singleStep() const {
        Q_D(const SeekBar);
        return d->singleStep;
    }

    void SeekBar::setSingleStep(double step) {
        Q_D(SeekBar);
        d->singleStep = step;
    }

    double SeekBar::pageStep() const {
        Q_D(const SeekBar);
        return d->pageStep;
    }

    void SeekBar::setPageStep(double step) {
        Q_D(SeekBar);
        d->pageStep = step;
    }

    bool SeekBar::hasTracking() const {
        Q_D(const SeekBar);
        return d->hasTracking;
    }

    void SeekBar::setTracking(bool enable) {
        Q_D(SeekBar);
        d->hasTracking = enable;
    }

    bool SeekBar::isSliderDown() const {
        Q_D(const SeekBar);
        return d->isSliderDown;
    }

    void SeekBar::setSliderDown(bool down) {
        Q_D(SeekBar);
        d->isSliderDown = down;
    }

    double SeekBar::sliderPosition() const {
        Q_D(const SeekBar);
        return d->sliderValue;
    }

    void SeekBar::setSliderPosition(double position) {
        Q_D(SeekBar);
        position = d->boundAndRound(position);
        if (qFuzzyCompare(position, d->sliderValue))
            return;
        d->sliderValue = position;
        if (!d->hasTracking)
            update();
        if (d->isSliderDown)
            emit sliderMoved(position);
        if (d->hasTracking)
            setValue(d->sliderValue);
    }

    double SeekBar::trackActiveStartValue() const {
        Q_D(const SeekBar);
        return d->trackActiveStartValue;
    }

    void SeekBar::setTrackActiveStartValue(double pos) {
        Q_D(SeekBar);
        d->trackActiveStartValue = pos;
        update();
    }

    double SeekBar::value() const {
        Q_D(const SeekBar);
        return d->value;
    }

    double SeekBar::defaultValue() const {
        Q_D(const SeekBar);
        return d->defaultValue;
    }

    void SeekBar::setDefaultValue(double value) {
        Q_D(SeekBar);
        d->defaultValue = value;
        update();
    }

    void SeekBar::setValue(double value) {
        Q_D(SeekBar);
        d->setValue(value);
    }

    void SeekBar::resetValue() {
        Q_D(SeekBar);
        d->setValue(d->defaultValue);
    }

    bool SeekBar::eventFilter(QObject *object, QEvent *event) {
        Q_D(SeekBar);
        if (event->type() == QEvent::HoverEnter) {
            d->thumbHoverAnimation->stop();
            d->thumbHoverAnimation->setStartValue(d->thumbBorderRatio);
            d->thumbHoverAnimation->setEndValue(77);
            d->thumbHoverAnimation->start();
        } else if (event->type() == QEvent::HoverLeave) {
            d->thumbHoverAnimation->stop();
            d->thumbHoverAnimation->setStartValue(d->thumbBorderRatio);
            d->thumbHoverAnimation->setEndValue(102);
            d->thumbHoverAnimation->start();
        }
        return QObject::eventFilter(object, event);
    }

    void SeekBar::paintEvent(QPaintEvent *event) {
        Q_D(SeekBar);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        // Fill background
        //     painter.fillRect(rect(), QColor("#d0d0d0"));

        // Draw slider track inactive(background)
        QPen pen;
        pen.setColor(d->trackInactiveColor);
        pen.setCapStyle(Qt::RoundCap);
        pen.setWidth(d->trackPenWidth);
        painter.setPen(pen);
        painter.drawLine(d->trackStartPoint, d->trackEndPoint);

        d->valuePos =
            int(d->actualLength * (d->sliderValue - d->minimum) / (d->maximum - d->minimum)) +
            d->padding;
        d->activeEndPoint.setX(d->valuePos);

        // Draw slider track active
        pen.setColor(d->trackActiveColor);
        pen.setWidth(d->trackPenWidth);
        painter.setPen(pen);
        painter.drawLine(d->activeStartPoint, d->activeEndPoint);

        // Draw handle

        // Calculate handle
        d->handlePenWidth = d->halfHeight * d->thumbBorderRatio / 255.0;
        //    qDebug() << d->thumbBorderRatio;
        d->handleRadius = d->halfHeight - d->handlePenWidth;
        pen.setColor(d->thumbBorderColor);
        pen.setWidthF(d->handlePenWidth);
        painter.setPen(pen);
        painter.setBrush(d->thumbFillColor);
        painter.drawEllipse(d->activeEndPoint, d->handleRadius, d->handleRadius);

        painter.end();
    }

    void SeekBar::resizeEvent(QResizeEvent *event) {
        Q_D(SeekBar);
        d->calculateParams();
    }

    void SeekBar::mouseMoveEvent(QMouseEvent *event) {
        Q_D(SeekBar);
        auto pos = event->pos();
        auto posValue =
            (pos.x() - d->padding) * (d->maximum - d->minimum) / d->actualLength + d->minimum;
        d->setSliderPosition(posValue);
        QWidget::mouseMoveEvent(event);
    }

    void SeekBar::mouseDoubleClickEvent(QMouseEvent *event) {
        Q_D(SeekBar);
        auto pos = event->pos();
        if (d->resetOnDoubleClick && d->mouseOnHandle(pos))
            resetValue();
        QWidget::mouseDoubleClickEvent(event);
    }

    void SeekBar::mousePressEvent(QMouseEvent *event) {
        Q_D(SeekBar);
        d->timer->start();
        d->thumbHoverAnimation->stop();
        d->thumbHoverAnimation->setStartValue(d->thumbBorderRatio);
        d->thumbHoverAnimation->setEndValue(114);
        d->thumbHoverAnimation->start();
        auto pos = event->pos();
        if (!d->mouseOnHandle(pos) && !d->doubleClickLocked) {
            auto posValue =
                (pos.x() - d->padding) * (d->maximum - d->minimum) / d->actualLength + d->minimum;
            d->setValue(posValue);
        }
        setSliderDown(true);
        d->doubleClickLocked = true;
    }

    void SeekBar::mouseReleaseEvent(QMouseEvent *event) {
        Q_D(SeekBar);
        d->thumbHoverAnimation->stop();
        d->thumbHoverAnimation->setStartValue(d->thumbBorderRatio);
        d->thumbHoverAnimation->setEndValue(77);
        d->thumbHoverAnimation->start();
        setSliderDown(false);
        d->setValue(d->sliderValue);
        QWidget::mouseReleaseEvent(event);
    }

    void SeekBar::keyPressEvent(QKeyEvent *event) {
        Q_D(SeekBar);
        switch (event->key()) {
            case Qt::Key_Left:
            case Qt::Key_Down:
                d->setValue(d->value - d->singleStep);
                break;
            case Qt::Key_Right:
            case Qt::Key_Up:
                d->setValue(d->value + d->singleStep);
                break;
            case Qt::Key_PageDown:
                d->setValue(d->value - d->pageStep);
                break;
            case Qt::Key_PageUp:
                d->setValue(d->value + d->pageStep);
                break;
            case Qt::Key_Home:
                d->setValue(d->minimum);
                break;
            case Qt::Key_End:
                d->setValue(d->maximum);
                break;
            case Qt::Key_Return:
            case Qt::Key_Space:
                if (event->isAutoRepeat())
                    d->setValue(d->defaultValue);
                break;
            default:
                event->ignore();
                break;
        }
    }

    double SeekBar::displayValue() const {
        Q_D(const SeekBar);
        return d->displayValueConverter(d->value);
    }

    void SeekBar::setDisplayValueConverter(const std::function<double(double)> &converter) {
        Q_D(SeekBar);
        d->displayValueConverter = converter;
    }

    SeekBar::SeekBar(QWidget *parent, SeekBarPrivate &d) : QWidget(parent), d_ptr(&d) {
    }

    QColor SeekBar::trackInactiveColor() const {
        Q_D(const SeekBar);
        return d->trackInactiveColor;
    }

    void SeekBar::setTrackInactiveColor(const QColor &color) {
        Q_D(SeekBar);
        d->trackInactiveColor = color;
        update();
    }

    QColor SeekBar::trackActiveColor() const {
        Q_D(const SeekBar);
        return d->trackActiveColor;
    }

    void SeekBar::setTrackActiveColor(const QColor &color) {
        Q_D(SeekBar);
        d->trackActiveColor = color;
        update();
    }

    QColor SeekBar::thumbFillColor() const {
        Q_D(const SeekBar);
        return d->thumbFillColor;
    }

    void SeekBar::setThumbFillColor(const QColor &color) {
        Q_D(SeekBar);
        d->thumbFillColor = color;
        update();
    }

    QColor SeekBar::thumbBorderColor() const {
        Q_D(const SeekBar);
        return d->thumbBorderColor;
    }

    void SeekBar::setThumbBorderColor(const QColor &color) {
        Q_D(SeekBar);
        d->thumbBorderColor = color;
        update();
    }

    int SeekBar::animationDuration() const {
        Q_D(const SeekBar);
        return d->animationDuration;
    }

    void SeekBar::setAnimationDuration(int dur) {
        Q_D(SeekBar);
        d->animationDuration = dur;
        d->thumbHoverAnimation->setDuration(d->animationDuration);
    }

    bool SeekBar::resetOnDoubleClick() const {
        Q_D(const SeekBar);
        return d->resetOnDoubleClick;
    }

    void SeekBar::setResetOnDoubleClick(bool a) {
        Q_D(SeekBar);
        d->resetOnDoubleClick = a;
    }

} // SVS