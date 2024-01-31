//
// Created by fluty on 2024/1/29.
//

#include "TracksView.h"

// #include <QSplitter>
#include <QDebug>
#include <QScroller>
#include <QScrollBar>

#include "Controller.h"
#include "Controls/TracksEditor/AudioClipGraphicsItem.h"
#include "Controls/TracksEditor/SingingClipGraphicsItem.h"
#include "Controls/TracksEditor/TracksBackgroundGraphicsItem.h"
#include "Controls/TracksEditor/TracksEditorGlobal.h"

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
void TracksView::onModelChanged(const DsModel &model) {
    if (m_tracksScene == nullptr)
        return;

    reset();
    m_tempo = model.tempo();
    int index = 0;
    for (const auto &track : model.tracks()) {
        insertTrackToView(track, index);
        index++;
    }
    emit trackCountChanged(m_tracksModel.tracks.count());
}
void TracksView::onTempoChanged(double tempo) {
    // notify audio clips
    m_tempo = tempo;
    emit tempoChanged(tempo);
}
void TracksView::onTrackChanged(DsModel::ChangeType type, const DsModel &model, int index) {
    switch (type) {
        case DsModel::Insert:
            qDebug() << "on track inserted" << index;
            insertTrackToView(model.tracks().at(index), index);
            emit trackCountChanged(m_tracksModel.tracks.count());
            break;
        case DsModel::Update:
            // TODO: update view
            break;
        case DsModel::Remove:
            qDebug() << "on track removed" << index;
            // remove selection
            emit selectedClipChanged(-1, -1);
            removeTrackFromView(index);
            emit trackCountChanged(m_tracksModel.tracks.count());
            break;
    }
}
void TracksView::onSceneSelectionChanged() {
    // find selected clip (the first one)
    bool foundSelectedClip = false;
    for (int i = 0; i < m_tracksModel.tracks.count(); i++) {
        auto track = m_tracksModel.tracks.at(i);
        for (int j = 0; j < track.clips.count(); j++) {
            auto clip = track.clips.at(j);
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
        auto widget = m_tracksModel.tracks.at(i).widget;
        widget->setNarrowMode(sy < TracksEditorGlobal::narrowModeScaleY);
        previousHeightSum += height;
    }
}
void TracksView::insertTrackToView(const DsTrack &dsTrack, int index) {
    Track track;
    for (const auto &clip : dsTrack.clips) {
        auto start = clip->start();
        auto clipStart = clip->clipStart();
        auto length = clip->length();
        auto clipLen = clip->clipLen();
        if (clip->type() == DsClip::Audio) {
            auto audioClip = clip.dynamicCast<DsAudioClip>();
            auto clipItem = new AudioClipGraphicsItem(0);
            clipItem->setStart(start);
            clipItem->setClipStart(clipStart);
            clipItem->setLength(length);
            clipItem->setClipLen(clipLen);
            clipItem->setGain(1.0);
            clipItem->setTrackIndex(index);
            clipItem->setPath(audioClip->path());
            clipItem->setTempo(m_tempo);
            clipItem->setVisibleRect(m_graphicsView->visibleRect());
            clipItem->setScaleX(m_graphicsView->scaleX());
            clipItem->setScaleY(m_graphicsView->scaleY());
            m_tracksScene->addItem(clipItem);
            connect(m_graphicsView, &TracksGraphicsView::scaleChanged, clipItem,
                    &AudioClipGraphicsItem::setScale);
            connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, clipItem,
                    &AudioClipGraphicsItem::setVisibleRect);
            connect(this, &TracksView::tempoChanged, clipItem,
                    &AudioClipGraphicsItem::onTempoChange);
            track.clips.append(clipItem); // TODO: insert by clip start pos
        } else if (clip->type() == DsClip::Singing) {
            auto singingClip = clip.dynamicCast<DsSingingClip>();
            auto clipItem = new SingingClipGraphicsItem(0);
            clipItem->setStart(start);
            clipItem->setClipStart(clipStart);
            clipItem->setLength(length);
            clipItem->setClipLen(clipLen);
            clipItem->setGain(1.0);
            clipItem->setTrackIndex(index);
            clipItem->loadNotes(singingClip->notes);
            clipItem->setVisibleRect(m_graphicsView->visibleRect());
            clipItem->setScaleX(m_graphicsView->scaleX());
            clipItem->setScaleY(m_graphicsView->scaleY());
            m_tracksScene->addItem(clipItem);
            connect(m_graphicsView, &TracksGraphicsView::scaleChanged, clipItem,
                    &SingingClipGraphicsItem::setScale);
            connect(m_graphicsView, &TracksGraphicsView::visibleRectChanged, clipItem,
                    &SingingClipGraphicsItem::setVisibleRect);
            track.clips.append(clipItem);
        }
    }
    auto newTrackItem = new QListWidgetItem;
    auto newTrackControlWidget = new TrackControlWidget(newTrackItem);
    newTrackItem->setSizeHint(QSize(TracksEditorGlobal::trackListWidth,
                                    TracksEditorGlobal::trackHeight * m_graphicsView->scaleY()));
    newTrackControlWidget->setTrackIndex(index + 1);
    newTrackControlWidget->setName(dsTrack.name().isEmpty() ? "New Track" : dsTrack.name());
    newTrackControlWidget->setControl(dsTrack.control());
    newTrackControlWidget->setNarrowMode(m_graphicsView->scaleY() <
                                         TracksEditorGlobal::narrowModeScaleY);
    m_trackListWidget->insertItem(index, newTrackItem);
    m_trackListWidget->setItemWidget(newTrackItem, newTrackControlWidget);
    track.widget = newTrackControlWidget;
    // connect(m_graphicsView, &TracksGraphicsView::scaleChanged, newTrackWidget,
    // &TrackControlWidget::setScale);

    connect(newTrackControlWidget, &TrackControlWidget::propertyChanged, this, [=] {
        auto name = newTrackControlWidget->name();
        auto control = newTrackControlWidget->control();
        auto i = m_trackListWidget->row(newTrackItem);
        emit trackPropertyChanged(name, control, index);
    });
    connect(newTrackControlWidget, &TrackControlWidget::insertNewTrackTriggered, this, [=] {
        auto i = m_trackListWidget->row(newTrackItem);
        emit insertNewTrackTriggered(i + 1); // insert after current track
    });
    connect(newTrackControlWidget, &TrackControlWidget::removeTrackTriggerd, this, [=] {
        auto i = m_trackListWidget->row(newTrackItem);
        emit removeTrackTriggerd(i);
    });
    m_tracksModel.tracks.insert(index, track);
    if (index < m_tracksModel.tracks.count()) // needs to update existed tracks' index
        for (int i = index + 1; i < m_tracksModel.tracks.count(); i++) {
            // Update track list items' index
            auto item = m_trackListWidget->item(i);
            auto widget = m_trackListWidget->itemWidget(item);
            auto trackWidget = dynamic_cast<TrackControlWidget *>(widget);
            trackWidget->setTrackIndex(i + 1);
            // Update clips' index
            for (auto &clipItem : m_tracksModel.tracks.at(i).clips) {
                clipItem->setTrackIndex(i);
            }
        }
}
void TracksView::removeTrackFromView(int index) {
    // remove from view
    auto track = m_tracksModel.tracks.at(index);
    for (auto clip : track.clips) {
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
            for (auto &clipItem : m_tracksModel.tracks.at(i).clips) {
                clipItem->setTrackIndex(i);
            }
        }
}
void TracksView::reset() {
    for (auto &track : m_tracksModel.tracks)
        for (auto clip : track.clips) {
            m_tracksScene->removeItem(clip);
            delete clip;
        }
    m_trackListWidget->clear();
    m_tracksModel.tracks.clear();
}