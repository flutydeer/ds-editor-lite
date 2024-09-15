//
// Created by fluty on 24-8-14.
//

#include "PitchEditorView.h"

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Utils/MathUtils.h"

PitchEditorView::PitchEditorView() {
    setFillCurve(false);
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
}

double PitchEditorView::valueToSceneY(double value) const {
    int min = 0;
    int max = 12700;
    return (12700 - MathUtils::clip(value, min, max) + 50) * scaleY() *
           ClipEditorGlobal::noteHeight / 100;
}

double PitchEditorView::sceneYToValue(double y) const {
    int min = 0;
    int max = 12700;
    auto value = -(y * 100 / ClipEditorGlobal::noteHeight / scaleY() - 12700 - 50);
    return MathUtils::clip(value, min, max);
}

// void CommonParamEditorView::drawOpensvipPitchParam(QPainter *painter) {
//     QPainterPath path;
//     bool firstPoint = true;
//     int prevPos = 0;
//     int prevValue = 0;
//     for (const auto &point : m_opensvipPitchParam) {
//         auto pos = std::get<0>(point) - 480 * 3; // opensvip's "feature"
//         auto value = std::get<1>(point);
//
//         if (pos < startTick()) {
//             prevPos = pos;
//             prevValue = value;
//             continue;
//         }
//
//         if (firstPoint) {
//             path.moveTo(tickToItemX(prevPos), pitchToItemY(prevValue));
//             path.lineTo(tickToItemX(pos), pitchToItemY(value));
//             firstPoint = false;
//         } else
//             path.lineTo(tickToItemX(pos), pitchToItemY(value));
//
//         if (pos > endTick()) {
//             path.lineTo(tickToItemX(pos), pitchToItemY(value));
//             break;
//         }
//     }
//     painter->drawPath(path);
// }