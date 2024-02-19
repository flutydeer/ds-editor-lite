//
// Created by fluty on 2023/11/14.
//

#include <QMouseEvent>

#include "TracksGraphicsScene.h"
#include "TracksGraphicsView.h"
#include "GraphicsItem/TracksBackgroundGraphicsItem.h"
#include "Utils/MathUtils.h"
#include "Controls/Menu.h"

TracksGraphicsView::TracksGraphicsView(TracksGraphicsScene *scene) : TimeGraphicsView(scene) {
    setScaleYMin(0.575);

    m_actionNewSingingClip = new QAction("New singing clip", this);
    connect(m_actionNewSingingClip, &QAction::triggered, this,
            [=] { emit addSingingClipTriggered(m_trackIndx, m_snappedTick); });

    m_actionAddAudioClip = new QAction("Add audio clip", this);
    connect(m_actionAddAudioClip, &QAction::triggered, this,
            [=] { emit addAudioClipTriggered(m_trackIndx, m_snappedTick); });
}
void TracksGraphicsView::setQuantize(int quantize) {
    m_quantize = quantize;
}
void TracksGraphicsView::mousePressEvent(QMouseEvent *event) {
    auto scenePos = mapToScene(event->position().toPoint());
    auto trackIndex = dynamic_cast<TracksGraphicsScene *>(scene())->trackIndexAt(scenePos.y());
    // qDebug() << trackIndex;
    CommonGraphicsView::mousePressEvent(event);
}
void TracksGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    auto scenePos = mapToScene(event->position().toPoint());
    auto trackIndex = dynamic_cast<TracksGraphicsScene *>(scene())->trackIndexAt(scenePos.y());
    if (trackIndex == -1)
        return;

    auto tick = dynamic_cast<TracksGraphicsScene *>(scene())->tickAt(scenePos.x());
    if (auto item = itemAt(event->pos())) {
        if (dynamic_cast<TracksBackgroundGraphicsItem *>(item)) {
            auto snapedTick = MathUtils::roundDown(tick, 1920 / m_quantize);
            emit addSingingClipTriggered(trackIndex, snapedTick);
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
            m_trackIndx = trackIndex;
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