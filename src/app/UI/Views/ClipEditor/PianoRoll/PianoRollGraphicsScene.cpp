//
// Created by fluty on 2024/1/23.
//

#include "PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Global/AppGlobal.h"

PianoRollGraphicsScene::PianoRollGraphicsScene() {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    constexpr auto w =
        static_cast<double>(AppGlobal::ticksPerWholeNote) / AppGlobal::ticksPerQuarterNote * ClipEditorGlobal::pixelsPerQuarterNote * 80; // TODO: Use project length
    constexpr auto h = 128 * ClipEditorGlobal::noteHeight;
    setSceneBaseSize(QSizeF(w, h));
}

void PianoRollGraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    // if (event->button() != Qt::LeftButton) {
    //     event->accept();
    //     return;
    // }
    QGraphicsScene::mousePressEvent(event);
}