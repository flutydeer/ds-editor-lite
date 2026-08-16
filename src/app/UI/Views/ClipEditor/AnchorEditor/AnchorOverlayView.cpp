#include "AnchorOverlayView.h"

#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/EditorItemGeometry.h"

#include <QLineF>
#include <QPainter>
#include <QSet>
#include <algorithm>
#include <cmath>

AnchorOverlayView::AnchorOverlayView(ValueMapper valueToSceneY, ValueMapper sceneYToValue)
    : m_valueToSceneY(std::move(valueToSceneY)), m_sceneYToValue(std::move(sceneYToValue)) {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    setTransparentMouseEvents(true);
}

void AnchorOverlayView::setOverlayState(const AnchorEditor::AnchorOverlayState *state) {
    m_state = state;
}

void AnchorOverlayView::setDisplayMode(const PitchDisplayMode mode) {
    if (m_displayMode == mode)
        return;
    m_displayMode = mode;
    update();
}

void AnchorOverlayView::setValueMappers(ValueMapper valueToSceneY, ValueMapper sceneYToValue) {
    m_valueToSceneY = std::move(valueToSceneY);
    m_sceneYToValue = std::move(sceneYToValue);
    update();
}

QColor AnchorOverlayView::anchorColor() const {
    return m_anchorColor;
}

void AnchorOverlayView::setAnchorColor(const QColor &color) {
    if (m_anchorColor == color)
        return;
    m_anchorColor = color;
    update();
}

QColor AnchorOverlayView::anchorSelectedColor() const {
    return m_anchorSelectedColor;
}

void AnchorOverlayView::setAnchorSelectedColor(const QColor &color) {
    if (m_anchorSelectedColor == color)
        return;
    m_anchorSelectedColor = color;
    update();
}

QColor AnchorOverlayView::anchorCurveColor() const {
    return m_anchorCurveColor;
}

void AnchorOverlayView::setAnchorCurveColor(const QColor &color) {
    if (m_anchorCurveColor == color)
        return;
    m_anchorCurveColor = color;
    update();
}

QColor AnchorOverlayView::anchorPreviewColor() const {
    return m_anchorPreviewColor;
}

void AnchorOverlayView::setAnchorPreviewColor(const QColor &color) {
    if (m_anchorPreviewColor == color)
        return;
    m_anchorPreviewColor = color;
    update();
}

double AnchorOverlayView::valueToSceneY(const double value) const {
    return m_valueToSceneY ? m_valueToSceneY(value) : value;
}

double AnchorOverlayView::sceneYToValue(const double y) const {
    return m_sceneYToValue ? m_sceneYToValue(y) : y;
}

void AnchorOverlayView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                              QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)
    if (!m_state || (!m_state->anchorVisible && !m_state->anchorEditActive))
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);
    if (m_state->anchorEditActive) {
        drawPreviewCurve(painter);
        drawDragPreviewCurve(painter);
    }
    drawAnchorCurves(painter);
    if (m_state->anchorEditActive)
        drawSelectionRect(painter);
}

void AnchorOverlayView::updateRectAndPos() {
    const auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}

void AnchorOverlayView::drawAnchorCurves(QPainter *painter) const {
    if (!m_state)
        return;

    const bool active = m_state->anchorEditActive;
    constexpr double anchorRadius = 2.0;
    constexpr double hoverRadius = 6.0;
    QColor normalColor = m_anchorColor;
    QColor curveColor = m_anchorCurveColor;
    const auto opacity = PitchDisplayStrategy::anchorOpacity(m_displayMode);
    normalColor.setAlpha(std::min(normalColor.alpha(), opacity.nodeMaximumAlpha));
    curveColor.setAlpha(std::min(curveColor.alpha(), opacity.curveMaximumAlpha));
    const QColor selectedColor = m_anchorSelectedColor;

    auto drawNodeAt = [&](double x, double y, AnchorNode *node) {
        const bool isSelected = active && m_state->selectedNodes.contains(node);
        const bool isHovered = active && (node == m_state->hoveredNode);

        QColor color = isSelected ? selectedColor : normalColor;
        double radius = anchorRadius;

        painter->setBrush(color);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(QPointF(x, y), radius, radius);

        if (isHovered || isSelected) {
            QPen pen(color, 1.5);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(QPointF(x, y), hoverRadius, hoverRadius);
        }
    };

    auto tickToLocalX = [this](int tick) { return tickToItemX(tick); };

    auto valueToLocalY = [this](int value) { return sceneYToItemY(valueToSceneY(value)); };

    auto drawCurve = [&](AnchorCurve *curve) {
        const auto nodes = PitchDisplayStrategy::anchorCurveNodes(curve, *m_state);
        if (nodes.isEmpty())
            return;

        QPen pen(curveColor, 1.5);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        painter->drawPath(interpolatedPath(nodes, painter->device()->devicePixelRatioF()));

        for (auto *node : nodes) {
            const double x = tickToLocalX(node->pos());
            const double y = valueToLocalY(node->value());
            drawNodeAt(x, y, node);
        }
    };

    for (auto *curve : m_state->visibleCurves)
        drawCurve(curve);
}

void AnchorOverlayView::drawPreviewCurve(QPainter *painter) const {
    if (!m_state || !m_state->currentCurve || m_state->dragging || !m_state->cursorInView)
        return;

    if (m_state->showMergePreview && m_state->mergeCandidateCurve) {
        drawMergePreviewCurve(painter);
        return;
    }

    if (!m_state->showPreview || !m_state->previewCurve)
        return;

    const auto &nodes = m_state->previewCurve->nodes().toList();
    if (nodes.isEmpty())
        return;

    const QPointF scenePreviewPos = m_state->previewScenePos;
    const double previewTick = sceneXToTick(scenePreviewPos.x());
    const double previewValue = sceneYToValue(scenePreviewPos.y());

    AnchorNode virtualNode(static_cast<int>(previewTick), static_cast<int>(previewValue));
    if (std::any_of(nodes.cbegin(), nodes.cend(), [&virtualNode](const AnchorNode *node) {
            return node->pos() == virtualNode.pos();
        })) {
        return;
    }

    QList<AnchorNode *> allNodes = nodes;
    auto it = std::lower_bound(allNodes.begin(), allNodes.end(), &virtualNode,
                               [](AnchorNode *a, AnchorNode *b) { return a->pos() < b->pos(); });
    int insertIdx = static_cast<int>(it - allNodes.begin());
    allNodes.insert(it, &virtualNode);

    AnchorNode::InterpMode savedLastMode = AnchorNode::Hermite;
    AnchorNode *oldLastNode = nullptr;
    bool isAppend = (insertIdx == allNodes.size() - 1);
    if (isAppend) {
        virtualNode.setInterpMode(AnchorNode::None);
        oldLastNode = nodes.last();
        savedLastMode = oldLastNode->interpMode();
        if (savedLastMode == AnchorNode::None) {
            auto idx = nodes.indexOf(oldLastNode);
            auto predecessorMode = (idx > 0) ? nodes[idx - 1]->interpMode() : AnchorNode::Hermite;
            oldLastNode->setInterpMode(predecessorMode);
        }
    } else {
        auto mode = AnchorNode::Hermite;
        for (int i = insertIdx - 1; i >= 0; i--) {
            if (allNodes[i]->pos() < virtualNode.pos()) {
                mode = allNodes[i]->interpMode();
                break;
            }
        }
        virtualNode.setInterpMode(mode);
    }

    QColor previewColor = m_anchorPreviewColor;
    previewColor.setAlpha(PitchDisplayStrategy::anchorPreviewAlpha());
    QPen pen(previewColor, 1.5, Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    painter->drawPath(interpolatedPath(allNodes, painter->device()->devicePixelRatioF()));

    const double cx = tickToItemX(virtualNode.pos());
    const double cy = sceneYToItemY(valueToSceneY(virtualNode.value()));
    painter->setPen(Qt::NoPen);
    painter->setBrush(previewColor);
    painter->drawEllipse(QPointF(cx, cy), 2.0, 2.0);

    if (isAppend && oldLastNode)
        oldLastNode->setInterpMode(savedLastMode);
}

void AnchorOverlayView::drawMergePreviewCurve(QPainter *painter) const {
    QList<AnchorNode *> allNodes;
    for (auto *node : m_state->currentCurve->nodes().toList())
        allNodes.append(node);
    for (auto *node : m_state->mergeCandidateCurve->nodes().toList())
        allNodes.append(node);

    std::sort(allNodes.begin(), allNodes.end(),
              [](AnchorNode *a, AnchorNode *b) { return a->pos() < b->pos(); });

    if (allNodes.size() < 2)
        return;

    QColor mergePreviewColor = m_anchorPreviewColor;
    mergePreviewColor.setAlpha(PitchDisplayStrategy::anchorInteractionPreviewAlpha());
    QPen pen(mergePreviewColor, 1.5, Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    painter->drawPath(interpolatedPath(allNodes, painter->device()->devicePixelRatioF()));
}

void AnchorOverlayView::drawDragPreviewCurve(QPainter *painter) const {
    if (!m_state || !m_state->dragging || m_state->dragNodeInfos.isEmpty())
        return;

    QSet<AnchorCurve *> targetCurves;
    for (const auto &info : m_state->dragNodeInfos) {
        if (info.targetCurve)
            targetCurves.insert(info.targetCurve);
    }

    if (targetCurves.isEmpty())
        return;

    auto tickToLocalX = [this](int tick) { return tickToItemX(tick); };
    auto valueToLocalY = [this](int value) { return sceneYToItemY(valueToSceneY(value)); };

    QColor dragPreviewColor = m_anchorPreviewColor;
    dragPreviewColor.setAlpha(PitchDisplayStrategy::anchorInteractionPreviewAlpha());
    QPen pen(dragPreviewColor, 1.5, Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    for (auto *target : targetCurves) {
        QList<AnchorNode *> allNodes = target->nodes().toList();
        QList<AnchorNode *> draggedNodes;
        for (const auto &info : m_state->dragNodeInfos) {
            if (info.targetCurve == target) {
                auto it = std::lower_bound(
                    allNodes.begin(), allNodes.end(), info.node,
                    [](AnchorNode *a, AnchorNode *b) { return a->pos() < b->pos(); });
                allNodes.insert(it, info.node);
                draggedNodes.append(info.node);
            }
        }

        if (allNodes.size() < 2)
            continue;

        painter->drawPath(interpolatedPath(allNodes, painter->device()->devicePixelRatioF()));

        painter->setPen(Qt::NoPen);
        painter->setBrush(dragPreviewColor);
        for (auto *node : draggedNodes) {
            const double x = tickToLocalX(node->pos());
            const double y = valueToLocalY(node->value());
            painter->drawEllipse(QPointF(x, y), 2.0, 2.0);
        }
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
    }
}

void AnchorOverlayView::drawSelectionRect(QPainter *painter) const {
    if (!m_state || !m_state->selecting)
        return;

    const auto rect = m_state->selectionSceneRect.normalized();
    const double x1 = sceneXToItemX(rect.left());
    const double y1 = sceneYToItemY(rect.top());
    const double x2 = sceneXToItemX(rect.right());
    const double y2 = sceneYToItemY(rect.bottom());
    QRectF localRect(QPointF(x1, y1), QPointF(x2, y2));

    const auto radius = EditorItemGeometry::adaptiveCornerRadius(localRect, 6.0);
    QColor selectionBorderColor = m_anchorPreviewColor;
    selectionBorderColor.setAlpha(PitchDisplayStrategy::anchorSelectionBorderAlpha());
    QColor selectionFillColor = m_anchorPreviewColor;
    selectionFillColor.setAlpha(PitchDisplayStrategy::anchorSelectionFillAlpha());
    painter->setPen(QPen(selectionBorderColor, 1.5));
    painter->setBrush(selectionFillColor);
    painter->drawRoundedRect(localRect, radius, radius);
}

QPainterPath AnchorOverlayView::interpolatedPath(const QList<AnchorNode *> &nodes,
                                                 const double devicePixelRatio) const {
    QPainterPath path;
    QList<AnchorNode *> renderNodes;
    renderNodes.reserve(nodes.size());
    for (auto *node : nodes) {
        if (!renderNodes.isEmpty() && renderNodes.last()->pos() == node->pos())
            renderNodes.last() = node;
        else
            renderNodes.append(node);
    }
    if (renderNodes.size() < 2)
        return path;

    const auto dpr = std::max(1.0, devicePixelRatio);
    const auto visibleLeft = rect().left() - 2.0 / dpr;
    const auto visibleRight = rect().right() + 2.0 / dpr;
    bool hasPoint = false;
    for (int i = 0; i < renderNodes.size() - 1; ++i) {
        const auto *firstNode = renderNodes.at(i);
        const auto *secondNode = renderNodes.at(i + 1);
        const auto segmentLeft = tickToItemX(firstNode->pos());
        const auto segmentRight = tickToItemX(secondNode->pos());
        const auto startX = std::max(segmentLeft, visibleLeft);
        const auto endX = std::min(segmentRight, visibleRight);
        if (endX < startX)
            continue;

        const auto *previousNode = i > 0 ? renderNodes.at(i - 1) : nullptr;
        const auto *nextNode = i + 2 < renderNodes.size() ? renderNodes.at(i + 2) : nullptr;
        const auto interpolator =
            AnchorCurve::createInterpolator(firstNode, secondNode, previousNode, nextNode);
        auto appendPoint = [&](const double x) {
            const auto tick = sceneXToTick(x + pos().x());
            const auto value = interpolator.evaluate(tick);
            const QPointF point(x, sceneYToItemY(valueToSceneY(value)));
            if (!hasPoint) {
                path.moveTo(point);
                hasPoint = true;
            } else if (QLineF(path.currentPosition(), point).length() > 0.001) {
                path.lineTo(point);
            }
            return point.y();
        };

        auto x = startX;
        auto previousY = appendPoint(x);
        auto physicalStep = 2.0;
        while (x < endX) {
            x = std::min(endX, x + physicalStep / dpr);
            const auto y = appendPoint(x);
            const auto physicalDeltaY = std::abs(y - previousY) * dpr;
            physicalStep = physicalDeltaY > 4.0 ? 0.5 : physicalDeltaY > 2.0 ? 1.0 : 2.0;
            previousY = y;
        }
    }
    return path;
}
