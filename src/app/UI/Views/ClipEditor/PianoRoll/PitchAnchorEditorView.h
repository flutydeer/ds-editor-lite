//
// Created by fluty on 26-4-30.
//

#ifndef PITCHANCHOREDITORVIEW_H
#define PITCHANCHOREDITORVIEW_H

#include "UI/Views/Common/TimeOverlayView.h"

#include <QColor>

struct AnchorOverlayState;
class AnchorNode;

class PitchAnchorEditorView : public TimeOverlayView {
    Q_OBJECT

public:
    PitchAnchorEditorView();

    void setOverlayState(const AnchorOverlayState *state);
    [[nodiscard]] double valueToSceneY(double value) const;
    [[nodiscard]] double sceneYToValue(double y) const;

    [[nodiscard]] QColor anchorColor() const;
    void setAnchorColor(const QColor &color);
    [[nodiscard]] QColor anchorSelectedColor() const;
    void setAnchorSelectedColor(const QColor &color);
    [[nodiscard]] QColor anchorCurveColor() const;
    void setAnchorCurveColor(const QColor &color);
    [[nodiscard]] QColor anchorPreviewColor() const;
    void setAnchorPreviewColor(const QColor &color);

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

    // Base colors; state-dependent alphas (active/preview tiers) are applied in draw methods
    QColor m_anchorColor = {220, 220, 220};
    QColor m_anchorSelectedColor = {155, 186, 255};
    QColor m_anchorCurveColor = {220, 220, 220};
    QColor m_anchorPreviewColor = {155, 186, 255};
};

#endif // PITCHANCHOREDITORVIEW_H
