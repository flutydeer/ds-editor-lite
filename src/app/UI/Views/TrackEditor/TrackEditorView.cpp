//
// Created by fluty on 2024/1/29.
//

#include "TrackEditorView.h"

#include "TrackControlView.h"
#include "TrackListHeaderView.h"
#include "TrackListView.h"
#include "TrackViewModel.h"
#include "TracksGraphicsScene.h"
#include "TracksGraphicsView.h"
#include "Controller/AppController.h"
#include "Controller/PlaybackController.h"
#include "Controller/TracksViewController.h"
#include "Global/TracksEditorGlobal.h"
#include "GraphicsItem/AudioClipView.h"
#include "GraphicsItem/SingingClipView.h"
#include "GraphicsItem/TracksBackgroundGraphicsItem.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Audio/AudioContext.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/LevelMeter.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Views/Common/TimelineView.h"

#include <QFileDialog>
#include <QMouseEvent>
#include <QScrollBar>
#include <QVBoxLayout>

TrackEditorView::TrackEditorView(QWidget *parent) : PanelView(AppGlobal::TracksEditor, parent) {
    trackController->setParentWidget(this);
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("TracksView");

    m_trackListView = new TrackListView;

    m_tracksScene = new TracksGraphicsScene;
    m_graphicsView = new TracksGraphicsView(m_tracksScene);
    m_graphicsView->centerOn(0, 0);
    m_graphicsView->setSceneLength(appStatus->projectEditableLength);
    m_gridItem = new TracksBackgroundGraphicsItem;
    m_gridItem->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    m_tracksScene->addTimeGrid(m_gridItem);
    m_trackListView->setGraphicsView(m_graphicsView);

    m_timeline = new TimelineView;
    m_timeline->setObjectName("tracksTimelineView");
    m_timeline->setTimeRange(m_graphicsView->startTick(), m_graphicsView->endTick());
    m_timeline->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    m_timeline->setFixedHeight(TracksEditorGlobal::trackViewHeaderHeight);

    auto gBar = m_graphicsView->verticalScrollBar();
    auto lBar = m_trackListView->verticalScrollBar();

    // auto splitter = new QSplitter;
    // splitter->setOrientation(Qt::Horizontal);
    // splitter->addWidget(tracklist);
    // splitter->addWidget(m_graphicsView);

    auto trackListPanelLayout = new QVBoxLayout;
    trackListPanelLayout->addWidget(new TrackListHeaderView);
    trackListPanelLayout->addWidget(m_trackListView);

    auto trackTimelineAndViewLayout = new QVBoxLayout;
    trackTimelineAndViewLayout->addWidget(m_timeline);
    trackTimelineAndViewLayout->addWidget(m_graphicsView);

    auto mainLayout = new QHBoxLayout;
    mainLayout->setSpacing(0);
    // layout->addWidget(splitter);
    mainLayout->addLayout(trackListPanelLayout);
    mainLayout->addLayout(trackTimelineAndViewLayout);
    mainLayout->setContentsMargins({1, 1, 1, 1});

    setLayout(mainLayout);
    setPanelActive(true);
    appController->registerPanel(this);
    installEventFilter(this);

    connect(m_trackListView, &QListWidget::currentRowChanged, appController,
            &AppController::selectTrack);
    connect(m_graphicsView, &TracksGraphicsView::scaleChanged, this,
            &TrackEditorView::onViewScaleChanged);
    connect(m_graphicsView, &TracksGraphicsView::sizeChanged, m_tracksScene,
            &TracksGraphicsScene::onViewResized);
    connect(this, &TrackEditorView::trackCountChanged, m_tracksScene,
            &TracksGraphicsScene::onTrackCountChanged);
    connect(this, &TrackEditorView::trackCountChanged, m_gridItem,
            &TracksBackgroundGraphicsItem::onTrackCountChanged);
    connect(appModel, &AppModel::selectedTrackChanged, m_gridItem,
            &TracksBackgroundGraphicsItem::onTrackSelectionChanged);
    connect(m_timeline, &TimelineView::wheelHorScale, m_graphicsView,
            &TracksGraphicsView::onWheelHorScale);
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, m_timeline,
            &TimelineView::setTimeRange);
    connect(gBar, &QScrollBar::valueChanged, lBar, &QScrollBar::setValue);
    // connect(lBar, &QScrollBar::valueChanged, gBar, &QScrollBar::setValue);

    connect(playbackController, &PlaybackController::positionChanged, this,
            &TrackEditorView::onPositionChanged);
    connect(playbackController, &PlaybackController::lastPositionChanged, this,
            &TrackEditorView::onLastPositionChanged);
    connect(AudioContext::instance(), &AudioContext::levelMeterUpdated, this,
            &TrackEditorView::onLevelMetersUpdated);
    connect(appModel, &AppModel::modelChanged, this, &TrackEditorView::onModelChanged);
    connect(appModel, &AppModel::trackChanged, this, &TrackEditorView::onTrackChanged);

    connect(appStatus, &AppStatus::projectEditableLengthChanged, m_graphicsView,
            &TracksGraphicsView::setSceneLength);
}

void TrackEditorView::onModelChanged() {
    if (m_tracksScene == nullptr)
        return;

    reset();
    int index = 0;
    for (const auto track : appModel->tracks()) {
        insertTrackToView(track, index);
        index++;
    }
    emit trackCountChanged(m_trackListViewModel.tracks.count());
}

void TrackEditorView::onTrackChanged(AppModel::TrackChangeType type, int index) {
    if (type == AppModel::Insert) {
        insertTrackToView(appModel->tracks().at(index), index);
    } else if (type == AppModel::Remove) {
        trackController->setActiveClip(-1);
        removeTrackFromView(index);
    }
    emit trackCountChanged(m_trackListViewModel.tracks.count());
}

void TrackEditorView::onClipChanged(Track::ClipChangeType type, Clip *clip) {
    if (type == Track::Inserted) {
        int trackIndex;
        appModel->findClipById(clip->id(), trackIndex);
        auto track = m_trackListViewModel.tracks.at(trackIndex);
        insertClipToTrack(clip, track, trackIndex);
        connect(clip, &Clip::propertyChanged, this, [=] { updateClipOnView(clip); });
    } else if (type == Track::Removed) {
        removeClipFromView(clip->id());
    }
    updateOverlappedState();
}

void TrackEditorView::onPositionChanged(double tick) {
    m_graphicsView->setPlaybackPosition(tick);
}

void TrackEditorView::onLastPositionChanged(double tick) {
    m_graphicsView->setLastPlaybackPosition(tick);
}

void TrackEditorView::onLevelMetersUpdated(const AppModel::LevelMetersUpdatedArgs &args) const {
    if (m_trackListViewModel.tracks.isEmpty())
        return;

    auto states = args.trackMeterStates;
    for (int i = 0; i < qMin(states.size(), m_trackListViewModel.tracks.size()); i++) {
        auto state = states.at(i);
        auto meter = m_trackListViewModel.tracks.at(i)->controlView->levelMeter();
        meter->setValue(state.valueL, state.valueR);
    }
}

// void TrackEditorView::onSceneSelectionChanged() const {
//     // find selected clip (the first one)
//     bool foundSelectedClip = false;
//     for (int i = 0; i < m_trackListViewModel.tracks.count(); i++) {
//         auto track = m_trackListViewModel.tracks.at(i);
//         for (int j = 0; j < track->clips.count(); j++) {
//             auto clip = track->clips.at(j);
//             if (clip->isSelected()) {
//                 foundSelectedClip = true;
//                 trackController->selectClip(clip->id());
//                 break;
//             }
//         }
//         if (foundSelectedClip)
//             break;
//     }
//     if (!foundSelectedClip)
//         trackController->selectClip(-1);
// }
void TrackEditorView::onViewScaleChanged(qreal sx, qreal sy) const {
    int previousHeightSum = 0;
    for (int i = 0; i < m_trackListView->count(); i++) {
        // adjust track item height
        auto item = m_trackListView->item(i);
        int height = qRound((i + 1) * TracksEditorGlobal::trackHeight * sy - previousHeightSum);
        item->setSizeHint(QSize(TracksEditorGlobal::trackListWidth, height));

        // hide pan and gain slider when sy is too small
        auto widget = m_trackListViewModel.tracks.at(i)->controlView;
        widget->setNarrowMode(sy < TracksEditorGlobal::narrowModeScaleY);
        previousHeightSum += height;
    }
}

void TrackEditorView::onRemoveTrackTriggered(int id) {
    auto track = appModel->findTrackById(id);
    auto dlg = new Dialog(this);
    dlg->setWindowTitle(tr("Warning"));
    dlg->setTitle(tr("Do you want to delete this track?"));
    dlg->setMessage(track->name());
    dlg->setModal(true);

    auto btnDelete = new Button(tr("Delete"));
    connect(btnDelete, &Button::clicked, dlg, &Dialog::accept);
    dlg->setNegativeButton(btnDelete);

    auto btnCancel = new AccentButton(tr("Cancel"));
    connect(btnCancel, &Button::clicked, dlg, &Dialog::reject);
    dlg->setPositiveButton(btnCancel);

    connect(dlg, &Dialog::accepted, this, [=] { trackController->onRemoveTrack(id); });

    dlg->show();
}

bool TrackEditorView::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QMouseEvent::MouseButtonPress) {
        // qDebug() << "TracksView MouseButtonPress";
        appController->onPanelClicked(AppGlobal::TracksEditor);
    }

    return QWidget::eventFilter(watched, event);
}

TrackViewModel *TrackEditorView::TrackListViewModel::findTrackById(int id) {
    for (const auto track : tracks) {
        if (track->id() == id)
            return track;
    }
    return nullptr;
}

void TrackEditorView::insertTrackToView(Track *dsTrack, int trackIndex) {
    connect(dsTrack, &Track::propertyChanged, this, [=] { updateTracksOnView(); });
    connect(dsTrack, &Track::clipChanged, this, &TrackEditorView::onClipChanged);

    auto track = new TrackViewModel(dsTrack->id());
    for (auto clip : dsTrack->clips()) {
        insertClipToTrack(clip, track, trackIndex);
    }
    auto newTrackItem = new QListWidgetItem;
    auto controlView = new TrackControlView(newTrackItem, dsTrack);
    newTrackItem->setSizeHint(
        QSize(TracksEditorGlobal::trackListWidth,
              static_cast<int>(TracksEditorGlobal::trackHeight * m_graphicsView->scaleY())));
    controlView->setTrackIndex(trackIndex + 1);
    controlView->setNarrowMode(m_graphicsView->scaleY() < TracksEditorGlobal::narrowModeScaleY);
    m_trackListView->insertItem(trackIndex, newTrackItem);
    m_trackListView->setItemWidget(newTrackItem, controlView);
    track->controlView = controlView;

    connect(controlView, &TrackControlView::insertNewTrackTriggered, this, [=] {
        auto i = m_trackListView->row(newTrackItem);
        trackController->onInsertNewTrack(i + 1); // insert after current track
    });
    connect(controlView, &TrackControlView::removeTrackTriggered, this,
            &TrackEditorView::onRemoveTrackTriggered);
    m_trackListViewModel.tracks.insert(trackIndex, track);
    if (trackIndex < m_trackListViewModel.tracks.count()) // needs to update existed tracks' index
        for (int i = trackIndex + 1; i < m_trackListViewModel.tracks.count(); i++) {
            // Update track list items' index
            auto item = m_trackListView->item(i);
            auto widget = m_trackListView->itemWidget(item);
            auto trackWidget = dynamic_cast<TrackControlView *>(widget);
            trackWidget->setTrackIndex(i + 1);
            // Update clips' index
            for (auto &clipItem : m_trackListViewModel.tracks.at(i)->clips) {
                clipItem->setTrackIndex(i);
            }
        }
}

void TrackEditorView::insertClipToTrack(Clip *clip, TrackViewModel *track, int trackIndex) {
    if (clip->clipType() == Clip::Audio) {
        auto audioClip = reinterpret_cast<AudioClip *>(clip);
        insertAudioClip(audioClip, track, trackIndex);
    } else if (clip->clipType() == Clip::Singing) {
        auto singingClip = reinterpret_cast<SingingClip *>(clip);
        insertSingingClip(singingClip, track, trackIndex);
    }
    connect(clip, &Clip::propertyChanged, this, [=] { updateClipOnView(clip); });
}

void TrackEditorView::insertSingingClip(SingingClip *clip, TrackViewModel *track, int trackIndex) {
    auto clipView = new SingingClipView(clip->id());
    clipView->loadCommonProperties(Clip::ClipCommonProperties(*clip));
    clipView->setTrackIndex(trackIndex);
    const auto &notesRef = clip->notes();
    clipView->loadNotes(notesRef);
    clipView->setDefaultLanguage(clip->defaultLanguage);
    clipView->setOverlapped(clip->overlapped());
    m_tracksScene->addCommonItem(clipView);
    qDebug() << "Singing clip graphics item added to scene" << clipView->id() << clipView->name();
    connect(clip, &SingingClip::defaultLanguageChanged, clipView,
            &SingingClipView::setDefaultLanguage);
    connect(clip, &SingingClip::noteChanged, clipView, &SingingClipView::onNoteListChanged);
    connect(appModel, &AppModel::quantizeChanged, clipView, &AbstractClipView::setQuantize);
    track->clips.append(clipView);
}

void TrackEditorView::insertAudioClip(AudioClip *clip, TrackViewModel *track, int trackIndex) {
    auto clipItem = new AudioClipView(clip->id());
    clipItem->loadCommonProperties(Clip::ClipCommonProperties(*clip));
    clipItem->setTrackIndex(trackIndex);
    clipItem->setPath(clip->path());
    clipItem->setTempo(appModel->tempo());
    clipItem->setOverlapped(clip->overlapped());
    clipItem->setAudioInfo(clip->audioInfo());
    m_tracksScene->addCommonItem(clipItem);
    qDebug() << "Audio clip graphics item added to scene" << clipItem->id() << clipItem->name();
    connect(appModel, &AppModel::tempoChanged, clipItem, &AudioClipView::onTempoChange);
    connect(appModel, &AppModel::quantizeChanged, clipItem, &AbstractClipView::setQuantize);
    track->clips.append(clipItem);
}

void TrackEditorView::removeClipFromView(int clipId) {
    auto clipView = findClipItemById(clipId);
    m_tracksScene->removeCommonItem(clipView);
    for (const auto &track : m_trackListViewModel.tracks) {
        if (track->clips.contains(clipView)) {
            track->clips.removeOne(clipView);
            break;
        }
    }
    // delete clipView;
}

AbstractClipView *TrackEditorView::findClipItemById(int id) {
    for (const auto &track : m_trackListViewModel.tracks)
        for (const auto clip : track->clips)
            if (clip->id() == id)
                return clip;
    return nullptr;
}

void TrackEditorView::updateTracksOnView() const {
    auto tracksModel = appModel->tracks();
    for (int i = 0; i < m_trackListViewModel.tracks.count(); i++) {
        auto widget = m_trackListViewModel.tracks.at(i)->controlView;
        auto track = tracksModel.at(i);
        widget->setName(track->name());
        widget->setControl(track->control());
    }
}

void TrackEditorView::updateClipOnView(Clip *clip) {
    // qDebug() << "TracksView::updateClipOnView" << clipId;
    auto item = findClipItemById(clip->id());
    item->setName(clip->name());
    item->setStart(clip->start());
    item->setClipStart(clip->clipStart());
    item->setLength(clip->length());
    item->setClipLen(clip->clipLen());
    item->setOverlapped(clip->overlapped());

    if (clip->clipType() == Clip::Audio) {
        auto audioClip = dynamic_cast<AudioClip *>(clip);
        auto audioItem = dynamic_cast<AudioClipView *>(item);
        audioItem->setPath(audioClip->path());
        audioItem->setAudioInfo(audioClip->audioInfo());
    } else if (clip->clipType() == Clip::Singing) {
        auto singingClip = dynamic_cast<SingingClip *>(clip);
        auto singingItem = dynamic_cast<SingingClipView *>(item);
        singingItem->loadNotes(singingClip->notes());
    }

    updateOverlappedState();
}

void TrackEditorView::removeTrackFromView(int index) {
    // disconnect(m_tracksScene, &TracksGraphicsScene::selectionChanged, this,
    //            &TrackEditorView::onSceneSelectionChanged);
    // remove from view
    auto track = m_trackListViewModel.tracks.at(index);
    for (auto clip : track->clips) {
        m_tracksScene->removeCommonItem(clip);
        delete clip;
    }
    auto item = m_trackListView->takeItem(index);
    m_trackListView->removeItemWidget(item);
    // remove from viewmodel
    m_trackListViewModel.tracks.removeAt(index);
    // update index
    if (index < m_trackListViewModel.tracks.count()) // needs to update existed tracks' index
        for (int i = index; i < m_trackListViewModel.tracks.count(); i++) {
            // Update track list items' index
            auto item = m_trackListView->item(i);
            auto widget = m_trackListView->itemWidget(item);
            auto trackWidget = dynamic_cast<TrackControlView *>(widget);
            trackWidget->setTrackIndex(i + 1);
            // Update clips' index
            for (auto &clipItem : m_trackListViewModel.tracks.at(i)->clips) {
                clipItem->setTrackIndex(i);
            }
        }
    // connect(m_tracksScene, &TracksGraphicsScene::selectionChanged, this,
    //         &TrackEditorView::onSceneSelectionChanged);
}

void TrackEditorView::updateOverlappedState() {
    for (const auto trackModel : appModel->tracks()) {
        auto track = m_trackListViewModel.findTrackById(trackModel->id());
        for (auto clipItem : track->clips) {
            auto dsClip = trackModel->findClipById(clipItem->id());
            clipItem->setOverlapped(dsClip->overlapped());
        }
    }
    m_graphicsView->update();
}

void TrackEditorView::reset() {
    // disconnect(m_tracksScene, &TracksGraphicsScene::selectionChanged, this,
    //         &TrackEditorView::onSceneSelectionChanged);
    for (auto &track : m_trackListViewModel.tracks)
        for (auto clip : track->clips) {
            m_tracksScene->removeCommonItem(clip);
            delete clip;
        }
    m_trackListView->clear();
    m_trackListViewModel.tracks.clear();
    // connect(m_tracksScene, &TracksGraphicsScene::selectionChanged, this,
    //         &TrackEditorView::onSceneSelectionChanged);
}