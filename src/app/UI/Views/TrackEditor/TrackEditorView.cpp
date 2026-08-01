//
// Created by fluty on 2024/1/29.
//

#include "TrackEditorView.h"

#include "TrackControlView.h"
#include "TrackEditorContextMenuController.h"
#include "TrackListHeaderView.h"
#include "TrackListView.h"
#include "TracksGraphicsScene.h"
#include "TracksGraphicsView.h"
#include "TracksRhiWidget.h"
#include "TrackViewModel.h"
#include "Controller/EditorViewController.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Global/TracksEditorGlobal.h"
#include "GraphicsItem/AudioClipView.h"
#include "GraphicsItem/SingingClipView.h"
#include "GraphicsItem/TrackEditorBackgroundView.h"
#include "InfoLane/InfoLaneHeaderView.h"
#include "InfoLane/TempoLaneView.h"
#include "InfoLane/TimeSignatureLaneView.h"
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "Model/AppStatus/AppStatus.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioContext.h"
#include <lite/GUI/Controls/LevelMeter.h>
#include <lite/GUI/Controls/LevelMeterViewModel.h>
#include "UI/Controls/LevelMeterManager.h"
#include "AppContext.h"
#include "UI/Utils/SpeakerMixDisplayUtils.h"
#include "UI/Views/Common/TimelineView.h"
#include "UI/Views/Common/EditorShortcutUtils.h"

#include <QFileDialog>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <lite/GUI/Controls/OverlaySplitter.h>

#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace {

    void updateSingingClipDisplay(SingingClip *clip, SingingClipView *view) {
        view->setSingerName(clip->singerInfo().name());
        view->setSpeakerName(SpeakerMixDisplayUtils::speakerDisplayName(
            clip->singerInfo(), clip->speakerInfo(), clip->speakerMixData()));
    }

} // namespace

TrackEditorView::TrackEditorView(QWidget *parent) : PanelView(AppGlobal::TracksEditor, parent) {
    trackController->setParentWidget(this);
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("TracksView");
    m_contextMenuController = new TrackEditorContextMenuController(this);

    m_trackListView = new TrackListView;

    const auto useRhi = appOptions->developer()->editorRenderBackend ==
                        DeveloperOption::EditorRenderBackend::RhiExperimental;
    if (useRhi) {
        m_rhiView = new TracksRhiWidget;
        m_rhiView->setSceneLength(appStatus->projectEditableLength);
    } else {
        createLegacyBackend();
    }
    auto *editorWidget =
        m_rhiView ? static_cast<QWidget *>(m_rhiView) : static_cast<QWidget *>(m_graphicsView);
    const auto initialStartTick = m_rhiView ? m_rhiView->startTick() : m_graphicsView->startTick();
    const auto initialEndTick = m_rhiView ? m_rhiView->endTick() : m_graphicsView->endTick();

    m_timeline = new TimelineView;
    m_timeline->setObjectName("tracksTimelineView");
    m_timeline->setTimeRange(initialStartTick, initialEndTick);
    m_timeline->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    m_timeline->setQuantize(128);
    m_timeline->setFixedHeight(TracksEditorGlobal::trackViewHeaderHeight);
    m_timeline->setCanEditLoop(true); // Enable loop editing in track editor

    m_tempoLane = new TempoLaneView;
    m_tempoLane->setFixedHeight(TracksEditorGlobal::infoLaneHeight);
    m_tempoLane->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    m_tempoLane->setQuantize(128);
    m_tempoLane->setTimeRange(initialStartTick, initialEndTick);

    m_tempoLaneHeader = new InfoLaneHeaderView;
    m_tempoLaneHeader->setObjectName("tempoLaneHeaderView");
    m_tempoLaneHeader->setTitle(tr("Tempo"));
    m_tempoLaneHeader->setFixedHeight(TracksEditorGlobal::infoLaneHeight);

    m_timeSignatureLane = new TimeSignatureLaneView;
    m_timeSignatureLane->setFixedHeight(TracksEditorGlobal::infoLaneHeight);
    m_timeSignatureLane->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    m_timeSignatureLane->setQuantize(128);
    m_timeSignatureLane->setTimeRange(initialStartTick, initialEndTick);

    m_timeSignatureLaneHeader = new InfoLaneHeaderView;
    m_timeSignatureLaneHeader->setObjectName("timeSignatureLaneHeaderView");
    m_timeSignatureLaneHeader->setTitle(tr("Time Signature"));
    m_timeSignatureLaneHeader->setFixedHeight(TracksEditorGlobal::infoLaneHeight);

    const auto trackListHeader = new TrackListHeaderView;

    const auto trackListPanelLayout = new QVBoxLayout;
    trackListPanelLayout->setContentsMargins({});
    trackListPanelLayout->setSpacing(0);
    trackListPanelLayout->addWidget(trackListHeader);
    trackListPanelLayout->addWidget(m_tempoLaneHeader);
    trackListPanelLayout->addWidget(m_timeSignatureLaneHeader);
    trackListPanelLayout->addWidget(m_trackListView);

    m_trackTimelineAndViewLayout = new QVBoxLayout;
    m_trackTimelineAndViewLayout->setContentsMargins({});
    m_trackTimelineAndViewLayout->setSpacing(0);
    m_trackTimelineAndViewLayout->addWidget(m_timeline);
    m_trackTimelineAndViewLayout->addWidget(m_tempoLane);
    m_trackTimelineAndViewLayout->addWidget(m_timeSignatureLane);
    m_trackTimelineAndViewLayout->addWidget(editorWidget);

    auto *trackListPanel = new QWidget;
    trackListPanel->setObjectName("trackListPanel");
    trackListPanel->setLayout(trackListPanelLayout);
    trackListPanel->setMinimumWidth(200);
    trackListPanel->setMaximumWidth(600);

    auto *trackTimelineAndView = new QWidget;
    trackTimelineAndView->setLayout(m_trackTimelineAndViewLayout);

    m_splitter = new OverlaySplitter(Qt::Horizontal);
    m_splitter->setObjectName("trackSplitter");
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(trackListPanel);
    m_splitter->addWidget(trackTimelineAndView);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({TracksEditorGlobal::trackListWidth, 1});

    const auto mainLayout = new QHBoxLayout;
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_splitter);
    mainLayout->setContentsMargins({1, 1, 1, 1});

    setLayout(mainLayout);
    setPanelActive(true);
    editorViewController->registerPanel(this);
    installEventFilter(this);

    connect(m_trackListView, &QListWidget::currentRowChanged, this,
            &TrackEditorView::setSelectedTrackIndex);
    connect(appStatus, &AppStatus::selectedTrackIndexChanged, this,
            &TrackEditorView::syncSelectedTrackToList);
    if (m_rhiView)
        connectRhiBackend();
    else
        connectLegacyBackend();
    registerEditorShortcuts();
    connect(trackListHeader, &TrackListHeaderView::tempoLaneToggled, this,
            [this](const bool visible) {
                m_tempoLaneHeader->setVisible(visible);
                m_tempoLane->setVisible(visible);
            });
    connect(trackListHeader, &TrackListHeaderView::timeSignatureLaneToggled, this,
            [this](const bool visible) {
                m_timeSignatureLaneHeader->setVisible(visible);
                m_timeSignatureLane->setVisible(visible);
            });
    connect(playbackController, &PlaybackController::positionChanged, this,
            &TrackEditorView::onPositionChanged);
    connect(playbackController, &PlaybackController::lastPositionChanged, this,
            &TrackEditorView::onLastPositionChanged);
    connect(appModel, &AppModel::modelChanged, this, &TrackEditorView::onModelChanged);
    connect(appModel, &AppModel::trackChanged, this, &TrackEditorView::onTrackChanged);
    connect(appModel, &AppModel::trackMoved, this, &TrackEditorView::onTrackMoved);
}

void TrackEditorView::registerEditorShortcuts() {
    using EditorShortcutUtils::add;
    add(this, QKeySequence::Cut, m_contextMenuController,
        &TrackEditorContextMenuController::cutSelection);
    add(this, QKeySequence::Copy, m_contextMenuController,
        &TrackEditorContextMenuController::copySelection);
    add(this, QKeySequence::Paste, m_contextMenuController,
        &TrackEditorContextMenuController::pasteSelection);
    add(this, QKeySequence::SelectAll, m_contextMenuController,
        &TrackEditorContextMenuController::selectAll);
    add(this, QKeySequence::Delete, m_contextMenuController,
        [this] { m_contextMenuController->deleteSelection(); });
    add(this, QKeySequence(Qt::Key_Backspace), m_contextMenuController,
        [this] { m_contextMenuController->deleteSelection(); });
}

TrackEditorView::~TrackEditorView() {
    editorViewController->unregisterPanel(this);
}

void TrackEditorView::createLegacyBackend() {
    m_tracksScene = new TracksGraphicsScene;
    m_graphicsView = new TracksGraphicsView(m_tracksScene);
    m_graphicsView->centerOn(0, 0);
    m_graphicsView->setSceneLength(appStatus->projectEditableLength);
    m_gridItem = new TrackEditorBackgroundView;
    m_gridItem->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    m_gridItem->setQuantize(128);
    m_graphicsView->setGridItem(m_gridItem);
    m_graphicsView->setSnapGrid(m_gridItem);
}

void TrackEditorView::connectLegacyBackend() {
    connect(m_graphicsView, &TracksGraphicsView::contextMenuRequested, this,
            [this](const TrackEditorMenuContext &context) {
                m_contextMenuController->showMenu(context, m_graphicsView);
            });
    connect(m_graphicsView, &TracksGraphicsView::scaleChanged, this,
            &TrackEditorView::onViewScaleChanged);
    connect(m_graphicsView, &TracksGraphicsView::sizeChanged, m_tracksScene,
            &TracksGraphicsScene::onViewResized);
    connect(this, &TrackEditorView::trackCountChanged, m_tracksScene,
            &TracksGraphicsScene::onTrackCountChanged);
    connect(this, &TrackEditorView::trackCountChanged, m_gridItem,
            &TrackEditorBackgroundView::onTrackCountChanged);
    connect(appStatus, &AppStatus::selectedTrackIndexChanged, m_gridItem,
            &TrackEditorBackgroundView::onTrackSelectionChanged);
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, m_timeline,
            &TimelineView::setTimeRange);
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, m_tempoLane,
            &InfoLaneView::setTimeRange);
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, m_timeSignatureLane,
            &InfoLaneView::setTimeRange);
    connect(m_timeline, &TimelineView::wheelHorScale, m_graphicsView,
            &TracksGraphicsView::onWheelHorScale);
    connect(m_tempoLane, &InfoLaneView::wheelHorScale, m_graphicsView,
            &TracksGraphicsView::onWheelHorScale);
    connect(m_tempoLane, &InfoLaneView::wheelHorScroll, m_graphicsView,
            &TracksGraphicsView::onWheelHorScroll);
    connect(m_tempoLane, &InfoLaneView::wheelVerScroll, m_graphicsView,
            &TracksGraphicsView::onWheelVerScroll);
    connect(m_timeSignatureLane, &InfoLaneView::wheelHorScale, m_graphicsView,
            &TracksGraphicsView::onWheelHorScale);
    connect(m_timeSignatureLane, &InfoLaneView::wheelHorScroll, m_graphicsView,
            &TracksGraphicsView::onWheelHorScroll);
    connect(m_timeSignatureLane, &InfoLaneView::wheelVerScroll, m_graphicsView,
            &TracksGraphicsView::onWheelVerScroll);
    connect(m_trackListView, &TrackListView::wheelVerScale, m_graphicsView,
            &TracksGraphicsView::onWheelVerScale);
    connect(m_trackListView, &TrackListView::wheelVerScroll, m_graphicsView,
            &TracksGraphicsView::onWheelVerScroll);
    connect(m_graphicsView->verticalScrollBar(), &QScrollBar::valueChanged,
            m_trackListView->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_trackListView->verticalScrollBar(), &QScrollBar::valueChanged,
            m_graphicsView->verticalScrollBar(), &QScrollBar::setValue);
    connect(appStatus, &AppStatus::projectEditableLengthChanged, m_graphicsView,
            &TracksGraphicsView::setSceneLength);
}

void TrackEditorView::connectRhiBackend() {
    connect(m_rhiView, &TracksRhiWidget::contextMenuRequested, this,
            [this](const TrackEditorMenuContext &context) {
                m_contextMenuController->showMenu(context, m_rhiView);
            });
    connect(m_rhiView, &TracksRhiWidget::scaleChanged, this, &TrackEditorView::onViewScaleChanged);
    connect(m_rhiView, &TracksRhiWidget::timeRangeChanged, m_timeline, &TimelineView::setTimeRange);
    connect(m_rhiView, &TracksRhiWidget::timeRangeChanged, m_tempoLane,
            &InfoLaneView::setTimeRange);
    connect(m_rhiView, &TracksRhiWidget::timeRangeChanged, m_timeSignatureLane,
            &InfoLaneView::setTimeRange);
    connect(m_timeline, &TimelineView::wheelHorScale, m_rhiView, &TracksRhiWidget::onWheelHorScale);
    connect(m_tempoLane, &InfoLaneView::wheelHorScale, m_rhiView,
            &TracksRhiWidget::onWheelHorScale);
    connect(m_tempoLane, &InfoLaneView::wheelHorScroll, m_rhiView,
            &TracksRhiWidget::onWheelHorScroll);
    connect(m_tempoLane, &InfoLaneView::wheelVerScroll, m_rhiView,
            &TracksRhiWidget::onWheelVerScroll);
    connect(m_timeSignatureLane, &InfoLaneView::wheelHorScale, m_rhiView,
            &TracksRhiWidget::onWheelHorScale);
    connect(m_timeSignatureLane, &InfoLaneView::wheelHorScroll, m_rhiView,
            &TracksRhiWidget::onWheelHorScroll);
    connect(m_timeSignatureLane, &InfoLaneView::wheelVerScroll, m_rhiView,
            &TracksRhiWidget::onWheelVerScroll);
    connect(m_trackListView, &TrackListView::wheelVerScale, m_rhiView,
            &TracksRhiWidget::onWheelVerScale);
    connect(m_trackListView, &TrackListView::wheelVerScroll, m_rhiView,
            &TracksRhiWidget::onWheelVerScroll);
    connect(m_rhiView, &TracksRhiWidget::verticalOffsetChanged,
            m_trackListView->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_trackListView->verticalScrollBar(), &QScrollBar::valueChanged, m_rhiView,
            &TracksRhiWidget::setVerticalOffset);
    connect(appStatus, &AppStatus::projectEditableLengthChanged, m_rhiView,
            &TracksRhiWidget::setSceneLength);
    connect(m_rhiView, &EditorRhiWidget::backendFailed, this,
            [this](const QString &reason) {
                qWarning().noquote()
                    << QStringLiteral("[TracksRhi] falling back to Legacy: %1").arg(reason);
                fallbackToLegacy();
            });
}

void TrackEditorView::fallbackToLegacy() {
    if (!m_rhiView || m_graphicsView)
        return;
    const auto state = m_rhiView->viewState();
    auto *failedView = m_rhiView;
    m_rhiView = nullptr;
    createLegacyBackend();
    m_trackTimelineAndViewLayout->replaceWidget(failedView, m_graphicsView);
    failedView->hide();
    failedView->deleteLater();
    connectLegacyBackend();
    for (const auto *trackViewModel : std::as_const(m_viewModel.tracks)) {
        auto *controlView = trackViewModel->controlView;
        connect(m_graphicsView, &TracksGraphicsView::scaleChanged, controlView,
                &TrackControlView::finishTrackNameEditing);
        connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, controlView,
                &TrackControlView::finishTrackNameEditing);
        connect(m_graphicsView, &TracksGraphicsView::sizeChanged, controlView,
                &TrackControlView::finishTrackNameEditing);
    }
    populateLegacyClipItems();
    emit trackCountChanged(m_viewModel.tracks.size());
    onViewScaleChanged(state.horizontalScale, state.verticalScale);
    QTimer::singleShot(0, this, [this, state] {
        setViewScale(state.horizontalScale, state.verticalScale);
        centerAt(state.centerTick, state.centerTrackIndex);
    });
}

void TrackEditorView::populateLegacyClipItems() {
    if (!m_graphicsView)
        return;
    for (int trackIndex = 0; trackIndex < m_viewModel.tracks.size(); ++trackIndex) {
        auto *trackViewModel = m_viewModel.tracks.at(trackIndex);
        for (auto *clip : trackViewModel->dsTrack->clips())
            onClipInserted(clip, trackViewModel, trackIndex);
    }
}

double TrackEditorView::activeScaleY() const {
    return m_rhiView ? m_rhiView->scaleY() : m_graphicsView->scaleY();
}

void TrackEditorView::changeEvent(QEvent *event) {
    PanelView::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        m_tempoLaneHeader->setTitle(tr("Tempo"));
        m_timeSignatureLaneHeader->setTitle(tr("Time Signature"));
    }
}

TrackPanelViewState TrackEditorView::viewState() const {
    if (m_rhiView)
        return m_rhiView->viewState();
    const auto scaleY = m_graphicsView->scaleY();
    const auto trackHeight = TracksEditorGlobal::trackHeight * scaleY;
    const auto centerTrackIndex =
        trackHeight > 0 ? m_graphicsView->visibleRect().center().y() / trackHeight - 0.5 : 0;
    return {
        .centerTick = (m_graphicsView->startTick() + m_graphicsView->endTick()) / 2,
        .centerTrackIndex = centerTrackIndex,
        .horizontalScale = m_graphicsView->scaleX(),
        .verticalScale = scaleY,
    };
}

bool TrackEditorView::centerAt(double tick, double trackIndex) const {
    if (!std::isfinite(tick) || !std::isfinite(trackIndex))
        return false;
    if (m_rhiView)
        return m_rhiView->centerAt(tick, trackIndex);

    m_graphicsView->stopViewportAnimations();
    m_graphicsView->setViewportCenterAtTick(tick);
    const auto centerSceneY =
        (trackIndex + 0.5) * TracksEditorGlobal::trackHeight * m_graphicsView->scaleY();
    m_graphicsView->setVerticalBarValue(
        qRound(centerSceneY - m_graphicsView->viewport()->height() / 2.0));
    return true;
}

bool TrackEditorView::setViewScale(double horizontalScale, double verticalScale) const {
    if (m_rhiView)
        return m_rhiView->setViewScale(horizontalScale, verticalScale);
    const auto previousState = viewState();
    if (!m_graphicsView->setViewportScale(horizontalScale, verticalScale))
        return false;
    return centerAt(previousState.centerTick, previousState.centerTrackIndex);
}

HistoryFocusVisibility TrackEditorView::focusVisibility(const HistoryFocus &focus) const {
    if (m_rhiView)
        return m_rhiView->focusVisibility(focus);
    if (focus.kind != HistoryFocusKind::TrackClips || !focus.isValid())
        return HistoryFocusVisibility::Unavailable;

    QRectF itemBounds;
    for (const auto id : focus.objectIds) {
        if (const auto item = findClipItemById(id))
            itemBounds = itemBounds.isNull() ? item->sceneBoundingRect()
                                             : itemBounds.united(item->sceneBoundingRect());
    }
    if (!itemBounds.isNull())
        return m_graphicsView->logicalVisibleRect().intersects(itemBounds)
                   ? HistoryFocusVisibility::Visible
                   : HistoryFocusVisibility::ScrollRequired;

    int trackIndex = focus.trackIndex;
    if (focus.trackId >= 0)
        appModel->findTrackById(focus.trackId, trackIndex);
    const auto visible = m_graphicsView->logicalVisibleRect();
    const auto left = m_graphicsView->sceneXForTick(focus.tickStart);
    const auto right = m_graphicsView->sceneXForTick(focus.tickEnd);
    const auto tickVisible = right >= visible.left() && left <= visible.right();
    const auto trackHeight = TracksEditorGlobal::trackHeight * m_graphicsView->scaleY();
    const auto trackTop = trackIndex * trackHeight;
    const auto trackBottom = trackTop + trackHeight;
    return tickVisible && trackBottom >= visible.top() && trackTop <= visible.bottom()
               ? HistoryFocusVisibility::Visible
               : HistoryFocusVisibility::ScrollRequired;
}

bool TrackEditorView::revealFocus(const HistoryFocus &focus) const {
    return revealFocus(focus, true);
}

bool TrackEditorView::revealFocus(const HistoryFocus &focus, const bool animated) const {
    if (m_rhiView)
        return m_rhiView->revealFocus(focus, animated);
    if (focus.kind != HistoryFocusKind::TrackClips || !focus.isValid())
        return false;

    m_tracksScene->clearSelection();
    QList<int> selectedIds;
    QRectF itemBounds;
    for (const auto id : focus.objectIds) {
        if (const auto item = findClipItemById(id)) {
            item->setSelected(true);
            selectedIds.append(id);
            itemBounds = itemBounds.isNull() ? item->sceneBoundingRect()
                                             : itemBounds.united(item->sceneBoundingRect());
        }
    }
    appStatus->selectedClips = selectedIds;
    if (!selectedIds.isEmpty())
        trackController->setActiveClip(selectedIds.first());

    if (!itemBounds.isNull()) {
        m_graphicsView->ensureSceneRectVisible(itemBounds, 24, 24, animated);
        return true;
    }

    int trackIndex = focus.trackIndex;
    if (focus.trackId >= 0)
        appModel->findTrackById(focus.trackId, trackIndex);
    if (trackIndex < 0)
        trackIndex = qRound((focus.valueStart + focus.valueEnd) / 2.0);
    if (trackIndex < 0)
        return false;
    const auto trackHeight = TracksEditorGlobal::trackHeight * m_graphicsView->scaleY();
    const auto left = m_graphicsView->sceneXForTick(focus.tickStart);
    const auto right = m_graphicsView->sceneXForTick(focus.tickEnd);
    m_graphicsView->ensureSceneRectVisible(
        QRectF(left, trackIndex * trackHeight, qMax(1.0, right - left), trackHeight), 24, 24,
        animated);
    return true;
}

void TrackEditorView::onModelChanged() {
    for (auto i = m_viewModel.tracks.count() - 1; i >= 0; i--) {
        const auto track = m_viewModel.tracks.at(i)->dsTrack;
        onTrackRemoved(track, i);
    }
    int index = 0;
    for (const auto track : appModel->tracks()) {
        onTrackInserted(track, index);
        index++;
    }
    emit trackCountChanged(m_viewModel.tracks.count());
}

void TrackEditorView::onTrackChanged(const AppModel::TrackChangeType type, const qsizetype index,
                                     Track *track) {
    if (type == AppModel::Insert)
        onTrackInserted(track, index);
    else if (type == AppModel::Remove)
        onTrackRemoved(track, index);
    emit trackCountChanged(m_viewModel.tracks.count());
}

void TrackEditorView::onTrackMoved(const qsizetype from, const qsizetype to) {
    const auto count = m_viewModel.tracks.size();
    if (from == to || from < 0 || from >= count || to < 0 || to >= count)
        return;

    const auto previousSelectedTrackIndex = static_cast<int>(appStatus->selectedTrackIndex);
    const QSignalBlocker listBlocker(m_trackListView);

    const auto destination = to > from ? to + 1 : to;
    if (!m_trackListView->model()->moveRow({}, from, {}, destination))
        return;
    m_viewModel.tracks.move(from, to);

    const auto firstChangedIndex = qMin(from, to);
    const auto lastChangedIndex = qMax(from, to);
    for (auto i = firstChangedIndex; i <= lastChangedIndex; ++i) {
        const auto trackVm = m_viewModel.tracks.at(i);
        trackVm->controlView->setTrackIndex(i + 1);
        for (const auto clipItem : trackVm->clips)
            clipItem->setTrackIndex(i);
    }

    auto selectedTrackIndex = previousSelectedTrackIndex;
    if (previousSelectedTrackIndex == from) {
        selectedTrackIndex = static_cast<int>(to);
    } else if (from < to && previousSelectedTrackIndex > from && previousSelectedTrackIndex <= to) {
        selectedTrackIndex = previousSelectedTrackIndex - 1;
    } else if (to < from && previousSelectedTrackIndex >= to && previousSelectedTrackIndex < from) {
        selectedTrackIndex = previousSelectedTrackIndex + 1;
    }
    setSelectedTrackIndex(selectedTrackIndex);
}

void TrackEditorView::onClipChanged(const Track::ClipChangeType type, Clip *clip,
                                    const Track *dsTrack) {
    if (m_rhiView) {
        m_rhiView->scheduleSnapshot();
        return;
    }
    const auto trackVm = m_viewModel.findTrack(dsTrack);
    Q_ASSERT(trackVm);
    if (type == Track::Inserted) {
        int trackIndex;
        appModel->findTrackById(dsTrack->id(), trackIndex);
        onClipInserted(clip, trackVm, trackIndex);
    } else if (type == Track::Removed) {
        onClipRemoved(clip, trackVm);
    }
}

void TrackEditorView::onPositionChanged(const double tick) const {
    if (m_rhiView)
        m_rhiView->setPlaybackPosition(tick);
    else
        m_graphicsView->setPlaybackPosition(tick);
}

void TrackEditorView::onLastPositionChanged(const double tick) const {
    if (m_rhiView)
        m_rhiView->setLastPlaybackPosition(tick);
    else
        m_graphicsView->setLastPlaybackPosition(tick);
}

void TrackEditorView::onViewScaleChanged(const qreal sx, const qreal sy) const {
    Q_UNUSED(sx);
    int previousHeightSum = 0;
    for (int i = 0; i < m_trackListView->count(); i++) {
        // adjust track item height
        const auto item = m_trackListView->item(i);
        const int height =
            qRound((i + 1) * TracksEditorGlobal::trackHeight * sy - previousHeightSum);
        item->setSizeHint(QSize(0, height));

        // hide pan and gain slider when sy is too small
        const auto widget = m_viewModel.tracks.at(i)->controlView;
        widget->setNarrowMode(sy < TracksEditorGlobal::narrowModeScaleY);
        previousHeightSum += height;
    }
}

void TrackEditorView::setSelectedTrackIndex(const int trackIndex) const {
    if (appStatus->selectedTrackIndex != trackIndex)
        appStatus->selectedTrackIndex = trackIndex;
    else
        syncSelectedTrackToList(trackIndex);
}

void TrackEditorView::syncSelectedTrackToList(const int trackIndex) const {
    const QSignalBlocker blocker(m_trackListView);
    if (trackIndex >= 0 && trackIndex < m_trackListView->count()) {
        m_trackListView->setCurrentRow(trackIndex);
    } else {
        m_trackListView->setCurrentItem(nullptr);
        m_trackListView->clearSelection();
    }
}

void TrackEditorView::onRemoveTrackTriggered(const int id) {
    trackController->onRemoveTrack(id);
}

bool TrackEditorView::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QMouseEvent::MouseButtonPress) {
        editorViewController->setActivePanel(AppGlobal::TracksEditor);
    }

    return QWidget::eventFilter(watched, event);
}

TrackViewModel *TrackEditorView::ViewModel::findTrack(const Track *dsTrack) {
    for (const auto trackVm : tracks)
        if (trackVm->dsTrack == dsTrack)
            return trackVm;
    return nullptr;
}

void TrackEditorView::onTrackInserted(Track *dsTrack, const qsizetype trackIndex) {
    // Preserve selection by logical track while row indexes shift.
    const auto previousSelectedTrackIndex = static_cast<int>(appStatus->selectedTrackIndex);
    const QSignalBlocker listBlocker(m_trackListView);

    connect(dsTrack, &Track::propertyChanged, this, [this] { onTrackPropertyChanged(); });
    connect(dsTrack, &Track::clipChanged, this,
            [dsTrack, this](const Track::ClipChangeType type, Clip *clip) {
                onClipChanged(type, clip, dsTrack);
            });

    const auto track = new TrackViewModel(dsTrack);
    if (m_graphicsView)
        for (const auto clip : dsTrack->clips())
            onClipInserted(clip, track, trackIndex);
    auto newTrackItem = new QListWidgetItem;
    const auto controlView = new TrackControlView(newTrackItem, dsTrack);
    controlView->setTrackNameOverlayParent(m_trackListView->viewport());
    if (m_rhiView) {
        connect(m_rhiView, &TracksRhiWidget::scaleChanged, controlView,
                &TrackControlView::finishTrackNameEditing);
        connect(m_rhiView, &TracksRhiWidget::visibleRectChanged, controlView,
                &TrackControlView::finishTrackNameEditing);
        connect(m_rhiView, &TracksRhiWidget::sizeChanged, controlView,
                &TrackControlView::finishTrackNameEditing);
    } else {
        connect(m_graphicsView, &TracksGraphicsView::scaleChanged, controlView,
                &TrackControlView::finishTrackNameEditing);
        connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, controlView,
                &TrackControlView::finishTrackNameEditing);
        connect(m_graphicsView, &TracksGraphicsView::sizeChanged, controlView,
                &TrackControlView::finishTrackNameEditing);
    }
    connect(m_trackListView->verticalScrollBar(), &QScrollBar::valueChanged, controlView,
            &TrackControlView::finishTrackNameEditing);
    newTrackItem->setSizeHint(
        QSize(0, static_cast<int>(TracksEditorGlobal::trackHeight * activeScaleY())));
    controlView->setTrackIndex(trackIndex + 1);
    controlView->setNarrowMode(activeScaleY() < TracksEditorGlobal::narrowModeScaleY);
    m_trackListView->insertItem(trackIndex, newTrackItem);
    m_trackListView->setItemWidget(newTrackItem, controlView);
    track->controlView = controlView;

    auto meter = controlView->levelMeter();
    auto mgr = AppContext::instance<LevelMeterManager>();
    auto vm = mgr ? mgr->viewModelAt(trackIndex) : nullptr;
    meter->bindTo(vm);
    if (vm)
        connect(meter, &LevelMeter::clipResetRequested, vm, &LevelMeterViewModel::resetClip);

    connect(controlView, &TrackControlView::insertNewTrackTriggered, this, [newTrackItem, this] {
        const auto i = m_trackListView->row(newTrackItem);
        trackController->onInsertNewTrack(i + 1); // insert after current track
    });
    connect(controlView, &TrackControlView::removeTrackTriggered, this,
            &TrackEditorView::onRemoveTrackTriggered);
    m_viewModel.tracks.insert(trackIndex, track);
    if (trackIndex < m_viewModel.tracks.count()) // needs to update existed tracks' index
        for (int i = trackIndex + 1; i < m_viewModel.tracks.count(); i++) {
            // Update track list items' index
            const auto item = m_trackListView->item(i);
            const auto widget = m_trackListView->itemWidget(item);
            const auto trackWidget = dynamic_cast<TrackControlView *>(widget);
            trackWidget->setTrackIndex(i + 1);
            // Update clips' index
            for (const auto &clipItem : m_viewModel.tracks.at(i)->clips) {
                clipItem->setTrackIndex(i);
            }
        }

    // An insertion before the selected row moves the same logical track down by one row.
    if (previousSelectedTrackIndex >= 0 &&
        static_cast<int>(trackIndex) <= previousSelectedTrackIndex)
        setSelectedTrackIndex(previousSelectedTrackIndex + 1);
    else
        syncSelectedTrackToList(previousSelectedTrackIndex);
}

void TrackEditorView::onClipInserted(Clip *clip, TrackViewModel *track, const int trackIndex) {
    if (!m_graphicsView)
        return;
    if (const auto cachedView = m_pendingRemoveClipViews.take(clip->id())) {
        // Cross-track move: reuse the cached clip view preserving its state
        cachedView->setTrackIndex(trackIndex);
        cachedView->setColorIndex(track->dsTrack->colorIndex());
        m_tracksScene->addCommonItem(cachedView);
        track->clips[clip] = cachedView;
        connect(clip, &Clip::propertyChanged, this, [clip, this] { updateClipOnView(clip); });
        // Reconnect type-specific signals that were disconnected by onClipRemoved
        if (clip->clipType() == Clip::Singing) {
            const auto singingClip = static_cast<SingingClip *>(clip);
            const auto singingView = static_cast<SingingClipView *>(cachedView);
            connect(singingClip, &SingingClip::voiceContextChanged, this,
                    [singingClip, singingView](const VoiceContextChange &) {
                        updateSingingClipDisplay(singingClip, singingView);
                    });
            connect(singingClip, &SingingClip::defaultLanguageChanged, singingView,
                    &SingingClipView::setDefaultLanguage);
            connect(singingClip, &SingingClip::noteChanged, singingView,
                    &SingingClipView::onNoteListChanged);
        } else if (clip->clipType() == Clip::Audio) {
            const auto audioView = static_cast<AudioClipView *>(cachedView);
            // The timeline may have changed while the view sat in the cache
            audioView->setTimeline(appModel->timeline());
            connect(appModel, &AppModel::timelineChanged, audioView,
                    [audioView] { audioView->setTimeline(appModel->timeline()); });
        }
        return;
    }
    if (clip->clipType() == Clip::Audio) {
        const auto audioClip = static_cast<AudioClip *>(clip);
        insertAudioClip(audioClip, track, trackIndex);
    } else if (clip->clipType() == Clip::Singing) {
        const auto singingClip = static_cast<SingingClip *>(clip);
        insertSingingClip(singingClip, track, trackIndex);
    }
    connect(clip, &Clip::propertyChanged, this, [clip, this] { updateClipOnView(clip); });
}

void TrackEditorView::insertSingingClip(SingingClip *clip, TrackViewModel *track,
                                        const int trackIndex) {
    auto clipView = new SingingClipView(clip->id());
    clipView->loadCommonProperties(Clip::ClipCommonProperties(*clip));
    clipView->setTrackIndex(trackIndex);
    clipView->setColorIndex(track->dsTrack->colorIndex());
    const auto &notesRef = clip->notes();
    clipView->loadNotes(notesRef);
    updateSingingClipDisplay(clip, clipView);
    clipView->setDefaultLanguage(clip->defaultLanguage());
    m_tracksScene->addCommonItem(clipView);
    qDebug() << "Singing clip graphics item added to scene" << clipView->id() << clipView->name();
    connect(
        clip, &SingingClip::voiceContextChanged, this,
        [clip, clipView](const VoiceContextChange &) { updateSingingClipDisplay(clip, clipView); });
    connect(clip, &SingingClip::defaultLanguageChanged, clipView,
            &SingingClipView::setDefaultLanguage);
    connect(clip, &SingingClip::noteChanged, clipView, &SingingClipView::onNoteListChanged);
    track->clips[clip] = clipView;
}

namespace {
    void applyAudioPathStatus(AudioClipView *clipView, const AudioClip::PathStatus status) {
        if (status == AudioClip::PathStatus::Missing) {
            clipView->setStatus(AppGlobal::Error);
            clipView->setErrorMessage({});
        } else {
            clipView->setStatus(AppGlobal::Loaded);
        }
    }
}

void TrackEditorView::insertAudioClip(AudioClip *clip, TrackViewModel *track,
                                      const int trackIndex) {
    const auto clipView = new AudioClipView(clip->id());
    clipView->loadCommonProperties(Clip::ClipCommonProperties(*clip));
    clipView->setTrackIndex(trackIndex);
    clipView->setColorIndex(track->dsTrack->colorIndex());
    clipView->setPath(clip->path());
    clipView->setTimeline(appModel->timeline());
    clipView->setAudioInfo(clip->audioInfo());
    applyAudioPathStatus(clipView, clip->pathStatus());
    m_tracksScene->addCommonItem(clipView);
    qDebug() << "Audio clip graphics item added to scene" << clipView->id() << clipView->name();
    connect(appModel, &AppModel::timelineChanged, clipView,
            [clipView] { clipView->setTimeline(appModel->timeline()); });
    connect(
        clip, &AudioClip::pathStatusChanged, clipView,
        [clipView](const AudioClip::PathStatus status) { applyAudioPathStatus(clipView, status); });
    track->clips[clip] = clipView;
}

void TrackEditorView::onClipRemoved(Clip *clip, TrackViewModel *track) {
    if (!m_graphicsView)
        return;
    qInfo() << "removeClipFromView" << clip->id();
    disconnect(clip, nullptr, this, nullptr);
    const auto clipView = findClipItemById(clip->id());
    m_tracksScene->removeCommonItem(clipView);
    track->clips.remove(clip);
    m_pendingRemoveClipViews.insert(clip->id(), clipView);
    // Clean up if the view is not reused by a subsequent onClipInserted
    QTimer::singleShot(0, this, [this, clipId = clip->id()] {
        if (const auto cachedView = m_pendingRemoveClipViews.take(clipId))
            delete cachedView;
    });
}

AbstractClipView *TrackEditorView::findClipItemById(const int id) const {
    for (const auto &track : m_viewModel.tracks)
        for (const auto clip : track->clips)
            if (clip->id() == id)
                return clip;
    return nullptr;
}

void TrackEditorView::onTrackPropertyChanged() const {
    const auto tracksModel = appModel->tracks();
    for (int i = 0; i < m_viewModel.tracks.count(); i++) {
        const auto widget = m_viewModel.tracks.at(i)->controlView;
        const auto track = tracksModel.at(i);
        widget->setName(track->name());
        widget->setControl(track->control());
        widget->updateTrackColor();

        for (auto clipView : m_viewModel.tracks.at(i)->clips.values())
            clipView->setColorIndex(track->colorIndex());
    }
    if (m_rhiView)
        m_rhiView->scheduleSnapshot();
}

void TrackEditorView::updateClipOnView(Clip *clip) {
    if (!m_graphicsView) {
        if (m_rhiView)
            m_rhiView->scheduleSnapshot();
        return;
    }
    const auto item = findClipItemById(clip->id());
    item->setName(clip->name());
    item->setStart(clip->start());
    item->setClipStart(clip->clipStart());
    item->setLength(clip->length());
    item->setClipLen(clip->clipLen());

    if (clip->clipType() == Clip::Audio) {
        const auto audioClip = dynamic_cast<AudioClip *>(clip);
        const auto audioItem = dynamic_cast<AudioClipView *>(item);
        audioItem->setPath(audioClip->path());
        audioItem->setAudioInfo(audioClip->audioInfo());
    } else if (clip->clipType() == Clip::Singing) {
        const auto singingClip = dynamic_cast<SingingClip *>(clip);
        const auto singingItem = dynamic_cast<SingingClipView *>(item);
        singingItem->loadNotes(singingClip->notes());
    }
}

void TrackEditorView::onTrackRemoved(const Track *dsTrack, const qsizetype index) {
    // Ignore QListWidget's transient current-row changes while rows are being removed.
    const auto previousSelectedTrackIndex = static_cast<int>(appStatus->selectedTrackIndex);
    const QSignalBlocker listBlocker(m_trackListView);

    disconnect(dsTrack, nullptr, this, nullptr);
    // remove from view
    const auto trackVm = m_viewModel.tracks.at(index);
    auto keys = trackVm->clips.keys();
    for (const auto &key : keys)
        onClipRemoved(key, trackVm);
    const auto item = m_trackListView->takeItem(index);
    m_trackListView->removeItemWidget(item);
    delete item;
    // remove from view model
    m_viewModel.tracks.removeAt(index);
    delete trackVm;
    // update index
    if (index < m_viewModel.tracks.count()) // needs to update existed tracks' index
        for (int i = index; i < m_viewModel.tracks.count(); i++) {
            // Update track list items' index
            const auto widgetItem = m_trackListView->item(i);
            const auto widget = m_trackListView->itemWidget(widgetItem);
            const auto trackWidget = dynamic_cast<TrackControlView *>(widget);
            trackWidget->setTrackIndex(i + 1);
            // Update clips' index
            for (const auto &clipItem : m_viewModel.tracks.at(i)->clips) {
                clipItem->setTrackIndex(i);
            }
        }
    // Removing a row before the selection shifts it up; removing the selected row selects its
    // neighbor.
    const auto removedTrackIndex = static_cast<int>(index);
    auto selectedTrackIndex = previousSelectedTrackIndex;
    if (previousSelectedTrackIndex > removedTrackIndex)
        selectedTrackIndex = previousSelectedTrackIndex - 1;
    else if (previousSelectedTrackIndex == removedTrackIndex)
        selectedTrackIndex = m_viewModel.tracks.isEmpty()
                                 ? -1
                                 : qMin(removedTrackIndex, m_viewModel.tracks.count() - 1);

    setSelectedTrackIndex(selectedTrackIndex);
}
