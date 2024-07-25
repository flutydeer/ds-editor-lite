//
// Created by fluty on 2024/1/29.
//

#include "TrackEditorView.h"

#include "TrackControlWidget.h"
#include "TrackListHeaderView.h"
#include "TrackListWidget.h"
#include "TrackViewModel.h"
#include "TracksGraphicsScene.h"
#include "TracksGraphicsView.h"
#include "Controller/AppController.h"
#include "Controller/PlaybackController.h"
#include "Controller/TracksViewController.h"
#include "Global/TracksEditorGlobal.h"
#include "GraphicsItem/AudioClipGraphicsItem.h"
#include "GraphicsItem/SingingClipGraphicsItem.h"
#include "GraphicsItem/TracksBackgroundGraphicsItem.h"
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

    m_trackListWidget = new TrackListWidget;
    connect(m_trackListWidget, &QListWidget::currentRowChanged, appController,
            &AppController::onTrackSelectionChanged);

    m_tracksScene = new TracksGraphicsScene;
    m_graphicsView = new TracksGraphicsView(m_tracksScene);
    // QScroller::grabGesture(m_graphicsView, QScroller::TouchGesture);
    m_graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_graphicsView->setEnsureSceneFillView(false);
    m_graphicsView->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    connect(m_graphicsView, &TracksGraphicsView::scaleChanged, this,
            &TrackEditorView::onViewScaleChanged);
    m_graphicsView->centerOn(0, 0);
    // connect(m_graphicsView, &TracksGraphicsView::resized, m_tracksScene,
    //         &TracksGraphicsScene::onGraphicsViewResized);
    connect(m_graphicsView, &TracksGraphicsView::sizeChanged, m_tracksScene,
            &TracksGraphicsScene::onViewResized);
    connect(this, &TrackEditorView::trackCountChanged, m_tracksScene,
            &TracksGraphicsScene::onTrackCountChanged);
    connect(m_tracksScene, &TracksGraphicsScene::selectionChanged, this,
            &TrackEditorView::onSceneSelectionChanged);
    connect(m_graphicsView, &TracksGraphicsView::addSingingClipTriggered, this,
            [=](int trackIndex, int tick) { emit newSingingClipTriggered(trackIndex, tick); });
    connect(m_graphicsView, &TracksGraphicsView::addAudioClipTriggered, this,
            [=](int trackIndex, int tick) {
                auto fileName =
                    QFileDialog::getOpenFileName(this, "Select an Audio File", ".",
                                                 "All Audio File (*.wav *.flac *.mp3);;Wave File "
                                                 "(*.wav);;Flac File (*.flac);;MP3 File (*.mp3)");
                if (fileName.isNull())
                    return;
                emit addAudioClipTriggered(fileName, trackIndex, tick);
            });

    m_gridItem = new TracksBackgroundGraphicsItem;
    m_gridItem->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    connect(this, &TrackEditorView::trackCountChanged, m_gridItem,
            &TracksBackgroundGraphicsItem::onTrackCountChanged);
    connect(appModel, &AppModel::modelChanged, m_gridItem, [=] {
        m_gridItem->setTimeSignature(appModel->timeSignature().numerator,
                                     appModel->timeSignature().denominator);
        m_gridItem->setQuantize(appModel->quantize());
    });
    connect(appModel, &AppModel::timeSignatureChanged, m_gridItem,
            &TimeGridGraphicsItem::setTimeSignature);
    connect(appModel, &AppModel::quantizeChanged, m_gridItem, &TimeGridGraphicsItem::setQuantize);
    connect(appModel, &AppModel::selectedTrackChanged, m_gridItem,
            &TracksBackgroundGraphicsItem::onTrackSelectionChanged);
    m_tracksScene->addTimeGrid(m_gridItem);

    m_timeline = new TimelineView;
    m_timeline->setObjectName("tracksTimelineView");
    m_timeline->setTimeRange(m_graphicsView->startTick(), m_graphicsView->endTick());
    m_timeline->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    connect(m_timeline, &TimelineView::wheelHorScale, m_graphicsView,
            &TracksGraphicsView::onWheelHorScale);
    connect(m_timeline, &TimelineView::setLastPositionTriggered, playbackController,
            [=](double tick) {
                playbackController->setLastPosition(tick);
                playbackController->setPosition(tick);
            });
    connect(appModel, &AppModel::modelChanged, m_timeline, [=] {
        m_timeline->setTimeSignature(appModel->timeSignature().numerator,
                                     appModel->timeSignature().denominator);
    });
    connect(appModel, &AppModel::timeSignatureChanged, m_timeline, &TimelineView::setTimeSignature);
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, m_timeline,
            &TimelineView::setTimeRange);
    connect(appModel, &AppModel::quantizeChanged, m_timeline, &TimelineView::setQuantize);

    auto gBar = m_graphicsView->verticalScrollBar();
    auto lBar = m_trackListWidget->verticalScrollBar();
    connect(gBar, &QScrollBar::valueChanged, lBar, &QScrollBar::setValue);
    connect(lBar, &QScrollBar::valueChanged, gBar, &QScrollBar::setValue);

    connect(playbackController, &PlaybackController::positionChanged, this,
            &TrackEditorView::onPositionChanged);
    connect(playbackController, &PlaybackController::lastPositionChanged, this,
            &TrackEditorView::onLastPositionChanged);
    //    connect(playbackController, &PlaybackController::levelMetersUpdated, this,
    //            &TracksView::onLevelMetersUpdated);
    connect(AudioContext::instance(), &AudioContext::levelMeterUpdated, this,
            &TrackEditorView::onLevelMetersUpdated);

    // auto splitter = new QSplitter;
    // splitter->setOrientation(Qt::Horizontal);
    // splitter->addWidget(tracklist);
    // splitter->addWidget(m_graphicsView);

    m_timeline->setFixedHeight(TracksEditorGlobal::trackViewHeaderHeight);

    auto trackListHeaderView = new TrackListHeaderView;
    trackListHeaderView->setFixedHeight(TracksEditorGlobal::trackViewHeaderHeight);

    auto trackListPanelLayout = new QVBoxLayout;
    trackListPanelLayout->addWidget(trackListHeaderView);
    trackListPanelLayout->addWidget(m_trackListWidget);

    auto trackTimelineAndViewLayout = new QVBoxLayout;
    trackTimelineAndViewLayout->addWidget(m_timeline);
    trackTimelineAndViewLayout->addWidget(m_graphicsView);

    auto mainLayout = new QHBoxLayout;
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({});
    // layout->addWidget(splitter);
    mainLayout->addLayout(trackListPanelLayout);
    mainLayout->addLayout(trackTimelineAndViewLayout);
    mainLayout->setContentsMargins({1, 1, 1, 1});

    setLayout(mainLayout);
    setPanelActivated(true);
    appController->registerPanel(this);
    installEventFilter(this);


    connect(appModel, &AppModel::modelChanged, this, &TrackEditorView::onModelChanged);
    connect(appModel, &AppModel::trackChanged, this, &TrackEditorView::onTrackChanged);
    connect(appModel, &AppModel::tempoChanged, this, &TrackEditorView::onTempoChanged);

    connect(this, &TrackEditorView::selectedClipChanged, trackController,
            &TracksViewController::onSelectedClipChanged);
    connect(this, &TrackEditorView::insertNewTrackTriggered, trackController,
            &TracksViewController::onInsertNewTrack);
    connect(this, &TrackEditorView::removeTrackTriggered, trackController,
            &TracksViewController::onRemoveTrack);
    connect(this, &TrackEditorView::addAudioClipTriggered, trackController,
            &TracksViewController::onAddAudioClip);
    connect(this, &TrackEditorView::newSingingClipTriggered, trackController,
            &TracksViewController::onNewSingingClip);
    connect(this, &TrackEditorView::clipPropertyChanged, trackController,
            &TracksViewController::onClipPropertyChanged);
    connect(this, &TrackEditorView::removeClipTriggered, trackController,
            &TracksViewController::onRemoveClip);
}
void TrackEditorView::onModelChanged() {
    if (m_tracksScene == nullptr)
        return;

    reset();
    m_tempo = appModel->tempo();
    int index = 0;
    for (const auto track : appModel->tracks()) {
        insertTrackToView(track, index);
        index++;
    }
    emit trackCountChanged(m_trackListViewModel.tracks.count());

    // appController->onRunG2p();
}
void TrackEditorView::onTempoChanged(double tempo) {
    // notify audio clips
    m_tempo = tempo;
    emit tempoChanged(tempo);
}
void TrackEditorView::onTrackChanged(AppModel::TrackChangeType type, int index) {
    switch (type) {
        case AppModel::Insert:
            // qDebug() << "on track inserted" << index;
            insertTrackToView(appModel->tracks().at(index), index);
            emit trackCountChanged(m_trackListViewModel.tracks.count());
            break;
        case AppModel::Remove:
            // qDebug() << "on track removed" << index;
            // remove selection
            emit selectedClipChanged(-1);
            removeTrackFromView(index);
            emit trackCountChanged(m_trackListViewModel.tracks.count());
            break;
    }
}
void TrackEditorView::onClipChanged(Track::ClipChangeType type, Clip *clip) {
    int trackIndex;
    appModel->findClipById(clip->id(), trackIndex);
    auto track = m_trackListViewModel.tracks.at(trackIndex);
    if (type == Track::Inserted) {
        insertClipToTrack(clip, track, trackIndex);
        connect(clip, &Clip::propertyChanged, this, [=] { updateClipOnView(clip); });
    } else if (type == Track::Removed) {
        removeClipFromView(clip->id());
    }
    updateOverlappedState();
}
void TrackEditorView::onPositionChanged(double tick) {
    m_timeline->setPosition(tick);
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
        auto meter = m_trackListViewModel.tracks.at(i)->widget->levelMeter();
        meter->setValue(state.valueL, state.valueR);
    }
}
void TrackEditorView::onSceneSelectionChanged() {
    // find selected clip (the first one)
    bool foundSelectedClip = false;
    for (int i = 0; i < m_trackListViewModel.tracks.count(); i++) {
        auto track = m_trackListViewModel.tracks.at(i);
        for (int j = 0; j < track->clips.count(); j++) {
            auto clip = track->clips.at(j);
            if (clip->isSelected()) {
                foundSelectedClip = true;
                qDebug() << "TracksView::onSceneSelectionChanged"
                         << "foundSelectedClip" << clip->id();
                emit selectedClipChanged(clip->id());
                break;
            }
        }
        if (foundSelectedClip)
            break;
    }
    if (!foundSelectedClip)
        emit selectedClipChanged(-1);
}
void TrackEditorView::onViewScaleChanged(qreal sx, qreal sy) {
    int previousHeightSum = 0;
    for (int i = 0; i < m_trackListWidget->count(); i++) {
        // adjust track item height
        auto item = m_trackListWidget->item(i);
        int height = qRound((i + 1) * TracksEditorGlobal::trackHeight * sy - previousHeightSum);
        item->setSizeHint(QSize(TracksEditorGlobal::trackListWidth, height));

        // hide pan and gain slider when sy is too small
        auto widget = m_trackListViewModel.tracks.at(i)->widget;
        widget->setNarrowMode(sy < TracksEditorGlobal::narrowModeScaleY);
        previousHeightSum += height;
    }
}
void TrackEditorView::onClipGraphicsItemRemoveTriggered(int id) {
    auto clip = findClipItemById(id);
    auto dlg = new Dialog(this);
    dlg->setWindowTitle(tr("Warning"));
    dlg->setTitle(tr("Do you want to delete this clip?"));
    dlg->setMessage(clip->name());
    dlg->setModal(true);

    auto btnDelete = new Button(tr("Delete"));
    connect(btnDelete, &Button::clicked, dlg, &Dialog::accept);
    dlg->setNegativeButton(btnDelete);

    auto btnCancel = new AccentButton(tr("Cancel"));
    connect(btnCancel, &Button::clicked, dlg, &Dialog::reject);
    dlg->setPositiveButton(btnCancel);

    connect(dlg, &Dialog::accepted, this, [=] { emit removeClipTriggered(id); });

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
    for (int clipIndex = 0; clipIndex < dsTrack->clips().count(); clipIndex++) {
        auto clip = dsTrack->clips().at(clipIndex);
        insertClipToTrack(clip, track, trackIndex);
    }
    auto newTrackItem = new QListWidgetItem;
    auto newTrackControlWidget = new TrackControlWidget(newTrackItem);
    newTrackItem->setSizeHint(
        QSize(TracksEditorGlobal::trackListWidth,
              static_cast<int>(TracksEditorGlobal::trackHeight * m_graphicsView->scaleY())));
    newTrackControlWidget->setTrackIndex(trackIndex + 1);
    newTrackControlWidget->setName(dsTrack->name());
    newTrackControlWidget->setControl(dsTrack->control());
    newTrackControlWidget->setNarrowMode(m_graphicsView->scaleY() <
                                         TracksEditorGlobal::narrowModeScaleY);
    m_trackListWidget->insertItem(trackIndex, newTrackItem);
    m_trackListWidget->setItemWidget(newTrackItem, newTrackControlWidget);
    track->widget = newTrackControlWidget;
    // connect(m_graphicsView, &TracksGraphicsView::scaleChanged, newTrackWidget,
    // &TrackControlWidget::setScale);


    connect(newTrackControlWidget, &TrackControlWidget::insertNewTrackTriggered, this, [=] {
        auto i = m_trackListWidget->row(newTrackItem);
        emit insertNewTrackTriggered(i + 1); // insert after current track
    });
    connect(newTrackControlWidget, &TrackControlWidget::removeTrackTriggerd, this, [=] {
        auto i = m_trackListWidget->row(newTrackItem);

        auto dlg = new Dialog(this);
        dlg->setWindowTitle(tr("Warning"));
        dlg->setTitle(tr("Do you want to delete this track?"));
        dlg->setMessage(newTrackControlWidget->name());
        dlg->setModal(true);

        auto btnDelete = new Button(tr("Delete"));
        connect(btnDelete, &Button::clicked, dlg, &Dialog::accept);
        dlg->setNegativeButton(btnDelete);

        auto btnCancel = new AccentButton(tr("Cancel"));
        connect(btnCancel, &Button::clicked, dlg, &Dialog::reject);
        dlg->setPositiveButton(btnCancel);

        connect(dlg, &Dialog::accepted, this, [=] { emit removeTrackTriggered(i); });

        dlg->show();
    });
    connect(newTrackControlWidget, &TrackControlWidget::addAudioClipTriggered, this, [=] {
        auto fileName =
            QFileDialog::getOpenFileName(this, tr("Select an Audio File"), ".",
                                         tr("All Audio File (*.wav *.flac *.mp3);;Wave File "
                                            "(*.wav);;Flac File (*.flac);;MP3 File (*.mp3)"));
        if (fileName.isNull())
            return;
        auto i = m_trackListWidget->row(newTrackItem);
        emit addAudioClipTriggered(fileName, i, 0);
    });
    m_trackListViewModel.tracks.insert(trackIndex, track);
    if (trackIndex < m_trackListViewModel.tracks.count()) // needs to update existed tracks' index
        for (int i = trackIndex + 1; i < m_trackListViewModel.tracks.count(); i++) {
            // Update track list items' index
            auto item = m_trackListWidget->item(i);
            auto widget = m_trackListWidget->itemWidget(item);
            auto trackWidget = dynamic_cast<TrackControlWidget *>(widget);
            trackWidget->setTrackIndex(i + 1);
            // Update clips' index
            for (auto &clipItem : m_trackListViewModel.tracks.at(i)->clips) {
                clipItem->setTrackIndex(i);
            }
        }
}
void TrackEditorView::insertClipToTrack(Clip *clip, TrackViewModel *track,
                                        int trackIndex) { // TODO: remove param track
    auto start = clip->start();
    auto clipStart = clip->clipStart();
    auto length = clip->length();
    auto clipLen = clip->clipLen();

    if (clip->clipType() == Clip::Audio) {
        auto audioClip = dynamic_cast<AudioClip *>(clip);
        auto clipItem = new AudioClipGraphicsItem(clip->id());
        clipItem->setName(clip->name());
        clipItem->setStart(start);
        clipItem->setClipStart(clipStart);
        clipItem->setLength(length);
        clipItem->setClipLen(clipLen);
        clipItem->setGain(audioClip->gain());
        clipItem->setTrackIndex(trackIndex);
        clipItem->setPath(audioClip->path());
        clipItem->setTempo(appModel->tempo());
        clipItem->setOverlapped(audioClip->overlapped());
        clipItem->setAudioInfo(audioClip->audioInfo());
        m_tracksScene->addCommonItem(clipItem);
        qDebug() << "Audio clip graphics item added to scene" << clipItem->id() << clipItem->name();
        connect(this, &TrackEditorView::tempoChanged, clipItem,
                &AudioClipGraphicsItem::onTempoChange);
        connect(appModel, &AppModel::quantizeChanged, clipItem,
                &AbstractClipGraphicsItem::setQuantize);
        connect(clipItem, &AudioClipGraphicsItem::propertyChanged, this, [=] {
            auto clip = findClipItemById(clipItem->id());
            if (clip == clipItem) {
                AudioClip::AudioClipProperties args;
                args.name = clipItem->name();
                args.id = clipItem->id();
                args.start = clipItem->start();
                args.clipStart = clipItem->clipStart();
                args.length = clipItem->length();
                args.clipLen = clipItem->clipLen();
                args.gain = clipItem->gain();
                args.mute = clipItem->mute();
                args.path = clipItem->path();
                emit clipPropertyChanged(args);
            }
        });
        connect(clipItem, &AbstractClipGraphicsItem::removeTriggered, this,
                &TrackEditorView::onClipGraphicsItemRemoveTriggered);
        track->clips.append(clipItem);
    } else if (clip->clipType() == Clip::Singing) {
        auto singingClip = dynamic_cast<SingingClip *>(clip);
        auto clipItem = new SingingClipGraphicsItem(clip->id());
        clipItem->setStart(start);
        clipItem->setClipStart(clipStart);
        clipItem->setLength(length);
        clipItem->setClipLen(clipLen);
        clipItem->setGain(singingClip->gain());
        clipItem->setTrackIndex(trackIndex);
        const auto &notesRef = singingClip->notes();
        clipItem->loadNotes(notesRef);
        clipItem->setOverlapped(singingClip->overlapped());
        m_tracksScene->addCommonItem(clipItem);
        qDebug() << "Singing clip graphics item added to scene" << clipItem->id()
                 << clipItem->name();
        connect(singingClip, &SingingClip::noteChanged, clipItem,
                &SingingClipGraphicsItem::onNoteListChanged);
        connect(clipItem, &AbstractClipGraphicsItem::removeTriggered, this,
                &TrackEditorView::onClipGraphicsItemRemoveTriggered);
        connect(appModel, &AppModel::quantizeChanged, clipItem,
                &AbstractClipGraphicsItem::setQuantize);
        connect(clipItem, &SingingClipGraphicsItem::propertyChanged, this, [=] {
            auto clip = findClipItemById(clipItem->id());
            if (clip == clipItem) {
                Clip::ClipCommonProperties args;
                args.name = clipItem->name();
                args.id = clipItem->id();
                args.start = clipItem->start();
                args.clipStart = clipItem->clipStart();
                args.length = clipItem->length();
                args.clipLen = clipItem->clipLen();
                args.gain = clipItem->gain();
                args.mute = clipItem->mute();
                emit clipPropertyChanged(args);
            }
        });
        track->clips.append(clipItem);
    }
    connect(clip, &Clip::propertyChanged, this, [=] { updateClipOnView(clip); });
}
void TrackEditorView::removeClipFromView(int clipId) {
    auto clipItem = findClipItemById(clipId);
    m_tracksScene->removeCommonItem(clipItem);
    int trackIndex = 0;
    for (const auto &track : m_trackListViewModel.tracks) {
        if (track->clips.contains(clipItem)) {
            track->clips.removeOne(clipItem);
            break;
        }
        trackIndex++;
    }
}
AbstractClipGraphicsItem *TrackEditorView::findClipItemById(int id) {
    for (const auto &track : m_trackListViewModel.tracks)
        for (const auto clip : track->clips)
            if (clip->id() == id)
                return clip;
    return nullptr;
}
void TrackEditorView::updateTracksOnView() const {
    auto tracksModel = appModel->tracks();
    for (int i = 0; i < m_trackListViewModel.tracks.count(); i++) {
        auto widget = m_trackListViewModel.tracks.at(i)->widget;
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
        auto audioItem = dynamic_cast<AudioClipGraphicsItem *>(item);
        audioItem->setPath(audioClip->path());
        audioItem->setAudioInfo(audioClip->audioInfo());
    } else if (clip->clipType() == Clip::Singing) {
        auto singingClip = dynamic_cast<SingingClip *>(clip);
        auto singingItem = dynamic_cast<SingingClipGraphicsItem *>(item);
        singingItem->loadNotes(singingClip->notes());
    }

    updateOverlappedState();
}
void TrackEditorView::removeTrackFromView(int index) {
    disconnect(m_tracksScene, &TracksGraphicsScene::selectionChanged, this,
               &TrackEditorView::onSceneSelectionChanged);
    // remove from view
    auto track = m_trackListViewModel.tracks.at(index);
    for (auto clip : track->clips) {
        m_tracksScene->removeCommonItem(clip);
        delete clip;
    }
    auto item = m_trackListWidget->takeItem(index);
    m_trackListWidget->removeItemWidget(item);
    // remove from viewmodel
    m_trackListViewModel.tracks.removeAt(index);
    // update index
    if (index < m_trackListViewModel.tracks.count()) // needs to update existed tracks' index
        for (int i = index; i < m_trackListViewModel.tracks.count(); i++) {
            // Update track list items' index
            auto item = m_trackListWidget->item(i);
            auto widget = m_trackListWidget->itemWidget(item);
            auto trackWidget = dynamic_cast<TrackControlWidget *>(widget);
            trackWidget->setTrackIndex(i + 1);
            // Update clips' index
            for (auto &clipItem : m_trackListViewModel.tracks.at(i)->clips) {
                clipItem->setTrackIndex(i);
            }
        }
    connect(m_tracksScene, &TracksGraphicsScene::selectionChanged, this,
            &TrackEditorView::onSceneSelectionChanged);
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
    for (auto &track : m_trackListViewModel.tracks)
        for (auto clip : track->clips) {
            m_tracksScene->removeCommonItem(clip);
            delete clip;
        }
    m_trackListWidget->clear();
    m_trackListViewModel.tracks.clear();
}