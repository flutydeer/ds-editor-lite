//
// Created by fluty on 24-8-14.
//

#ifndef PITCHEDITORVIEW_H
#define PITCHEDITORVIEW_H

#include "UI/Views/ClipEditor/CommonParamEditorView.h"

class PitchEditorView : public CommonParamEditorView {
    Q_OBJECT
public:
    PitchEditorView();

private:
    void drawGraduates(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    [[nodiscard]] double valueToSceneY(double value) const override;
    [[nodiscard]] double sceneYToValue(double y) const override;
    // void drawOpensvipPitchParam(QPainter *painter);
};



#endif // PITCHEDITORVIEW_H
