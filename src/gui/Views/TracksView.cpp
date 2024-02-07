//
// Created by fluty on 2024/1/29.
//

#include "TracksView.h"

// #include <QSplitter>
#include <QDebug>
#include <QScroller>
#include <QScrollBar>
#include <QFileDialog>

#include "Audio/AudioSystem.h"
#include "Audio/AudioContext.h"
#include "Controller/AppController.h"
#include "Controller/PlaybackController.h"
#include "Controls/Base/TimelineView.h"
#include "Controls/TracksEditor/AudioClipGraphicsItem.h"
#include "Controls/TracksEditor/SingingClipGraphicsItem.h"
#include "Controls/TracksEditor/TracksEditorGlobal.h"
#include "TrackListHeaderView.h"

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

    m_graphicsView = new TracksGraphicsView;
    // QScroller::grabGesture(m_graphicsView, QScroller::TouchGesture);
    m_graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_graphicsView->setEnsureSceneFillView(false);
    connect(m_graphicsView, &TracksGraphicsView::scaleChanged, this,
            &TracksView::onViewScaleChanged);
    m_tracksScene = new TracksGraphicsScene;
    m_graphicsView->setScene(m_tracksScene);
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
    m_tracksScene->addItem(m_gridItem);

    m_timeline = new TimelineView;
    m_timeline->setTimeRange(m_gridItem->startTick(), m_gridItem->endTick());
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
    connect(m_gridItem, &TimeGridGraphicsItem::timeRangeChanged, m_timeline,
            &TimelineView::setTimeRange);
    connect(appModel, &AppModel::quantizeChanged, m_timeline, &TimelineView::setQuantize);

    m_scenePlayPosIndicator = new TimeIndicatorGraphicsItem;
    m_scenePlayPosIndicator->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    m_scenePlayPosIndicator->setScale(m_graphicsView->scaleX(), 1);
    m_scenePlayPosIndicator->setVisibleRect(m_graphicsView->visibleRect());
    QPen curPlayPosPen;
    curPlayPosPen.setWidth(1);
    curPlayPosPen.setColor(QColor(255, 204, 153));
    m_scenePlayPosIndicator->setPen(curPlayPosPen);
    connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, m_scenePlayPosIndicator,
            &TimeIndicatorGraphicsItem::setVisibleRect);
    connect(m_graphicsView, &TracksGraphicsView::scaleChanged, m_scenePlayPosIndicator,
            &TimeIndicatorGraphicsItem::setScale);
    m_scenePlayPosIndicator->setZValue(2);
    m_tracksScene->addItem(m_scenePlayPosIndicator);

    m_sceneLastPlayPosIndicator = new TimeIndicatorGraphicsItem;
    m_sceneLastPlayPosIndicator->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    m_sceneLastPlayPosIndicator->setScale(m_graphicsView->scaleX(), 1);
    m_sceneLastPlayPosIndicator->setVisibleRect(m_graphicsView->visibleRect());
    QPen lastPlayPosPen;
    lastPlayPosPen.setWidth(1);
    lastPlayPosPen.setColor(QColor(160, 160, 160));
    lastPlayPosPen.setStyle(Qt::DashLine);
    m_sceneLastPlayPosIndicator->setPen(lastPlayPosPen);
    connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, m_sceneLastPlayPosIndicator,
            &TimeIndicatorGraphicsItem::setVisibleRect);
    connect(m_graphicsView, &TracksGraphicsView::scaleChanged, m_sceneLastPlayPosIndicator,
            &TimeIndicatorGraphicsItem::setScale);
    m_sceneLastPlayPosIndicator->setZValue(2);
    m_tracksScene->addItem(m_sceneLastPlayPosIndicator);

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
        insertTrackToView(*track, index);
        index++;
    }
    emit trackCountChanged(m_tracksModel.tracks.count());

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
            insertTrackToView(*model->tracks().at(index), index);
            emit trackCountChanged(m_tracksModel.tracks.count());
            break;
        case AppModel::PropertyUpdate:
            // qDebug() << "on track updated" << index;
            updateTracksOnView();
            break;
        case AppModel::Remove:
            // qDebug() << "on track removed" << index;
            // remove selection
            emit selectedClipChanged(-1, -1);
            removeTrackFromView(index);
            emit trackCountChanged(m_tracksModel.tracks.count());
            break;
    }
}
void TracksView::onClipChanged(DsTrack::ClipChangeType type, int trackIndex, int clipId) {
    auto trackModel = AppModel::instance()->tracks().at(trackIndex);
    auto track = m_tracksModel.tracks.at(trackIndex);
    auto dsClip = trackModel->findClipById(clipId);
    switch (type) {
        case DsTrack::Inserted:
            qDebug() << "TracksView on clip inserted" << trackIndex << clipId;
            insertClipToTrack(dsClip, track, trackIndex);
            break;

        case DsTrack::PropertyChanged:
            qDebug() << "TracksView on clip updated" << trackIndex << clipId;
            updateClipOnView(dsClip, clipId);
            break;

        case DsTrack::Removed:
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
    m_scenePlayPosIndicator->onTimeChanged(tick);
}
void TracksView::onLastPositionChanged(double tick) {
    m_sceneLastPlayPosIndicator->onTimeChanged(tick);
}
void TracksView::onLevelMetersUpdated(const AppModel::LevelMetersUpdatedArgs &args) {
    if (m_tracksModel.tracks.isEmpty())
        return;

    auto states = args.trackMeterStates;
    for (int i = 0; i < qMin(states.size(), m_tracksModel.tracks.size()); i++) {
        auto state = states.at(i);
        auto meter = m_tracksModel.tracks.at(i)->widget->levelMeter();
        meter->setValue(state.valueL, state.valueR);
    }
}
void TracksView::onSceneSelectionChanged() {
    // find selected clip (the first one)
    bool foundSelectedClip = false;
    for (int i = 0; i < m_tracksModel.tracks.count(); i++) {
        auto track = m_tracksModel.tracks.at(i);
        for (int j = 0; j < track->clips.count(); j++) {
            auto clip = track->clips.at(j);
            if (clip->isSelected()) {
                foundSelectedClip = true;
                qDebug() << "TracksView::onSceneSelectionChanged"
                         << "foundSelectedClip" << i << clip->id();
                emit selectedClipChanged(i, clip->id());
                break;
            }
        }
        if (foundSelectedClip)
            break;
    }
    if (!foundSelectedClip)
        emit selectedClipChanged(-1, -1);
}
void TracksView::onViewScaleChanged(qreal sx, qreal sy) {
    int previousHeightSum = 0;
    for (int i = 0; i < m_trackListWidget->count(); i++) {
        // adjust track item height
        auto item = m_trackListWidget->item(i);
        int height = qRound((i + 1) * TracksEditorGlobal::trackHeight * sy - previousHeightSum);
        item->setSizeHint(QSize(TracksEditorGlobal::trackListWidth, height));

        // hide pan and gain slider when sy is too small
        auto widget = m_tracksModel.tracks.at(i)->widget;
        widget->setNarrowMode(sy < TracksEditorGlobal::narrowModeScaleY);
        previousHeightSum += height;
    }
}
void TracksView::onClipRemoveTriggered(int clipId) {
}
void TracksView::insertTrackToView(const DsTrack &dsTrack, int trackIndex) {
    connect(&dsTrack, &DsTrack::clipChanged, this, [=](DsTrack::ClipChangeType type, int clipId) {
        onClipChanged(type, trackIndex, clipId);
    });
    auto track = new Track;
    for (int clipIndex = 0; clipIndex < dsTrack.clips().count(); clipIndex++) {
        auto clip = dsTrack.clips().at(clipIndex);
        insertClipToTrack(clip, track, trackIndex);
    }
    auto newTrackItem = new QListWidgetItem;
    auto newTrackControlWidget = new TrackControlWidget(newTrackItem);
    newTrackItem->setSizeHint(QSize(TracksEditorGlobal::trackListWidth,
                                    TracksEditorGlobal::trackHeight * m_graphicsView->scaleY()));
    newTrackControlWidget->setTrackIndex(trackIndex + 1);
    newTrackControlWidget->setName(dsTrack.name());
    newTrackControlWidget->setControl(dsTrack.control());
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
        DsTrack::TrackPropertyChangedArgs args;
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
    m_tracksModel.tracks.insert(trackIndex, track);
    if (trackIndex < m_tracksModel.tracks.count()) // needs to update existed tracks' index
        for (int i = trackIndex + 1; i < m_tracksModel.tracks.count(); i++) {
            // Update track list items' index
            auto item = m_trackListWidget->item(i);
            auto widget = m_trackListWidget->itemWidget(item);
            auto trackWidget = dynamic_cast<TrackControlWidget *>(widget);
            trackWidget->setTrackIndex(i + 1);
            // Update clips' index
            for (auto &clipItem : m_tracksModel.tracks.at(i)->clips) {
                clipItem->setTrackIndex(i);
            }
        }
}
void TracksView::insertClipToTrack(DsClip *clip, Track *track,
                                   int trackIndex) { // TODO: remove param track
    auto start = clip->start();
    auto clipStart = clip->clipStart();
    auto length = clip->length();
    auto clipLen = clip->clipLen();

    if (clip->type() == DsClip::Audio) {
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
                DsClip::AudioClipPropertyChangedArgs args;
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
    } else if (clip->type() == DsClip::Singing) {
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
                DsClip::ClipPropertyChangedArgs args;
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
    for (const auto &track : m_tracksModel.tracks) {
        if (track->clips.contains(clipItem)) {
            track->clips.removeOne(clipItem);
            break;
        }
        trackIndex++;
    }
}
AbstractClipGraphicsItem *TracksView::findClipItemById(int id) {
    for (const auto &track : m_tracksModel.tracks)
        for (const auto clip : track->clips)
            if (clip->id() == id)
                return clip;
    return nullptr;
}
void TracksView::updateTracksOnView() {
    auto tracksModel = AppModel::instance()->tracks();
    for (int i = 0; i < m_tracksModel.tracks.count(); i++) {
        auto widget = m_tracksModel.tracks.at(i)->widget;
        auto track = tracksModel.at(i);
        widget->setName(track->name());
        widget->setControl(track->control());
    }
}
void TracksView::updateClipOnView(DsClip *clip, int clipId) {
    qDebug() << "TracksView::updateClipOnView" << clipId;
    auto item = findClipItemById(clipId);
    item->setStart(clip->start());
    item->setClipStart(clip->clipStart());
    item->setLength(clip->length());
    item->setClipLen(clip->clipLen());
    item->setOverlapped(clip->overlapped());

    if (clip->type() == DsClip::Audio) {
        auto audioClip = dynamic_cast<DsAudioClip *>(clip);
        auto audioItem = dynamic_cast<AudioClipGraphicsItem *>(item);
        if (audioItem->path() != audioClip->path())
            audioItem->setPath(audioClip->path());
    } else if (clip->type() == DsClip::Singing) {
        auto singingClip = dynamic_cast<DsSingingClip *>(clip);
        auto singingItem = dynamic_cast<SingingClipGraphicsItem *>(item);
        singingItem->loadNotes(singingClip->notes());
    }
}
void TracksView::removeTrackFromView(int index) {
    disconnect(m_tracksScene, &TracksGraphicsScene::selectionChanged, this,
               &TracksView::onSceneSelectionChanged);
    // remove from view
    auto track = m_tracksModel.tracks.at(index);
    for (auto clip : track->clips) {
        m_tracksScene->removeItem(clip);
        delete clip;
    }
    auto item = m_trackListWidget->takeItem(index);
    m_trackListWidget->removeItemWidget(item);
    // remove from viewmodel
    m_tracksModel.tracks.removeAt(index);
    // update index
    if (index < m_tracksModel.tracks.count()) // needs to update existed tracks' index
        for (int i = index; i < m_tracksModel.tracks.count(); i++) {
            // Update track list items' index
            auto item = m_trackListWidget->item(i);
            auto widget = m_trackListWidget->itemWidget(item);
            auto trackWidget = dynamic_cast<TrackControlWidget *>(widget);
            trackWidget->setTrackIndex(i + 1);
            // Update clips' index
            for (auto &clipItem : m_tracksModel.tracks.at(i)->clips) {
                clipItem->setTrackIndex(i);
            }
        }
    connect(m_tracksScene, &TracksGraphicsScene::selectionChanged, this,
            &TracksView::onSceneSelectionChanged);
}
void TracksView::updateOverlappedState(int trackIndex) {
    auto trackModel = AppModel::instance()->tracks().at(trackIndex);
    auto track = m_tracksModel.tracks.at(trackIndex);
    for (auto clipItem : track->clips) {
        auto dsClip = trackModel->findClipById(clipItem->id());
        clipItem->setOverlapped(dsClip->overlapped());
    }
    m_graphicsView->update();
}
void TracksView::reset() {
    for (auto &track : m_tracksModel.tracks)
        for (auto clip : track->clips) {
            m_tracksScene->removeItem(clip);
            delete clip;
        }
    m_trackListWidget->clear();
    m_tracksModel.tracks.clear();
}