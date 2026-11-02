#include "AudioChannelAxisView.h"

#include "AudioDbAxis.h"
#include "AudioLaneGeometry.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>

using namespace ClipEditorGlobal;

AudioChannelAxisView::AudioChannelAxisView(QWidget *parent) : QWidget(parent) {
    setObjectName("audioChannelAxisView");
    setFixedWidth(audioChannelAxisWidth);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void AudioChannelAxisView::setChannelCount(const int count) {
    if (m_channelCount == count)
        return;
    m_channelCount = count;
    update();
}

int AudioChannelAxisView::channelCount() const {
    return m_channelCount;
}

void AudioChannelAxisView::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setFont(font());

    const auto bounds = QRectF(rect());
    const auto lanes = AudioLaneGeometry::laneCount(m_channelCount);

    // The minimum spacing follows the font so labels never overlap once the
    // platform picks a taller default face
    const auto minLabelSpacing =
        std::max(audioDbAxisMinLabelSpacing, painter.fontMetrics().height() + 2);

    AudioDbAxis::Colors colors;
    colors.tickColor = m_tickColor;
    colors.labelColor = m_labelColor;
    colors.infiniteLabelColor = m_infiniteLabelColor;
    colors.separatorColor = m_separatorColor;
    colors.channelTagColor = m_channelTagColor;

    for (auto lane = 0; lane < lanes; lane++) {
        const auto laneRect = AudioLaneGeometry::laneRect(bounds, m_channelCount, lane);
        if (laneRect.isEmpty())
            continue;
        painter.fillRect(laneRect, m_laneBackgroundColor);
        // Lane separator matching the one the canvas draws, so the bands read as
        // continuous rows across the axis and the waveform
        if (lane > 0) {
            painter.setPen(QPen(m_separatorColor, 1.0));
            const auto y = qRound(laneRect.top()) + 0.5;
            painter.drawLine(QPointF(laneRect.left(), y), QPointF(laneRect.right(), y));
        }
        const auto ticks =
            AudioDbAxis::buildTicks(AudioLaneGeometry::laneHalfHeight(bounds, m_channelCount),
                                    minLabelSpacing);
        AudioDbAxis::paint(&painter, laneRect, ticks,
                           AudioLaneGeometry::channelTag(m_channelCount, lane), colors);
    }

    // Every lane shares the canvas tone, so the column needs its own edge to read
    // as a column. Drawn after the bands so the lane separators cannot interrupt it.
    painter.setPen(QPen(m_separatorColor, 1.0));
    const auto edge = std::floor(bounds.right()) - 0.5;
    painter.drawLine(QPointF(edge, bounds.top()), QPointF(edge, bounds.bottom()));
}

QColor AudioChannelAxisView::tickColor() const {
    return m_tickColor;
}

void AudioChannelAxisView::setTickColor(const QColor &color) {
    m_tickColor = color;
    update();
}

QColor AudioChannelAxisView::labelColor() const {
    return m_labelColor;
}

void AudioChannelAxisView::setLabelColor(const QColor &color) {
    m_labelColor = color;
    update();
}

QColor AudioChannelAxisView::infiniteLabelColor() const {
    return m_infiniteLabelColor;
}

void AudioChannelAxisView::setInfiniteLabelColor(const QColor &color) {
    m_infiniteLabelColor = color;
    update();
}

QColor AudioChannelAxisView::separatorColor() const {
    return m_separatorColor;
}

void AudioChannelAxisView::setSeparatorColor(const QColor &color) {
    m_separatorColor = color;
    update();
}

QColor AudioChannelAxisView::channelTagColor() const {
    return m_channelTagColor;
}

void AudioChannelAxisView::setChannelTagColor(const QColor &color) {
    m_channelTagColor = color;
    update();
}

QColor AudioChannelAxisView::laneBackgroundColor() const {
    return m_laneBackgroundColor;
}

void AudioChannelAxisView::setLaneBackgroundColor(const QColor &color) {
    m_laneBackgroundColor = color;
    update();
}
