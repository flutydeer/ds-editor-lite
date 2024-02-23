//
// Created by fluty on 2024/1/25.
//

#ifndef PITCHEDITORGRAPHICSITEM_H
#define PITCHEDITORGRAPHICSITEM_H

#include "Utils/OverlapableSerialList.h"
#include "Views/Common/OverlayGraphicsItem.h"

class Curve;
class DrawCurve;

class PitchEditorGraphicsItem final : public OverlayGraphicsItem {
    Q_OBJECT

public:
    enum EditMode { Free, Anchor, Off };
    explicit PitchEditorGraphicsItem();

    EditMode editMode() const;
    void setEditMode(const EditMode &mode);
    void loadOpensvipPitchParam();
    void loadOriginal(const OverlapableSerialList<DrawCurve> &curves);
    void loadEdited(const OverlapableSerialList<DrawCurve> &curves);
    const OverlapableSerialList<DrawCurve> &editedCurves() const;

signals:
    void editCompleted();

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    // void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    // void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    // void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    // void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void updateRectAndPos() override;
    void drawOpensvipPitchParam(QPainter *painter);
    void drawHandDrawCurves(QPainter *painter, const OverlapableSerialList<DrawCurve> &curves);
    void drawLine(const QPoint &p1, const QPoint &p2, DrawCurve *curve);

    EditMode m_editMode = Off;

    enum DrawCurveEditType { CreateNewCurve, EditExistCurve, None };

    QPoint m_mouseDownPos;
    QPoint m_prevPos;
    DrawCurve *m_editingCurve = nullptr;
    DrawCurveEditType m_drawCurveEditType = None;
    bool m_mouseMoved = false;
    OverlapableSerialList<DrawCurve> m_drawCurvesEdited;
    OverlapableSerialList<DrawCurve> m_drawCurvesOriginal;

    QList<std::tuple<int, int>> m_opensvipPitchParam;

    double startTick() const;
    double endTick() const;
    double sceneXToTick(double x) const;
    double tickToSceneX(double tick) const;
    double sceneXToItemX(double x) const;
    double tickToItemX(double tick) const;
    double pitchToSceneY(double pitch) const;
    double sceneYToItemY(double y) const;
    double pitchToItemY(double pitch) const;
    double sceneYToPitch(double y) const;

    DrawCurve *curveAt(double tick);
    QList<DrawCurve *> curvesIn(int startTick, int endTick);
};

#endif // PITCHEDITORGRAPHICSITEM_H
