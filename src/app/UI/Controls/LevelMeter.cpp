//
// Created by fluty on 2023/8/6.
//

#include "LevelMeter.h"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>

#include "Utils/VolumeUtils.h"

LevelMeter::LevelMeter(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_Hover);

    smoothValueTimer.setInterval(16);
    // m_timer->start();
    connect(&smoothValueTimer, &QTimer::timeout, this, [=]() {
        double sumL = 0;
        double sumR = 0;
        for (int i = 0; i < m_bufferSize; i++) {
            sumL += m_bufferL[i];
            sumR += m_bufferR[i];
        }
        m_smoothedLevelL = sumL / m_bufferSize;
        m_smoothedLevelR = sumR / m_bufferSize;
        // qDebug() << averageLevel;
        //        repaint();
        update();
    });
    initBuffer(32);

    // toolTipFilter = new ToolTipFilter(this);
    // toolTipFilter->setFollowCursor(true);
    // toolTipFilter->setShowDelay(0);
    // installEventFilter(toolTipFilter);
}

LevelMeter::~LevelMeter() {
    delete[] m_bufferL;
    delete[] m_bufferR;
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

    // draw clip indicator
    auto leftIndicatorRect =
        QRectF(leftChannelLeft, paddedRect.top(), channelWidth, m_clipIndicatorLength);
    auto rightIndicatorRect =
        QRectF(rightChannelLeft, paddedRect.top(), channelWidth, m_clipIndicatorLength);

    painter.fillRect(leftIndicatorRect, m_clippedL ? m_colorClipped : m_colorDimmed);
    painter.fillRect(rightIndicatorRect, m_clippedR ? m_colorClipped : m_colorDimmed);

    // draw levels
    auto leftLevelBar = QRectF(leftChannelLeft, channelTop, channelWidth, channelLength);
    auto rightLevelBar = QRectF(rightChannelLeft, channelTop, channelWidth, channelLength);
    if (m_style == MeterStyle::Segmented) {
        drawSegmentedBar(painter, leftLevelBar, m_smoothedLevelL);
        drawSegmentedBar(painter, rightLevelBar, m_smoothedLevelR);
    } else if (m_style == MeterStyle::Gradient) {
        drawGradientBar(painter, leftLevelBar, m_smoothedLevelL);
        drawGradientBar(painter, rightLevelBar, m_smoothedLevelR);
    }

    // draw cursor value
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

void LevelMeter::initBuffer(int bufferSize) {
    m_bufferSize = bufferSize;
    delete[] m_bufferL;
    delete[] m_bufferR;
    m_bufferL = new double[m_bufferSize];
    m_bufferR = new double[m_bufferSize];
    resetBuffer();
}

void LevelMeter::readSample(double sampleL, double sampleR) {
    m_bufferL[m_bufferPos] = sampleL;
    m_bufferR[m_bufferPos] = sampleR;
    m_bufferPos++;
    if (m_bufferPos == m_bufferSize) {
        m_bufferPos = 0;
    }
    if (sampleL > 1)
        m_clippedL = true;
    if (sampleR > 1)
        m_clippedR = true;
}

void LevelMeter::setValue(double valueL, double valueR) {
    m_smoothedLevelL = VolumeUtils::dBToLinear(valueL);
    m_smoothedLevelR = VolumeUtils::dBToLinear(valueR);
    if (m_smoothedLevelL > 1)
        m_clippedL = true;
    if (m_smoothedLevelR > 1)
        m_clippedR = true;
    update();
}

void LevelMeter::setClipped(bool onL, bool onR) {
    m_clippedL = onL;
    m_clippedR = onR;
}

int LevelMeter::bufferSize() const {
    return m_bufferSize;
}

void LevelMeter::setBufferSize(int size) {
    setFreeze(true);
    delete[] m_bufferL;
    delete[] m_bufferR;
    initBuffer(size);
    setFreeze(false);
}

bool LevelMeter::freeze() const {
    return m_freezed;
}

void LevelMeter::setFreeze(bool on) {
    if (on) {
        smoothValueTimer.stop();
        m_freezed = true;
    } else {
        smoothValueTimer.start();
        m_freezed = false;
    }
}

void LevelMeter::resetBuffer() {
    for (int i = 0; i < m_bufferSize; i++)
        m_bufferL[i] = 0;
    m_bufferPos = 0;
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
    auto mouseOnBar = [&](double y) { return y >= channelTop && y <= channelTop + channelLength; };
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
    auto yToLinear = [&](const double &y) { return 1 - (y - channelTop) / channelLength; };

    auto cursorY = mapFromGlobal(QCursor::pos()).y();
    auto db = VolumeUtils::linearTodB(yToLinear(cursorY));
    auto toolTip = gainValueToString(db);
    m_currentValueText = gainValueToString(db);

    // setToolTip(toolTip);
    // toolTipFilter->setTitle(toolTip);
}

void LevelMeter::handleHoverOnClipIndicator() {
    auto clipped = m_clippedL || m_clippedR;
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

QString LevelMeter::gainValueToString(double gain) {
    if (gain == -70)
        return "-inf";
    auto absVal = QString::number(qAbs(gain), 'f', 1);
    QString sig = "";
    if (gain > 0) {
        sig = "+";
    } else if (gain < 0 && gain <= -0.1) {
        sig = "-";
    }
    return sig + absVal /* + "dB" */;
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
