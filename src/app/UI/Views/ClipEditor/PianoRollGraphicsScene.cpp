//
// Created by fluty on 2024/1/23.
//

#include <QGraphicsSceneMouseEvent>

#include "Global/ClipEditorGlobal.h"
#include "PianoRollGraphicsScene.h"

using namespace ClipEditorGlobal;

PianoRollGraphicsScene::PianoRollGraphicsScene() {
    auto w = 1920 / 480 * pixelsPerQuarterNote * 100;
    auto h = 128 * noteHeight;
    setSceneSize(QSizeF(w, h));
}
void PianoRollGraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->accept();
        return;
    }
    QGraphicsScene::mousePressEvent(event);
}