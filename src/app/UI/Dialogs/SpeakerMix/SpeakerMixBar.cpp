#include "SpeakerMixBar.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/SpeakerMixUtils.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <cmath>

SpeakerMixBar::SpeakerMixBar(QWidget *parent)
    : QWidget(parent), m_draggingIndex(-1), m_dragOffset(0), m_isDragging(false),
      m_readOnly(false) {
    setFixedHeight(m_trackHeight + 2 * m_margin);
    setMinimumWidth(300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_values = {100};
    m_labels = {"default"};
    m_segmentColors = {AppColorPalette::instance()->baseColor(0)};
    updateDividers();

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void SpeakerMixBar::setValues(const QVector<int> &values) {
    QVector<double> doubleValues;
    doubleValues.reserve(values.size());
    for (const int value : values)
        doubleValues.append(value);
    setInternalValues(doubleValues);
}

void SpeakerMixBar::setDoubleValues(const QVector<double> &values) {
    setInternalValues(values);
}

void SpeakerMixBar::setLabels(const QVector<QString> &labels) {
    m_labels = labels;
    update();
}

void SpeakerMixBar::setSegmentColors(const QVector<QColor> &colors) {
    m_segmentColors = colors.isEmpty() ? QVector<QColor>{AppColorPalette::instance()->baseColor(0)}
                                       : colors;
    update();
}

QVector<int> SpeakerMixBar::getValues() const {
    return roundedValues();
}

QVector<double> SpeakerMixBar::getDoubleValues() const {
    return m_values;
}

void SpeakerMixBar::setReadOnly(const bool readOnly) {
    if (m_readOnly == readOnly)
        return;

    m_readOnly = readOnly;
    if (m_readOnly) {
        m_isDragging = false;
        m_draggingIndex = -1;
        unsetCursor();
    }
    update();
}

bool SpeakerMixBar::isReadOnly() const {
    return m_readOnly;
}

void SpeakerMixBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int trackTop = m_margin;
    const QRect trackRect(m_margin, trackTop, width() - 2 * m_margin, m_trackHeight);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#2A2E38"));
    painter.drawRoundedRect(trackRect, 4, 4);

    const QVector<int> displayValues = roundedValues();
    double currentPosition = 0;
    const int segmentCount = m_values.size();
    for (int i = 0; i < segmentCount; ++i) {
        const double segmentEnd = currentPosition + m_values[i];
        const int segmentStartX = valueToPixel(currentPosition);
        const int segmentEndX = valueToPixel(segmentEnd);

        QRect segmentRect(segmentStartX, trackTop, segmentEndX - segmentStartX, m_trackHeight);
        QColor segmentColor = m_segmentColors[i % m_segmentColors.size()];

        painter.setBrush(segmentColor);
        painter.setPen(Qt::NoPen);
        if (segmentCount == 1) {
            painter.drawRoundedRect(segmentRect, 3, 3);
        } else if (i == 0) {
            painter.drawRoundedRect(segmentRect.adjusted(0, 0, -1, 0), 3, 3);
        } else if (i == segmentCount - 1) {
            painter.drawRoundedRect(segmentRect.adjusted(1, 0, 0, 0), 3, 3);
        } else {
            painter.drawRoundedRect(segmentRect.adjusted(1, 0, -1, 0), 3, 3);
        }

        if (segmentRect.width() > 0 && segmentRect.width() < 24) {
            currentPosition = segmentEnd;
            continue;
        }

        if (segmentRect.width() > 40 && i < m_labels.size()) {
            QRect nameRect = segmentRect;
            nameRect.setTop(nameRect.top() + 4);
            nameRect.setBottom(nameRect.top() + 15);

            painter.setPen(QColor("#111318"));
            QFont nameFont = painter.font();
            nameFont.setPointSizeF(9.0);
            painter.setFont(nameFont);
            painter.drawText(nameRect, Qt::AlignCenter, m_labels[i]);
        }

        if (segmentRect.width() > 25) {
            QRect valueRect = segmentRect;
            valueRect.setTop(valueRect.top() + 20);

            painter.setPen(QColor("#111318"));
            QFont valueFont = painter.font();
            valueFont.setPointSizeF(8.5);
            painter.setFont(valueFont);
            painter.drawText(valueRect, Qt::AlignCenter,
                             QString::number(displayValues.value(i)) + "%");
        }

        currentPosition = segmentEnd;
    }

    for (int i = 1; i < m_dividers.size() - 1; ++i) {
        const int lineX = valueToPixel(m_dividers[i]);
        const QColor lineColor = i == m_draggingIndex ? QColor("#FFFFFF") : QColor("#21242B");

        painter.setPen(QPen(lineColor, 4));
        painter.drawLine(lineX, trackTop, lineX, trackTop + m_trackHeight);
    }
}

QRect SpeakerMixBar::getHandleRect(const int dividerIndex) const {
    const int handleX = valueToPixel(m_dividers[dividerIndex]);
    return QRect(handleX - m_handleWidth / 2, m_margin, m_handleWidth, m_trackHeight);
}

int SpeakerMixBar::findHandleAtPosition(const QPoint &position) const {
    for (int i = 1; i < m_dividers.size() - 1; ++i) {
        QRect handleArea = getHandleRect(i);
        handleArea.adjust(-m_handleTouchMargin, 0, m_handleTouchMargin, 0);

        if (handleArea.contains(position)) {
            return i;
        }
    }
    return -1;
}

void SpeakerMixBar::mousePressEvent(QMouseEvent *event) {
    if (m_readOnly)
        return;

    if (event->button() == Qt::LeftButton) {
        m_draggingIndex = findHandleAtPosition(event->pos());
        if (m_draggingIndex != -1) {
            m_isDragging = true;
            // Record the offset between click position and handle center
            // so the handle doesn't jump to cursor on drag start.
            const int handleCenterX = valueToPixel(m_dividers[m_draggingIndex]);
            m_dragOffset = event->pos().x() - handleCenterX;
            m_dragStartValues = m_values;
            setCursor(Qt::SizeHorCursor);
            update();
        }
    }
}

void SpeakerMixBar::mouseMoveEvent(QMouseEvent *event) {
    if (m_readOnly) {
        unsetCursor();
        return;
    }

    if (m_isDragging && m_draggingIndex >= 1 && m_draggingIndex < m_dividers.size() - 1) {
        // Compensate for the initial click offset so the handle stays
        // where the user grabbed it rather than jumping to cursor center.
        const int adjustedX = event->pos().x() - m_dragOffset;
        const double newValue = pixelToSnappedValue(adjustedX);

        const bool proportional = event->modifiers().testFlag(Qt::AltModifier);
        const int splitIndex = m_draggingIndex - 1;
        m_values = proportional ? SpeakerMixUtils::proportionalDragWeights(
                                      m_dragStartValues, splitIndex, newValue, 100.0)
                                : SpeakerMixUtils::adjacentDragWeights(m_dragStartValues,
                                                                       splitIndex, newValue, 100.0);
        updateDividers();

        update();
        emit valuesChanged(m_values);
    } else {
        const bool handleHovered = findHandleAtPosition(event->pos()) != -1;
        setCursor(handleHovered ? Qt::SizeHorCursor : Qt::ArrowCursor);
    }
}

void SpeakerMixBar::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    if (m_isDragging) {
        m_isDragging = false;
        m_draggingIndex = -1;
        setCursor(Qt::ArrowCursor);
        update();
    }
}

void SpeakerMixBar::updateDividers() {
    m_dividers.clear();
    m_dividers.append(0);

    double accumulated = 0;
    for (const double value : m_values) {
        accumulated += value;
        m_dividers.append(accumulated);
    }
}

void SpeakerMixBar::setInternalValues(const QVector<double> &values) {
    m_values = SpeakerMixUtils::normalizeFullWeights(values, 100.0);
    updateDividers();
    update();
}

QVector<int> SpeakerMixBar::roundedValues() const {
    return SpeakerMixUtils::fullWeightsToPercentages(m_values, 100.0);
}

int SpeakerMixBar::valueToPixel(const double value) const {
    return m_margin + static_cast<int>(std::lround((width() - 2 * m_margin) * value / 100.0));
}

double SpeakerMixBar::pixelToSnappedValue(const int pixel) const {
    const int trackWidth = width() - 2 * m_margin;
    if (trackWidth <= 0)
        return 0;

    const double value = (pixel - m_margin) * 100.0 / trackWidth;
    return SpeakerMixUtils::snapCumulativeToPercent(value, 100.0);
}
