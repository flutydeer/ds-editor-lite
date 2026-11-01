#include "AudioViewGraphicsView.h"

#include "AudioViewBackground.h"
#include "AudioViewGraphicsScene.h"
#include "AudioWaveformLaneView.h"
#include "Global/AppGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/ClipEditor/PianoRoll/ClipRangeOverlay.h"

#include <QResizeEvent>

#include <cmath>

using namespace ClipEditorGlobal;

AudioViewGraphicsView::AudioViewGraphicsView(AudioViewGraphicsScene *scene, QWidget *parent)
    : TimeGraphicsView(scene, true, parent), m_audioScene(scene) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("AudioViewGraphicsView");
    setScaleXMax(5);
    setPixelsPerQuarterNote(pixelsPerQuarterNote);
    setSceneVisibility(false);
    setDragBehavior(DragBehavior::None);
    setMinimumHeight(0);
    // The lanes always tile the viewport, so the vertical axis has neither a
    // zoom nor a scroll range
    setScaleYMin(1.0);
    setScrollBarVisibility(Qt::Vertical, false);

    m_background = new AudioViewBackground;
    m_background->setPixelsPerQuarterNote(pixelsPerQuarterNote);
    m_background->setQuantize(appStatus->pianoRollQuantize);
    setGridItem(m_background);
    connect(appStatus, &AppStatus::pianoRollQuantizeChanged, m_background,
            &AudioViewBackground::setQuantize);
    applyLaneColors();

    m_clipRangeOverlay = new ClipRangeOverlay;
    m_clipRangeOverlay->setZValue(3);
    m_clipRangeOverlay->setTransparentMouseEvents(true);
    m_audioScene->addCommonItem(m_clipRangeOverlay);
}

void AudioViewGraphicsView::setContent(const AudioViewContent &content) {
    if (m_content.clipId != content.clipId)
        m_viewportPositioned = false;
    m_content = content;

    rebuildLanes();
    applyContentToLanes();

    m_background->setChannelCount(content.isValid() ? content.channels() : 1);
    m_clipRangeOverlay->setClipRange(content.contentStartTick(), content.clipLen);
    setSceneLength(std::max(0, content.materialEndTick()));
    setSceneVisibility(content.isValid());
}

const AudioViewContent &AudioViewGraphicsView::content() const {
    return m_content;
}

void AudioViewGraphicsView::rebuildLanes() {
    const auto lanes = m_content.isValid() ? m_content.laneCount() : 1;
    while (m_laneViews.size() > lanes) {
        auto *lane = m_laneViews.takeLast();
        m_audioScene->removeCommonItem(lane);
        delete lane;
    }
    while (m_laneViews.size() < lanes) {
        auto *lane = new AudioWaveformLaneView(static_cast<int>(m_laneViews.size()), lanes);
        lane->setCenterLineColor(m_waveformCenterLineColor);
        m_audioScene->addCommonItem(lane);
        m_laneViews.append(lane);
    }
    for (auto index = 0; index < m_laneViews.size(); index++)
        m_laneViews.at(index)->setChannel(index, lanes);
}

void AudioViewGraphicsView::applyContentToLanes() {
    for (auto *lane : std::as_const(m_laneViews)) {
        lane->setAudioInfo(m_content.audioInfo);
        lane->setTimeline(m_content.timeline);
        lane->setPath(m_content.path);
        lane->setLoadState(m_content.loadState);
        lane->setErrorMessage(m_content.errorMessage);
        lane->setMaterialTicks(m_content.start, m_content.contentStartTick());
        lane->setTrackColorIndex(m_content.trackColorIndex);
    }
}

void AudioViewGraphicsView::applyLaneColors() {
    if (m_background) {
        m_background->setLaneBackgroundColor(m_laneBackgroundColor);
        m_background->setLaneSeparatorColor(m_laneSeparatorColor);
    }
    for (auto *lane : std::as_const(m_laneViews))
        lane->setCenterLineColor(m_waveformCenterLineColor);
    if (m_clipRangeOverlay)
        m_clipRangeOverlay->setFillColor(m_clipRangeOverlayColor);
}

double AudioViewGraphicsView::viewCenterTick() const {
    return (startTick() + endTick()) * 0.5;
}

bool AudioViewGraphicsView::centerAt(const double center) {
    if (!std::isfinite(center) || center < 0.0)
        return false;
    stopViewportAnimations();
    setViewportCenterAtTick(center);
    return true;
}

bool AudioViewGraphicsView::setTimeViewport(const double center, const double horizontalScale) {
    if (!std::isfinite(center) || center < 0.0 || !std::isfinite(horizontalScale) ||
        horizontalScale <= 0.0) {
        return false;
    }
    const auto previousScale = scaleX();
    const auto previousCenter = viewCenterTick();
    if (!setViewportScale(horizontalScale, 1.0))
        return false;
    if (centerAt(center))
        return true;
    setViewportScale(previousScale, 1.0);
    centerAt(previousCenter);
    return false;
}

void AudioViewGraphicsView::positionViewportAtContent() {
    if (!m_content.isValid() || m_viewportPositioned || viewport()->width() <= 0)
        return;
    m_viewportPositioned = true;
    const auto contentCenter = (m_content.contentStartTick() + m_content.contentEndTick()) * 0.5;
    centerAt(contentCenter);
    notifyVisibleRectChanged();
}

void AudioViewGraphicsView::resizeEvent(QResizeEvent *event) {
    // The scene is exactly one viewport tall: lane bands are derived from it, so
    // it has to be updated before the base class recomputes the scroll ranges
    m_audioScene->setViewportHeight(viewport()->height());
    TimeGraphicsView::resizeEvent(event);
    positionViewportAtContent();
}

void AudioViewGraphicsView::afterSetScale() {
    TimeGraphicsView::afterSetScale();
    // A stray vertical zoom (wheel, touch pinch) would make the scene taller than
    // the viewport and open a vertical scroll range that has no meaning here
    if (scaleY() != 1.0)
        setScaleY(1.0);
}

QColor AudioViewGraphicsView::laneBackgroundColor() const {
    return m_laneBackgroundColor;
}

void AudioViewGraphicsView::setLaneBackgroundColor(const QColor &color) {
    m_laneBackgroundColor = color;
    applyLaneColors();
}

QColor AudioViewGraphicsView::laneSeparatorColor() const {
    return m_laneSeparatorColor;
}

void AudioViewGraphicsView::setLaneSeparatorColor(const QColor &color) {
    m_laneSeparatorColor = color;
    applyLaneColors();
}

QColor AudioViewGraphicsView::waveformCenterLineColor() const {
    return m_waveformCenterLineColor;
}

void AudioViewGraphicsView::setWaveformCenterLineColor(const QColor &color) {
    m_waveformCenterLineColor = color;
    applyLaneColors();
}

QColor AudioViewGraphicsView::clipRangeOverlayColor() const {
    return m_clipRangeOverlayColor;
}

void AudioViewGraphicsView::setClipRangeOverlayColor(const QColor &color) {
    m_clipRangeOverlayColor = color;
    applyLaneColors();
}
