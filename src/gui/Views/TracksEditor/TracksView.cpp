//
// Created by fluty on 2024/1/29.
//

#include "TracksView.h"

// #include <QSplitter>
#include <QDebug>
#include <QScroller>
#include <QScrollBar>
#include <QFileDialog>

#include "TracksEditorGlobal.h"
#include "TrackListHeaderView.h"
#include "Audio/AudioSystem.h"
#include "Audio/AudioContext.h"
#include "Controller/AppController.h"
#include "Controller/PlaybackController.h"
#include "GraphicsItem/AudioClipGraphicsItem.h"
#include "GraphicsItem/SingingClipGraphicsItem.h"

TracksView::TracksView() {
    m_trackListWidget = new QListWidget;
    // tracklist->setMinimumWidth(120);
    m_trackListWidget->setMinimumWidth(TracksEditorGlobal::trackListWidth);
    m_trackListWidget->setMaximumWidth(TracksEditorGlobal::trackListWidth);
    m_trackListWidget->setViewMode(QListView::ListMode);
    m_trackListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_trackListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_trackListWidget->setVerticalScrollMode(QListWidget::ScrollPerPixel);
    m_trackListWidget->setStyleSheet("QListWidget { background: #2A2B2C; border: none; "
                                     "border-right: 1px solid #202020; outline:0px;"
                                     "border-top: 1px solid #202020;"
                                     "margin-bottom: 16px } "
                                     "QListWidget::item:hover { background: #2E2F30 }"
                                     "QListWidget::item:selected { background: #373839 }");
    m_trackListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    QScroller::grabGesture(m_trackListWidget, QScroller::TouchGesture);
    connect(m_trackListWidget, &QListWidget::currentRowChanged, AppController::instance(),
            &AppController::onTrackSelectionChanged);


    m_tracksScene = new TracksGraphicsScene;
    m_graphicsView = new TracksGraphicsView(m_tracksScene);
    // QScroller::grabGesture(m_graphicsView, QScroller::TouchGesture);
    m_graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_graphicsView->setEnsureSceneFillView(false);
    m_graphicsView->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    connect(m_graphicsView, &TracksGraphicsView::scaleChanged, this,
            &TracksView::onViewScaleChanged);
    m_graphicsView->centerOn(0, 0);
    connect(m_graphicsView, &TracksGraphicsView::scaleChanged, m_tracksScene,
            &TracksGraphicsScene::setScale);
    // connect(m_graphicsView, &TracksGraphicsView::resized, m_tracksScene,
    //         &TracksGraphicsScene::onGraphicsViewResized);
    connect(m_graphicsView, &TracksGraphicsView::sizeChanged, m_tracksScene,
            &TracksGraphicsScene::onViewResized);
    connect(this, &TracksView::trackCountChanged, m_tracksScene,
            &TracksGraphicsScene::onTrackCountChanged);
    connect(m_tracksScene, &TracksGraphicsScene::selectionChanged, this,
            &TracksView::onSceneSelectionChanged);
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
    connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, m_gridItem,
            &TimeGridGraphicsItem::setVisibleRect);
    connect(m_graphicsView, &TracksGraphicsView::scaleChanged, m_gridItem,
            &TimeGridGraphicsItem::setScale);
    connect(this, &TracksView::trackCountChanged, m_gridItem,
            &TracksBackgroundGraphicsItem::onTrackCountChanged);
    auto appModel = AppModel::instance();
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
    m_timeline->setTimeRange(m_graphicsView->startTick(), m_graphicsView->endTick());
    m_timeline->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    connect(m_timeline, &TimelineView::wheelHorScale, m_graphicsView,
            &TracksGraphicsView::onWheelHorScale);
    auto playbackController = PlaybackController::instance();
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
            &TracksView::onPositionChanged);
    connect(playbackController, &PlaybackController::lastPositionChanged, this,
            &TracksView::onLastPositionChanged);
    //    connect(playbackController, &PlaybackController::levelMetersUpdated, this,
    //            &TracksView::onLevelMetersUpdated);
    connect(AudioSystem::instance()->audioContext(), &AudioContext::levelMeterUpdated, this,
            &TracksView::onLevelMetersUpdated);

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

    setLayout(mainLayout);
}
void TracksView::onModelChanged() {
    if (m_tracksScene == nullptr)
        return;

    reset();
    auto model = AppModel::instance();
    m_tempo = model->tempo();
    int index = 0;
    for (const auto track : model->tracks()) {
        insertTrackToView(track, index);
        index++;
    }
    emit trackCountChanged(m_trackListViewModel.tracks.count());

    // AppController::instance()->onRunG2p();
}
void TracksView::onTempoChanged(double tempo) {
    // notify audio clips
    m_tempo = tempo;
    emit tempoChanged(tempo);
}
void TracksView::onTrackChanged(AppModel::TrackChangeType type, int index) {
    auto model = AppModel::instance();
    switch (type) {
        case AppModel::Insert:
            // qDebug() << "on track inserted" << index;
            insertTrackToView(model->tracks().at(index), index);
            emit trackCountChanged(m_trackListViewModel.tracks.count());
            break;
        case AppModel::PropertyUpdate:
            // qDebug() << "on track updated" << index;
            updateTracksOnView();
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
void TracksView::onClipChanged(Track::ClipChangeType type, int trackIndex, int clipId) {
    auto trackModel = AppModel::instance()->tracks().at(trackIndex);
    auto track = m_trackListViewModel.tracks.at(trackIndex);
    auto dsClip = trackModel->findClipById(clipId);
    switch (type) {
        case Track::Inserted:
            qDebug() << "TracksView on clip inserted" << trackIndex << clipId;
            insertClipToTrack(dsClip, track, trackIndex);
            break;

        case Track::PropertyChanged:
            qDebug() << "TracksView on clip updated" << trackIndex << clipId;
            updateClipOnView(dsClip, clipId);
            break;

        case Track::Removed:
            qDebug() << "TracksView on clip removed" << trackIndex << clipId;
            removeClipFromView(clipId);
            break;
    }
    for (auto overlappedItem : trackModel->clips().overlappedItems()) {
        qDebug() << "overlappedItem" << overlappedItem->id();
    }
    updateOverlappedState(trackIndex);
}
void TracksView::onPositionChanged(double tick) {
    m_timeline->setPosition(tick);
    m_graphicsView->setPlaybackPosition(tick);
}
void TracksView::onLastPositionChanged(double tick) {
    m_graphicsView->setLastPlaybackPosition(tick);
}
void TracksView::onLevelMetersUpdated(const AppModel::LevelMetersUpdatedArgs &args) {
    if (m_trackListViewModel.tracks.isEmpty())
        return;

    auto states = args.trackMeterStates;
    for (int i = 0; i < qMin(states.size(), m_trackListViewModel.tracks.size()); i++) {
        auto state = states.at(i);
        auto meter = m_trackListViewModel.tracks.at(i)->widget->levelMeter();
        meter->setValue(state.valueL, state.valueR);
    }
}
void TracksView::onSceneSelectionChanged() {
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
void TracksView::onViewScaleChanged(qreal sx, qreal sy) {
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
void TracksView::insertTrackToView(Track *dsTrack, int trackIndex) {
    connect(dsTrack, &Track::clipChanged, this, [=](Track::ClipChangeType type, int clipId) {
        // workaround for slot executed for 2 times
        // if (m_prevClipId == clipId && m_prevClipChangeType == type)
        //     return;

        auto index = AppModel::instance()->tracks().indexOf(dsTrack);
        onClipChanged(type, index, clipId);
        m_prevClipId = clipId;
        m_prevClipChangeType = type;
    });
    auto track = new TrackViewModel;
    for (int clipIndex = 0; clipIndex < dsTrack->clips().count(); clipIndex++) {
        auto clip = dsTrack->clips().at(clipIndex);
        insertClipToTrack(clip, track, trackIndex);
    }
    auto newTrackItem = new QListWidgetItem;
    auto newTrackControlWidget = new TrackControlWidget(newTrackItem);
    newTrackItem->setSizeHint(QSize(TracksEditorGlobal::trackListWidth,
                                    TracksEditorGlobal::trackHeight * m_graphicsView->scaleY()));
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

    connect(newTrackControlWidget, &TrackControlWidget::propertyChanged, this, [=] {
        auto control = newTrackControlWidget->control();
        auto i = m_trackListWidget->row(newTrackItem);
        Track::TrackProperties args;
        args.name = newTrackControlWidget->name();
        args.gain = control.gain();
        args.pan = control.pan();
        args.mute = control.mute();
        args.solo = control.solo();
        args.index = i;
        emit trackPropertyChanged(args);
    });
    connect(newTrackControlWidget, &TrackControlWidget::insertNewTrackTriggered, this, [=] {
        auto i = m_trackListWidget->row(newTrackItem);
        emit insertNewTrackTriggered(i + 1); // insert after current track
    });
    connect(newTrackControlWidget, &TrackControlWidget::removeTrackTriggerd, this, [=] {
        auto i = m_trackListWidget->row(newTrackItem);
        emit removeTrackTriggerd(i);
    });
    connect(newTrackControlWidget, &TrackControlWidget::addAudioClipTriggered, this, [=] {
        auto fileName =
            QFileDialog::getOpenFileName(this, "Select an Audio File", ".",
                                         "All Audio File (*.wav *.flac *.mp3);;Wave File "
                                         "(*.wav);;Flac File (*.flac);;MP3 File (*.mp3)");
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
void TracksView::insertClipToTrack(Clip *clip, TrackViewModel *track,
                                   int trackIndex) { // TODO: remove param track
    auto start = clip->start();
    auto clipStart = clip->clipStart();
    auto length = clip->length();
    auto clipLen = clip->clipLen();

    if (clip->type() == Clip::Audio) {
        auto audioClip = dynamic_cast<DsAudioClip *>(clip);
        auto clipItem = new AudioClipGraphicsItem(clip->id());
        clipItem->setContext(this);
        clipItem->setName(clip->name());
        clipItem->setStart(start);
        clipItem->setClipStart(clipStart);
        clipItem->setLength(length);
        clipItem->setClipLen(clipLen);
        clipItem->setGain(audioClip->gain());
        clipItem->setTrackIndex(trackIndex);
        clipItem->setPath(audioClip->path());
        clipItem->setTempo(m_tempo);
        clipItem->setOverlapped(audioClip->overlapped());
        clipItem->setVisibleRect(m_graphicsView->visibleRect());
        clipItem->setScaleX(m_graphicsView->scaleX());
        clipItem->setScaleY(m_graphicsView->scaleY());
        m_tracksScene->addItem(clipItem);
        qDebug() << "Audio clip graphics item added to scene" << clipItem->id() << clipItem->name();
        connect(m_graphicsView, &TracksGraphicsView::scaleChanged, clipItem,
                &AudioClipGraphicsItem::setScale);
        connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, clipItem,
                &AudioClipGraphicsItem::setVisibleRect);
        connect(this, &TracksView::tempoChanged, clipItem, &AudioClipGraphicsItem::onTempoChange);
        connect(AppModel::instance(), &AppModel::quantizeChanged, clipItem,
                &AbstractClipGraphicsItem::setQuantize);
        connect(clipItem, &AudioClipGraphicsItem::propertyChanged, this, [=] {
            auto clip = findClipItemById(clipItem->id());
            if (clip == clipItem) {
                Clip::AudioClipPropertyChangedArgs args;
                args.trackIndex = trackIndex;
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
                [=](int id) { emit removeClipTriggered(id); });
        track->clips.append(clipItem);
    } else if (clip->type() == Clip::Singing) {
        auto singingClip = dynamic_cast<DsSingingClip *>(clip);
        auto clipItem = new SingingClipGraphicsItem(clip->id());
        clipItem->setContext(this);
        clipItem->setStart(start);
        clipItem->setClipStart(clipStart);
        clipItem->setLength(length);
        clipItem->setClipLen(clipLen);
        clipItem->setGain(singingClip->gain());
        clipItem->setTrackIndex(trackIndex);
        const auto &notesRef = singingClip->notes();
        clipItem->loadNotes(notesRef);
        clipItem->setOverlapped(singingClip->overlapped());
        clipItem->setVisibleRect(m_graphicsView->visibleRect());
        clipItem->setScaleX(m_graphicsView->scaleX());
        clipItem->setScaleY(m_graphicsView->scaleY());
        m_tracksScene->addItem(clipItem);
        qDebug() << "Singing clip graphics item added to scene" << clipItem->id()
                 << clipItem->name();
        connect(m_graphicsView, &TracksGraphicsView::scaleChanged, clipItem,
                &SingingClipGraphicsItem::setScale);
        connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, clipItem,
                &SingingClipGraphicsItem::setVisibleRect);
        connect(clipItem, &AbstractClipGraphicsItem::removeTriggered, this,
                [=](int id) { emit removeClipTriggered(id); });
        connect(AppModel::instance(), &AppModel::quantizeChanged, clipItem,
                &AbstractClipGraphicsItem::setQuantize);
        connect(clipItem, &SingingClipGraphicsItem::propertyChanged, this, [=] {
            auto clip = findClipItemById(clipItem->id());
            if (clip == clipItem) {
                Clip::ClipCommonProperties args;
                args.trackIndex = trackIndex;
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
}
void TracksView::removeClipFromView(int clipId) {
    auto clipItem = findClipItemById(clipId);
    m_tracksScene->removeItem(clipItem);
    int trackIndex = 0;
    for (const auto &track : m_trackListViewModel.tracks) {
        if (track->clips.contains(clipItem)) {
            track->clips.removeOne(clipItem);
            break;
        }
        trackIndex++;
    }
}
AbstractClipGraphicsItem *TracksView::findClipItemById(int id) {
    for (const auto &track : m_trackListViewModel.tracks)
        for (const auto clip : track->clips)
            if (clip->id() == id)
                return clip;
    return nullptr;
}
void TracksView::updateTracksOnView() {
    auto tracksModel = AppModel::instance()->tracks();
    for (int i = 0; i < m_trackListViewModel.tracks.count(); i++) {
        auto widget = m_trackListViewModel.tracks.at(i)->widget;
        auto track = tracksModel.at(i);
        widget->setName(track->name());
        widget->setControl(track->control());
    }
}
void TracksView::updateClipOnView(Clip *clip, int clipId) {
    // qDebug() << "TracksView::updateClipOnView" << clipId;
    auto item = findClipItemById(clipId);
    item->setName(clip->name());
    item->setStart(clip->start());
    item->setClipStart(clip->clipStart());
    item->setLength(clip->length());
    item->setClipLen(clip->clipLen());
    item->setOverlapped(clip->overlapped());

    if (clip->type() == Clip::Audio) {
        auto audioClip = dynamic_cast<DsAudioClip *>(clip);
        auto audioItem = dynamic_cast<AudioClipGraphicsItem *>(item);
        if (audioItem->path() != audioClip->path())
            audioItem->setPath(audioClip->path());
    } else if (clip->type() == Clip::Singing) {
        auto singingClip = dynamic_cast<DsSingingClip *>(clip);
        auto singingItem = dynamic_cast<SingingClipGraphicsItem *>(item);
        singingItem->loadNotes(singingClip->notes());
    }
}
void TracksView::removeTrackFromView(int index) {
    disconnect(m_tracksScene, &TracksGraphicsScene::selectionChanged, this,
               &TracksView::onSceneSelectionChanged);
    // remove from view
    auto track = m_trackListViewModel.tracks.at(index);
    for (auto clip : track->clips) {
        m_tracksScene->removeItem(clip);
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
            &TracksView::onSceneSelectionChanged);
}
void TracksView::updateOverlappedState(int trackIndex) {
    auto trackModel = AppModel::instance()->tracks().at(trackIndex);
    // qDebug() << "app model track clip count" << trackModel->clips().count();
    auto track = m_trackListViewModel.tracks.at(trackIndex);
    // qDebug() << "tracks view model clip count" << track->clips.count();
    for (auto clipItem : track->clips) {
        auto dsClip = trackModel->findClipById(clipItem->id());
        clipItem->setOverlapped(dsClip->overlapped());
    }
    m_graphicsView->update();
}
void TracksView::reset() {
    for (auto &track : m_trackListViewModel.tracks)
        for (auto clip : track->clips) {
            m_tracksScene->removeItem(clip);
            delete clip;
        }
    m_trackListWidget->clear();
    m_trackListViewModel.tracks.clear();
}