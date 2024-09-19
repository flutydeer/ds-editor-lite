//
// Created by fluty on 2024/1/25.
//

#ifndef PITCHEDITORGRAPHICSITEM_H
#define PITCHEDITORGRAPHICSITEM_H

#include "Interface/IAtomicAction.h"
#include "Model/AppModel/DrawCurve.h"
#include "UI/Views/Common/TimeOverlayGraphicsItem.h"

class CommonParamEditorView : public TimeOverlayGraphicsItem, public IAtomicAction {
    Q_OBJECT

public:
    enum EditMode { Free, Anchor, Off };

    explicit CommonParamEditorView();

    void loadOriginal(const QList<DrawCurve *> &curves);
    void loadEdited(const QList<DrawCurve *> &curves);
    void clearParams();
    void setEraseMode(bool on);
    [[nodiscard]] const QList<DrawCurve *> &editedCurves() const;
    void discardAction() override;
    void commitAction() override;

signals:
    void editCompleted(const QList<DrawCurve *> &curves);

protected:
    [[nodiscard]] bool fillCurve() const;
    void setFillCurve(bool on);
    [[nodiscard]] virtual double valueToSceneY(double value) const;
    [[nodiscard]] virtual double sceneYToValue(double y) const;

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void updateRectAndPos() override;
    void drawCurves(QPainter *painter, const QList<DrawCurve *> &curves) const;
    static void drawLine(const QPoint &p1, const QPoint &p2, DrawCurve &curve);

    bool m_showDebugInfo = false;
    bool m_fillCurve = true;

    enum EditType { DrawOnInterval, DrawOnCurve, Erase, None };

    bool m_mouseDown = false;
    Qt::MouseButton m_mouseDownButton = Qt::NoButton;
    QPoint m_mouseDownPos; // x: tick, y: value
    QPoint m_prevPos;
    DrawCurve *m_editingCurve = nullptr;
    EditType m_editType = None;
    bool m_eraseMode = false;
    bool m_newCurveCreated = false;
    bool m_mouseMoved = false;
    QList<DrawCurve *> m_drawCurvesEdited;
    QList<DrawCurve *> m_drawCurvesOriginal;
    QList<DrawCurve *> m_drawCurvesEditedBak;

    [[nodiscard]] double valueToItemY(double value) const;
    DrawCurve *curveAt(double tick);
    QList<DrawCurve *> curvesIn(int startTick, int endTick);

    const int paddingTopBottom = 6;
};

#endif // PITCHEDITORGRAPHICSITEM_H
