#ifndef ANCHOROVERLAYVIEW_H
#define ANCHOROVERLAYVIEW_H

#include "AnchorEditController.h"
#include "UI/Views/Common/TimeOverlayView.h"

#include <QColor>

#include <functional>

class AnchorNode;
class QPainterPath;

class AnchorOverlayView : public TimeOverlayView {
    Q_OBJECT

public:
    enum class DisplayMode { Final, Draw, Anchor };
    using ValueMapper = std::function<double(double)>;

    AnchorOverlayView(ValueMapper valueToSceneY, ValueMapper sceneYToValue);

    void setOverlayState(const AnchorEditor::AnchorOverlayState *state);
    void setDisplayMode(DisplayMode mode);
    void setValueMappers(ValueMapper valueToSceneY, ValueMapper sceneYToValue);
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

    [[nodiscard]] QPainterPath interpolatedPath(const QList<AnchorNode *> &nodes,
                                                double devicePixelRatio) const;

    const AnchorEditor::AnchorOverlayState *m_state = nullptr;
    DisplayMode m_displayMode = DisplayMode::Final;
    ValueMapper m_valueToSceneY;
    ValueMapper m_sceneYToValue;

    // Base colors; state-dependent alphas (active/preview tiers) are applied in draw methods
    QColor m_anchorColor = {220, 220, 220};
    QColor m_anchorSelectedColor = {155, 186, 255};
    QColor m_anchorCurveColor = {220, 220, 220};
    QColor m_anchorPreviewColor = {155, 186, 255};
};

#endif // ANCHOROVERLAYVIEW_H
