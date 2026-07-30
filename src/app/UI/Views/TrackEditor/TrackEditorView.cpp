//
// Created by fluty on 2024/1/29.
//

#include "TrackEditorView.h"

#include "TrackControlView.h"
#include "TrackListHeaderView.h"
#include "TrackListView.h"
#include "ITrackEditorCanvas.h"
#include "TrackViewModel.h"
#include "Controller/EditorViewController.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Global/TracksEditorGlobal.h"
#include "InfoLane/InfoLaneHeaderView.h"
#include "InfoLane/TempoLaneView.h"
#include "InfoLane/TimeSignatureLaneView.h"
#include "Model/AppStatus/AppStatus.h"
#include <lite/GUI/Controls/LevelMeter.h>
#include <lite/GUI/Controls/LevelMeterViewModel.h>
#include "UI/Controls/LevelMeterManager.h"
#include "AppContext.h"
#include "UI/Views/Common/TimelineView.h"
#include "UI/Views/EditorCanvas/EditorCanvasFactory.h"

#include <QFileDialog>
#include <QMouseEvent>
#include <QLoggingCategory>
#include <QScrollBar>
#include <QSignalBlocker>
#include <lite/GUI/Controls/OverlaySplitter.h>

#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

Q_LOGGING_CATEGORY(lcTrackEditorCanvas, "ds.editor.canvas.track")

TrackEditorView::TrackEditorView(QWidget *parent) : PanelView(AppGlobal::TracksEditor, parent) {
    trackController->setParentWidget(this);
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("TracksView");

    m_trackListView = new TrackListView;

    m_canvas = EditorCanvasFactory::createTrackCanvas(this);
    m_canvas->setSceneLength(appStatus->projectEditableLength);
    m_trackListView->setCanvas(m_canvas);

    m_timeline = new TimelineView;
    m_timeline->setObjectName("tracksTimelineView");
    const auto initialViewport = m_canvas->viewportState();
    m_timeline->setTimeRange(initialViewport.startTick, initialViewport.endTick);
    m_timeline->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    m_timeline->setQuantize(128);
    m_timeline->setFixedHeight(TracksEditorGlobal::trackViewHeaderHeight);
    m_timeline->setCanEditLoop(true); // Enable loop editing in track editor

    m_tempoLane = new TempoLaneView;
    m_tempoLane->setFixedHeight(TracksEditorGlobal::infoLaneHeight);
    m_tempoLane->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    m_tempoLane->setQuantize(128);
    m_tempoLane->setTimeRange(initialViewport.startTick, initialViewport.endTick);

    m_tempoLaneHeader = new InfoLaneHeaderView;
    m_tempoLaneHeader->setObjectName("tempoLaneHeaderView");
    m_tempoLaneHeader->setTitle(tr("Tempo"));
    m_tempoLaneHeader->setFixedHeight(TracksEditorGlobal::infoLaneHeight);

    m_timeSignatureLane = new TimeSignatureLaneView;
    m_timeSignatureLane->setFixedHeight(TracksEditorGlobal::infoLaneHeight);
    m_timeSignatureLane->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    m_timeSignatureLane->setQuantize(128);
    m_timeSignatureLane->setTimeRange(initialViewport.startTick, initialViewport.endTick);

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
    m_trackTimelineAndViewLayout->addWidget(m_canvas->widget());

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
    connectCanvas();
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

    connect(appStatus, &AppStatus::projectEditableLengthChanged, this,
            [this](const int tick) { m_canvas->setSceneLength(tick); });
}

void TrackEditorView::connectCanvas() {
    m_trackListView->setCanvas(m_canvas);
    connect(m_canvas, &ITrackEditorCanvas::scaleChanged, this,
            &TrackEditorView::onViewScaleChanged);
    connect(m_canvas, &ITrackEditorCanvas::timeRangeChanged, m_timeline,
            &TimelineView::setTimeRange);
    connect(m_canvas, &ITrackEditorCanvas::timeRangeChanged, m_tempoLane,
            &InfoLaneView::setTimeRange);
    connect(m_canvas, &ITrackEditorCanvas::timeRangeChanged, m_timeSignatureLane,
            &InfoLaneView::setTimeRange);
    connect(m_canvas, &ITrackEditorCanvas::rendererFailed, this,
            &TrackEditorView::scheduleLegacyFallback);

    connect(m_timeline, &TimelineView::wheelHorScale, m_canvas,
            &ITrackEditorCanvas::onWheelHorScale);
    connect(m_tempoLane, &InfoLaneView::wheelHorScale, m_canvas,
            &ITrackEditorCanvas::onWheelHorScale);
    connect(m_tempoLane, &InfoLaneView::wheelHorScroll, m_canvas,
            &ITrackEditorCanvas::onWheelHorScroll);
    connect(m_tempoLane, &InfoLaneView::wheelVerScroll, m_canvas,
            &ITrackEditorCanvas::onWheelVerScroll);
    connect(m_timeSignatureLane, &InfoLaneView::wheelHorScale, m_canvas,
            &ITrackEditorCanvas::onWheelHorScale);
    connect(m_timeSignatureLane, &InfoLaneView::wheelHorScroll, m_canvas,
            &ITrackEditorCanvas::onWheelHorScroll);
    connect(m_timeSignatureLane, &InfoLaneView::wheelVerScroll, m_canvas,
            &ITrackEditorCanvas::onWheelVerScroll);

    const auto canvasBar = m_canvas->verticalScrollBar();
    const auto listBar = m_trackListView->verticalScrollBar();
    connect(canvasBar, &QScrollBar::valueChanged, listBar, &QScrollBar::setValue);
    connect(listBar, &QScrollBar::valueChanged, canvasBar, &QScrollBar::setValue);

    for (const auto *track : m_viewModel.tracks) {
        connect(m_canvas, &ITrackEditorCanvas::scaleChanged, track->controlView,
                &TrackControlView::finishTrackNameEditing);
        connect(m_canvas, &ITrackEditorCanvas::visibleRectChanged, track->controlView,
                &TrackControlView::finishTrackNameEditing);
        connect(m_canvas, &ITrackEditorCanvas::sizeChanged, track->controlView,
                &TrackControlView::finishTrackNameEditing);
    }
}

void TrackEditorView::scheduleLegacyFallback(const QString &reason) {
    if (m_canvas->backend() != EditorCanvasBackend::ExperimentalRhi || m_fallbackPending)
        return;
    m_fallbackPending = true;
    qCWarning(lcTrackEditorCanvas) << "RHI canvas failed; falling back to Legacy:" << reason;
    QTimer::singleShot(0, this, &TrackEditorView::replaceCanvasWithLegacy);
}

void TrackEditorView::replaceCanvasWithLegacy() {
    const auto viewport = m_canvas->viewportState();
    auto *failedCanvas = m_canvas;
    auto *failedWidget = failedCanvas->widget();
    m_trackTimelineAndViewLayout->removeWidget(failedWidget);
    m_trackListView->setCanvas(nullptr);
    delete failedCanvas;
    delete failedWidget;

    m_canvas = EditorCanvasFactory::createTrackCanvas(EditorCanvasBackend::Legacy, this);
    m_canvas->setSceneLength(appStatus->projectEditableLength);
    m_trackTimelineAndViewLayout->insertWidget(3, m_canvas->widget());
    connectCanvas();
    m_canvas->restoreViewportState(viewport);
    onViewScaleChanged(viewport.horizontalScale, viewport.verticalScale);
    m_fallbackPending = false;
}

TrackEditorView::~TrackEditorView() {
    editorViewController->unregisterPanel(this);
}

void TrackEditorView::changeEvent(QEvent *event) {
    PanelView::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        m_tempoLaneHeader->setTitle(tr("Tempo"));
        m_timeSignatureLaneHeader->setTitle(tr("Time Signature"));
    }
}

TrackPanelViewState TrackEditorView::viewState() const {
    const auto state = m_canvas->viewportState();
    return {
        .centerTick = state.centerTick,
        .centerTrackIndex = state.centerTrack,
        .horizontalScale = state.horizontalScale,
        .verticalScale = state.verticalScale,
    };
}

bool TrackEditorView::centerAt(const double tick, const double trackIndex) const {
    return m_canvas->centerAt(tick, trackIndex);
}

bool TrackEditorView::setViewScale(const double horizontalScale, const double verticalScale) const {
    const auto previousState = viewState();
    if (!m_canvas->setViewportScale(horizontalScale, verticalScale))
        return false;
    return centerAt(previousState.centerTick, previousState.centerTrackIndex);
}

HistoryFocusVisibility TrackEditorView::focusVisibility(const HistoryFocus &focus) const {
    return m_canvas->focusVisibility(focus);
}

bool TrackEditorView::revealFocus(const HistoryFocus &focus) const {
    return revealFocus(focus, true);
}

bool TrackEditorView::revealFocus(const HistoryFocus &focus, const bool animated) const {
    return m_canvas->revealFocus(focus, animated);
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
    m_canvas->refreshSnapshot(EditorDirtyDomain::All);
}

void TrackEditorView::onTrackChanged(const AppModel::TrackChangeType type, const qsizetype index,
                                     Track *track) {
    if (type == AppModel::Insert)
        onTrackInserted(track, index);
    else if (type == AppModel::Remove)
        onTrackRemoved(track, index);
    emit trackCountChanged(m_viewModel.tracks.count());
    m_canvas->refreshSnapshot(EditorDirtyDomain::Geometry);
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
    m_canvas->refreshSnapshot(EditorDirtyDomain::Geometry);
}

void TrackEditorView::onPositionChanged(const double tick) const {
    m_canvas->setPlaybackPosition(tick);
}

void TrackEditorView::onLastPositionChanged(const double tick) const {
    m_canvas->setLastPlaybackPosition(tick);
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

    const auto track = new TrackViewModel(dsTrack);
    auto newTrackItem = new QListWidgetItem;
    const auto controlView = new TrackControlView(newTrackItem, dsTrack);
    controlView->setTrackNameOverlayParent(m_trackListView->viewport());
    connect(m_canvas, &ITrackEditorCanvas::scaleChanged, controlView,
            &TrackControlView::finishTrackNameEditing);
    connect(m_canvas, &ITrackEditorCanvas::visibleRectChanged, controlView,
            &TrackControlView::finishTrackNameEditing);
    connect(m_canvas, &ITrackEditorCanvas::sizeChanged, controlView,
            &TrackControlView::finishTrackNameEditing);
    connect(m_trackListView->verticalScrollBar(), &QScrollBar::valueChanged, controlView,
            &TrackControlView::finishTrackNameEditing);
    const auto scaleY = m_canvas->viewportState().verticalScale;
    newTrackItem->setSizeHint(QSize(0, static_cast<int>(TracksEditorGlobal::trackHeight * scaleY)));
    controlView->setTrackIndex(trackIndex + 1);
    controlView->setNarrowMode(scaleY < TracksEditorGlobal::narrowModeScaleY);
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
        }

    // An insertion before the selected row moves the same logical track down by one row.
    if (previousSelectedTrackIndex >= 0 &&
        static_cast<int>(trackIndex) <= previousSelectedTrackIndex)
        setSelectedTrackIndex(previousSelectedTrackIndex + 1);
    else
        syncSelectedTrackToList(previousSelectedTrackIndex);
    m_canvas->refreshSnapshot(EditorDirtyDomain::Geometry);
}

void TrackEditorView::onTrackPropertyChanged() const {
    const auto tracksModel = appModel->tracks();
    for (int i = 0; i < m_viewModel.tracks.count(); i++) {
        const auto widget = m_viewModel.tracks.at(i)->controlView;
        const auto track = tracksModel.at(i);
        widget->setName(track->name());
        widget->setControl(track->control());
        widget->updateTrackColor();
    }
    m_canvas->refreshSnapshot(EditorDirtyDomain::Style | EditorDirtyDomain::Geometry);
}

void TrackEditorView::onTrackRemoved(const Track *dsTrack, const qsizetype index) {
    // Ignore QListWidget's transient current-row changes while rows are being removed.
    const auto previousSelectedTrackIndex = static_cast<int>(appStatus->selectedTrackIndex);
    const QSignalBlocker listBlocker(m_trackListView);

    disconnect(dsTrack, nullptr, this, nullptr);
    // remove from view
    const auto trackVm = m_viewModel.tracks.at(index);
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
    m_canvas->refreshSnapshot(EditorDirtyDomain::Geometry);
}
