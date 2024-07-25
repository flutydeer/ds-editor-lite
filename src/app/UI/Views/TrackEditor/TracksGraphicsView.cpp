//
// Created by fluty on 2023/11/14.
//

#include "TracksGraphicsView.h"

#include "TracksGraphicsScene.h"
#include "Controller/TracksViewController.h"
#include "GraphicsItem/TracksBackgroundGraphicsItem.h"
#include "Model/AppModel.h"
#include "Utils/MathUtils.h"
#include "UI/Controls/Menu.h"

#include <QMouseEvent>
#include <QFileDialog>

TracksGraphicsView::TracksGraphicsView(TracksGraphicsScene *scene, QWidget *parent)
    : TimeGraphicsView(scene, parent) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("TracksGraphicsView");

    setScaleYMin(0.575);

    m_actionNewSingingClip = new QAction(tr("New singing clip"), this);
    connect(m_actionNewSingingClip, &QAction::triggered, this,
            &TracksGraphicsView::onNewSingingClip);

    m_actionAddAudioClip = new QAction(tr("Insert audio clip"), this);
    connect(m_actionAddAudioClip, &QAction::triggered, this, &TracksGraphicsView::onAddAudioClip);
}
void TracksGraphicsView::setQuantize(int quantize) {
    m_quantize = quantize;
}
void TracksGraphicsView::onNewSingingClip() const {
    trackController->onNewSingingClip(m_trackIndex, m_tick);
}
void TracksGraphicsView::onAddAudioClip() {
    auto fileName = QFileDialog::getOpenFileName(this, tr("Select an Audio File"), ".",
                                                 tr("All Audio File (*.wav *.flac *.mp3);;Wave File "
                                                 "(*.wav);;Flac File (*.flac);;MP3 File (*.mp3)"));
    if (fileName.isNull())
        return;
    auto track = appModel->tracks().at(m_trackIndex);
    trackController->onAddAudioClip(fileName, track->id(), m_tick);
}
void TracksGraphicsView::mousePressEvent(QMouseEvent *event) {
    // auto scenePos = mapToScene(event->position().toPoint());
    // auto trackIndex = dynamic_cast<TracksGraphicsScene *>(scene())->trackIndexAt(scenePos.y());
    // qDebug() << trackIndex;
    CommonGraphicsView::mousePressEvent(event);
}
void TracksGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    auto scenePos = mapToScene(event->position().toPoint());
    m_trackIndex = dynamic_cast<TracksGraphicsScene *>(scene())->trackIndexAt(scenePos.y());
    if (m_trackIndex == -1)
        return;

    auto tick = dynamic_cast<TracksGraphicsScene *>(scene())->tickAt(scenePos.x());
    if (auto item = itemAt(event->pos())) {
        if (dynamic_cast<TracksBackgroundGraphicsItem *>(item)) {
            m_tick = MathUtils::roundDown(tick, 1920 / m_quantize);
            onNewSingingClip();
        }
    }

    CommonGraphicsView::mouseDoubleClickEvent(event);
}
void TracksGraphicsView::contextMenuEvent(QContextMenuEvent *event) {
    auto scenePos = mapToScene(event->pos());
    auto trackIndex = dynamic_cast<TracksGraphicsScene *>(scene())->trackIndexAt(scenePos.y());
    if (trackIndex == -1)
        return;

    auto tick = dynamic_cast<TracksGraphicsScene *>(scene())->tickAt(scenePos.x());
    if (auto item = itemAt(event->pos())) {
        if (dynamic_cast<TracksBackgroundGraphicsItem *>(item)) {
            m_trackIndex = trackIndex;
            m_tick = tick;
            m_snappedTick = MathUtils::roundDown(tick, 1920 / m_quantize);
            Menu menu(this);
            menu.addAction(m_actionNewSingingClip);
            menu.addAction(m_actionAddAudioClip);
            menu.exec(event->globalPos());
        } else
            CommonGraphicsView::contextMenuEvent(event);
    }
}