//
// Created by fluty on 24-8-14.
//

#ifndef PITCHEDITORVIEW_H
#define PITCHEDITORVIEW_H

#include "CommonParamEditorView.h"

class PitchEditorView : public CommonParamEditorView {
    Q_OBJECT
public:
    PitchEditorView();

protected:
    [[nodiscard]] double valueToSceneY(double value) const override;
    [[nodiscard]] double sceneYToValue(double y) const override;
    // void drawOpensvipPitchParam(QPainter *painter);
};



#endif // PITCHEDITORVIEW_H
