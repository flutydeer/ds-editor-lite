#include "AudioView.h"

#include "AudioChannelAxisView.h"
#include "AudioViewGraphicsScene.h"
#include "AudioViewGraphicsView.h"
#include "AudioViewRhiWidget.h"
#include "Controller/AudioDecodingController.h"
#include "Global/AppGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/TimeGraphicsView.h"
#include "UI/Views/Common/TimelineView.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

using namespace ClipEditorGlobal;

AudioView::AudioView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    const auto useRhi = appOptions->developer()->editorRenderBackend ==
                        DeveloperOption::EditorRenderBackend::RhiExperimental;
    if (useRhi) {
        m_rhiView = new AudioViewRhiWidget(this);
        m_editorWidget = m_rhiView;
    } else {
        createLegacyBackend();
        m_editorWidget = m_graphicsView;
    }

    m_timelineView = new TimelineView;
    m_timelineView->setObjectName("audioViewTimelineView");
    m_timelineView->setPixelsPerQuarterNote(pixelsPerQuarterNote);
    m_timelineView->setQuantize(appStatus->pianoRollQuantize);
    m_timelineView->setFixedHeight(timelineViewHeight);
    connect(appStatus, &AppStatus::pianoRollQuantizeChanged, m_timelineView,
            &TimelineView::setQuantize);

    m_axisView = new AudioChannelAxisView;

    const auto topLeftSpacing = new QWidget();
    topLeftSpacing->setObjectName("audioViewTopLeftSpacing");
    topLeftSpacing->setFixedHeight(timelineViewHeight);
    topLeftSpacing->setFixedWidth(audioChannelAxisWidth);

    m_axisColumn = new QWidget;
    m_axisColumn->setObjectName("audioViewAxisColumn");
    m_axisColumn->setFixedWidth(audioChannelAxisWidth);
    const auto axisLayout = new QVBoxLayout;
    axisLayout->setContentsMargins(0, 0, 0, 0);
    axisLayout->setSpacing(0);
    axisLayout->addWidget(topLeftSpacing);
    axisLayout->addWidget(m_axisView, 1);
    m_axisColumn->setLayout(axisLayout);

    m_lbTip = new QLabel(tr("Select a clip to edit"));
    m_lbTip->setObjectName("lbNullClipTip");
    m_lbTip->setAlignment(Qt::AlignCenter);

    m_rightLayout = new QVBoxLayout;
    m_rightLayout->setContentsMargins(0, 0, 0, 0);
    m_rightLayout->setSpacing(0);
    m_rightLayout->addWidget(m_timelineView);
    m_rightLayout->addWidget(m_editorWidget);

    const auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_axisColumn);
    layout->addLayout(m_rightLayout, 1);
    layout->addWidget(m_lbTip, 1);
    setLayout(layout);

    connect(m_timelineView, &TimelineView::wheelHorScale, this, &AudioView::onWheelHorScale);
    // A decode finishing writes audioInfo without touching the clip's own signals, so the load
    // state has to come from the controller that owns it
    connect(audioDecodingController, &AudioDecodingController::loadStateChanged, this,
            [this](const int clipId, const AppGlobal::AudioClipLoadState) {
                if (m_clip && m_clip->id() == clipId)
                    refreshContent();
            });
    if (m_rhiView)
        connectRhiBackend();
    else
        connectLegacyBackend();
    updateContentVisibility();
}

AudioView::~AudioView() = default;

void AudioView::createLegacyBackend() {
    m_scene = new AudioViewGraphicsScene;
    m_graphicsView = new AudioViewGraphicsView(m_scene, this);
}

void AudioView::connectLegacyBackend() {
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, m_timelineView,
            &TimelineView::setTimeRange);
    connect(m_graphicsView, &TimeGraphicsView::scaleChanged, this,
            [this](const double horizontal, double) { emit scaleChanged(horizontal); });
    connect(m_graphicsView->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &AudioView::horizontalBarValueChanged);
    m_timelineView->setTimeRange(m_graphicsView->startTick(), m_graphicsView->endTick());
}

void AudioView::connectRhiBackend() {
    connect(m_rhiView, &AudioViewRhiWidget::timeRangeChanged, m_timelineView,
            &TimelineView::setTimeRange);
    connect(m_rhiView, &AudioViewRhiWidget::scaleChanged, this, &AudioView::scaleChanged);
    connect(m_rhiView, &AudioViewRhiWidget::horizontalBarValueChanged, this,
            &AudioView::horizontalBarValueChanged);
    connect(m_rhiView, &AudioViewRhiWidget::backendUnavailable, this, &AudioView::fallbackToLegacy);
    m_timelineView->setTimeRange(m_rhiView->startTick(), m_rhiView->endTick());
}

void AudioView::fallbackToLegacy() {
    if (!m_rhiView || m_graphicsView)
        return;
    const auto state = viewState();
    auto *failedView = m_rhiView;
    m_rhiView = nullptr;
    createLegacyBackend();
    m_editorWidget = m_graphicsView;
    m_rightLayout->replaceWidget(failedView, m_graphicsView);
    failedView->hide();
    failedView->deleteLater();
    connectLegacyBackend();
    applyContent();
    updateContentVisibility();
    QTimer::singleShot(0, this, [this, state] {
        setTimeViewport(state.centerTick, state.horizontalScale);
    });
    qWarning() << "[AudioViewRhi] backend failed; restored Legacy view";
}

void AudioView::setDataContext(AudioClip *clip) {
    if (m_clip)
        disconnect(m_clip, nullptr, this, nullptr);
    m_clip = clip;
    if (m_clip) {
        connect(m_clip, &Clip::propertyChanged, this, &AudioView::refreshContent);
        connect(m_clip, &AudioClip::pathChanged, this, &AudioView::refreshContent);
        connect(m_clip, &AudioClip::sourceChanged, this, &AudioView::refreshContent);
        connect(m_clip, &AudioClip::pathStatusChanged, this,
                [this](AudioClip::PathStatus) { refreshContent(); });
    }
    applyContent();
    updateContentVisibility();
}

void AudioView::refreshContent() {
    applyContent();
}

AudioViewContent AudioView::buildContent() const {
    AudioViewContent content;
    if (!m_clip)
        return content;
    content.clipId = m_clip->id();
    content.start = m_clip->start();
    content.clipStart = m_clip->clipStart();
    content.clipLen = m_clip->clipLen();
    content.length = m_clip->length();
    content.path = m_clip->path();
    content.audioInfo = m_clip->audioInfo();
    content.timeline = appModel->timeline();
    content.loadState = audioDecodingController->loadState(m_clip);
    content.trackColorIndex = m_trackColorIndex;
    return content;
}

void AudioView::setTrackColorIndex(const int index) {
    if (m_trackColorIndex == index)
        return;
    m_trackColorIndex = index;
    applyContent();
}

void AudioView::applyContent() {
    const auto content = buildContent();
    m_axisView->setChannelCount(content.isValid() ? content.channels() : 1);
    if (m_rhiView)
        m_rhiView->setContent(content);
    else
        m_graphicsView->setContent(content);
}

void AudioView::updateContentVisibility() const {
    const auto hasClip = m_clip != nullptr;
    m_timelineView->setVisible(hasClip);
    m_editorWidget->setVisible(hasClip);
    m_axisColumn->setVisible(hasClip);
    m_lbTip->setVisible(!hasClip);
}

PianoRollViewState AudioView::viewState() const {
    const auto center = m_rhiView ? m_rhiView->viewCenterTick()
                                  : (m_graphicsView->startTick() + m_graphicsView->endTick()) * 0.5;
    // The audio view has no pitch axis and no vertical zoom, so the vertical half
    // of the state is a constant and the edit mode is never anything but Select
    return {center, 60.0, scaleX(), 1.0, Select};
}

bool AudioView::centerAt(const double tick) const {
    if (m_rhiView)
        return m_rhiView->centerAt(tick);
    return m_graphicsView->centerAt(tick);
}

bool AudioView::setTimeViewport(const double centerTick, const double horizontalScale) const {
    if (m_rhiView)
        return m_rhiView->setTimeViewport(centerTick, horizontalScale);
    return m_graphicsView->setTimeViewport(centerTick, horizontalScale);
}

double AudioView::scaleX() const {
    return m_rhiView ? m_rhiView->scaleX() : m_graphicsView->scaleX();
}

int AudioView::horizontalBarValue() const {
    return m_rhiView ? m_rhiView->horizontalBarValue() : m_graphicsView->horizontalBarValue();
}

void AudioView::onWheelHorScale(QWheelEvent *event) const {
    if (m_rhiView)
        m_rhiView->onWheelHorScale(event);
    else
        m_graphicsView->onWheelHorScale(event);
}

void AudioView::onWheelHorScroll(QWheelEvent *event) const {
    if (m_rhiView)
        m_rhiView->onWheelHorScroll(event);
    else
        m_graphicsView->onWheelHorScroll(event);
}

void AudioView::setHorizontalBarValue(const int value) const {
    if (m_rhiView)
        m_rhiView->setHorizontalBarValue(value);
    else
        m_graphicsView->setHorizontalBarValue(value);
}

void AudioView::setPlaybackPosition(const double tick) const {
    if (m_rhiView)
        m_rhiView->setPlaybackPosition(tick);
    else
        m_graphicsView->setPlaybackPosition(tick);
}

void AudioView::setLastPlaybackPosition(const double tick) const {
    if (m_rhiView)
        m_rhiView->setLastPlaybackPosition(tick);
    else
        m_graphicsView->setLastPlaybackPosition(tick);
}

void AudioView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        m_lbTip->setText(tr("Select a clip to edit"));
}
