//
// Created by fluty on 26-4-30.
//

#ifndef EDITPITCHANCHORHANDLER_H
#define EDITPITCHANCHORHANDLER_H

#include "PianoRollEditHandler.h"

#include <QList>
#include <QPointF>
#include <QRectF>

class AnchorNode;
class AnchorCurve;
class QMenu;

struct AnchorOverlayState {
    bool anchorEditMode = false;
    bool editing = false;
    AnchorCurve *currentCurve = nullptr;
    QList<AnchorNode *> selectedNodes;
    AnchorNode *hoveredNode = nullptr;
    QPointF previewPos;
    bool showPreview = false;

    QPointF dragStartPos;
    bool dragging = false;

    QRectF selectionRect;
    bool selecting = false;

    QList<AnchorCurve *> visibleCurves;
    bool cursorInView = true;
};

class EditPitchAnchorHandler final : public PianoRollEditHandler {
public:
    void activate() override;
    void deactivate() override;

    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;
    bool mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool keyPressEvent(QKeyEvent *event) override;

    void commit() override;
    void discard() override;

    void setCursorInView(bool inView);

    [[nodiscard]] const AnchorOverlayState &overlayState() const;

private:
    AnchorOverlayState m_state;

    [[nodiscard]] AnchorNode *anchorNodeAt(const QPointF &scenePos) const;
    [[nodiscard]] AnchorCurve *anchorCurveAt(int tick) const;
    [[nodiscard]] QList<AnchorCurve *> anchorCurves() const;

    QList<AnchorCurve *> m_localCurves;

    void enterEditingState(AnchorCurve *curve, AnchorNode *node = nullptr);
    void exitEditingState();
    void selectNode(AnchorNode *node);
    void clearSelection();
    void deleteSelectedNodes();
    void createAnchorAt(const QPointF &scenePos);
    void updatePreview(const QPointF &scenePos);
    void triggerRepaint();

    static constexpr double kAnchorHitRadius = 6.0;
};

#endif // EDITPITCHANCHORHANDLER_H
