#ifndef PITCHEDITORGRAPHICSITEM_H
#define PITCHEDITORGRAPHICSITEM_H

#include "Interface/IAtomicAction.h"
#include "UI/Views/ClipEditor/DrawCurveEditUtils.h"
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include "UI/Views/Common/TimeOverlayView.h"

#include <QColor>

class ParamProperties;
class AnchorCurve;
class QPainterPath;
class QPen;

class CommonParamEditorView : public TimeOverlayView, public IAtomicAction {
    Q_OBJECT

public:
    explicit CommonParamEditorView(const ParamProperties &properties);
    ~CommonParamEditorView() override;

    void setParamProperties(const ParamProperties &properties);
    void loadOriginal(const QList<DrawCurve *> &curves);
    void loadEdited(const QList<DrawCurve *> &curves);
    void loadAnchorEdited(const QList<AnchorCurve *> &curves);
    void clearParams();
    void cancelEdit();
    void setEraseMode(bool on);
    void setBakeMode(bool on);
    void setBaseCurveVisible(bool visible);
    [[nodiscard]] const QList<DrawCurve *> &editedCurves() const;
    [[nodiscard]] double sceneYForValue(double value) const;
    [[nodiscard]] double valueAtSceneY(double y) const;
    void discardAction() override;
    void commitAction() override;

    [[nodiscard]] QColor graduateColor() const;
    void setGraduateColor(const QColor &color);
    [[nodiscard]] QColor originalCurveColor() const;
    void setOriginalCurveColor(const QColor &color);
    [[nodiscard]] QColor editedCurveColor() const;
    void setEditedCurveColor(const QColor &color);
    [[nodiscard]] QColor backgroundLayerColor() const;
    void setBackgroundLayerColor(const QColor &color);

    [[nodiscard]] bool useTrackColorForEditedCurve() const;
    void setUseTrackColorForEditedCurve(bool on);

signals:
    void editStarted();
    void editCommitted();
    void editDiscarded();
    void editCompleted(const QList<DrawCurve *> &curves);

protected:
    [[nodiscard]] virtual double valueToSceneY(double value) const;
    [[nodiscard]] virtual double sceneYToValue(double y) const;
    virtual void drawGraduates(QPainter *painter, const QStyleOptionGraphicsItem *option,
                               QWidget *widget);
    [[nodiscard]] const QList<DrawCurve *> &originalCurves() const;
    void drawCurveBorder(QPainter *painter, const QList<DrawCurve *> &curves) const;
    [[nodiscard]] QColor resolvedEditedCurveColor() const;

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void updateRectAndPos() override;
    bool cancelEditState();
    void drawCurvePolygon(QPainter *painter, const QList<DrawCurve *> &curves) const;
    void drawEditedCurveBorders(QPainter *painter, const QPen &pen) const;
    [[nodiscard]] QPainterPath anchorCoveragePath() const;
    bool m_showDebugInfo = false;

    enum EditType { Draw, Erase, Bake, None };

    bool m_mouseDown = false;
    Qt::MouseButton m_mouseDownButton = Qt::NoButton;
    QPoint m_mouseDownPos; // x: tick, y: value
    QPoint m_prevPos;
    DrawCurveEditUtils::StrokeState m_drawStroke;
    EditType m_editType = None;
    bool m_eraseMode = false;
    bool m_bakeMode = false;
    bool m_baseCurveVisible = true;
    bool m_mouseMoved = false;
    QList<DrawCurve *> m_drawCurvesEdited;
    QList<DrawCurve *> m_anchorCurvesEdited;
    QList<DrawCurve *> m_drawCurvesOriginal;
    QList<DrawCurve *> m_drawCurvesEditedBak;

    [[nodiscard]] double valueToItemY(double value) const;
    const int paddingTopBottom = 2;
    const ParamProperties *m_properties;

    // Base colors; per-layer alpha (foreground/background) is applied in paint()
    QColor m_graduateColor = {72, 75, 78};
    QColor m_originalCurveColor = {255, 255, 255, 96};
    QColor m_editedCurveColor = {255, 255, 255};
    QColor m_backgroundLayerColor = {41, 44, 54};
    bool m_useTrackColorForEditedCurve = false;
};

#endif // PITCHEDITORGRAPHICSITEM_H
