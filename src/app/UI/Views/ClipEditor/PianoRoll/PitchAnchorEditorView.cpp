//
// Created by fluty on 26-4-30.
//

#include "PitchAnchorEditorView.h"

#include "EditPitchAnchorHandler.h"
#include "Model/AppModel/AnchorCurve.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Utils/MathUtils.h"

#include <QPainter>
#include <cmath>
#include <opendspxinterpolator/interpolator.h>

PitchAnchorEditorView::PitchAnchorEditorView() {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    setTransparentMouseEvents(true);
}

void PitchAnchorEditorView::setOverlayState(const AnchorOverlayState *state) {
    m_state = state;
}

double PitchAnchorEditorView::valueToSceneY(const double value) const {
    constexpr int min = 0;
    constexpr int max = 12700;
    return (12700 - MathUtils::clip(value, min, max) + 50) * scaleY() *
           ClipEditorGlobal::noteHeight / 100;
}

double PitchAnchorEditorView::sceneYToValue(const double y) const {
    constexpr int min = 0;
    constexpr int max = 12700;
    const auto value = -(y * 100 / ClipEditorGlobal::noteHeight / scaleY() - 12700 - 50);
    return MathUtils::clip(value, min, max);
}

void PitchAnchorEditorView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)
    if (!m_state || !m_state->anchorEditMode)
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);
    drawPreviewCurve(painter);
    drawDragPreviewCurve(painter);
    drawAnchorCurves(painter);
    drawSelectionRect(painter);
}

void PitchAnchorEditorView::updateRectAndPos() {
    const auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}

void PitchAnchorEditorView::drawAnchorCurves(QPainter *painter) const {
    if (!m_state)
        return;

    constexpr double anchorRadius = 2.0;
    constexpr double hoverRadius = 6.0;
    const QColor normalColor(220, 220, 220);
    const QColor selectedColor(155, 186, 255);
    const QColor curveColor(220, 220, 220, 200);

    auto drawNodeAt = [&](double x, double y, AnchorNode *node) {
        const bool isSelected = m_state->selectedNodes.contains(node);
        const bool isHovered = (node == m_state->hoveredNode);

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

    auto tickToLocalX = [this](int tick) {
        return tickToItemX(tick);
    };

    auto valueToLocalY = [this](int value) {
        return sceneYToItemY(valueToSceneY(value));
    };

    auto interpolateSegment = [&](AnchorNode *n1, AnchorNode *n2, AnchorNode *ref1,
                                  AnchorNode *ref2) {
        const double x1 = n1->pos();
        const double y1 = n1->value();
        const double x2 = n2->pos();
        const double y2 = n2->value();

        auto interp = opendspx::Interpolator<double>::createLinear(x1, y1, x2, y2);
        if (n1->interpMode() == AnchorNode::Linear) {
        } else {
            if (ref1 && ref2) {
                interp = opendspx::Interpolator<double>::create(
                    x1, y1, x2, y2, ref1->pos(), ref1->value(), ref2->pos(), ref2->value());
            } else if (ref1) {
                interp = opendspx::Interpolator<double>::createWithRef1Only(
                    x1, y1, x2, y2, ref1->pos(), ref1->value());
            } else if (ref2) {
                interp = opendspx::Interpolator<double>::createWithRef2Only(
                    x1, y1, x2, y2, ref2->pos(), ref2->value());
            } else {
                interp = opendspx::Interpolator<double>::createLinear(x1, y1, x2, y2);
            }
        }

        QPainterPath path;
        const double startX = tickToLocalX(n1->pos());
        const double endX = tickToLocalX(n2->pos());
        bool first = true;
        double prevLy = 0;
        double step = 2.0;
        double px = startX;
        while (px <= endX) {
            const double tick = sceneXToTick(px + pos().x());
            const double val = interp.evaluate(tick);
            const double ly = valueToLocalY(static_cast<int>(val));
            if (first) {
                path.moveTo(px, ly);
                first = false;
            } else {
                path.lineTo(px, ly);
                double dy = std::abs(ly - prevLy);
                step = (dy > 4.0) ? 0.5 : (dy > 2.0) ? 1.0 : 2.0;
            }
            prevLy = ly;
            px += step;
        }
        if (!first) {
            const double tick = sceneXToTick(endX + pos().x());
            const double val = interp.evaluate(tick);
            path.lineTo(endX, valueToLocalY(static_cast<int>(val)));
        }
        QPen pen(curveColor, 1.5);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    };

    auto drawCurve = [&](AnchorCurve *curve) {
        auto allNodes = curve->nodes().toList();
        if (allNodes.isEmpty())
            return;

        bool skipDraggedNodes = m_state->dragging && m_state->dragTargetCurve &&
                                curve == m_state->dragSourceCurve;
        QList<AnchorNode *> nodes;
        if (skipDraggedNodes) {
            for (auto *node : allNodes) {
                if (!m_state->selectedNodes.contains(node))
                    nodes.append(node);
            }
        } else {
            nodes = allNodes;
        }
        if (nodes.isEmpty())
            return;

        for (int i = 0; i < nodes.size() - 1; i++) {
            auto *n1 = nodes[i];
            auto *n2 = nodes[i + 1];
            auto *ref1 = (i > 0) ? nodes[i - 1] : nullptr;
            auto *ref2 = (i + 2 < nodes.size()) ? nodes[i + 2] : nullptr;
            interpolateSegment(n1, n2, ref1, ref2);
        }

        for (auto *node : nodes) {
            const double x = tickToLocalX(node->pos());
            const double y = valueToLocalY(node->value());
            drawNodeAt(x, y, node);
        }
    };

    for (auto *curve : m_state->visibleCurves)
        drawCurve(curve);
}

void PitchAnchorEditorView::drawPreviewCurve(QPainter *painter) const {
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

    const QPointF scenePreviewPos = m_state->previewPos + visibleRect().topLeft();
    const double previewTick = sceneXToTick(scenePreviewPos.x());
    const double previewValue = sceneYToValue(scenePreviewPos.y());

    AnchorNode virtualNode(static_cast<int>(previewTick), static_cast<int>(previewValue));

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
            auto predecessorMode =
                (idx > 0) ? nodes[idx - 1]->interpMode() : AnchorNode::Hermite;
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

    auto tickToLocalX = [this](int tick) { return tickToItemX(tick); };
    auto valueToLocalY = [this](int value) { return sceneYToItemY(valueToSceneY(value)); };

    auto interpolateSegment = [&](AnchorNode *n1, AnchorNode *n2, AnchorNode *ref1,
                                  AnchorNode *ref2) {
        const double x1 = n1->pos();
        const double y1 = n1->value();
        const double x2 = n2->pos();
        const double y2 = n2->value();

        auto interp = opendspx::Interpolator<double>::createLinear(x1, y1, x2, y2);
        if (n1->interpMode() != AnchorNode::Linear) {
            if (ref1 && ref2) {
                interp = opendspx::Interpolator<double>::create(
                    x1, y1, x2, y2, ref1->pos(), ref1->value(), ref2->pos(), ref2->value());
            } else if (ref1) {
                interp = opendspx::Interpolator<double>::createWithRef1Only(
                    x1, y1, x2, y2, ref1->pos(), ref1->value());
            } else if (ref2) {
                interp = opendspx::Interpolator<double>::createWithRef2Only(
                    x1, y1, x2, y2, ref2->pos(), ref2->value());
            }
        }

        const double startX = tickToLocalX(n1->pos());
        const double endX = tickToLocalX(n2->pos());

        QPainterPath path;
        bool first = true;
        double prevLy = 0;
        double step = 2.0;
        const double dir = (endX < startX) ? -1.0 : 1.0;
        double px = startX;
        while ((dir > 0) ? (px <= endX) : (px >= endX)) {
            const double tick = sceneXToTick(px + pos().x());
            const double val = interp.evaluate(tick);
            const double ly = valueToLocalY(static_cast<int>(val));
            if (first) {
                path.moveTo(px, ly);
                first = false;
            } else {
                path.lineTo(px, ly);
                double dy = std::abs(ly - prevLy);
                step = (dy > 4.0) ? 0.5 : (dy > 2.0) ? 1.0 : 2.0;
            }
            prevLy = ly;
            px += dir * step;
        }
        if (!first) {
            const double tick = sceneXToTick(endX + pos().x());
            const double val = interp.evaluate(tick);
            path.lineTo(endX, valueToLocalY(static_cast<int>(val)));
        }
        painter->drawPath(path);
    };

    QPen pen(QColor(155, 186, 255, 128), 1.5, Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    for (int i = 0; i < allNodes.size() - 1; i++) {
        auto *n1 = allNodes[i];
        auto *n2 = allNodes[i + 1];
        auto *ref1 = (i > 0) ? allNodes[i - 1] : nullptr;
        auto *ref2 = (i + 2 < allNodes.size()) ? allNodes[i + 2] : nullptr;
        interpolateSegment(n1, n2, ref1, ref2);
    }

    const double cx = tickToItemX(virtualNode.pos());
    const double cy = sceneYToItemY(valueToSceneY(virtualNode.value()));
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(155, 186, 255, 128));
    painter->drawEllipse(QPointF(cx, cy), 2.0, 2.0);

    if (isAppend && oldLastNode)
        oldLastNode->setInterpMode(savedLastMode);
}

void PitchAnchorEditorView::drawMergePreviewCurve(QPainter *painter) const {
    QList<AnchorNode *> allNodes;
    for (auto *node : m_state->currentCurve->nodes().toList())
        allNodes.append(node);
    for (auto *node : m_state->mergeCandidateCurve->nodes().toList())
        allNodes.append(node);

    std::sort(allNodes.begin(), allNodes.end(),
              [](AnchorNode *a, AnchorNode *b) { return a->pos() < b->pos(); });

    if (allNodes.size() < 2)
        return;

    auto tickToLocalX = [this](int tick) { return tickToItemX(tick); };
    auto valueToLocalY = [this](int value) { return sceneYToItemY(valueToSceneY(value)); };

    auto interpolateSegment = [&](AnchorNode *n1, AnchorNode *n2, AnchorNode *ref1,
                                  AnchorNode *ref2) {
        const double x1 = n1->pos();
        const double y1 = n1->value();
        const double x2 = n2->pos();
        const double y2 = n2->value();

        auto interp = opendspx::Interpolator<double>::createLinear(x1, y1, x2, y2);
        if (n1->interpMode() != AnchorNode::Linear) {
            if (ref1 && ref2) {
                interp = opendspx::Interpolator<double>::create(
                    x1, y1, x2, y2, ref1->pos(), ref1->value(), ref2->pos(), ref2->value());
            } else if (ref1) {
                interp = opendspx::Interpolator<double>::createWithRef1Only(
                    x1, y1, x2, y2, ref1->pos(), ref1->value());
            } else if (ref2) {
                interp = opendspx::Interpolator<double>::createWithRef2Only(
                    x1, y1, x2, y2, ref2->pos(), ref2->value());
            }
        }

        const double startX = tickToLocalX(n1->pos());
        const double endX = tickToLocalX(n2->pos());

        QPainterPath path;
        bool first = true;
        double prevLy = 0;
        double step = 2.0;
        double px = startX;
        while (px <= endX) {
            const double tick = sceneXToTick(px + pos().x());
            const double val = interp.evaluate(tick);
            const double ly = valueToLocalY(static_cast<int>(val));
            if (first) {
                path.moveTo(px, ly);
                first = false;
            } else {
                path.lineTo(px, ly);
                double dy = std::abs(ly - prevLy);
                step = (dy > 4.0) ? 0.5 : (dy > 2.0) ? 1.0 : 2.0;
            }
            prevLy = ly;
            px += step;
        }
        if (!first) {
            const double tick = sceneXToTick(endX + pos().x());
            const double val = interp.evaluate(tick);
            path.lineTo(endX, valueToLocalY(static_cast<int>(val)));
        }
        painter->drawPath(path);
    };

    QPen pen(QColor(155, 186, 255, 160), 1.5, Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    for (int i = 0; i < allNodes.size() - 1; i++) {
        auto *n1 = allNodes[i];
        auto *n2 = allNodes[i + 1];
        auto *ref1 = (i > 0) ? allNodes[i - 1] : nullptr;
        auto *ref2 = (i + 2 < allNodes.size()) ? allNodes[i + 2] : nullptr;
        interpolateSegment(n1, n2, ref1, ref2);
    }
}

void PitchAnchorEditorView::drawDragPreviewCurve(QPainter *painter) const {
    if (!m_state || !m_state->dragging || !m_state->dragTargetCurve)
        return;

    QList<AnchorNode *> allNodes = m_state->dragTargetCurve->nodes().toList();
    for (auto *node : m_state->selectedNodes) {
        auto it = std::lower_bound(allNodes.begin(), allNodes.end(), node,
                                   [](AnchorNode *a, AnchorNode *b) { return a->pos() < b->pos(); });
        allNodes.insert(it, node);
    }

    if (allNodes.size() < 2)
        return;

    auto tickToLocalX = [this](int tick) { return tickToItemX(tick); };
    auto valueToLocalY = [this](int value) { return sceneYToItemY(valueToSceneY(value)); };

    auto interpolateSegment = [&](AnchorNode *n1, AnchorNode *n2, AnchorNode *ref1,
                                  AnchorNode *ref2) {
        const double x1 = n1->pos();
        const double y1 = n1->value();
        const double x2 = n2->pos();
        const double y2 = n2->value();

        auto interp = opendspx::Interpolator<double>::createLinear(x1, y1, x2, y2);
        if (n1->interpMode() != AnchorNode::Linear) {
            if (ref1 && ref2) {
                interp = opendspx::Interpolator<double>::create(
                    x1, y1, x2, y2, ref1->pos(), ref1->value(), ref2->pos(), ref2->value());
            } else if (ref1) {
                interp = opendspx::Interpolator<double>::createWithRef1Only(
                    x1, y1, x2, y2, ref1->pos(), ref1->value());
            } else if (ref2) {
                interp = opendspx::Interpolator<double>::createWithRef2Only(
                    x1, y1, x2, y2, ref2->pos(), ref2->value());
            }
        }

        const double startX = tickToLocalX(n1->pos());
        const double endX = tickToLocalX(n2->pos());

        QPainterPath path;
        bool first = true;
        double prevLy = 0;
        double step = 2.0;
        double px = startX;
        while (px <= endX) {
            const double tick = sceneXToTick(px + pos().x());
            const double val = interp.evaluate(tick);
            const double ly = valueToLocalY(static_cast<int>(val));
            if (first) {
                path.moveTo(px, ly);
                first = false;
            } else {
                path.lineTo(px, ly);
                double dy = std::abs(ly - prevLy);
                step = (dy > 4.0) ? 0.5 : (dy > 2.0) ? 1.0 : 2.0;
            }
            prevLy = ly;
            px += step;
        }
        if (!first) {
            const double tick = sceneXToTick(endX + pos().x());
            const double val = interp.evaluate(tick);
            path.lineTo(endX, valueToLocalY(static_cast<int>(val)));
        }
        painter->drawPath(path);
    };

    QPen pen(QColor(155, 186, 255, 160), 1.5, Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    for (int i = 0; i < allNodes.size() - 1; i++) {
        auto *n1 = allNodes[i];
        auto *n2 = allNodes[i + 1];
        auto *ref1 = (i > 0) ? allNodes[i - 1] : nullptr;
        auto *ref2 = (i + 2 < allNodes.size()) ? allNodes[i + 2] : nullptr;
        interpolateSegment(n1, n2, ref1, ref2);
    }
}

void PitchAnchorEditorView::drawSelectionRect(QPainter *painter) const {
    if (!m_state || !m_state->selecting)
        return;

    const auto rect = m_state->selectionRect.normalized();
    const double x1 = sceneXToItemX(rect.left());
    const double y1 = sceneYToItemY(rect.top());
    const double x2 = sceneXToItemX(rect.right());
    const double y2 = sceneYToItemY(rect.bottom());
    QRectF localRect(QPointF(x1, y1), QPointF(x2, y2));

    const auto radius = std::min({6.0, localRect.width() / 2, localRect.height() / 2});
    painter->setPen(QPen(QColor(155, 186, 255, 200), 1.5));
    painter->setBrush(QColor(155, 186, 255, 64));
    painter->drawRoundedRect(localRect, radius, radius);
}
