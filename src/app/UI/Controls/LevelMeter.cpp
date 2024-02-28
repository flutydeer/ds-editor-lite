//
// Created by fluty on 2023/8/6.
//

#include "LevelMeter.h"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>

#include "Utils/VolumeUtils.h"

LevelMeter::LevelMeter(QWidget *parent) : QWidget(parent) {
    m_timer = new QTimer;
    m_timer->setInterval(16);
    // m_timer->start();
    connect(m_timer, &QTimer::timeout, this, [=]() {
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
}
LevelMeter::~LevelMeter() {
    delete[] m_bufferL;
    delete[] m_bufferR;
    delete m_timer;
}

void LevelMeter::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    // Fill background
    painter.setPen(Qt::NoPen);
    painter.fillRect(rect(), m_colorBackground);
    auto paddedRect = QRectF(rect().left() + m_spacing, rect().top() + m_spacing,
                             rect().width() - 2 * m_spacing, rect().height() - 2 * m_spacing);
    auto channelWidth = (paddedRect.width() - m_spacing) / 2;
    auto leftChannelLeft = paddedRect.left();
    auto rightChannelLeft = leftChannelLeft + channelWidth + m_spacing;
    auto channelTop = paddedRect.top() + m_clipIndicatorLength + m_spacing;
    auto channelLength = paddedRect.bottom() - channelTop;

    // draw clip indicator
    auto leftIndicatorRect =
        QRectF(leftChannelLeft, paddedRect.top(), channelWidth, m_clipIndicatorLength);
    auto rightIndicatorRect =
        QRectF(rightChannelLeft, paddedRect.top(), channelWidth, m_clipIndicatorLength);

    if (m_clippedL)
        painter.fillRect(leftIndicatorRect, m_colorCritical);
    if (m_clippedR)
        painter.fillRect(rightIndicatorRect, m_colorCritical);

    auto drawVerticalBar = [&](const QRectF &rect, const double level) {
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

        // draw safe chunk
        if (level < m_safeThreshold) {
            auto height = levelLength;
            auto top = rectTop + rectHeight - height;
            auto chunk = QRectF(rectLeft, top, rectWidth, height);
            painter.fillRect(chunk, m_colorSafe);
        } else if (level < m_warnThreshold) {
            painter.fillRect(fullSafeChunk, m_colorSafe);

            auto height = levelLength - safeLength;
            auto top = rectTop + rectHeight - levelLength;
            auto chunk = QRectF(rectLeft, top, rectWidth, height);
            painter.fillRect(chunk, m_colorWarn);
        } else if (level < 1) {
            painter.fillRect(fullSafeChunk, m_colorSafe);
            painter.fillRect(fullWarnChunk, m_colorWarn);

            auto height = levelLength - safeLength - warnLength;
            auto top = rectTop + rectHeight - levelLength;
            auto chunk = QRectF(rectLeft, top, rectWidth, height);
            painter.fillRect(chunk, m_colorCritical);
        } else {
            painter.fillRect(fullSafeChunk, m_colorSafe);
            painter.fillRect(fullWarnChunk, m_colorWarn);
            painter.fillRect(fullCriticalChunk, m_colorCritical);
        }
    };

    // draw levels
    auto leftLevelBar = QRectF(leftChannelLeft, channelTop, channelWidth, channelLength);
    auto rightLevelBar = QRectF(rightChannelLeft, channelTop, channelWidth, channelLength);
    drawVerticalBar(leftLevelBar, m_smoothedLevelL);
    drawVerticalBar(rightLevelBar, m_smoothedLevelR);
}
void LevelMeter::mousePressEvent(QMouseEvent *event) {
    auto cursorPos = event->position();
    if (event->button() == Qt::LeftButton && mouseOnClipIndicator(cursorPos))
        setClipped(false, false);
    // QWidget::mousePressEvent(event);
}

void LevelMeter::initBuffer(int bufferSize) {
    m_bufferSize = bufferSize;
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
        m_timer->stop();
        m_freezed = true;
    } else {
        m_timer->start();
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