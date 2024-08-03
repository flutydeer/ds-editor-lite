//
// Created by fluty on 2024/1/23.
//

#include "PianoRollGraphicsScene.h"

#include <QGraphicsSceneMouseEvent>

#include "Global/ClipEditorGlobal.h"
#include "GraphicsItem/PianoRollBackgroundGraphicsItem.h"
#include "Model/AppModel/AppModel.h"

using namespace ClipEditorGlobal;

PianoRollGraphicsScene::PianoRollGraphicsScene() {
    auto w = 1920 / 480 * pixelsPerQuarterNote * 100;
    auto h = 128 * noteHeight;
    setSceneSize(QSizeF(w, h));

    auto gridItem = new PianoRollBackgroundGraphicsItem;
    gridItem->setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    addTimeGrid(gridItem);
}
void PianoRollGraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->accept();
        return;
    }
    QGraphicsScene::mousePressEvent(event);
}