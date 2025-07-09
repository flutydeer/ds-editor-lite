//
// Created by fluty on 2023/8/6.
//

#include "LevelMeter.h"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QVariantAnimation>

#include "Utils/VolumeUtils.h"

LevelMeter::LevelMeter(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_Hover);

    m_leftChannel.peakHoldTimer = new QTimer(this);
    m_rightChannel.peakHoldTimer = new QTimer(this);
    m_leftChannel.peakHoldTimer->setSingleShot(true);
    m_rightChannel.peakHoldTimer->setSingleShot(true);

    m_leftChannel.decayAnimation = new QVariantAnimation(this);
    m_rightChannel.decayAnimation = new QVariantAnimation(this);

    connect(m_leftChannel.peakHoldTimer, &QTimer::timeout, [this] {
        startDecayAnimation(m_leftChannel);
    });

    connect(m_rightChannel.peakHoldTimer, &QTimer::timeout, [this] {
        startDecayAnimation(m_rightChannel);
    });

    auto setupAnimation = [&](QVariantAnimation *anim, ChannelData &channel) {
        anim->setDuration(m_decayTime);
        anim->setEasingCurve(QEasingCurve::InOutCubic);
    };

    setupAnimation(m_leftChannel.decayAnimation, m_leftChannel);
    setupAnimation(m_rightChannel.decayAnimation, m_rightChannel);

    connect(m_leftChannel.decayAnimation, &QVariantAnimation::valueChanged,
            [this](const QVariant &value) {
                handleAnimationUpdate(value, m_leftChannel);
            });

    connect(m_rightChannel.decayAnimation, &QVariantAnimation::valueChanged,
            [this](const QVariant &value) {
                handleAnimationUpdate(value, m_rightChannel);
            });

    connect(m_leftChannel.decayAnimation, &QVariantAnimation::finished, [this]() {
        if (m_leftChannel.isDecaying && !m_leftChannel.peakHoldTimer->isActive()) {
            m_leftChannel.displayedPeak = 0.0;
            m_leftChannel.isDecaying = false;
            update();
        }
    });

    connect(m_rightChannel.decayAnimation, &QVariantAnimation::finished, [this]() {
        if (m_rightChannel.isDecaying && !m_rightChannel.peakHoldTimer->isActive()) {
            m_rightChannel.displayedPeak = 0.0;
            m_rightChannel.isDecaying = false;
            update();
        }
    });
}

void LevelMeter::resizeEvent(QResizeEvent *event) {
    paddedRect = QRectF(rect().left() + m_padding, rect().top() + m_padding,
                        rect().width() - 2 * m_padding, rect().height() - 2 * m_padding);
    channelWidth = (paddedRect.width() - m_spacing) / 2;
    channelTop = paddedRect.top() + m_clipIndicatorLength + m_spacing;
    channelLength = paddedRect.bottom() - channelTop;
}

void LevelMeter::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    auto leftChannelLeft = paddedRect.left();
    auto rightChannelLeft = leftChannelLeft + channelWidth + m_spacing;

    // Draw clip indicator
    auto leftIndicatorRect =
        QRectF(leftChannelLeft, paddedRect.top(), channelWidth, m_clipIndicatorLength);
    auto rightIndicatorRect =
        QRectF(rightChannelLeft, paddedRect.top(), channelWidth, m_clipIndicatorLength);

    painter.fillRect(leftIndicatorRect, m_leftChannel.clipped ? m_colorClipped : m_colorDimmed);
    painter.fillRect(rightIndicatorRect, m_leftChannel.clipped ? m_colorClipped : m_colorDimmed);

    // Draw levels
    auto leftLevelBar = QRectF(leftChannelLeft, channelTop, channelWidth, channelLength);
    auto rightLevelBar = QRectF(rightChannelLeft, channelTop, channelWidth, channelLength);
    if (m_style == MeterStyle::Segmented) {
        drawSegmentedBar(painter, leftLevelBar, m_leftChannel.currentLevel);
        drawSegmentedBar(painter, rightLevelBar, m_rightChannel.currentLevel);
    } else if (m_style == MeterStyle::Gradient) {
        drawGradientBar(painter, leftLevelBar, m_leftChannel.currentLevel);
        drawGradientBar(painter, rightLevelBar, m_rightChannel.currentLevel);
    }
    painter.setClipRect(rect());

    // Draw peak hold value
    drawPeakHold(painter, leftLevelBar, m_leftChannel.displayedPeak);
    drawPeakHold(painter, rightLevelBar, m_rightChannel.displayedPeak);

    // Draw cursor value
    if (m_showValueWhenHover && m_mouseOnBar) {
        painter.setClipRect(rect());

        // Draw value background
        auto textHeight = painter.fontMetrics().height();
        auto textRect = QRectF(rect().left(), mouseY - textHeight, rect().width(), textHeight);
        painter.setBrush(QColor(33, 36, 43, 192));
        painter.setPen(Qt::NoPen);
        painter.drawRect(textRect);

        // Draw value text
        QPen currentValuePen;
        currentValuePen.setColor(m_colorCurrentValue);
        currentValuePen.setWidthF(1.2);
        painter.setPen(currentValuePen);
        painter.setBrush(Qt::NoBrush);
        QTextOption textOption(Qt::AlignCenter);
        painter.drawText(textRect, m_currentValueText, textOption);

        // Draw line
        auto line = QLineF(rect().left(), mouseY, rect().right(), mouseY);
        painter.drawLine(line);
    }
}

void LevelMeter::mousePressEvent(QMouseEvent *event) {
    auto cursorPos = event->position();
    if (event->button() == Qt::LeftButton && mouseOnClipIndicator(cursorPos))
        setClipped(false, false);

    QWidget::mousePressEvent(event);
    event->ignore();
}

void LevelMeter::setValue(double valueL, double valueR) {
    m_leftChannel.currentLevel = VolumeUtils::dBToLinear(valueL);
    m_rightChannel.currentLevel = VolumeUtils::dBToLinear(valueR);
    auto clippedValueL = m_leftChannel.currentLevel;
    auto clippedValueR = m_rightChannel.currentLevel;

    if (m_leftChannel.currentLevel > 1) {
        m_leftChannel.clipped = true;
        clippedValueL = 1;
    }
    if (m_rightChannel.currentLevel > 1) {
        m_rightChannel.clipped = true;
        clippedValueR = 1;
    }

    updatePeakValue(m_leftChannel, clippedValueL);
    updatePeakValue(m_rightChannel, clippedValueR);

    update();
}

double LevelMeter::peakValue() const {
    return VolumeUtils::linearTodB(getPeakValueForTextDisplaying());
}

void LevelMeter::setClipped(bool onL, bool onR) {
    m_leftChannel.clipped = onL;
    m_rightChannel.clipped = onR;
}

void LevelMeter::clearClipped() {
    setClipped(false, false);
}

bool LevelMeter::mouseOnClipIndicator(const QPointF &pos) const {
    // return pos.y() <= m_spacing + m_clipIndicatorLength + m_spacing;
    return pos.y() <= m_clipIndicatorLength + 8;
}

bool LevelMeter::event(QEvent *event) {
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove) {
        onHover(static_cast<QHoverEvent *>(event));
    } else if (event->type() == QEvent::HoverLeave) {
        m_mouseOnBar = false;
        update();
    }
    return QWidget::event(event);
}

void LevelMeter::onHover(QHoverEvent *event) {
    auto cursorY = mapFromGlobal(QCursor::pos()).y();
    auto mouseOnBar = [&](double y) {
        return y >= channelTop && y <= channelTop + channelLength;
    };
    auto mouseOnIndicator = [&](double y) {
        return y >= paddedRect.top() && y <= paddedRect.top() + m_clipIndicatorLength;
    };
    mouseY = cursorY;

    if (mouseOnBar(cursorY)) {
        m_mouseOnBar = true;
        handleHoverOnBar();
    } else if (mouseOnIndicator(cursorY)) {
        m_mouseOnBar = false;
        handleHoverOnClipIndicator();
    } else {
        m_mouseOnBar = false;
        // setToolTip({});
    }
    update();
}

void LevelMeter::handleHoverOnBar() {
    auto yToLinear = [&](const double &y) {
        return 1 - (y - channelTop) / channelLength;
    };

    auto cursorY = mapFromGlobal(QCursor::pos()).y();
    auto db = VolumeUtils::linearTodB(yToLinear(cursorY));
    auto toolTip = gainValueToString(db);
    m_currentValueText = gainValueToString(db);

    // setToolTip(toolTip);
    // toolTipFilter->setTitle(toolTip);
}

void LevelMeter::handleHoverOnClipIndicator() {
    auto clipped = m_leftChannel.clipped || m_rightChannel.clipped;
    if (clipped) {
        auto toolTip = tr("Clipped");
        // setToolTip(toolTip);
        // toolTipFilter->setTitle(toolTip);
    }
}

void LevelMeter::drawSegmentedBar(QPainter &painter, const QRectF &rect, const double &level) {
    auto rectLeft = rect.left();
    auto rectTop = rect.top();
    auto rectWidth = rect.width();
    auto rectHeight = rect.height();

    auto safeLength = rectHeight * m_safeThreshold;
    auto warnLength = rectHeight * m_warnThreshold - safeLength;

    auto safeStart = rectTop + rectHeight - safeLength;
    auto warnStart = rectTop + rectHeight - safeLength - warnLength;
    auto criticalStart = rectTop;
    auto levelLength = rectHeight * level;
    auto criticalLength = rectHeight - safeLength - warnLength;

    auto fullSafeChunk = QRectF(rectLeft, safeStart, rectWidth, safeLength);
    auto fullWarnChunk = QRectF(rectLeft, warnStart, rectWidth, warnLength);
    auto fullCriticalChunk = QRectF(rectLeft, criticalStart, rectWidth, criticalLength);

    if (level <= 1) {
        // Draw background
        painter.setClipRect(rect);
        painter.fillRect(rect, m_colorDimmed);

        auto height = rect.height() * level;
        auto top = rect.bottom() - height;
        auto clipRect = QRectF(rect.left(), top, rect.width(), height);
        painter.setClipRect(clipRect);
        painter.fillRect(fullSafeChunk, m_colorSafe);
        painter.fillRect(fullWarnChunk, m_colorWarn);
        painter.fillRect(fullCriticalChunk, m_colorCritical);
    } else {
        painter.fillRect(rect, m_colorClipped);
    }
}

void LevelMeter::drawGradientBar(QPainter &painter, const QRectF &rect, const double &level) {
    // Draw background
    painter.setClipRect(rect);
    painter.fillRect(rect, m_colorDimmed);

    if (level <= 1) {
        QLinearGradient gradient(0, 0, 0, rect.height());
        gradient.setColorAt(0, m_colorCritical);
        gradient.setColorAt(m_safeThresholdAlt, m_colorWarn);
        gradient.setColorAt(1, m_colorSafe);
        auto height = rect.height() * level;
        auto top = rect.bottom() - height;
        auto clipRect = QRectF(rect.left(), top, rect.width(), height);
        painter.setClipRect(clipRect);
        painter.fillRect(rect, gradient);
    } else {
        painter.setClipRect(rect);
        painter.fillRect(rect, m_colorClipped);
    }
}

void LevelMeter::drawPeakHold(QPainter &painter, const QRectF &rect, double level) {
    if (level < 0.01)
        return;

    QPen pen;
    pen.setColor(m_colorPeakHold);
    pen.setWidthF(m_peakHoldWidth);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (level > 1)
        level = 1;
    auto height = rect.height() * level;
    auto top = rect.bottom() - height;
    auto p1 = QPointF(rect.left(), top);
    auto p2 = QPointF(rect.right(), top);
    painter.drawLine({p1, p2});
}

void LevelMeter::startDecayAnimation(ChannelData &channel) {
    // qInfo() << "LevelMeter::startDecayAnimation";
    if (channel.displayedPeak <= 0.0)
        return;

    channel.isDecaying = true;
    channel.decayAnimation->stop();
    channel.decayAnimation->setStartValue(channel.displayedPeak);
    channel.decayAnimation->setEndValue(0.0);
    channel.decayAnimation->start();
}

void LevelMeter::updatePeakValue(ChannelData &channel, double clippedValue) {
    // qDebug() << "LevelMeter::updatePeakValue" << channel.displayedPeak << newValue;
    if (channel.currentLevel > channel.displayedPeak) {
        cancelDecayAnimation(channel);

        channel.peak = channel.currentLevel;
        channel.displayedPeak = channel.peak;
        channel.peakHoldTimer->stop();
        channel.peakHoldTimer->start(m_peakHoldTime);

        notifyDisplayedPeakChange();
    }
}

void LevelMeter::cancelDecayAnimation(ChannelData &channel) {
    if (channel.isDecaying) {
        channel.decayAnimation->stop();
        channel.isDecaying = false;
    }
}

void LevelMeter::handleAnimationUpdate(const QVariant &value, ChannelData &channel) {
    channel.displayedPeak = value.toDouble();
    notifyDisplayedPeakChange();
    update();
}

void LevelMeter::notifyDisplayedPeakChange() {
    auto peakValue = getPeakValueForTextDisplaying();
    if (!qFuzzyCompare(m_lastPeakValue, peakValue)) {
        emit peakValueChanged(VolumeUtils::linearTodB(peakValue));
        m_lastPeakValue = peakValue;
    }
}

double LevelMeter::getPeakValueForTextDisplaying() const {
    auto peakL = qFuzzyCompare(m_leftChannel.displayedPeak, 1.0)
                 && m_leftChannel.peak > m_leftChannel.displayedPeak
                     ? m_leftChannel.peak
                     : m_leftChannel.displayedPeak;
    auto peakR = qFuzzyCompare(m_rightChannel.displayedPeak, 1.0)
                 && m_rightChannel.peak > m_rightChannel.displayedPeak
                     ? m_rightChannel.peak
                     : m_rightChannel.displayedPeak;
    return std::max(peakL, peakR);
}

QString LevelMeter::gainValueToString(double gain) {
    if (gain <= -54.0)
        return "-∞";

    if (qAbs(gain) < 0.05)
        return "0.0";

    const QString absVal = QString::number(qAbs(gain), 'f', 1);
    QString sign;

    if (gain > 0.05)
        sign = "+";
    else if (gain < -0.05)
        sign = "-";

    return sign + absVal;
}

double LevelMeter::padding() const {
    return m_padding;
}

void LevelMeter::setPadding(double padding) {
    m_padding = padding;
    update();
}

double LevelMeter::spacing() const {
    return m_spacing;
}

void LevelMeter::setSpacing(double spacing) {
    m_spacing = spacing;
    update();
}

double LevelMeter::clipIndicatorLength() const {
    return m_clipIndicatorLength;
}

void LevelMeter::setClipIndicatorLength(double length) {
    m_clipIndicatorLength = length;
    update();
}

bool LevelMeter::showValueWhenHover() const {
    return m_showValueWhenHover;
}

void LevelMeter::setShowValueWhenHover(bool on) {
    m_showValueWhenHover = on;
    update();
}

QColor LevelMeter::dimmedColor() const {
    return m_colorDimmed;
}

void LevelMeter::setDimmedColor(const QColor &color) {
    m_colorDimmed = color;
    update();
}

QColor LevelMeter::clippedColor() const {
    return m_colorClipped;
}

void LevelMeter::setClippedColor(const QColor &color) {
    m_colorClipped = color;
    update();
}

QColor LevelMeter::safeColor() const {
    return m_colorSafe;
}

void LevelMeter::setSafeColor(const QColor &color) {
    m_colorSafe = color;
    update();
}

QColor LevelMeter::warnColor() const {
    return m_colorWarn;
}

void LevelMeter::setWarnColor(const QColor &color) {
    m_colorWarn = color;
    update();
}

QColor LevelMeter::criticalColor() const {
    return m_colorCritical;
}

void LevelMeter::setCriticalColor(const QColor &color) {
    m_colorCritical = color;
    update();
}

QColor LevelMeter::currentValueColor() const {
    return m_colorCurrentValue;
}

void LevelMeter::setCurrentValueColor(const QColor &color) {
    m_colorCurrentValue = color;
    update();
}