//
// Created by fluty on 26-4-30.
//

#include "EditPitchAnchorHandler.h"

#include "PianoRollGraphicsView.h"
#include "PianoRollGraphicsView_p.h"
#include "PitchAnchorEditorView.h"

#include "Model/AppModel/AnchorCurve.h"
#include "UI/Controls/Menu.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>

void EditPitchAnchorHandler::activate() {
    m_state.anchorEditMode = true;
    d->m_anchorEditor->setOverlayState(&m_state);
    q->setMouseTracking(true);
    triggerRepaint();
}

void EditPitchAnchorHandler::deactivate() {
    q->setMouseTracking(false);
    m_state.anchorEditMode = false;
    m_state.showPreview = false;
    m_state.hoveredNode = nullptr;
    m_state.selecting = false;
    m_state.dragging = false;
    triggerRepaint();
}

bool EditPitchAnchorHandler::mousePressEvent(QMouseEvent *event) {
    const auto scenePos = q->mapToScene(event->position().toPoint());
    const auto node = anchorNodeAt(scenePos);

    if (event->button() == Qt::LeftButton) {
        if (m_state.editing) {
            if (node) {
                if (m_state.showMergePreview && node == m_state.mergeEndpointNode) {
                    mergeCurves(m_state.mergeCandidateCurve);
                } else {
                    selectNode(node);
                    m_state.dragStartPos = scenePos;
                    m_state.dragging = false;
                }
            } else {
                createAnchorAt(scenePos);
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
                for (auto *node : m_state.selectedNodes) {
                    m_state.currentCurve->removeNode(node);
                    const auto tick = static_cast<int>(q->sceneXToTick(scenePos.x()));
                    const auto value = static_cast<int>(
                        d->m_anchorEditor->sceneYToValue(scenePos.y()));
                    node->setPos(tick);
                    node->setValue(value);
                    m_state.currentCurve->insertNode(node);
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
            const auto scenePos = q->mapToScene(event->pos());
            updatePreview(scenePos);
            triggerRepaint();
            return true;
        }

        if (m_state.selecting) {
            m_state.selecting = false;
            if (!m_state.selectedNodes.isEmpty()) {
                auto *curve = anchorCurveAt(m_state.selectedNodes.first()->pos());
                enterEditingState(curve);
            }
            m_state.selectionRect = QRectF();
            triggerRepaint();
            return true;
        }
    }
    return true;
}

void EditPitchAnchorHandler::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;
    const auto scenePos = q->mapToScene(event->position().toPoint());
    createAnchorAt(scenePos);
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
    auto *deleteAction = menu->addAction(QObject::tr("Delete"));
    QObject::connect(deleteAction, &QAction::triggered, q, [this] {
        deleteSelectedNodes();
    });
    menu->exec(event->globalPos());
    menu->deleteLater();
}

bool EditPitchAnchorHandler::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && m_state.editing) {
        exitEditingState();
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
    // TODO: persist to model
}

void EditPitchAnchorHandler::discard() {
    m_state.dragging = false;
    m_state.selecting = false;
    triggerRepaint();
}

const AnchorOverlayState &EditPitchAnchorHandler::overlayState() const {
    return m_state;
}

void EditPitchAnchorHandler::setCursorInView(bool inView) {
    m_state.cursorInView = inView;
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

AnchorCurve *EditPitchAnchorHandler::anchorCurveAt(int tick) const {
    for (auto *curve : anchorCurves()) {
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
    if (!m_state.currentCurve || m_state.selectedNodes.isEmpty())
        return;
    for (auto *node : m_state.selectedNodes)
        m_state.currentCurve->removeNode(node);
    clearSelection();
    if (m_state.currentCurve->nodes().toList().isEmpty()) {
        m_localCurves.removeOne(m_state.currentCurve);
        delete m_state.currentCurve;
        exitEditingState();
    }
    triggerRepaint();
}

void EditPitchAnchorHandler::createAnchorAt(const QPointF &scenePos) {
    const auto tick = static_cast<int>(q->sceneXToTick(scenePos.x()));
    const auto value = static_cast<int>(d->m_anchorEditor->sceneYToValue(scenePos.y()));

    auto *curve = m_state.currentCurve;
    if (!curve) {
        curve = anchorCurveAt(tick);
    }
    if (!curve) {
        curve = new AnchorCurve;
        m_localCurves.append(curve);
    }

    auto *node = new AnchorNode(tick, value);
    node->setInterpMode(AnchorNode::Hermite);
    curve->insertNode(node);

    enterEditingState(curve, node);
}

void EditPitchAnchorHandler::updatePreview(const QPointF &scenePos) {
    m_state.previewPos = q->mapFromScene(scenePos.toPoint());
    m_state.showPreview = m_state.editing && m_state.currentCurve != nullptr &&
                          m_state.hoveredNode == nullptr && !m_state.showMergePreview;
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
        m_state.currentCurve->insertNode(node);
    }

    m_localCurves.removeOne(target);
    delete target;

    m_state.mergeCandidateCurve = nullptr;
    m_state.mergeEndpointNode = nullptr;
    m_state.showMergePreview = false;
}

void EditPitchAnchorHandler::triggerRepaint() {
    m_state.visibleCurves = anchorCurves();
    if (d->m_anchorEditor)
        d->m_anchorEditor->update();
}
