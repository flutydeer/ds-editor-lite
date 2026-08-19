#ifndef ANCHOREDITCONTROLLER_H
#define ANCHOREDITCONTROLLER_H

#include <lite/ProjectModel/AppModel/AnchorCurve.h>

#include <QList>
#include <QPointF>
#include <QRectF>

#include <functional>
#include <utility>

namespace AnchorEditor {

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
        QPointF previewScenePos;
        int previewTick = 0;
        bool showPreview = false;
        AnchorCurve *previewCurve = nullptr;

        QPointF dragStartScenePos;
        bool dragging = false;
        QList<DragNodeInfo> dragNodeInfos;

        QRectF selectionSceneRect;
        bool selecting = false;

        QList<AnchorCurve *> visibleCurves;
        bool cursorInView = true;

        AnchorCurve *mergeCandidateCurve = nullptr;
        AnchorNode *mergeEndpointNode = nullptr;
        bool showMergePreview = false;
    };

    struct CoordinateMapper {
        std::function<int(double)> sceneXToTick;
        std::function<double(int)> tickToSceneX;
        std::function<int(double)> sceneYToValue;
        std::function<double(int)> valueToSceneY;
    };

    enum class EditFinishReason { Commit, Discard };

    struct MenuInfo {
        bool valid = false;
        bool interpolationEnabled = false;
        bool mixedInterpolation = false;
        AnchorNode::InterpMode interpolation = AnchorNode::None;
    };

    class AnchorEditController final {
    public:
        struct HostCallbacks {
            std::function<bool()> beginEdit;
            std::function<void(const QList<AnchorCurve *> &)> publish;
            std::function<void(EditFinishReason)> finishEdit;
            std::function<void()> stateChanged;
        };

        AnchorEditController() = default;
        ~AnchorEditController();
        AnchorEditController(const AnchorEditController &) = delete;
        AnchorEditController &operator=(const AnchorEditController &) = delete;

        void setCoordinateMapper(CoordinateMapper mapper);
        void setHostCallbacks(HostCallbacks callbacks);
        void setEditActive(bool active);
        void setAlwaysVisible(bool visible);
        void loadFromModel(const QList<AnchorCurve *> &curves);

        [[nodiscard]] const QList<AnchorCurve *> &curves() const;
        [[nodiscard]] const AnchorOverlayState &state() const;
        [[nodiscard]] quint64 curveRevision() const;

        bool pressAt(const QPointF &scenePos, Qt::MouseButton button);
        bool moveAt(const QPointF &scenePos, Qt::MouseButtons buttons);
        bool releaseAt(const QPointF &scenePos, Qt::MouseButton button);
        void doubleClickAt(const QPointF &scenePos, Qt::MouseButton button);
        void hoverEnter();
        void hoverMoveAt(const QPointF &scenePos);
        void hoverLeave();
        void cancel();
        void exitEditing();

        [[nodiscard]] Qt::Orientations edgeAutoScrollAxes() const;
        void continueDragAtScene(const QPointF &scenePos);

        bool prepareMenu(const QPointF &scenePos, MenuInfo &info);
        void setSelectedInterpolation(AnchorNode::InterpMode mode);
        void deleteSelectedNodes();

    private:
        [[nodiscard]] bool mapperReady() const;
        [[nodiscard]] AnchorNode *anchorNodeAt(const QPointF &scenePos) const;
        [[nodiscard]] AnchorCurve *anchorCurveAt(int tick, AnchorCurve *exclude = nullptr) const;
        [[nodiscard]] std::pair<int, int> reachableBounds(AnchorCurve *curve) const;
        [[nodiscard]] AnchorCurve *findOwnerCurve(AnchorNode *node) const;
        [[nodiscard]] static AnchorNode *findNodeAtTick(AnchorCurve *curve, int tick,
                                                        AnchorNode *exclude = nullptr);
        void removeOverlappingNodes(AnchorCurve *curve, AnchorNode *keep);
        void cleanupIncompleteCurve(AnchorCurve *curve);
        void discardProvisionalCurve();

        void enterEditingState(AnchorCurve *curve, AnchorNode *node = nullptr);
        void exitEditingState();
        void selectNode(AnchorNode *node);
        void clearSelection();
        void createAnchorAt(const QPointF &scenePos);
        void updatePreview(const QPointF &scenePos);
        void updateMergeCandidate(const QPointF &scenePos);
        void mergeCurves(AnchorCurve *target);
        void updateNodeDragAt(const QPointF &scenePos);
        void updateSelectionRectAt(const QPointF &scenePos);

        bool beginMutation();
        void commitMutationIfReady();
        void commitMutation();
        void discardMutation();
        void removeIncompleteCurves();
        void clearInteractionState(bool leaveEditing);
        void notifyChanged();
        void notifyCurvesChanged();
        void replaceOwnedCurves(const QList<AnchorCurve *> &source,
                                QList<AnchorCurve *> &destination);

        CoordinateMapper m_mapper;
        HostCallbacks m_callbacks;
        AnchorOverlayState m_state;
        QList<AnchorCurve *> m_curves;
        QList<AnchorCurve *> m_backupCurves;
        AnchorCurve *m_provisionalCurve = nullptr;
        bool m_mutationActive = false;
        bool m_publishing = false;
        quint64 m_curveRevision = 0;

        static constexpr double kAnchorHitRadius = 6.0;
        static constexpr double kDragThreshold = 3.0;
    };

} // namespace AnchorEditor

#endif // ANCHOREDITCONTROLLER_H
