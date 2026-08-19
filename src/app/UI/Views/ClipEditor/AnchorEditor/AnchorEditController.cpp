#include "AnchorEditController.h"
#include "AnchorEditUtils.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <climits>

namespace AnchorEditor {

    AnchorEditController::~AnchorEditController() {
        qDeleteAll(m_backupCurves);
        qDeleteAll(m_curves);
    }

    void AnchorEditController::setCoordinateMapper(CoordinateMapper mapper) {
        m_mapper = std::move(mapper);
    }

    void AnchorEditController::setHostCallbacks(HostCallbacks callbacks) {
        m_callbacks = std::move(callbacks);
    }

    void AnchorEditController::setEditActive(const bool active) {
        if (m_state.anchorEditActive == active)
            return;
        if (!active)
            cancel();
        m_state.anchorEditActive = active;
        if (active)
            m_state.cursorInView = true;
        notifyChanged();
    }

    void AnchorEditController::setAlwaysVisible(const bool visible) {
        if (m_state.anchorVisible == visible)
            return;
        m_state.anchorVisible = visible;
        notifyChanged();
    }

    void AnchorEditController::loadFromModel(const QList<AnchorCurve *> &curves) {
        if (m_publishing)
            return;
        if (m_mutationActive)
            discardMutation();
        clearInteractionState(true);
        m_provisionalCurve = nullptr;
        replaceOwnedCurves(curves, m_curves);
        m_state.visibleCurves = m_curves;
        notifyCurvesChanged();
    }

    const QList<AnchorCurve *> &AnchorEditController::curves() const {
        return m_curves;
    }

    const AnchorOverlayState &AnchorEditController::state() const {
        return m_state;
    }

    quint64 AnchorEditController::curveRevision() const {
        return m_curveRevision;
    }

    bool AnchorEditController::pressAt(const QPointF &scenePos, const Qt::MouseButton button) {
        if (!m_state.anchorEditActive || !mapperReady())
            return false;
        if (button != Qt::LeftButton)
            return true;

        auto *node = anchorNodeAt(scenePos);
        if (m_state.editing) {
            if (node) {
                if (m_state.showMergePreview && node == m_state.mergeEndpointNode) {
                    if (beginMutation()) {
                        mergeCurves(m_state.mergeCandidateCurve);
                        commitMutationIfReady();
                        notifyCurvesChanged();
                    }
                } else {
                    if (!m_state.selectedNodes.contains(node))
                        selectNode(node);
                    m_state.dragStartScenePos = scenePos;
                    m_state.dragging = false;
                    m_state.dragNodeInfos.clear();
                }
            } else if (beginMutation()) {
                createAnchorAt(scenePos);
                m_state.dragStartScenePos = scenePos;
                m_state.dragging = false;
                commitMutationIfReady();
                notifyCurvesChanged();
            }
        } else if (node) {
            selectNode(node);
            enterEditingState(m_state.currentCurve, node);
            m_state.dragStartScenePos = scenePos;
            m_state.dragging = false;
            m_state.dragNodeInfos.clear();
        } else {
            m_state.selectionSceneRect = QRectF(scenePos, QSizeF());
            m_state.selecting = true;
        }
        notifyChanged();
        return true;
    }

    bool AnchorEditController::moveAt(const QPointF &scenePos, const Qt::MouseButtons buttons) {
        if (!m_state.anchorEditActive || !mapperReady())
            return false;

        if (buttons & Qt::LeftButton) {
            if (m_state.editing && !m_state.selectedNodes.isEmpty()) {
                const auto delta = scenePos - m_state.dragStartScenePos;
                if (!m_state.dragging && delta.manhattanLength() > kDragThreshold) {
                    if (!beginMutation())
                        return true;
                    m_state.dragging = true;
                }
                if (m_state.dragging)
                    updateNodeDragAt(scenePos);
                return true;
            }
            if (m_state.selecting) {
                updateSelectionRectAt(scenePos);
                return true;
            }
        } else {
            hoverMoveAt(scenePos);
        }
        return true;
    }

    bool AnchorEditController::releaseAt(const QPointF &scenePos, const Qt::MouseButton button) {
        if (!m_state.anchorEditActive || button != Qt::LeftButton)
            return false;

        if (m_state.dragging) {
            m_state.dragging = false;
            QSet<AnchorCurve *> sourcesToCleanup;
            for (auto &info : m_state.dragNodeInfos) {
                if (info.targetCurve && info.targetCurve != info.sourceCurve) {
                    info.sourceCurve->removeNode(info.node);
                    info.targetCurve->insertNode(info.node);
                    sourcesToCleanup.insert(info.sourceCurve);
                }
            }
            for (auto &info : m_state.dragNodeInfos) {
                auto *finalCurve = info.targetCurve ? info.targetCurve : info.sourceCurve;
                removeOverlappingNodes(finalCurve, info.node);
            }
            for (auto *curve : sourcesToCleanup)
                cleanupIncompleteCurve(curve);
            if (!m_state.selectedNodes.isEmpty())
                m_state.currentCurve = findOwnerCurve(m_state.selectedNodes.first());
            m_state.dragNodeInfos.clear();
            updatePreview(scenePos);
            commitMutationIfReady();
            notifyChanged();
            return true;
        }

        if (m_state.selecting) {
            m_state.selecting = false;
            if (!m_state.selectedNodes.isEmpty()) {
                QSet<AnchorCurve *> involvedCurves;
                for (auto *selected : m_state.selectedNodes) {
                    if (auto *owner = findOwnerCurve(selected))
                        involvedCurves.insert(owner);
                }
                if (involvedCurves.size() == 1)
                    enterEditingState(*involvedCurves.begin());
                else
                    m_state.editing = true;
            }
            m_state.selectionSceneRect = {};
            notifyChanged();
        }
        return true;
    }

    void AnchorEditController::doubleClickAt(const QPointF &scenePos,
                                             const Qt::MouseButton button) {
        if (!m_state.anchorEditActive || button != Qt::LeftButton || !mapperReady())
            return;
        if (anchorNodeAt(scenePos))
            return;
        if (beginMutation()) {
            createAnchorAt(scenePos);
            m_state.dragStartScenePos = scenePos;
            m_state.dragging = false;
            commitMutationIfReady();
            notifyCurvesChanged();
        }
    }

    void AnchorEditController::hoverEnter() {
        m_state.cursorInView = true;
        notifyChanged();
    }

    void AnchorEditController::hoverMoveAt(const QPointF &scenePos) {
        if (!m_state.anchorEditActive || !mapperReady())
            return;
        auto *hovered = anchorNodeAt(scenePos);
        const bool hoverChanged = hovered != m_state.hoveredNode;
        m_state.hoveredNode = hovered;
        if (m_state.editing) {
            updateMergeCandidate(scenePos);
            updatePreview(scenePos);
        }
        if (hoverChanged || m_state.editing)
            notifyChanged();
    }

    void AnchorEditController::hoverLeave() {
        m_state.cursorInView = false;
        m_state.hoveredNode = nullptr;
        m_state.showPreview = false;
        m_state.showMergePreview = false;
        notifyChanged();
    }

    void AnchorEditController::cancel() {
        if (m_mutationActive)
            discardMutation();
        clearInteractionState(true);
        notifyChanged();
    }

    void AnchorEditController::exitEditing() {
        if (m_mutationActive)
            discardMutation();
        exitEditingState();
        m_state.dragging = false;
        m_state.selecting = false;
        m_state.dragNodeInfos.clear();
        m_state.selectionSceneRect = {};
        notifyChanged();
    }

    Qt::Orientations AnchorEditController::edgeAutoScrollAxes() const {
        if ((m_state.editing && m_state.dragging) || m_state.selecting)
            return Qt::Horizontal | Qt::Vertical;
        return {};
    }

    void AnchorEditController::continueDragAtScene(const QPointF &scenePos) {
        if (m_state.editing && m_state.dragging && !m_state.selectedNodes.isEmpty())
            updateNodeDragAt(scenePos);
        else if (m_state.selecting)
            updateSelectionRectAt(scenePos);
    }

    bool AnchorEditController::prepareMenu(const QPointF &scenePos, MenuInfo &info) {
        info = {};
        auto *node = anchorNodeAt(scenePos);
        if (!node) {
            if (m_state.editing)
                exitEditing();
            return false;
        }

        if (m_state.selectedNodes.size() <= 1 || !m_state.selectedNodes.contains(node)) {
            clearSelection();
            selectNode(node);
            enterEditingState(m_state.currentCurve, node);
        }
        notifyChanged();

        const auto currentMode = m_state.selectedNodes.first()->interpMode();
        const auto allSame =
            std::all_of(m_state.selectedNodes.cbegin(), m_state.selectedNodes.cend(),
                        [currentMode](const AnchorNode *selected) {
                            return selected->interpMode() == currentMode;
                        });
        const auto isLastNode = [this](AnchorNode *selected) {
            const auto *owner = findOwnerCurve(selected);
            const auto nodes = owner ? owner->nodes().toList() : QList<AnchorNode *>();
            return !nodes.isEmpty() && nodes.last() == selected;
        };
        const auto containsLast =
            std::any_of(m_state.selectedNodes.cbegin(), m_state.selectedNodes.cend(), isLastNode);
        info.valid = true;
        info.interpolationEnabled = !containsLast;
        info.mixedInterpolation = !allSame;
        info.interpolation = currentMode;
        return true;
    }

    void AnchorEditController::setSelectedInterpolation(const AnchorNode::InterpMode mode) {
        if (mode != AnchorNode::Linear && mode != AnchorNode::Hermite)
            return;
        bool needsChange = false;
        for (auto *node : m_state.selectedNodes) {
            const auto *owner = findOwnerCurve(node);
            const auto nodes = owner ? owner->nodes().toList() : QList<AnchorNode *>();
            if (!nodes.isEmpty() && nodes.last() != node && node->interpMode() != mode) {
                needsChange = true;
                break;
            }
        }
        if (!needsChange || !beginMutation())
            return;
        for (auto *node : m_state.selectedNodes) {
            const auto *owner = findOwnerCurve(node);
            const auto nodes = owner ? owner->nodes().toList() : QList<AnchorNode *>();
            if (!nodes.isEmpty() && nodes.last() != node)
                node->setInterpMode(mode);
        }
        commitMutationIfReady();
        notifyCurvesChanged();
    }

    void AnchorEditController::deleteSelectedNodes() {
        if (m_state.selectedNodes.isEmpty() || !beginMutation())
            return;

        QHash<AnchorCurve *, QList<AnchorNode *>> nodesByCurve;
        for (auto *node : m_state.selectedNodes) {
            if (auto *curve = findOwnerCurve(node))
                nodesByCurve[curve].append(node);
        }
        const bool discardingOnlyProvisional =
            m_provisionalCurve && nodesByCurve.size() == 1 &&
            nodesByCurve.contains(m_provisionalCurve);
        clearSelection();
        for (auto it = nodesByCurve.begin(); it != nodesByCurve.end(); ++it) {
            auto *curve = it.key();
            for (auto *node : it.value()) {
                curve->removeNode(node);
                if (m_state.hoveredNode == node)
                    m_state.hoveredNode = nullptr;
                delete node;
            }
            const auto remaining = curve->nodes().toList();
            if (remaining.size() < 2) {
                m_curves.removeOne(curve);
                if (m_state.currentCurve == curve)
                    m_state.currentCurve = nullptr;
                if (m_state.hoveredNode && remaining.contains(m_state.hoveredNode))
                    m_state.hoveredNode = nullptr;
                if (m_provisionalCurve == curve)
                    m_provisionalCurve = nullptr;
                delete curve;
            } else {
                remaining.last()->setInterpMode(AnchorNode::None);
            }
        }
        if (!m_state.currentCurve)
            exitEditingState();
        if (discardingOnlyProvisional) {
            discardMutation();
            notifyChanged();
            return;
        }
        commitMutationIfReady();
        notifyCurvesChanged();
    }

    bool AnchorEditController::mapperReady() const {
        return m_mapper.sceneXToTick && m_mapper.tickToSceneX && m_mapper.sceneYToValue &&
               m_mapper.valueToSceneY;
    }

    AnchorNode *AnchorEditController::anchorNodeAt(const QPointF &scenePos) const {
        AnchorNode *nearest = nullptr;
        auto nearestDistanceSquared = kAnchorHitRadius * kAnchorHitRadius;
        for (auto *curve : m_curves) {
            for (auto *node : curve->nodes().toList()) {
                const auto dx = scenePos.x() - m_mapper.tickToSceneX(node->pos());
                const auto dy = scenePos.y() - m_mapper.valueToSceneY(node->value());
                const auto distanceSquared = dx * dx + dy * dy;
                if (distanceSquared <= kAnchorHitRadius * kAnchorHitRadius &&
                    (!nearest || distanceSquared < nearestDistanceSquared)) {
                    nearest = node;
                    nearestDistanceSquared = distanceSquared;
                }
            }
        }
        return nearest;
    }

    AnchorCurve *AnchorEditController::anchorCurveAt(const int tick, AnchorCurve *exclude) const {
        for (auto *curve : m_curves) {
            if (curve == exclude)
                continue;
            const auto nodes = curve->nodes().toList();
            if (!nodes.isEmpty() && tick >= nodes.first()->pos() && tick <= nodes.last()->pos())
                return curve;
        }
        return nullptr;
    }

    std::pair<int, int> AnchorEditController::reachableBounds(AnchorCurve *curve) const {
        if (!curve || curve->nodes().toList().isEmpty())
            return {0, INT_MAX};
        const auto nodes = curve->nodes().toList();
        int minimum = 0;
        int maximum = INT_MAX;
        for (auto *other : m_curves) {
            if (other == curve || other->nodes().toList().isEmpty())
                continue;
            const auto otherNodes = other->nodes().toList();
            if (otherNodes.last()->pos() < nodes.first()->pos())
                minimum = std::max(minimum, otherNodes.last()->pos() + 1);
            if (otherNodes.first()->pos() > nodes.last()->pos())
                maximum = std::min(maximum, otherNodes.first()->pos() - 1);
        }
        return {minimum, maximum};
    }

    AnchorCurve *AnchorEditController::findOwnerCurve(AnchorNode *node) const {
        for (auto *curve : m_curves) {
            if (curve->nodes().toList().contains(node))
                return curve;
        }
        return nullptr;
    }

    AnchorNode *AnchorEditController::findNodeAtTick(AnchorCurve *curve, const int tick,
                                                     AnchorNode *exclude) {
        if (!curve)
            return nullptr;
        for (auto *node : curve->nodes().toList()) {
            if (node != exclude && node->pos() == tick)
                return node;
        }
        return nullptr;
    }

    void AnchorEditController::removeOverlappingNodes(AnchorCurve *curve, AnchorNode *keep) {
        if (!curve || !keep)
            return;
        QList<AnchorNode *> remove;
        for (auto *node : curve->nodes().toList()) {
            if (node != keep && node->pos() == keep->pos())
                remove.append(node);
        }
        for (auto *node : remove) {
            curve->removeNode(node);
            m_state.selectedNodes.removeOne(node);
            if (m_state.hoveredNode == node)
                m_state.hoveredNode = nullptr;
            delete node;
        }
    }

    void AnchorEditController::cleanupIncompleteCurve(AnchorCurve *curve) {
        if (!curve || AnchorEditor::isCompleteAnchorCurve(curve))
            return;
        if (curve == m_provisionalCurve) {
            discardProvisionalCurve();
            if (!m_state.currentCurve)
                exitEditingState();
            return;
        }
        m_curves.removeOne(curve);
        if (m_state.currentCurve == curve)
            m_state.currentCurve = nullptr;
        for (auto *node : curve->nodes().toList()) {
            m_state.selectedNodes.removeOne(node);
            if (m_state.hoveredNode == node)
                m_state.hoveredNode = nullptr;
        }
        delete curve;
        if (!m_state.currentCurve)
            exitEditingState();
    }

    void AnchorEditController::discardProvisionalCurve() {
        auto *curve = m_provisionalCurve;
        if (!curve)
            return;
        m_provisionalCurve = nullptr;
        m_curves.removeOne(curve);
        if (m_state.currentCurve == curve)
            m_state.currentCurve = nullptr;
        for (auto *node : curve->nodes().toList()) {
            m_state.selectedNodes.removeOne(node);
            if (m_state.hoveredNode == node)
                m_state.hoveredNode = nullptr;
        }
        m_state.showPreview = false;
        m_state.previewCurve = nullptr;
        m_state.showMergePreview = false;
        m_state.mergeCandidateCurve = nullptr;
        m_state.mergeEndpointNode = nullptr;
        delete curve;
    }

    void AnchorEditController::enterEditingState(AnchorCurve *curve, AnchorNode *node) {
        m_state.editing = true;
        m_state.currentCurve = curve;
        if (node)
            selectNode(node);
    }

    void AnchorEditController::exitEditingState() {
        m_state.editing = false;
        m_state.currentCurve = nullptr;
        m_state.showPreview = false;
        m_state.previewCurve = nullptr;
        m_state.showMergePreview = false;
        m_state.mergeCandidateCurve = nullptr;
        m_state.mergeEndpointNode = nullptr;
        clearSelection();
    }

    void AnchorEditController::selectNode(AnchorNode *node) {
        auto *owner = findOwnerCurve(node);
        if (m_provisionalCurve && owner != m_provisionalCurve)
            discardProvisionalCurve();
        m_state.selectedNodes = {node};
        m_state.currentCurve = owner;
    }

    void AnchorEditController::clearSelection() {
        m_state.selectedNodes.clear();
    }

    void AnchorEditController::createAnchorAt(const QPointF &scenePos) {
        const auto tick = std::max(0, m_mapper.sceneXToTick(scenePos.x()));
        const auto value = m_mapper.sceneYToValue(scenePos.y());
        AnchorCurve *curve = nullptr;
        if (m_state.editing && m_state.currentCurve) {
            if (auto *other = anchorCurveAt(tick, m_state.currentCurve)) {
                curve = other;
            } else {
                const auto [minimum, maximum] = reachableBounds(m_state.currentCurve);
                if (tick >= minimum && tick <= maximum)
                    curve = m_state.currentCurve;
            }
        } else {
            curve = anchorCurveAt(tick);
        }
        if (m_provisionalCurve && curve != m_provisionalCurve)
            discardProvisionalCurve();
        const bool createdCurve = !curve;
        if (createdCurve) {
            curve = new AnchorCurve;
            m_curves.append(curve);
        }
        if (auto *existing = findNodeAtTick(curve, tick)) {
            enterEditingState(curve, existing);
            m_state.hoveredNode = existing;
            m_state.showPreview = false;
            m_state.previewCurve = nullptr;
            return;
        }

        const auto existingNodes = curve->nodes().toList();
        auto *oldLast = existingNodes.isEmpty() ? nullptr : existingNodes.last();
        auto *node = new AnchorNode(tick, value);
        if (!oldLast || tick > oldLast->pos()) {
            node->setInterpMode(AnchorNode::None);
            if (oldLast) {
                auto predecessorMode = oldLast->interpMode();
                if (predecessorMode == AnchorNode::None) {
                    const auto index = existingNodes.indexOf(oldLast);
                    predecessorMode =
                        index > 0 ? existingNodes.at(index - 1)->interpMode() : AnchorNode::Hermite;
                }
                oldLast->setInterpMode(predecessorMode);
            }
        } else {
            auto mode = AnchorNode::Hermite;
            for (int i = existingNodes.size() - 1; i >= 0; --i) {
                if (existingNodes.at(i)->pos() < tick) {
                    mode = existingNodes.at(i)->interpMode();
                    break;
                }
            }
            node->setInterpMode(mode);
        }
        curve->insertNode(node);
        if (createdCurve)
            m_provisionalCurve = curve;
        if (m_provisionalCurve == curve && AnchorEditor::isCompleteAnchorCurve(curve))
            m_provisionalCurve = nullptr;
        enterEditingState(curve, node);
        m_state.hoveredNode = node;
        m_state.showPreview = false;
        m_state.previewCurve = nullptr;
    }

    void AnchorEditController::updatePreview(const QPointF &scenePos) {
        m_state.previewScenePos = scenePos;
        m_state.previewTick = std::max(0, m_mapper.sceneXToTick(scenePos.x()));
        m_state.showPreview = m_state.editing && m_state.currentCurve && !m_state.hoveredNode &&
                              !m_state.showMergePreview && m_state.cursorInView;
        m_state.previewCurve = nullptr;
        if (!m_state.showPreview)
            return;
        if (auto *other = anchorCurveAt(m_state.previewTick, m_state.currentCurve)) {
            m_state.previewCurve = other;
        } else {
            const auto [minimum, maximum] = reachableBounds(m_state.currentCurve);
            if (m_state.previewTick >= minimum && m_state.previewTick <= maximum)
                m_state.previewCurve = m_state.currentCurve;
        }
    }

    void AnchorEditController::updateMergeCandidate(const QPointF &scenePos) {
        m_state.mergeCandidateCurve = nullptr;
        m_state.mergeEndpointNode = nullptr;
        m_state.showMergePreview = false;
        if (!m_state.editing || !m_state.currentCurve)
            return;
        const auto currentNodes = m_state.currentCurve->nodes().toList();
        if (currentNodes.isEmpty())
            return;
        for (auto *curve : m_curves) {
            if (curve == m_state.currentCurve)
                continue;
            const auto nodes = curve->nodes().toList();
            if (nodes.isEmpty())
                continue;
            AnchorNode *candidate = nullptr;
            if (nodes.last()->pos() < currentNodes.first()->pos())
                candidate = nodes.last();
            else if (nodes.first()->pos() > currentNodes.last()->pos())
                candidate = nodes.first();
            if (!candidate)
                continue;
            const auto dx = scenePos.x() - m_mapper.tickToSceneX(candidate->pos());
            const auto dy = scenePos.y() - m_mapper.valueToSceneY(candidate->value());
            if (dx * dx + dy * dy <= kAnchorHitRadius * kAnchorHitRadius) {
                m_state.mergeCandidateCurve = curve;
                m_state.mergeEndpointNode = candidate;
                m_state.showMergePreview = true;
                return;
            }
        }
    }

    void AnchorEditController::mergeCurves(AnchorCurve *target) {
        if (!m_state.currentCurve || !target || target == m_state.currentCurve)
            return;
        const auto nodes = target->nodes().toList();
        for (auto *node : nodes) {
            target->removeNode(node);
            if (findNodeAtTick(m_state.currentCurve, node->pos()))
                delete node;
            else
                m_state.currentCurve->insertNode(node);
        }
        m_curves.removeOne(target);
        delete target;
        m_state.mergeCandidateCurve = nullptr;
        m_state.mergeEndpointNode = nullptr;
        m_state.showMergePreview = false;
    }

    void AnchorEditController::updateNodeDragAt(const QPointF &scenePos) {
        if (m_state.dragNodeInfos.isEmpty()) {
            for (auto *node : m_state.selectedNodes) {
                m_state.dragNodeInfos.append(
                    {node, findOwnerCurve(node), nullptr, node->pos(), node->value()});
            }
        }
        const auto deltaTick = m_mapper.sceneXToTick(scenePos.x()) -
                               m_mapper.sceneXToTick(m_state.dragStartScenePos.x());
        const auto deltaValue = m_mapper.sceneYToValue(scenePos.y()) -
                                m_mapper.sceneYToValue(m_state.dragStartScenePos.y());
        for (auto &info : m_state.dragNodeInfos) {
            if (!info.sourceCurve)
                continue;
            info.sourceCurve->removeNode(info.node);
            info.node->setPos(std::max(0, info.startTick + deltaTick));
            const auto candidateValue = info.startValue + deltaValue;
            info.node->setValue(m_mapper.sceneYToValue(m_mapper.valueToSceneY(candidateValue)));
            info.sourceCurve->insertNode(info.node);
            info.targetCurve = anchorCurveAt(info.node->pos(), info.sourceCurve);
        }
        notifyCurvesChanged();
    }

    void AnchorEditController::updateSelectionRectAt(const QPointF &scenePos) {
        m_state.selectionSceneRect.setBottomRight(scenePos);
        const auto rect = m_state.selectionSceneRect.normalized();
        clearSelection();
        for (auto *curve : m_curves) {
            for (auto *node : curve->nodes().toList()) {
                if (rect.contains(m_mapper.tickToSceneX(node->pos()),
                                  m_mapper.valueToSceneY(node->value())))
                    m_state.selectedNodes.append(node);
            }
        }
        notifyChanged();
    }

    bool AnchorEditController::beginMutation() {
        if (m_mutationActive)
            return true;
        if (m_callbacks.beginEdit && !m_callbacks.beginEdit())
            return false;
        replaceOwnedCurves(m_curves, m_backupCurves);
        m_mutationActive = true;
        return true;
    }

    void AnchorEditController::commitMutationIfReady() {
        if (m_provisionalCurve) {
            if (!AnchorEditor::isCompleteAnchorCurve(m_provisionalCurve))
                return;
            m_provisionalCurve = nullptr;
        }
        commitMutation();
    }

    void AnchorEditController::commitMutation() {
        if (!m_mutationActive)
            return;
        removeIncompleteCurves();
        m_provisionalCurve = nullptr;
        m_state.visibleCurves = m_curves;
        m_publishing = true;
        if (m_callbacks.publish)
            m_callbacks.publish(m_curves);
        m_publishing = false;
        qDeleteAll(m_backupCurves);
        m_backupCurves.clear();
        m_mutationActive = false;
        if (m_callbacks.finishEdit)
            m_callbacks.finishEdit(EditFinishReason::Commit);
    }

    void AnchorEditController::discardMutation() {
        if (!m_mutationActive)
            return;
        qDeleteAll(m_curves);
        m_curves = m_backupCurves;
        m_backupCurves.clear();
        m_mutationActive = false;
        m_provisionalCurve = nullptr;
        m_state.visibleCurves = m_curves;
        ++m_curveRevision;
        if (m_callbacks.finishEdit)
            m_callbacks.finishEdit(EditFinishReason::Discard);
    }

    void AnchorEditController::removeIncompleteCurves() {
        const auto curves = m_curves;
        for (auto *curve : curves)
            cleanupIncompleteCurve(curve);
    }

    void AnchorEditController::clearInteractionState(const bool leaveEditing) {
        m_state.dragging = false;
        m_state.selecting = false;
        m_state.dragNodeInfos.clear();
        m_state.selectionSceneRect = {};
        m_state.hoveredNode = nullptr;
        m_state.showPreview = false;
        m_state.previewCurve = nullptr;
        m_state.showMergePreview = false;
        m_state.mergeCandidateCurve = nullptr;
        m_state.mergeEndpointNode = nullptr;
        if (leaveEditing)
            exitEditingState();
    }

    void AnchorEditController::notifyChanged() {
        m_state.visibleCurves = m_curves;
        if (m_callbacks.stateChanged)
            m_callbacks.stateChanged();
    }

    void AnchorEditController::notifyCurvesChanged() {
        ++m_curveRevision;
        notifyChanged();
    }

    void AnchorEditController::replaceOwnedCurves(const QList<AnchorCurve *> &source,
                                                  QList<AnchorCurve *> &destination) {
        qDeleteAll(destination);
        destination.clear();
        destination.reserve(source.size());
        for (const auto *curve : source) {
            if (AnchorEditor::isCompleteAnchorCurve(curve))
                destination.append(new AnchorCurve(*curve));
        }
    }

} // namespace AnchorEditor
