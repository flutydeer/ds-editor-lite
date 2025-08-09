//
// Created by fluty on 24-8-14.
//

#include "PitchEditorView.h"

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Utils/MathUtils.h"

PitchEditorView::PitchEditorView()
    : CommonParamEditorView(m_properties) {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
}

void PitchEditorView::drawGraduates(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                    QWidget *widget) {
    // CommonParamEditorView::drawGraduates(painter, option, widget);
}

double PitchEditorView::valueToSceneY(const double value) const {
    constexpr int min = 0;
    constexpr int max = 12700;
    return (12700 - MathUtils::clip(value, min, max) + 50) * scaleY() *
           ClipEditorGlobal::noteHeight / 100;
}

double PitchEditorView::sceneYToValue(const double y) const {
    constexpr int min = 0;
    constexpr int max = 12700;
    const auto value = -(y * 100 / ClipEditorGlobal::noteHeight / scaleY() - 12700 - 50);
    return MathUtils::clip(value, min, max);
}