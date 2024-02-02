//
// Created by fluty on 2024/1/29.
//

#include "TracksView.h"

// #include <QSplitter>
#include <QDebug>
#include <QScroller>
#include <QScrollBar>

#include "Controller/AppController.h"
#include "Controls/TracksEditor/AudioClipGraphicsItem.h"
#include "Controls/TracksEditor/SingingClipGraphicsItem.h"
#include "Controls/TracksEditor/TracksBackgroundGraphicsItem.h"
#include "Controls/TracksEditor/TracksEditorGlobal.h"


#include <QFileDialog>

TracksView::TracksView() {
    m_trackListWidget = new QListWidget;
    // tracklist->setMinimumWidth(120);
    m_trackListWidget->setMinimumWidth(TracksEditorGlobal::trackListWidth);
    m_trackListWidget->setMaximumWidth(TracksEditorGlobal::trackListWidth);
    m_trackListWidget->setViewMode(QListView::ListMode);
    m_trackListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_trackListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_trackListWidget->setVerticalScrollMode(QListWidget::ScrollPerPixel);
    QScroller::grabGesture(m_trackListWidget, QScroller::TouchGesture);
    // m_trackListWidget->setStyleSheet("QListWidget::item{ height: 72px }");

    m_graphicsView = new TracksGraphicsView;
    // QScroller::grabGesture(m_graphicsView, QScroller::TouchGesture);
    m_graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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

    auto gridItem = new TracksBackgroundGraphicsItem;
    gridItem->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, gridItem,
            &TimeGridGraphicsItem::setVisibleRect);
    connect(m_graphicsView, &TracksGraphicsView::scaleChanged, gridItem,
            &TimeGridGraphicsItem::setScale);
    connect(this, &TracksView::trackCountChanged, gridItem,
            &TracksBackgroundGraphicsItem::onTrackCountChanged);
    m_tracksScene->addItem(gridItem);

    auto gBar = m_graphicsView->verticalScrollBar();
    auto lBar = m_trackListWidget->verticalScrollBar();
    connect(gBar, &QScrollBar::valueChanged, lBar, &QScrollBar::setValue);

    // auto splitter = new QSplitter;
    // splitter->setOrientation(Qt::Horizontal);
    // splitter->addWidget(tracklist);
    // splitter->addWidget(m_graphicsView);
    auto layout = new QHBoxLayout;
    layout->setSpacing(0);
    layout->setContentsMargins({});
    // layout->addWidget(splitter);
    layout->addWidget(m_trackListWidget);
    layout->addWidget(m_graphicsView);
    setLayout(layout);
}
void TracksView::onModelChanged() {
    if (m_tracksScene == nullptr)
        return;

    reset();
    auto model = AppModel::instance();
    m_tempo = model->tempo();
    int index = 0;
    for (const auto &track : model->tracks()) {
        insertTrackToView(*track, index);
        index++;
    }
    emit trackCountChanged(m_tracksModel.tracks.count());
}
void TracksView::onTempoChanged(double tempo) {
    // notify audio clips
    m_tempo = tempo;
    emit tempoChanged(tempo);
}
void TracksView::onTrackChanged(AppModel::TrackChangeType type,int index) {
    auto model = AppModel::instance();
    switch (type) {
        case AppModel::Insert:
            qDebug() << "on track inserted" << index;
            insertTrackToView(*model->tracks().at(index), index);
            emit trackCountChanged(m_tracksModel.tracks.count());
            break;
        case AppModel::Update:
            qDebug() << "on track updated" << index;
            break;
        case AppModel::Remove:
            qDebug() << "on track removed" << index;
            // remove selection
            emit selectedClipChanged(-1, -1);
            removeTrackFromView(index);
            emit trackCountChanged(m_tracksModel.tracks.count());
            break;
    }
}
void TracksView::onClipChanged(DsTrack::ClipChangeType type, int trackIndex, int clipIndex) {
    auto trackModel = AppModel::instance()->tracks().at(trackIndex);
    auto track = m_tracksModel.tracks.at(trackIndex);
    AbstractClipGraphicsItem *clipItem;
    switch (type) {
        case DsTrack::Insert:
            qDebug() << "on clip inserted" << trackIndex << clipIndex;
            insertClipToTrack(trackModel->clips().at(clipIndex), track, trackIndex, clipIndex);
            break;
        // case DsTrack::Update:
        //     qDebug() << "fetch data from clip model";
        //     clipItem = track->clips.at(clipIndex);
        //     if (clipItem->start() != dsClip->start())
        //         clipItem->setStart(dsClip->start());
        //     if (clipItem->overlapped() != dsClip->overlapped())
        //         clipItem->setOverlapped(dsClip->overlapped());
        //     break;
        case DsTrack::Remove:
            qDebug() << "on clip removed" << trackIndex << clipIndex;
            removeClipFromTrack(track, clipIndex);
            break;
    }
    updateOverlappedState(trackIndex);
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
                emit selectedClipChanged(i, j);
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
void TracksView::insertTrackToView(const DsTrack &dsTrack, int trackIndex) {
    connect(&dsTrack, &DsTrack::clipChanged, this,
            [=](DsTrack::ClipChangeType type, int clipIndex) {
                onClipChanged(type, trackIndex, clipIndex);
            });
    auto track = new Track;
    for (int clipIndex = 0; clipIndex < dsTrack.clips().count(); clipIndex++) {
        auto clip = dsTrack.clips().at(clipIndex);
        insertClipToTrack(clip, track, trackIndex, clipIndex);
    }
    auto newTrackItem = new QListWidgetItem;
    auto newTrackControlWidget = new TrackControlWidget(newTrackItem);
    newTrackItem->setSizeHint(QSize(TracksEditorGlobal::trackListWidth,
                                    TracksEditorGlobal::trackHeight * m_graphicsView->scaleY()));
    newTrackControlWidget->setTrackIndex(trackIndex + 1);
    newTrackControlWidget->setName(dsTrack.name().isEmpty() ? "New Track" : dsTrack.name());
    newTrackControlWidget->setControl(dsTrack.control());
    newTrackControlWidget->setNarrowMode(m_graphicsView->scaleY() <
                                         TracksEditorGlobal::narrowModeScaleY);
    m_trackListWidget->insertItem(trackIndex, newTrackItem);
    m_trackListWidget->setItemWidget(newTrackItem, newTrackControlWidget);
    track->widget = newTrackControlWidget;
    // connect(m_graphicsView, &TracksGraphicsView::scaleChanged, newTrackWidget,
    // &TrackControlWidget::setScale);

    connect(newTrackControlWidget, &TrackControlWidget::propertyChanged, this, [=] {
        auto name = newTrackControlWidget->name();
        auto control = newTrackControlWidget->control();
        auto i = m_trackListWidget->row(newTrackItem);
        emit trackPropertyChanged(name, control, trackIndex);
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
        emit addAudioClipTriggered(fileName, i);
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
                                   int trackIndex, int clipIndex) { // TODO: remove param track
    auto start = clip->start();
    auto clipStart = clip->clipStart();
    auto length = clip->length();
    auto clipLen = clip->clipLen();
    if (clip->type() == DsClip::Audio) {
        auto audioClip = dynamic_cast<DsAudioClip *>(clip);
        auto clipItem = new AudioClipGraphicsItem(0);
        clipItem->setStart(start);
        clipItem->setClipStart(clipStart);
        clipItem->setLength(length);
        clipItem->setClipLen(clipLen);
        clipItem->setGain(1.0);
        clipItem->setTrackIndex(trackIndex);
        clipItem->setPath(audioClip->path());
        clipItem->setTempo(m_tempo);
        clipItem->setOverlapped(audioClip->overlapped());
        clipItem->setVisibleRect(m_graphicsView->visibleRect());
        clipItem->setScaleX(m_graphicsView->scaleX());
        clipItem->setScaleY(m_graphicsView->scaleY());
        m_tracksScene->addItem(clipItem);
        connect(m_graphicsView, &TracksGraphicsView::scaleChanged, clipItem,
                &AudioClipGraphicsItem::setScale);
        connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, clipItem,
                &AudioClipGraphicsItem::setVisibleRect);
        connect(this, &TracksView::tempoChanged, clipItem, &AudioClipGraphicsItem::onTempoChange);
        connect(clipItem, &AudioClipGraphicsItem::propertyChanged, this, [=] {
            auto track = m_tracksModel.tracks.at(trackIndex);
            for (int j = 0; j < track->clips.count(); j++) {
                auto clip = track->clips.at(j);
                if (clip == clipItem) {
                    TracksViewController::AudioClipPropertyChangedArgs args;
                    args.trackIndex = trackIndex;
                    args.clipIndex = j;
                    args.start = clipItem->start();
                    args.clipStart = clipItem->clipStart();
                    args.length = clipItem->length();
                    args.clipLen = clipItem->clipLen();
                    args.gain = clipItem->gain();
                    args.mute = clipItem->mute();
                    emit clipPropertyChanged(args);
                    break;
                }
            }
        });
        track->clips.insert(clipIndex, clipItem);
    } else if (clip->type() == DsClip::Singing) {
        auto singingClip = dynamic_cast<DsSingingClip *>(clip);
        auto clipItem = new SingingClipGraphicsItem(0);
        clipItem->setStart(start);
        clipItem->setClipStart(clipStart);
        clipItem->setLength(length);
        clipItem->setClipLen(clipLen);
        clipItem->setGain(1.0);
        clipItem->setTrackIndex(trackIndex);
        clipItem->loadNotes(singingClip->notes);
        clipItem->setOverlapped(singingClip->overlapped());
        clipItem->setVisibleRect(m_graphicsView->visibleRect());
        clipItem->setScaleX(m_graphicsView->scaleX());
        clipItem->setScaleY(m_graphicsView->scaleY());
        m_tracksScene->addItem(clipItem);
        connect(m_graphicsView, &TracksGraphicsView::scaleChanged, clipItem,
                &SingingClipGraphicsItem::setScale);
        connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, clipItem,
                &SingingClipGraphicsItem::setVisibleRect);
        track->clips.insert(clipIndex, clipItem);
    }
}
void TracksView::removeClipFromTrack(Track *track, int clipIndex) {
    auto clip = track->clips.at(clipIndex);
    m_tracksScene->removeItem(clip);
    track->clips.removeAt(clipIndex);
}

void TracksView::removeTrackFromView(int index) {
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
}
void TracksView::updateOverlappedState(int trackIndex) {
    auto trackModel = AppModel::instance()->tracks().at(trackIndex);
    auto track = m_tracksModel.tracks.at(trackIndex);
    int i = 0;
    for (auto clipItem : track->clips) {
        auto dsClip = trackModel->clips().at(i);
        if (clipItem->overlapped() != dsClip->overlapped())
            clipItem->setOverlapped(dsClip->overlapped());
        i++;
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