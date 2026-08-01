//
// Created by fluty on 24-8-14.
//

#ifndef PITCHEDITORVIEW_H
#define PITCHEDITORVIEW_H

#include "PitchDisplayStrategy.h"
#include <lite/ProjectModel/AppModel/ParamProperties.h>
#include "UI/Views/ClipEditor/CommonParamEditorView.h"

struct AnchorOverlayState;
class QPainterPath;

class PitchEditorView final : public CommonParamEditorView {
    Q_OBJECT
public:
    PitchEditorView();
    ~PitchEditorView() override;

    void setDisplayMode(PitchDisplayMode mode);
    void setAnchorOverlayState(const AnchorOverlayState *state);
    void loadOriginal(const QList<DrawCurve *> &curves);
    void loadEdited(const QList<DrawCurve *> &curves);
    void clearParams();
    [[nodiscard]] double valueToSceneY(double value) const override;
    [[nodiscard]] double sceneYToValue(double y) const override;

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void drawGraduates(QPainter *painter, const QStyleOptionGraphicsItem *option,
                       QWidget *widget) override;
    [[nodiscard]] QPainterPath coveragePath(const QList<PitchDisplayInterval> &coverage) const;
    void drawCurveLayer(QPainter *painter, const QList<DrawCurve *> &curves, const QColor &color,
                        const QList<PitchDisplayInterval> &hiddenCoverage,
                        const QList<PitchDisplayInterval> &dashedCoverage) const;
    void rebuildMergedCurves();

    PitchParamProperties m_properties;
    QList<DrawCurve *> m_mergedCurves;
    const AnchorOverlayState *m_anchorState = nullptr;
    PitchDisplayMode m_displayMode = PitchDisplayMode::Final;
};

#endif // PITCHEDITORVIEW_H
