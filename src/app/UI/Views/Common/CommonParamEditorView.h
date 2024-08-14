//
// Created by fluty on 2024/1/25.
//

#ifndef PITCHEDITORGRAPHICSITEM_H
#define PITCHEDITORGRAPHICSITEM_H

#include "OverlayGraphicsItem.h"
#include "Model/AppModel/DrawCurve.h"

class CommonParamEditorView final : public OverlayGraphicsItem {
    Q_OBJECT

public:
    enum EditMode { Free, Anchor, Off };
    explicit CommonParamEditorView();

    [[nodiscard]] EditMode editMode() const;
    void setEditMode(const EditMode &mode);
    void loadOriginal(const QList<DrawCurve *> &curves);
    void loadEdited(const QList<DrawCurve *> &curves);
    [[nodiscard]] const QList<DrawCurve*> &editedCurves() const;

signals:
    void editCompleted();

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    // void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    // void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    // void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void updateRectAndPos() override;
    // void drawOpensvipPitchParam(QPainter *painter);
    void drawHandDrawCurves(QPainter *painter, const QList<DrawCurve *> &curves) const;
    static void drawLine(const QPoint &p1, const QPoint &p2, DrawCurve &curve);

    EditMode m_editMode = Off;

    enum DrawCurveEditType { CreateNewCurve, EditExistCurve, None };

    QPoint m_mouseDownPos;
    QPoint m_prevPos;
    DrawCurve *m_editingCurve = nullptr;
    DrawCurveEditType m_drawCurveEditType = None;
    bool m_mouseMoved = false;
    QList<DrawCurve *> m_drawCurvesEdited;
    QList<DrawCurve *> m_drawCurvesOriginal;

    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    [[nodiscard]] double sceneXToTick(double x) const;
    [[nodiscard]] double tickToSceneX(double tick) const;
    [[nodiscard]] double sceneXToItemX(double x) const;
    [[nodiscard]] double tickToItemX(double tick) const;
    [[nodiscard]] double pitchToSceneY(double pitch) const;
    [[nodiscard]] double sceneYToItemY(double y) const;
    [[nodiscard]] double pitchToItemY(double pitch) const;
    [[nodiscard]] double sceneYToPitch(double y) const;
    DrawCurve *curveAt(double tick);
    QList<DrawCurve *> curvesIn(int startTick, int endTick);
};

#endif // PITCHEDITORGRAPHICSITEM_H
