//
// Created by fluty on 2023/8/6.
//

#include "LevelMeter.h"
#include "LevelMeterViewModel.h"

#include <QMouseEvent>
#include <QPainter>

#include "Utils/VolumeUtils.h"

LevelMeter::LevelMeter(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_Hover);
}

void LevelMeter::bindTo(LevelMeterViewModel *viewModel) {
    if (m_viewModel == viewModel)
        return;

    if (m_viewModel)
        disconnect(m_viewModel, nullptr, this, nullptr);

    m_viewModel = viewModel;

    if (m_viewModel) {
        connect(m_viewModel, &LevelMeterViewModel::levelChanged, this,
                QOverload<>::of(&LevelMeter::update));
        connect(m_viewModel, &LevelMeterViewModel::peakChanged, this,
                QOverload<>::of(&LevelMeter::update));
        connect(m_viewModel, &LevelMeterViewModel::clipStateChanged, this,
                QOverload<>::of(&LevelMeter::update));
        connect(m_viewModel, &LevelMeterViewModel::peakValueChanged, this,
                &LevelMeter::peakValueChanged);
    }

    update();
}

LevelMeterViewModel *LevelMeter::viewModel() const {
    return m_viewModel;
}

double LevelMeter::peakValue() const {
    if (m_viewModel)
        return m_viewModel->peakValue();
    return -70.0;
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

    const auto leftChannelLeft = paddedRect.left();
    const auto rightChannelLeft = leftChannelLeft + channelWidth + m_spacing;

    const auto leftIndicatorRect =
        QRectF(leftChannelLeft, paddedRect.top(), channelWidth, m_clipIndicatorLength);
    const auto rightIndicatorRect =
        QRectF(rightChannelLeft, paddedRect.top(), channelWidth, m_clipIndicatorLength);

    bool clippedL = false;
    bool clippedR = false;
    double levelL = 0.0;
    double levelR = 0.0;
    double peakL = 0.0;
    double peakR = 0.0;

    if (m_viewModel) {
        clippedL = m_viewModel->clippedL();
        clippedR = m_viewModel->clippedR();
        levelL = m_viewModel->levelL();
        levelR = m_viewModel->levelR();
        peakL = m_viewModel->displayedPeakL();
        peakR = m_viewModel->displayedPeakR();
    }

    painter.fillRect(leftIndicatorRect, clippedL ? m_colorClipped : m_colorDimmed);
    painter.fillRect(rightIndicatorRect, clippedR ? m_colorClipped : m_colorDimmed);

    const auto leftLevelBar = QRectF(leftChannelLeft, channelTop, channelWidth, channelLength);
    const auto rightLevelBar = QRectF(rightChannelLeft, channelTop, channelWidth, channelLength);
    if (m_style == MeterStyle::Segmented) {
        drawSegmentedBar(painter, leftLevelBar, levelL);
        drawSegmentedBar(painter, rightLevelBar, levelR);
    } else if (m_style == MeterStyle::Gradient) {
        drawGradientBar(painter, leftLevelBar, levelL);
        drawGradientBar(painter, rightLevelBar, levelR);
    }
    painter.setClipRect(rect());

    drawPeakHold(painter, leftLevelBar, peakL);
    drawPeakHold(painter, rightLevelBar, peakR);

    if (m_showValueWhenHover && m_mouseOnBar) {
        painter.setClipRect(rect());

        const auto textHeight = painter.fontMetrics().height();
        const auto textRect =
            QRectF(rect().left(), mouseY - textHeight, rect().width(), textHeight);
        painter.setBrush(m_colorValueBack);
        painter.setPen(Qt::NoPen);
        painter.drawRect(textRect);

        QPen currentValuePen;
        currentValuePen.setColor(m_colorCurrentValue);
        currentValuePen.setWidthF(1.2);
        painter.setPen(currentValuePen);
        painter.setBrush(Qt::NoBrush);
        const QTextOption textOption(Qt::AlignCenter);
        painter.drawText(textRect, m_currentValueText, textOption);

        const auto line = QLineF(rect().left(), mouseY, rect().right(), mouseY);
        painter.drawLine(line);
    }
}

void LevelMeter::mousePressEvent(QMouseEvent *event) {
    const auto cursorPos = event->position();
    if (event->button() == Qt::LeftButton && mouseOnClipIndicator(cursorPos))
        emit clipResetRequested();

    QWidget::mousePressEvent(event);
    event->ignore();
}

bool LevelMeter::mouseOnClipIndicator(const QPointF &pos) const {
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

void LevelMeter::onHover(const QHoverEvent *event) {
    Q_UNUSED(event);
    const auto cursorY = mapFromGlobal(QCursor::pos()).y();
    auto mouseOnBar = [&](const double y) {
        return y >= channelTop && y <= channelTop + channelLength;
    };
    auto mouseOnIndicator = [&](const double y) {
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
    }
    update();
}

void LevelMeter::handleHoverOnBar() {
    auto yToLinear = [&](const double &y) { return 1 - (y - channelTop) / channelLength; };

    const auto cursorY = mapFromGlobal(QCursor::pos()).y();
    const auto db = VolumeUtils::linearTodB(yToLinear(cursorY));
    m_currentValueText = gainValueToString(db);
}

void LevelMeter::handleHoverOnClipIndicator() const {
}

void LevelMeter::drawSegmentedBar(QPainter &painter, const QRectF &rect,
                                  double level) const {
    const auto rectLeft = rect.left();
    const auto rectTop = rect.top();
    const auto rectWidth = rect.width();
    const auto rectHeight = rect.height();

    const auto safeLength = rectHeight * m_safeThreshold;
    const auto warnLength = rectHeight * m_warnThreshold - safeLength;

    const auto safeStart = rectTop + rectHeight - safeLength;
    const auto warnStart = rectTop + rectHeight - safeLength - warnLength;
    const auto criticalStart = rectTop;
    const auto criticalLength = rectHeight - safeLength - warnLength;

    const auto fullSafeChunk = QRectF(rectLeft, safeStart, rectWidth, safeLength);
    const auto fullWarnChunk = QRectF(rectLeft, warnStart, rectWidth, warnLength);
    const auto fullCriticalChunk = QRectF(rectLeft, criticalStart, rectWidth, criticalLength);

    if (level <= 1) {
        painter.setClipRect(rect);
        painter.fillRect(rect, m_colorDimmed);

        const auto height = rect.height() * level;
        const auto top = rect.bottom() - height;
        const auto clipRect = QRectF(rect.left(), top, rect.width(), height);
        painter.setClipRect(clipRect);
        painter.fillRect(fullSafeChunk, m_colorSafe);
        painter.fillRect(fullWarnChunk, m_colorWarn);
        painter.fillRect(fullCriticalChunk, m_colorCritical);
    } else {
        painter.fillRect(rect, m_colorClipped);
    }
}

void LevelMeter::drawGradientBar(QPainter &painter, const QRectF &rect, double level) const {
    painter.setClipRect(rect);
    painter.fillRect(rect, m_colorDimmed);

    if (level <= 1) {
        QLinearGradient gradient(0, 0, 0, rect.height());
        gradient.setColorAt(0, m_colorCritical);
        gradient.setColorAt(m_safeThresholdAlt, m_colorWarn);
        gradient.setColorAt(1, m_colorSafe);
        const auto height = rect.height() * level;
        const auto top = rect.bottom() - height;
        const auto clipRect = QRectF(rect.left(), top, rect.width(), height);
        painter.setClipRect(clipRect);
        painter.fillRect(rect, gradient);
    } else {
        painter.setClipRect(rect);
        painter.fillRect(rect, m_colorClipped);
    }
}

void LevelMeter::drawPeakHold(QPainter &painter, const QRectF &rect, double level) const {
    if (level < 0.01)
        return;

    QPen pen;
    pen.setColor(m_colorPeakHold);
    pen.setWidthF(m_peakHoldWidth);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (level > 1)
        level = 1;
    const auto height = rect.height() * level;
    const auto top = rect.bottom() - height;
    auto p1 = QPointF(rect.left(), top);
    auto p2 = QPointF(rect.right(), top);
    painter.drawLine({p1, p2});
}

QString LevelMeter::gainValueToString(const double gain) {
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

void LevelMeter::setPadding(const double padding) {
    m_padding = padding;
    update();
}

double LevelMeter::spacing() const {
    return m_spacing;
}

void LevelMeter::setSpacing(const double padding) {
    m_spacing = padding;
    update();
}

double LevelMeter::clipIndicatorLength() const {
    return m_clipIndicatorLength;
}

void LevelMeter::setClipIndicatorLength(const double padding) {
    m_clipIndicatorLength = padding;
    update();
}

bool LevelMeter::showValueWhenHover() const {
    return m_showValueWhenHover;
}

void LevelMeter::setShowValueWhenHover(const bool on) {
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

QColor LevelMeter::peakHoldColor() const {
    return m_colorPeakHold;
}

void LevelMeter::setPeakHoldColor(const QColor &color) {
    m_colorPeakHold = color;
    update();
}

QColor LevelMeter::valueBackColor() const {
    return m_colorValueBack;
}

void LevelMeter::setValueBackColor(const QColor &color) {
    m_colorValueBack = color;
    update();
}
