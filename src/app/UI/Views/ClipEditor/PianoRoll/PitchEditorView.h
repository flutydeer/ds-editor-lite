//
// Created by fluty on 24-8-14.
//

#ifndef PITCHEDITORVIEW_H
#define PITCHEDITORVIEW_H

#include "Model/AppModel/ParamProperties.h"
#include "UI/Views/ClipEditor/CommonParamEditorView.h"

struct AnchorOverlayState;

class PitchEditorView final : public CommonParamEditorView {
    Q_OBJECT
public:
    PitchEditorView();

    void setAnchorOverlayState(const AnchorOverlayState *state);
    [[nodiscard]] double valueToSceneY(double value) const override;
    [[nodiscard]] double sceneYToValue(double y) const override;

private:
    void drawGraduates(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void drawOverlay(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void drawAnchorCurves(QPainter *painter) const;
    void drawPreviewCurve(QPainter *painter) const;
    void drawSelectionRect(QPainter *painter) const;

    const AnchorOverlayState *m_anchorState = nullptr;
    PitchParamProperties m_properties;
};



#endif // PITCHEDITORVIEW_H
