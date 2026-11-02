#include "AudioWaveformLaneView.h"

#include "AudioLaneGeometry.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/WaveformRenderUtils.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QPainter>

using namespace ClipEditorGlobal;

AudioWaveformLaneView::AudioWaveformLaneView(const int channel, const int channelCount,
                                             QGraphicsItem *parent)
    : AbstractGraphicsRectItem(parent), m_channel(channel), m_channelCount(channelCount) {
    setAcceptedMouseButtons(Qt::NoButton);
}

void AudioWaveformLaneView::setChannel(const int channel, const int channelCount) {
    if (m_channel == channel && m_channelCount == channelCount)
        return;
    m_channel = channel;
    m_channelCount = channelCount;
    updateRectAndPos();
}

int AudioWaveformLaneView::channel() const {
    return m_channel;
}

void AudioWaveformLaneView::setAudioInfo(const AudioInfoModel &info) {
    m_audioInfo = info;
    m_waveformSampler.invalidate();
    update();
}

void AudioWaveformLaneView::setTimeline(const Timeline &timelineState) {
    m_timeline = timelineState;
    m_waveformSampler.invalidate();
    update();
}

void AudioWaveformLaneView::setPath(const QString &path) {
    m_path = path;
    m_waveformSampler.setPath(path);
    update();
}

void AudioWaveformLaneView::setLoadState(const AppGlobal::AudioClipLoadState state) {
    if (m_loadState == state)
        return;
    m_loadState = state;
    update();
}

void AudioWaveformLaneView::setMaterialTicks(const int materialStartTick,
                                             const int visibleStartTick) {
    m_materialStartTick = materialStartTick;
    m_visibleStartTick = visibleStartTick;
    m_waveformSampler.invalidate();
    update();
}

void AudioWaveformLaneView::setTrackColorIndex(const int index) {
    if (m_trackColorIndex == index)
        return;
    m_trackColorIndex = index;
    update();
}

void AudioWaveformLaneView::setCenterLineColor(const QColor &color) {
    if (m_centerLineColor == color)
        return;
    m_centerLineColor = color;
    update();
}

void AudioWaveformLaneView::invalidateWaveform() {
    m_waveformSampler.invalidate();
    update();
}

void AudioWaveformLaneView::updateRectAndPos() {
    const auto visible = visibleRect();
    if (visible.isEmpty()) {
        setPos(0, 0);
        setRect(QRectF());
        return;
    }
    const auto lane = AudioLaneGeometry::laneRect(visible, m_channelCount, m_channel);
    setPos(lane.topLeft());
    setRect(QRectF(0, 0, lane.width(), lane.height()));
    update();
}

void AudioWaveformLaneView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)
    const auto previewRect = rect();
    if (previewRect.isEmpty())
        return;

    const auto color = AppColorPalette::instance()->baseColor(m_trackColorIndex);

    // Silence line: the waveform is linear, so this is the zero amplitude row
    painter->setPen(QPen(m_centerLineColor, 1.0));
    const auto centerY = qRound(previewRect.center().y()) + 0.5;
    painter->drawLine(QPointF(previewRect.left(), centerY), QPointF(previewRect.right(), centerY));

    // A clip that is still loading or whose file went missing has no waveform to draw. The hint
    // itself is painted once for the whole canvas by the view, not once per lane
    if (m_loadState != AppGlobal::AudioClipLoadState::Loaded)
        return;

    const auto waveform = m_waveformSampler.sample({
        .audioInfo = &m_audioInfo,
        .timeline = &m_timeline,
        .materialStartTick = m_materialStartTick,
        .visibleStartTick = m_visibleStartTick,
        .channel = m_channel,
        .previewSceneRect = mapRectToScene(previewRect),
        .visibleSceneRect = visibleRect(),
        .horizontalScale = scaleX(),
        .pixelsPerQuarterNote = static_cast<double>(pixelsPerQuarterNote),
        .leftMarginPx = 0.0,
        .sceneTickOffset = m_visibleStartTick,
        .devicePixelRatio = painter->deviceTransform().m11(),
    });

    bool invertible = false;
    const auto sceneToItem = sceneTransform().inverted(&invertible);
    if (!invertible)
        return;
    WaveformRenderUtils::renderWaveform(painter, color, WaveformRenderUtils::FilledMode, waveform,
                                        sceneToItem);
}
