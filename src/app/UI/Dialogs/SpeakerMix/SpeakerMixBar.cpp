#include "SpeakerMixBar.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

SpeakerMixBar::SpeakerMixBar(QWidget *parent)
    : QWidget(parent), m_draggingIndex(-1), m_isDragging(false), m_readOnly(false) {
    setFixedHeight(40);
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

    const int trackTop = 0;
    const QRect trackRect(m_margin, trackTop, width() - 2 * m_margin, m_trackHeight);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#2A2E38"));
    painter.drawRoundedRect(trackRect, 4, 4);

    int currentPosition = 0;
    for (int i = 0; i < m_values.size(); ++i) {
        const int segmentEnd = currentPosition + m_values[i];
        const int segmentStartX = valueToPixel(currentPosition);
        const int segmentEndX = valueToPixel(segmentEnd);

        QRect segmentRect(segmentStartX, trackTop, segmentEndX - segmentStartX, m_trackHeight);
        QColor segmentColor = m_segmentColors[i % m_segmentColors.size()];

        painter.setBrush(segmentColor);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(segmentRect.adjusted(0, 0, -1, 0), 3, 3);

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
            painter.drawText(valueRect, Qt::AlignCenter, QString::number(m_values[i]) + "%");
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
    const int trackTop = 0;
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
    if (m_readOnly)
        return;

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
    if (m_readOnly) {
        unsetCursor();
        return;
    }

    if (m_isDragging && m_draggingIndex >= 1 && m_draggingIndex < m_dividers.size() - 1) {
        const int newValue = pixelToValue(event->pos().x());

        constexpr int minSpacing = 0;
        const int minAllowed = m_dividers[m_draggingIndex - 1] + minSpacing;
        const int maxAllowed = m_dividers[m_draggingIndex + 1] - minSpacing;

        if (newValue >= minAllowed && newValue <= maxAllowed) {
            m_dividers[m_draggingIndex] = newValue;
            m_values[m_draggingIndex - 1] =
                m_dividers[m_draggingIndex] - m_dividers[m_draggingIndex - 1];
            m_values[m_draggingIndex] =
                m_dividers[m_draggingIndex + 1] - m_dividers[m_draggingIndex];

            update();
            emit valuesChanged(m_values);
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
    for (int &value : m_values) {
        value = std::clamp(value, 0, 100);
    }

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
    const int trackWidth = width() - 2 * m_margin;
    if (trackWidth <= 0)
        return 0;

    return std::clamp((pixel - m_margin) * 100 / trackWidth, 0, 100);
}
