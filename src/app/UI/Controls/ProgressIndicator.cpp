//
// Created by fluty on 2023/8/14.
//

#include "ProgressIndicator.h"

#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QtMath>

ProgressIndicator::ProgressIndicator(QWidget *parent) : QWidget(parent) {
    initUi();
}

ProgressIndicator::ProgressIndicator(const IndicatorStyle indicatorStyle, QWidget *parent)
    : QWidget(parent), m_indicatorStyle(indicatorStyle) {
    initUi();
}

void ProgressIndicator::initUi() {
    m_timer.setInterval(8);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        update();
    });
    m_colorPalette = colorPaletteNormal;

    constexpr int animationDurationBase = 250;
    const auto duration = animationLevel() == AnimationGlobal::None
                              ? 0
                              : getScaledAnimationTime(animationDurationBase);

    m_valueAnimation.setTargetObject(this);
    m_valueAnimation.setPropertyName("apparentValue");
    m_valueAnimation.setDuration(duration);
    m_valueAnimation.setEasingCurve(QEasingCurve::OutQuart);

    m_SecondaryValueAnimation.setTargetObject(this);
    m_SecondaryValueAnimation.setPropertyName("apparentSecondaryValue");
    m_SecondaryValueAnimation.setDuration(duration);
    m_SecondaryValueAnimation.setEasingCurve(QEasingCurve::OutQuart);

    m_currentTaskValueAnimation.setTargetObject(this);
    m_currentTaskValueAnimation.setPropertyName("apparentCurrentTaskValue");
    m_currentTaskValueAnimation.setDuration(duration);
    m_currentTaskValueAnimation.setEasingCurve(QEasingCurve::OutQuart);

    switch (m_indicatorStyle) {
        case HorizontalBar:
            setFixedHeight(4);
            calculateBarParams();
            break;
        case Ring:
            setMinimumSize(16, 16);
            setMaximumSize(64, 64);
            calculateRingParams();
            break;
    }

    initializeAnimation();
}

void ProgressIndicator::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen;

    auto drawBarBackground = [&] {
        // Draw track inactive(background)
        pen.setColor(m_colorPalette.inactive);
        pen.setCapStyle(Qt::RoundCap);
        pen.setWidth(m_penWidth);
        painter.setPen(pen);
        painter.drawLine(m_trackStart, m_trackEnd);
    };

    auto drawBarNormalProgress = [&] {
        // Calculate secondary progress value
        const auto valueStartPos = m_padding;
        const auto valueStartPoint = QPoint(valueStartPos, m_halfRectHeight);
        const auto secondaryValuePos =
            static_cast<int>(m_actualLength * (m_apparentSecondaryValue - m_min) /
                             (m_max - m_min)) +
            m_padding;
        const auto secondaryValuePoint2 = QPoint(secondaryValuePos, m_halfRectHeight);

        // Draw secondary progress value
        pen.setColor(m_colorPalette.secondary);
        pen.setWidth(m_penWidth);
        painter.setPen(pen);
        painter.drawLine(valueStartPoint, secondaryValuePoint2);

        // Calculate progress value
        const auto valueLength =
            static_cast<int>(m_actualLength * (m_apparentValue - m_min) / (m_max - m_min));
        const auto valuePos = valueLength + m_padding;
        const auto valuePoint = QPoint(valuePos, m_halfRectHeight);

        // Draw progress value
        pen.setColor(m_colorPalette.total);
        pen.setWidth(m_penWidth);
        painter.setPen(pen);
        painter.drawLine(valueStartPoint, valuePoint);

        // Calculate current task progress value
        const auto curTaskValuePos =
            static_cast<int>(valueLength * (m_apparentCurrentTaskValue - m_min) / (m_max - m_min)) +
            m_padding;
        const auto curTaskValuePoint = QPoint(curTaskValuePos, m_halfRectHeight);

        // Draw current task progress value
        pen.setColor(m_colorPalette.currentTask);
        pen.setWidth(m_penWidth);
        painter.setPen(pen);
        painter.drawLine(valueStartPoint, curTaskValuePoint);
    };

    auto drawBarIndeterminateProgress = [&] {
        const double time = m_elapsedTimer.elapsed() / 1000.0;
        const double totalLength = m_actualLength;
        const double headPos =
            std::fmod(time * m_indeterminateSpeed * totalLength / 360.0, totalLength);
        const double barLength =
            m_indeterminateMinLength / 360.0 * totalLength +
            (m_indeterminateMaxLength - m_indeterminateMinLength) / 360.0 * totalLength *
                (0.5 + 0.5 * std::sin(time * m_indeterminateFrequency * 2.0 * M_PI));
        double tailPos = headPos - barLength;

        const auto headX = static_cast<int>(headPos) + m_padding;
        auto tailX = static_cast<int>(tailPos) + m_padding;

        pen.setColor(m_colorPalette.total);
        pen.setWidth(m_penWidth);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);

        if (tailPos >= 0) {
            painter.drawLine(QPoint(tailX, m_halfRectHeight), QPoint(headX, m_halfRectHeight));
        } else {
            const auto wrapTailX = static_cast<int>(totalLength + tailPos) + m_padding;
            painter.drawLine(QPoint(m_trackStart.x(), m_halfRectHeight),
                             QPoint(headX, m_halfRectHeight));
            painter.drawLine(QPoint(wrapTailX, m_halfRectHeight),
                             QPoint(m_trackEnd.x(), m_halfRectHeight));
        }
    };

    auto drawRingBackground = [&] {
        pen.setColor(m_colorPalette.inactive);
        pen.setWidth(m_penWidth);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawEllipse(m_ringRect);
    };

    auto drawRingNormalProgress = [&] {
        auto start = [] { return 90 * 16; };
        auto getSpanAngle = [](const int spanAngle) { return -spanAngle * 16; };

        // Calculate secondary progress value
        const auto secondaryAngle =
            static_cast<int>(360 * (m_apparentSecondaryValue - m_min) / (m_max - m_min));
        const auto secondarySpan = getSpanAngle(secondaryAngle);

        // Draw secondary progress value
        pen.setColor(m_colorPalette.secondary);
        painter.setPen(pen);
        painter.drawArc(m_ringRect, start(), secondarySpan);

        // Calculate progress value
        const auto valueAngle = static_cast<int>(360 * (m_apparentValue - m_min) / (m_max - m_min));
        const auto valueSpan = getSpanAngle(valueAngle);

        // Draw secondary progress value
        pen.setColor(m_colorPalette.total);
        painter.setPen(pen);
        painter.drawArc(m_ringRect, start(), valueSpan);

        // Calculate current task progress value
        const auto curTaskAngle =
            static_cast<int>(valueAngle * (m_apparentCurrentTaskValue - m_min) / (m_max - m_min));
        const auto curTaskSpan = getSpanAngle(curTaskAngle);

        // Draw secondary progress value
        pen.setColor(m_colorPalette.currentTask);
        painter.setPen(pen);
        painter.drawArc(m_ringRect, start(), curTaskSpan);
    };

    auto drawRingIndeterminateProgress = [&] {
        const double time = m_elapsedTimer.elapsed() / 1000.0;
        const double headAngle = std::fmod(time * m_indeterminateSpeed, 360.0);
        const double arcLength =
            m_indeterminateMinLength +
            (m_indeterminateMaxLength - m_indeterminateMinLength) *
                (0.5 + 0.5 * std::sin(time * m_indeterminateFrequency * 2.0 * M_PI));
        const double tailAngle = headAngle - arcLength;

        const int startAngle = static_cast<int>((90.0 - headAngle) * 16);
        const int spanAngle = static_cast<int>(arcLength * 16);

        pen.setColor(m_colorPalette.total);
        pen.setWidth(m_penWidth);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawArc(m_ringRect, startAngle, spanAngle);
    };

    if (m_indicatorStyle == HorizontalBar) {
        drawBarBackground();
        if (!m_indeterminate)
            drawBarNormalProgress();
        else
            drawBarIndeterminateProgress();
    } else if (m_indicatorStyle == Ring) {
        drawRingBackground();
        if (!m_indeterminate)
            drawRingNormalProgress();
        else
            drawRingIndeterminateProgress();
    }

    painter.end();
}

double ProgressIndicator::value() const {
    return m_value;
}

void ProgressIndicator::setValue(const double value) {
    m_value = value;
    m_valueAnimation.stop();
    m_valueAnimation.setStartValue(m_apparentValue);
    m_valueAnimation.setEndValue(m_value);
    m_valueAnimation.start();
}

double ProgressIndicator::minimum() const {
    return m_min;
}

void ProgressIndicator::setMinimum(const double minimum) {
    m_min = minimum;
    update();
}

double ProgressIndicator::maximum() const {
    return m_max;
}

void ProgressIndicator::setMaximum(const double maximum) {
    m_max = maximum;
    update();
}

void ProgressIndicator::setRange(const double minimum, const double maximum) {
    m_min = minimum;
    m_max = maximum;
    update();
}

double ProgressIndicator::secondaryValue() const {
    return m_secondaryValue;
}

void ProgressIndicator::setSecondaryValue(const double value) {
    m_secondaryValue = value;
    m_SecondaryValueAnimation.stop();
    m_SecondaryValueAnimation.setStartValue(m_apparentSecondaryValue);
    m_SecondaryValueAnimation.setEndValue(m_secondaryValue);
    m_SecondaryValueAnimation.start();
}

void ProgressIndicator::setCurrentTaskValue(const double value) {
    m_currentTaskValue = value;
    m_currentTaskValueAnimation.stop();
    m_currentTaskValueAnimation.setStartValue(m_apparentCurrentTaskValue);
    m_currentTaskValueAnimation.setEndValue(m_currentTaskValue);
    m_currentTaskValueAnimation.start();
}

void ProgressIndicator::reset() {
    m_value = 0;
    m_secondaryValue = 0;
    m_currentTaskValue = 0;
    update();
}

double ProgressIndicator::currentTaskValue() const {
    return m_currentTaskValue;
}

bool ProgressIndicator::indeterminate() const {
    return m_indeterminate;
}

void ProgressIndicator::setIndeterminate(const bool on) {
    m_indeterminate = on;
    if (m_indeterminate) {
        m_elapsedTimer.start();
        m_timer.start();
    } else {
        m_timer.stop();
    }
    update();
}

int ProgressIndicator::thumbProgress() const {
    return m_thumbProgress;
}

void ProgressIndicator::setThumbProgress(const int x) {
    m_thumbProgress = x;
    update();
}

TaskGlobal::Status ProgressIndicator::taskStatus() const {
    return m_taskStatus;
}

void ProgressIndicator::setTaskStatus(const TaskGlobal::Status status) {
    m_taskStatus = status;
    applyColorPalette();
    update();
}

void ProgressIndicator::applyColorPalette() {
    switch (m_taskStatus) {
        case TaskGlobal::Normal:
            m_colorPalette = colorPaletteNormal;
            break;
        case TaskGlobal::Warning:
            m_colorPalette = colorPaletteWarning;
            break;
        case TaskGlobal::Error:
            m_colorPalette = colorPaletteError;
            break;
    }
}

QColor ProgressIndicator::inactiveColor() const {
    return colorPaletteNormal.inactive;
}

void ProgressIndicator::setInactiveColor(const QColor &color) {
    if (colorPaletteNormal.inactive == color)
        return;
    // The inactive color is shared across all three palettes
    colorPaletteNormal.inactive = color;
    colorPaletteWarning.inactive = color;
    colorPaletteError.inactive = color;
    applyColorPalette();
    update();
}

QColor ProgressIndicator::normalTotalColor() const {
    return colorPaletteNormal.total;
}

void ProgressIndicator::setNormalTotalColor(const QColor &color) {
    if (colorPaletteNormal.total == color)
        return;
    colorPaletteNormal.total = color;
    applyColorPalette();
    update();
}

QColor ProgressIndicator::normalSecondaryColor() const {
    return colorPaletteNormal.secondary;
}

void ProgressIndicator::setNormalSecondaryColor(const QColor &color) {
    if (colorPaletteNormal.secondary == color)
        return;
    colorPaletteNormal.secondary = color;
    applyColorPalette();
    update();
}

QColor ProgressIndicator::normalCurrentTaskColor() const {
    return colorPaletteNormal.currentTask;
}

void ProgressIndicator::setNormalCurrentTaskColor(const QColor &color) {
    if (colorPaletteNormal.currentTask == color)
        return;
    colorPaletteNormal.currentTask = color;
    applyColorPalette();
    update();
}

QColor ProgressIndicator::warningTotalColor() const {
    return colorPaletteWarning.total;
}

void ProgressIndicator::setWarningTotalColor(const QColor &color) {
    if (colorPaletteWarning.total == color)
        return;
    colorPaletteWarning.total = color;
    applyColorPalette();
    update();
}

QColor ProgressIndicator::warningSecondaryColor() const {
    return colorPaletteWarning.secondary;
}

void ProgressIndicator::setWarningSecondaryColor(const QColor &color) {
    if (colorPaletteWarning.secondary == color)
        return;
    colorPaletteWarning.secondary = color;
    applyColorPalette();
    update();
}

QColor ProgressIndicator::warningCurrentTaskColor() const {
    return colorPaletteWarning.currentTask;
}

void ProgressIndicator::setWarningCurrentTaskColor(const QColor &color) {
    if (colorPaletteWarning.currentTask == color)
        return;
    colorPaletteWarning.currentTask = color;
    applyColorPalette();
    update();
}

QColor ProgressIndicator::errorTotalColor() const {
    return colorPaletteError.total;
}

void ProgressIndicator::setErrorTotalColor(const QColor &color) {
    if (colorPaletteError.total == color)
        return;
    colorPaletteError.total = color;
    applyColorPalette();
    update();
}

QColor ProgressIndicator::errorSecondaryColor() const {
    return colorPaletteError.secondary;
}

void ProgressIndicator::setErrorSecondaryColor(const QColor &color) {
    if (colorPaletteError.secondary == color)
        return;
    colorPaletteError.secondary = color;
    applyColorPalette();
    update();
}

QColor ProgressIndicator::errorCurrentTaskColor() const {
    return colorPaletteError.currentTask;
}

void ProgressIndicator::setErrorCurrentTaskColor(const QColor &color) {
    if (colorPaletteError.currentTask == color)
        return;
    colorPaletteError.currentTask = color;
    applyColorPalette();
    update();
}

void ProgressIndicator::resizeEvent(QResizeEvent *event) {
    switch (m_indicatorStyle) {
        case HorizontalBar:
            calculateBarParams();
            break;
        case Ring:
            calculateRingParams();
            break;
    }
}

void ProgressIndicator::calculateBarParams() {
    m_penWidth = rect().height();
    m_padding = m_penWidth / 2;
    m_halfRectHeight = rect().height() / 2;

    // Calculate track inactive(background)
    m_trackStart = QPoint(rect().left() + m_padding, m_halfRectHeight);
    m_trackEnd = QPoint(rect().right() - m_padding, m_halfRectHeight);
    m_actualLength = rect().width() - m_padding - m_padding;
}

void ProgressIndicator::calculateRingParams() {
    const auto minLength = qMin(rect().width(), rect().height());
    m_penWidth = static_cast<int>(minLength * 0.15);
    m_padding = m_penWidth;

    const auto left = (rect().width() - minLength) / 2 + m_padding;
    const auto top = (rect().height() - minLength) / 2 + m_padding;
    const auto width = minLength - 2 * m_padding;
    const auto height = width;
    m_ringRect = QRect(left, top, width, height);
}

double ProgressIndicator::apparentValue() const {
    return m_apparentValue;
}

void ProgressIndicator::setApparentValue(const double x) {
    m_apparentValue = x;
    update();
}

double ProgressIndicator::apparentSecondaryValue() const {
    return m_apparentSecondaryValue;
}

void ProgressIndicator::setApparentSecondaryValue(const double x) {
    m_apparentSecondaryValue = x;
    update();
}

double ProgressIndicator::apparentCurrentTaskValue() const {
    return m_apparentCurrentTaskValue;
}

void ProgressIndicator::setApparentCurrentTaskValue(const double x) {
    m_apparentCurrentTaskValue = x;
    update();
}