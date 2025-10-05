#include "SpeakerMixBar.h"
#include <QPainter>
#include <QMouseEvent>

SpeakerMixBar::SpeakerMixBar(QWidget *parent)
    : QWidget(parent), m_draggingIndex(-1), m_isDragging(false) {
    setMinimumHeight(100);
    setMinimumWidth(300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_values = {100};
    m_labels = {"default"};
    m_segmentColors = getDefaultColors();
    updateDividers();

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void SpeakerMixBar::setValues(const QVector<int> &values) {
    m_values = values;
    adjustValuesToSum100();
    updateDividers();
    update();
}

void SpeakerMixBar::setLabels(const QVector<QString> &labels) {
    m_labels = labels;
    update();
}

QVector<int> SpeakerMixBar::getValues() const {
    return m_values;
}

void SpeakerMixBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().window());

    constexpr int labelAreaTop = 5;
    const int trackTop = labelAreaTop + m_labelHeight + 5;
    const QRect trackRect(m_margin, trackTop, width() - 2 * m_margin, m_trackHeight);

    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().shadow());
    painter.drawRoundedRect(trackRect, 6, 6);

    int currentPosition = 0;
    for (int i = 0; i < m_values.size(); ++i) {
        const int segmentEnd = currentPosition + m_values[i];
        const int segmentStartX = valueToPixel(currentPosition);
        const int segmentEndX = valueToPixel(segmentEnd);

        QRect segmentRect(segmentStartX, trackTop, segmentEndX - segmentStartX, m_trackHeight);
        QColor segmentColor = m_segmentColors[i % m_segmentColors.size()];

        painter.setBrush(segmentColor);
        painter.setPen(QPen(segmentColor.darker(120), 1.5));
        painter.drawRoundedRect(segmentRect, 4, 4);

        if (segmentRect.width() > 40 && i < m_labels.size()) {
            QRect nameRect = segmentRect;
            nameRect.setBottom(nameRect.top() + nameRect.height() / 2 - 2);

            painter.setPen(Qt::white);
            QFont nameFont = painter.font();
            nameFont.setPointSize(8);
            nameFont.setBold(true);
            painter.setFont(nameFont);
            painter.drawText(nameRect, Qt::AlignCenter | Qt::TextWordWrap, m_labels[i]);
        }

        if (segmentRect.width() > 25) {
            QRect valueRect = segmentRect;
            valueRect.setTop(valueRect.top() + valueRect.height() / 2 + 2);

            painter.setPen(Qt::white);
            QFont valueFont = painter.font();
            valueFont.setPointSize(9);
            valueFont.setBold(true);
            painter.setFont(valueFont);
            painter.drawText(valueRect, Qt::AlignCenter, QString::number(m_values[i]) + "%");
        }

        currentPosition = segmentEnd;
    }

    for (int i = 1; i < m_dividers.size() - 1; ++i) {
        const int lineX = valueToPixel(m_dividers[i]);
        const QColor lineColor = i == m_draggingIndex ? Qt::yellow : Qt::white;

        painter.setPen(QPen(lineColor, 2));
        painter.drawLine(lineX, trackTop, lineX, trackTop + m_trackHeight);
    }
}

QRect SpeakerMixBar::getHandleRect(const int dividerIndex) const {
    const int trackTop = 5 + m_labelHeight + 5;
    const int handleX = valueToPixel(m_dividers[dividerIndex]);
    return QRect(handleX - m_handleWidth / 2, trackTop, m_handleWidth, m_trackHeight);
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
    if (event->button() == Qt::LeftButton) {
        m_draggingIndex = findHandleAtPosition(event->pos());
        if (m_draggingIndex != -1) {
            m_isDragging = true;
            setCursor(Qt::SizeHorCursor);
            update();
        }
    }
}

void SpeakerMixBar::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDragging && m_draggingIndex >= 1 && m_draggingIndex < m_dividers.size() - 1) {
        const int newValue = pixelToValue(event->pos().x());

        constexpr int minSpacing = 2;
        const int minAllowed = m_dividers[m_draggingIndex - 1] + minSpacing;
        const int maxAllowed = m_dividers[m_draggingIndex + 1] - minSpacing;

        if (newValue >= minAllowed && newValue <= maxAllowed) {
            m_dividers[m_draggingIndex] = newValue;
            m_values[m_draggingIndex - 1] =
                m_dividers[m_draggingIndex] - m_dividers[m_draggingIndex - 1];
            m_values[m_draggingIndex] =
                m_dividers[m_draggingIndex + 1] - m_dividers[m_draggingIndex];

            update();
            emit valuesChanged();
        }
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

    int accumulated = 0;
    for (const int value : m_values) {
        accumulated += value;
        m_dividers.append(accumulated);
    }
}

void SpeakerMixBar::adjustValuesToSum100() {
    int total = 0;
    for (const int value : m_values) {
        total += value;
    }

    if (total != 100 && !m_values.isEmpty()) {
        m_values.last() += 100 - total;
        if (m_values.last() < 0) {
            m_values.last() = 0;
        }
    }
}

int SpeakerMixBar::valueToPixel(const int value) const {
    return m_margin + (width() - 2 * m_margin) * value / 100;
}

int SpeakerMixBar::pixelToValue(const int pixel) const {
    return (pixel - m_margin) * 100 / (width() - 2 * m_margin);
}