//
// Created by fluty on 26-4-30.
//

#ifndef EDITPITCHANCHORHANDLER_H
#define EDITPITCHANCHORHANDLER_H

#include "PianoRollEditHandler.h"

#include <QList>
#include <QPointF>
#include <QRectF>

#include <climits>
#include <utility>

class AnchorNode;
class AnchorCurve;
class QMenu;

struct DragNodeInfo {
    AnchorNode *node = nullptr;
    AnchorCurve *sourceCurve = nullptr;
    AnchorCurve *targetCurve = nullptr;
    int startTick = 0;
    int startValue = 0;
};

struct AnchorOverlayState {
    bool anchorVisible = false;
    bool anchorEditActive = false;
    bool editing = false;
    AnchorCurve *currentCurve = nullptr;
    QList<AnchorNode *> selectedNodes;
    AnchorNode *hoveredNode = nullptr;
    QPointF previewPos;
    bool showPreview = false;
    AnchorCurve *previewCurve = nullptr;

    QPointF dragStartPos;
    bool dragging = false;
    QList<DragNodeInfo> dragNodeInfos;

    QRectF selectionRect;
    bool selecting = false;

    QList<AnchorCurve *> visibleCurves;
    bool cursorInView = true;

    AnchorCurve *mergeCandidateCurve = nullptr;
    AnchorNode *mergeEndpointNode = nullptr;
    bool showMergePreview = false;
};

class EditPitchAnchorHandler final : public PianoRollEditHandler {
public:
    void activate() override;
    void deactivate() override;

    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;
    bool mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool keyPressEvent(QKeyEvent *event) override;

    void commit() override;
    void discard() override;

    [[nodiscard]] Qt::Orientations edgeAutoScrollAxes() const override;
    void continueDragAt(const QPoint &viewportPos) override;

    void setAlwaysVisible(bool visible);
    void loadFromModel(const QList<AnchorCurve *> &curves);

    [[nodiscard]] const AnchorOverlayState &overlayState() const;

private:
    AnchorOverlayState m_state;

    [[nodiscard]] AnchorNode *anchorNodeAt(const QPointF &scenePos) const;
    [[nodiscard]] AnchorCurve *anchorCurveAt(int tick, AnchorCurve *exclude = nullptr) const;
    [[nodiscard]] QList<AnchorCurve *> anchorCurves() const;
    [[nodiscard]] std::pair<int, int> getReachableBounds(AnchorCurve *curve) const;
    [[nodiscard]] AnchorCurve *findOwnerCurve(AnchorNode *node) const;
    [[nodiscard]] static AnchorNode *findNodeAtTick(AnchorCurve *curve, int tick,
                                                    AnchorNode *exclude = nullptr);
    void removeOverlappingNodes(AnchorCurve *curve, AnchorNode *keep);
    void transferNodeToCurve(AnchorNode *node, AnchorCurve *from, AnchorCurve *to);
    void detachNodeToNewCurve(AnchorNode *node, AnchorCurve *from);
    void cleanupEmptyCurve(AnchorCurve *curve);

    QList<AnchorCurve *> m_localCurves;
    bool m_committing = false;

    void enterEditingState(AnchorCurve *curve, AnchorNode *node = nullptr);
    void exitEditingState();
    void selectNode(AnchorNode *node);
    void clearSelection();
    void deleteSelectedNodes();
    void createAnchorAt(const QPointF &scenePos);
    void updatePreview(const QPointF &scenePos);
    void updateMergeCandidate(const QPointF &scenePos);
    void mergeCurves(AnchorCurve *target);
    void triggerRepaint();
    void updateNodeDragAt(const QPointF &scenePos);
    void updateSelectionRectAt(const QPointF &scenePos);

    static constexpr double kAnchorHitRadius = 6.0;
};

#endif // EDITPITCHANCHORHANDLER_H
