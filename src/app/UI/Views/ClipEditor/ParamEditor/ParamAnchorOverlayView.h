#ifndef PARAMANCHOROVERLAYVIEW_H
#define PARAMANCHOROVERLAYVIEW_H

#include "UI/Views/ClipEditor/AnchorEditor/AnchorOverlayView.h"

namespace AnchorEditor {
    class AnchorEditController;
}
class QGraphicsSceneContextMenuEvent;

class ParamAnchorOverlayView final : public AnchorOverlayView {
    Q_OBJECT

public:
    ParamAnchorOverlayView(AnchorEditor::AnchorEditController *controller,
                           ValueMapper valueToSceneY, ValueMapper sceneYToValue);
    void setInteractive(bool interactive);
    void setAnchorsVisible(bool visible);

signals:
    void autoScrollRequested(Qt::Orientations axes);
    void autoScrollStopped();
    void contextMenuRequested(QPointF scenePos, QPoint screenPos);

private:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    AnchorEditor::AnchorEditController *m_controller;
};

#endif // PARAMANCHOROVERLAYVIEW_H
