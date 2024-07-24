//
// Created by fluty on 2024/1/25.
//

#ifndef PITCHEDITORGRAPHICSITEM_H
#define PITCHEDITORGRAPHICSITEM_H

#include "Utils/OverlappableSerialList.h"
#include "UI/Views/Common/OverlayGraphicsItem.h"

class Curve;
class DrawCurve;

class PitchEditorGraphicsItem final : public OverlayGraphicsItem {
    Q_OBJECT

public:
    enum EditMode { Free, Anchor, Off };
    explicit PitchEditorGraphicsItem();

    [[nodiscard]] EditMode editMode() const;
    void setEditMode(const EditMode &mode);
    void loadOpensvipPitchParam();
    void loadOriginal(const OverlappableSerialList<DrawCurve> &curves);
    void loadEdited(const OverlappableSerialList<DrawCurve> &curves);
    [[nodiscard]] const OverlappableSerialList<DrawCurve> &editedCurves() const;

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
    void drawHandDrawCurves(QPainter *painter, const OverlappableSerialList<DrawCurve> &curves);
    static void drawLine(const QPoint &p1, const QPoint &p2, DrawCurve *curve);

    EditMode m_editMode = Off;

    enum DrawCurveEditType { CreateNewCurve, EditExistCurve, None };

    QPoint m_mouseDownPos;
    QPoint m_prevPos;
    DrawCurve *m_editingCurve = nullptr;
    DrawCurveEditType m_drawCurveEditType = None;
    bool m_mouseMoved = false;
    OverlappableSerialList<DrawCurve> m_drawCurvesEdited;
    OverlappableSerialList<DrawCurve> m_drawCurvesOriginal;

    QList<std::tuple<int, int>> m_opensvipPitchParam;

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
