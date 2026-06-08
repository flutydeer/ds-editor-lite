//
// Created by fluty on 26-4-30.
//

#include "EditPitchAnchorHandler.h"

#include "PianoRollGraphicsView.h"
#include "PianoRollGraphicsView_p.h"
#include "PitchAnchorEditorView.h"

#include "Model/AppModel/AnchorCurve.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"
#include "Controller/ClipController.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Controls/Menu.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSet>

namespace {
    void beginPitchEditSession() {
        if (editSessionManager->hasActiveTransaction())
            return;
        const auto clip = dynamic_cast<SingingClip *>(clipController->clip());
        if (!clip)
            return;
        editSessionManager->beginTransaction(AppStatus::EditObjectType::Param, clip->id(), {}, {},
                                             {}, {ParamInfo::Pitch});
        appStatus->currentEditObject = AppStatus::EditObjectType::Param;
    }
}

void EditPitchAnchorHandler::activate() {
    m_state.anchorEditActive = true;
    d->m_anchorEditor->setOverlayState(&m_state);
    q->setMouseTracking(true);
    triggerRepaint();
}

void EditPitchAnchorHandler::deactivate() {
    q->setMouseTracking(false);
    m_state.anchorEditActive = false;
    m_state.showPreview = false;
    m_state.hoveredNode = nullptr;
    m_state.selecting = false;
    m_state.dragging = false;
    editSessionManager->endActiveTransaction(EditSessionEndReason::Cancel);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    triggerRepaint();
}

bool EditPitchAnchorHandler::mousePressEvent(QMouseEvent *event) {
    const auto scenePos = q->mapToScene(event->position().toPoint());
    const auto node = anchorNodeAt(scenePos);

    if (event->button() == Qt::LeftButton) {
        beginPitchEditSession();
        if (m_state.editing) {
            if (node) {
                if (m_state.showMergePreview && node == m_state.mergeEndpointNode) {
                    mergeCurves(m_state.mergeCandidateCurve);
                } else {
                    if (!m_state.selectedNodes.contains(node))
                        selectNode(node);
                    m_state.dragStartPos = scenePos;
                    m_state.dragging = false;
                }
            } else {
                createAnchorAt(scenePos);
                m_state.dragStartPos = scenePos;
                m_state.dragging = false;
            }
        } else {
            if (node) {
                selectNode(node);
                enterEditingState(m_state.currentCurve, node);
                m_state.dragStartPos = scenePos;
                m_state.dragging = false;
            } else {
                m_state.selectionRect = QRectF(scenePos, QSizeF(0, 0));
                m_state.selecting = true;
            }
        }
        triggerRepaint();
        return true;
    }

    return true;
}

bool EditPitchAnchorHandler::mouseMoveEvent(QMouseEvent *event) {
    const auto scenePos = q->mapToScene(event->position().toPoint());

    if (event->buttons() & Qt::LeftButton) {
        if (m_state.editing && !m_state.selectedNodes.isEmpty()) {
            const auto delta = scenePos - m_state.dragStartPos;
            if (!m_state.dragging && delta.manhattanLength() > 3)
                m_state.dragging = true;

            if (m_state.dragging) {
                if (m_state.dragNodeInfos.isEmpty()) {
                    for (auto *node : m_state.selectedNodes) {
                        DragNodeInfo info;
                        info.node = node;
                        info.sourceCurve = findOwnerCurve(node);
                        info.startTick = node->pos();
                        info.startValue = node->value();
                        m_state.dragNodeInfos.append(info);
                    }
                }

                const auto deltaTick = static_cast<int>(q->sceneXToTick(scenePos.x()) -
                                                        q->sceneXToTick(m_state.dragStartPos.x()));
                const auto deltaValue =
                    static_cast<int>(d->m_anchorEditor->sceneYToValue(scenePos.y()) -
                                     d->m_anchorEditor->sceneYToValue(m_state.dragStartPos.y()));

                for (auto &info : m_state.dragNodeInfos) {
                    info.sourceCurve->removeNode(info.node);
                    info.node->setPos(info.startTick + deltaTick);
                    info.node->setValue(info.startValue + deltaValue);
                    info.sourceCurve->insertNode(info.node);
                    info.targetCurve = anchorCurveAt(info.node->pos(), info.sourceCurve);
                }

                triggerRepaint();
            }
            return true;
        }

        if (m_state.selecting) {
            m_state.selectionRect.setBottomRight(scenePos);
            const auto rect = m_state.selectionRect.normalized();
            clearSelection();
            for (auto *curve : anchorCurves()) {
                for (auto *node : curve->nodes().toList()) {
                    const auto x = q->tickToSceneX(node->pos());
                    const auto y = d->m_anchorEditor->valueToSceneY(node->value());
                    if (rect.contains(x, y))
                        m_state.selectedNodes.append(node);
                }
            }
            triggerRepaint();
            return true;
        }
    } else {
        auto *hovered = anchorNodeAt(scenePos);
        if (hovered != m_state.hoveredNode) {
            m_state.hoveredNode = hovered;
            triggerRepaint();
        }

        if (m_state.editing) {
            updateMergeCandidate(scenePos);
            updatePreview(scenePos);
            triggerRepaint();
        }
    }
    return true;
}

bool EditPitchAnchorHandler::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
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
                cleanupEmptyCurve(curve);

            if (!m_state.selectedNodes.isEmpty())
                m_state.currentCurve = findOwnerCurve(m_state.selectedNodes.first());

            m_state.dragNodeInfos.clear();

            const auto scenePos = q->mapToScene(event->pos());
            updatePreview(scenePos);
            triggerRepaint();
            commit();
            return true;
        }

        if (m_state.selecting) {
            m_state.selecting = false;
            if (!m_state.selectedNodes.isEmpty()) {
                QSet<AnchorCurve *> involvedCurves;
                for (auto *node : m_state.selectedNodes)
                    involvedCurves.insert(findOwnerCurve(node));
                if (involvedCurves.size() == 1)
                    enterEditingState(*involvedCurves.begin());
                else
                    m_state.editing = true;
            }
            m_state.selectionRect = QRectF();
            triggerRepaint();
            editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
            appStatus->currentEditObject = AppStatus::EditObjectType::None;
            return true;
        }
    }
    editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    return true;
}

void EditPitchAnchorHandler::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;
    const auto scenePos = q->mapToScene(event->position().toPoint());
    createAnchorAt(scenePos);
    m_state.dragStartPos = scenePos;
    m_state.dragging = false;
    triggerRepaint();
}

void EditPitchAnchorHandler::contextMenuEvent(QContextMenuEvent *event) {
    const auto scenePos = q->mapToScene(event->pos());
    auto *node = anchorNodeAt(scenePos);

    if (!node) {
        if (m_state.editing) {
            exitEditingState();
            triggerRepaint();
        }
        return;
    }

    if (m_state.selectedNodes.size() <= 1 || !m_state.selectedNodes.contains(node)) {
        clearSelection();
        selectNode(node);
        enterEditingState(m_state.currentCurve, node);
    }
    triggerRepaint();

    auto *menu = new Menu(q);

    const auto &curveNodes = m_state.currentCurve->nodes().toList();
    auto *lastCurveNode = curveNodes.isEmpty() ? nullptr : curveNodes.last();

    auto currentMode = m_state.selectedNodes.first()->interpMode();
    bool allSame =
        std::all_of(m_state.selectedNodes.begin(), m_state.selectedNodes.end(),
                    [currentMode](AnchorNode *n) { return n->interpMode() == currentMode; });

    auto *linearAction = menu->addAction(QObject::tr("Linear"));
    linearAction->setCheckable(true);
    linearAction->setChecked(allSame && currentMode == AnchorNode::Linear);
    QObject::connect(linearAction, &QAction::triggered, q, [this, lastCurveNode] {
        for (auto *n : m_state.selectedNodes) {
            if (n == lastCurveNode)
                continue;
            n->setInterpMode(AnchorNode::Linear);
        }
        triggerRepaint();
        commit();
    });

    auto *hermiteAction = menu->addAction(QObject::tr("Hermite"));
    hermiteAction->setCheckable(true);
    hermiteAction->setChecked(allSame && currentMode == AnchorNode::Hermite);
    QObject::connect(hermiteAction, &QAction::triggered, q, [this, lastCurveNode] {
        for (auto *n : m_state.selectedNodes) {
            if (n == lastCurveNode)
                continue;
            n->setInterpMode(AnchorNode::Hermite);
        }
        triggerRepaint();
        commit();
    });

    auto *interpGroup = new QActionGroup(menu);
    interpGroup->setExclusive(true);
    interpGroup->addAction(linearAction);
    interpGroup->addAction(hermiteAction);
    if (!allSame) {
        linearAction->setChecked(false);
        hermiteAction->setChecked(false);
    }

    bool allAreLast = std::all_of(m_state.selectedNodes.begin(), m_state.selectedNodes.end(),
                                  [lastCurveNode](AnchorNode *n) { return n == lastCurveNode; });
    if (allAreLast) {
        linearAction->setEnabled(false);
        hermiteAction->setEnabled(false);
    }

    menu->addSeparator();

    auto *deleteAction = menu->addAction(QObject::tr("Delete"));
    QObject::connect(deleteAction, &QAction::triggered, q, [this] { deleteSelectedNodes(); });
    menu->exec(event->globalPos());
    menu->deleteLater();
}

bool EditPitchAnchorHandler::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && m_state.editing) {
        exitEditingState();
        editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
        appStatus->currentEditObject = AppStatus::EditObjectType::None;
        triggerRepaint();
        return true;
    }
    if (event->key() == Qt::Key_Delete && !m_state.selectedNodes.isEmpty()) {
        deleteSelectedNodes();
        return true;
    }
    return false;
}

void EditPitchAnchorHandler::commit() {
    auto *clip = dynamic_cast<SingingClip *>(clipController->clip());
    if (!clip)
        return;
    beginPitchEditSession();

    QList<Curve *> combined;
    const auto &existing = clip->params.getParamByName(ParamInfo::Pitch)->curves(Param::Edited);
    for (auto *curve : existing) {
        if (curve->type() == Curve::Draw)
            combined.append(new DrawCurve(*dynamic_cast<DrawCurve *>(curve)));
    }
    for (auto *curve : m_localCurves)
        combined.append(new AnchorCurve(*curve));

    m_committing = true;
    clipController->onParamEdited(ParamInfo::Pitch, combined);
    m_committing = false;
    editSessionManager->endActiveTransaction(EditSessionEndReason::Commit);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void EditPitchAnchorHandler::discard() {
    m_state.dragging = false;
    m_state.selecting = false;
    editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    triggerRepaint();
}

const AnchorOverlayState &EditPitchAnchorHandler::overlayState() const {
    return m_state;
}

void EditPitchAnchorHandler::hoverEnterEvent(QHoverEvent *event) {
    Q_UNUSED(event);
    m_state.cursorInView = true;
    triggerRepaint();
}

void EditPitchAnchorHandler::hoverLeaveEvent(QHoverEvent *event) {
    Q_UNUSED(event);
    m_state.cursorInView = false;
    triggerRepaint();
}

void EditPitchAnchorHandler::setAlwaysVisible(bool visible) {
    m_state.anchorVisible = visible;
    triggerRepaint();
}

void EditPitchAnchorHandler::loadFromModel(const QList<AnchorCurve *> &curves) {
    if (m_committing) {
        qDeleteAll(curves);
        return;
    }
    exitEditingState();
    qDeleteAll(m_localCurves);
    m_localCurves.clear();
    for (auto *curve : curves)
        m_localCurves.append(curve);
    triggerRepaint();
}

AnchorNode *EditPitchAnchorHandler::anchorNodeAt(const QPointF &scenePos) const {
    for (auto *curve : anchorCurves()) {
        for (auto *node : curve->nodes().toList()) {
            const auto x = q->tickToSceneX(node->pos());
            const auto y = d->m_anchorEditor->valueToSceneY(node->value());
            const auto dx = scenePos.x() - x;
            const auto dy = scenePos.y() - y;
            if (dx * dx + dy * dy <= kAnchorHitRadius * kAnchorHitRadius)
                return node;
        }
    }
    return nullptr;
}

AnchorCurve *EditPitchAnchorHandler::anchorCurveAt(int tick, AnchorCurve *exclude) const {
    for (auto *curve : anchorCurves()) {
        if (curve == exclude)
            continue;
        const auto &nodes = curve->nodes().toList();
        if (nodes.isEmpty())
            continue;
        if (tick >= nodes.first()->pos() && tick <= nodes.last()->pos())
            return curve;
    }
    return nullptr;
}

QList<AnchorCurve *> EditPitchAnchorHandler::anchorCurves() const {
    return m_localCurves;
}

std::pair<int, int> EditPitchAnchorHandler::getReachableBounds(AnchorCurve *curve) const {
    if (!curve)
        return {INT_MIN, INT_MAX};

    const auto &curveNodes = curve->nodes().toList();
    if (curveNodes.isEmpty())
        return {INT_MIN, INT_MAX};

    int curveFirst = curveNodes.first()->pos();
    int curveLast = curveNodes.last()->pos();

    int minBound = INT_MIN;
    int maxBound = INT_MAX;

    for (auto *other : m_localCurves) {
        if (other == curve)
            continue;
        const auto &otherNodes = other->nodes().toList();
        if (otherNodes.isEmpty())
            continue;
        int otherLast = otherNodes.last()->pos();
        int otherFirst = otherNodes.first()->pos();
        if (otherLast < curveFirst)
            minBound = qMax(minBound, otherLast + 1);
        if (otherFirst > curveLast)
            maxBound = qMin(maxBound, otherFirst - 1);
    }

    return {minBound, maxBound};
}

AnchorCurve *EditPitchAnchorHandler::findOwnerCurve(AnchorNode *node) const {
    for (auto *curve : m_localCurves) {
        if (curve->nodes().toList().contains(node))
            return curve;
    }
    return nullptr;
}

AnchorNode *EditPitchAnchorHandler::findNodeAtTick(AnchorCurve *curve, int tick,
                                                   AnchorNode *exclude) {
    if (!curve)
        return nullptr;
    for (auto *node : curve->nodes().toList()) {
        if (node != exclude && node->pos() == tick)
            return node;
    }
    return nullptr;
}

void EditPitchAnchorHandler::removeOverlappingNodes(AnchorCurve *curve, AnchorNode *keep) {
    if (!curve || !keep)
        return;
    QList<AnchorNode *> toRemove;
    for (auto *node : curve->nodes().toList()) {
        if (node != keep && node->pos() == keep->pos())
            toRemove.append(node);
    }
    for (auto *node : toRemove) {
        curve->removeNode(node);
        m_state.selectedNodes.removeOne(node);
        if (m_state.hoveredNode == node)
            m_state.hoveredNode = nullptr;
        delete node;
    }
}

void EditPitchAnchorHandler::transferNodeToCurve(AnchorNode *node, AnchorCurve *from,
                                                 AnchorCurve *to) {
    from->removeNode(node);
    to->insertNode(node);
    m_state.currentCurve = to;
    cleanupEmptyCurve(from);
}

void EditPitchAnchorHandler::detachNodeToNewCurve(AnchorNode *node, AnchorCurve *from) {
    from->removeNode(node);

    auto *newCurve = new AnchorCurve;
    newCurve->insertNode(node);
    m_localCurves.append(newCurve);
    m_state.currentCurve = newCurve;
    cleanupEmptyCurve(from);
}

void EditPitchAnchorHandler::cleanupEmptyCurve(AnchorCurve *curve) {
    if (!curve || !curve->nodes().toList().isEmpty())
        return;
    m_localCurves.removeOne(curve);
    if (m_state.currentCurve == curve) {
        m_state.currentCurve = nullptr;
        exitEditingState();
    }
    delete curve;
}

void EditPitchAnchorHandler::enterEditingState(AnchorCurve *curve, AnchorNode *node) {
    m_state.editing = true;
    m_state.currentCurve = curve;
    if (node)
        selectNode(node);
}

void EditPitchAnchorHandler::exitEditingState() {
    m_state.editing = false;
    m_state.currentCurve = nullptr;
    m_state.showPreview = false;
    clearSelection();
}

void EditPitchAnchorHandler::selectNode(AnchorNode *node) {
    m_state.selectedNodes.clear();
    m_state.selectedNodes.append(node);
    for (auto *curve : anchorCurves()) {
        if (curve->nodes().toList().contains(node)) {
            m_state.currentCurve = curve;
            break;
        }
    }
}

void EditPitchAnchorHandler::clearSelection() {
    m_state.selectedNodes.clear();
}

void EditPitchAnchorHandler::deleteSelectedNodes() {
    if (m_state.selectedNodes.isEmpty())
        return;

    QHash<AnchorCurve *, QList<AnchorNode *>> nodesByCurve;
    for (auto *node : m_state.selectedNodes) {
        auto *curve = findOwnerCurve(node);
        if (curve)
            nodesByCurve[curve].append(node);
    }
    clearSelection();

    for (auto it = nodesByCurve.begin(); it != nodesByCurve.end(); ++it) {
        auto *curve = it.key();
        for (auto *node : it.value())
            curve->removeNode(node);
        const auto &remaining = curve->nodes().toList();
        if (remaining.isEmpty()) {
            m_localCurves.removeOne(curve);
            if (m_state.currentCurve == curve)
                m_state.currentCurve = nullptr;
            delete curve;
        } else {
            remaining.last()->setInterpMode(AnchorNode::None);
        }
    }

    if (!m_state.currentCurve)
        exitEditingState();
    triggerRepaint();
    commit();
}

void EditPitchAnchorHandler::createAnchorAt(const QPointF &scenePos) {
    const auto tick = static_cast<int>(q->sceneXToTick(scenePos.x()));
    const auto value = static_cast<int>(d->m_anchorEditor->sceneYToValue(scenePos.y()));

    AnchorCurve *curve = nullptr;

    if (m_state.editing && m_state.currentCurve) {
        auto *otherCurve = anchorCurveAt(tick, m_state.currentCurve);
        if (otherCurve) {
            curve = otherCurve;
        } else {
            auto [minBound, maxBound] = getReachableBounds(m_state.currentCurve);
            if (tick >= minBound && tick <= maxBound) {
                curve = m_state.currentCurve;
            } else {
                curve = new AnchorCurve;
                m_localCurves.append(curve);
            }
        }
    } else {
        curve = anchorCurveAt(tick);
        if (!curve) {
            curve = new AnchorCurve;
            m_localCurves.append(curve);
        }
    }

    auto *existing = findNodeAtTick(curve, tick);
    if (existing) {
        enterEditingState(curve, existing);
        return;
    }

    const auto &existingNodes = curve->nodes().toList();
    auto *oldLast = existingNodes.isEmpty() ? nullptr : existingNodes.last();

    auto *node = new AnchorNode(tick, value);

    if (!oldLast || tick > oldLast->pos()) {
        node->setInterpMode(AnchorNode::None);
        if (oldLast) {
            auto predecessorMode = oldLast->interpMode();
            if (predecessorMode == AnchorNode::None) {
                auto idx = existingNodes.indexOf(oldLast);
                if (idx > 0)
                    predecessorMode = existingNodes[idx - 1]->interpMode();
                else
                    predecessorMode = AnchorNode::Hermite;
            }
            oldLast->setInterpMode(predecessorMode);
        }
    } else {
        auto mode = AnchorNode::Hermite;
        for (int i = existingNodes.size() - 1; i >= 0; i--) {
            if (existingNodes[i]->pos() < tick) {
                mode = existingNodes[i]->interpMode();
                break;
            }
        }
        node->setInterpMode(mode);
    }

    curve->insertNode(node);

    enterEditingState(curve, node);
    commit();
}

void EditPitchAnchorHandler::updatePreview(const QPointF &scenePos) {
    m_state.previewPos = q->mapFromScene(scenePos.toPoint());
    m_state.showPreview = m_state.editing && m_state.currentCurve != nullptr &&
                          m_state.hoveredNode == nullptr && !m_state.showMergePreview;

    m_state.previewCurve = nullptr;
    if (m_state.showPreview) {
        const auto tick = static_cast<int>(q->sceneXToTick(scenePos.x()));
        auto *otherCurve = anchorCurveAt(tick, m_state.currentCurve);
        if (otherCurve) {
            m_state.previewCurve = otherCurve;
        } else {
            auto [minBound, maxBound] = getReachableBounds(m_state.currentCurve);
            if (tick >= minBound && tick <= maxBound)
                m_state.previewCurve = m_state.currentCurve;
        }
    }
}

void EditPitchAnchorHandler::updateMergeCandidate(const QPointF &scenePos) {
    m_state.mergeCandidateCurve = nullptr;
    m_state.mergeEndpointNode = nullptr;
    m_state.showMergePreview = false;

    if (!m_state.editing || !m_state.currentCurve)
        return;

    const auto &currentNodes = m_state.currentCurve->nodes().toList();
    if (currentNodes.isEmpty())
        return;

    auto *currentFirst = currentNodes.first();
    auto *currentLast = currentNodes.last();

    for (auto *curve : m_localCurves) {
        if (curve == m_state.currentCurve)
            continue;
        const auto &nodes = curve->nodes().toList();
        if (nodes.isEmpty())
            continue;

        auto *otherFirst = nodes.first();
        auto *otherLast = nodes.last();

        AnchorNode *candidateNode = nullptr;
        if (otherLast->pos() < currentFirst->pos())
            candidateNode = otherLast;
        else if (otherFirst->pos() > currentLast->pos())
            candidateNode = otherFirst;
        else
            continue;

        const auto x = q->tickToSceneX(candidateNode->pos());
        const auto y = d->m_anchorEditor->valueToSceneY(candidateNode->value());
        const auto dx = scenePos.x() - x;
        const auto dy = scenePos.y() - y;
        if (dx * dx + dy * dy <= kAnchorHitRadius * kAnchorHitRadius) {
            m_state.mergeCandidateCurve = curve;
            m_state.mergeEndpointNode = candidateNode;
            m_state.showMergePreview = true;
            return;
        }
    }
}

void EditPitchAnchorHandler::mergeCurves(AnchorCurve *target) {
    if (!m_state.currentCurve || !target || target == m_state.currentCurve)
        return;

    QList<AnchorNode *> nodesToMove;
    for (auto *node : target->nodes().toList())
        nodesToMove.append(node);

    for (auto *node : nodesToMove) {
        target->removeNode(node);
        if (findNodeAtTick(m_state.currentCurve, node->pos())) {
            delete node;
        } else {
            m_state.currentCurve->insertNode(node);
        }
    }

    m_localCurves.removeOne(target);
    delete target;

    m_state.mergeCandidateCurve = nullptr;
    m_state.mergeEndpointNode = nullptr;
    m_state.showMergePreview = false;
    commit();
}

void EditPitchAnchorHandler::triggerRepaint() {
    m_state.visibleCurves = anchorCurves();
    if (d->m_anchorEditor)
        d->m_anchorEditor->update();
}
