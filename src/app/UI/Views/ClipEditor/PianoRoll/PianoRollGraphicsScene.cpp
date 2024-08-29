//
// Created by fluty on 2024/1/23.
//

#include "PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

PianoRollGraphicsScene::PianoRollGraphicsScene() {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    auto w = 1920.0 / 480 * ClipEditorGlobal::pixelsPerQuarterNote * 80;// TODO: Use project length
    auto h = 128 * ClipEditorGlobal::noteHeight;
    setSceneBaseSize(QSizeF(w, h));
}

void PianoRollGraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    // if (event->button() != Qt::LeftButton) {
    //     event->accept();
    //     return;
    // }
    QGraphicsScene::mousePressEvent(event);
}