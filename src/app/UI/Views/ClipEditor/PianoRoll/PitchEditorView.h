//
// Created by fluty on 24-8-14.
//

#ifndef PITCHEDITORVIEW_H
#define PITCHEDITORVIEW_H

#include "Model/AppModel/ParamProperties.h"
#include "UI/Views/ClipEditor/CommonParamEditorView.h"

class PitchEditorView final : public CommonParamEditorView {
    Q_OBJECT
public:
    PitchEditorView();

private:
    void drawGraduates(QPainter *painter, const QStyleOptionGraphicsItem *option,
                       QWidget *widget) override;
    double valueToSceneY(double value) const override;
    double sceneYToValue(double y) const override;

    PitchParamProperties m_properties;
};



#endif // PITCHEDITORVIEW_H
