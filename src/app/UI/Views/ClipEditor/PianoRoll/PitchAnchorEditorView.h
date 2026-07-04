//
// Created by fluty on 26-4-30.
//

#ifndef PITCHANCHOREDITORVIEW_H
#define PITCHANCHOREDITORVIEW_H

#include "UI/Views/Common/TimeOverlayView.h"

struct AnchorOverlayState;
class AnchorNode;

class PitchAnchorEditorView : public TimeOverlayView {
    Q_OBJECT

public:
    PitchAnchorEditorView();

    void setOverlayState(const AnchorOverlayState *state);
    [[nodiscard]] double valueToSceneY(double value) const;
    [[nodiscard]] double sceneYToValue(double y) const;

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void updateRectAndPos() override;

    void drawAnchorCurves(QPainter *painter) const;
    void drawPreviewCurve(QPainter *painter) const;
    void drawMergePreviewCurve(QPainter *painter) const;
    void drawDragPreviewCurve(QPainter *painter) const;
    void drawSelectionRect(QPainter *painter) const;

    void interpolateSegment(QPainter *painter, AnchorNode *n1, AnchorNode *n2,
                            AnchorNode *ref1, AnchorNode *ref2) const;

    const AnchorOverlayState *m_state = nullptr;
};

#endif // PITCHANCHOREDITORVIEW_H
