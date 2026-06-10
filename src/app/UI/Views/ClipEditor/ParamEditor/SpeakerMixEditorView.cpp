//
// Created by fluty on 2026/5/4.
//

#include "SpeakerMixEditorView.h"

#include "UI/Utils/TrackColorPalette.h"
#include "UI/Controls/Menu.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <limits>

SpeakerMixEditorView::SpeakerMixEditorView() {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    setTransparentMouseEvents(false);
    setFlag(ItemIsFocusable);

    auto *palette = TrackColorPalette::instance();
    m_speakers = {
        {QStringLiteral("spk1"), palette->baseColor(0), palette->speakerMixParamFill(0),
         palette->speakerMixDotFill(0)},
        {QStringLiteral("spk2"), palette->baseColor(1), palette->speakerMixParamFill(1),
         palette->speakerMixDotFill(1)},
        {QStringLiteral("spk3"), palette->baseColor(2), palette->speakerMixParamFill(2),
         palette->speakerMixDotFill(2)},
    };

    m_keyframes = {
        {0,    {0.33, 0.33}},
        {480,  {0.60, 0.20}},
        {960,  {0.10, 0.40}},
        {1440, {0.33, 0.33}},
    };
}

const QList<SpeakerMixSpeaker> &SpeakerMixEditorView::speakers() const {
    return m_speakers;
}

const QList<SpeakerMixKeyframe> &SpeakerMixEditorView::keyframes() const {
    return m_keyframes;
}

int SpeakerMixEditorView::keyframeCount() const {
    return m_keyframes.size();
}

int SpeakerMixEditorView::selectedKeyframeIndex() const {
    return m_state.selectedKeyframeIndex;
}

double SpeakerMixEditorView::previousKeyframeTick(double currentTick) const {
    double prevTick = -1;
    for (const auto &kf : m_keyframes) {
        if (kf.tick < currentTick)
            prevTick = kf.tick;
        else
            break;
    }
    return prevTick;
}

double SpeakerMixEditorView::nextKeyframeTick(double currentTick) const {
    for (const auto &kf : m_keyframes) {
        if (kf.tick > currentTick)
            return kf.tick;
    }
    return -1;
}

void SpeakerMixEditorView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                 QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (m_speakers.isEmpty() || m_keyframes.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);
    drawStackedArea(painter);
    drawKeyframeDots(painter);
    drawSelectionRect(painter);
}

void SpeakerMixEditorView::updateRectAndPos() {
    const auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}

void SpeakerMixEditorView::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    setFocus();

    const auto itemPos = event->pos();
    const auto hit = hitTest(itemPos);

    if (hit.keyframeIndex >= 0) {
        m_state.selectedKeyframeIndex = hit.keyframeIndex;
        m_state.selectedSplitIndex = hit.splitIndex;
        m_state.selectedKeyframeIndices.clear();
        startDrag(event->scenePos());
    } else {
        m_state.selectedKeyframeIndex = -1;
        m_state.selectedSplitIndex = -1;
        m_state.selectedKeyframeIndices.clear();
        startIntervalSelection(itemPos);
    }

    update();
    event->accept();
}

void SpeakerMixEditorView::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        if (m_state.dragging || m_state.selectedKeyframeIndex >= 0) {
            updateDrag(event->scenePos());
        } else if (m_state.selecting) {
            updateIntervalSelection(event->pos());
        }
    } else {
        updateHover(event->pos());
    }
    update();
    event->accept();
}

void SpeakerMixEditorView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_state.dragging) {
            endDrag();
        } else if (m_state.selecting) {
            endIntervalSelection();
        }
    }
    update();
    event->accept();
}

void SpeakerMixEditorView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;

    const auto itemPos = event->pos();
    const auto hit = hitTest(itemPos);
    if (hit.keyframeIndex < 0) {
        const double sceneX = itemPos.x() + pos().x();
        const int tick = static_cast<int>(sceneXToTick(sceneX));
        addKeyframeAt(tick);
    }
    update();
    event->accept();
}

void SpeakerMixEditorView::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    updateHover(event->pos());
    update();
    event->accept();
}

void SpeakerMixEditorView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Delete) {
        deleteSelectedKeyframe();
        update();
        event->accept();
        return;
    }
    event->ignore();
}

void SpeakerMixEditorView::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    const auto itemPos = event->pos();
    const auto hit = hitTest(itemPos);

    if (hit.keyframeIndex < 0)
        return;

    if (hit.keyframeIndex != m_state.selectedKeyframeIndex) {
        m_state.selectedKeyframeIndex = hit.keyframeIndex;
        m_state.selectedSplitIndex = hit.splitIndex;
        m_state.selectedKeyframeIndices.clear();
        update();
    }

    auto &kf = m_keyframes[hit.keyframeIndex];
    const bool isInitial = (kf.tick == 0);

    auto views = scene()->views();
    auto *menu = new Menu(views.isEmpty() ? nullptr : views.first());

    auto *deleteAction = menu->addAction(tr("Delete"));
    deleteAction->setEnabled(!isInitial);
    connect(deleteAction, &QAction::triggered, this, [this] {
        deleteSelectedKeyframe();
        update();
    });

    menu->exec(event->screenPos());
    menu->deleteLater();
}

QList<double> SpeakerMixEditorView::interpolateWeights(const double tick) const {
    if (m_keyframes.isEmpty())
        return {};

    const int n = m_speakers.size();

    auto expandWeights = [&](const SpeakerMixKeyframe &kf) {
        QList<double> result = kf.weights;
        double sum = 0;
        for (auto w : result)
            sum += w;
        result.append(1.0 - sum);
        return result;
    };

    if (tick <= m_keyframes.first().tick)
        return expandWeights(m_keyframes.first());

    if (tick >= m_keyframes.last().tick)
        return expandWeights(m_keyframes.last());

    int idx = 0;
    for (int i = 0; i < m_keyframes.size() - 1; i++) {
        if (tick >= m_keyframes[i].tick && tick < m_keyframes[i + 1].tick) {
            idx = i;
            break;
        }
    }

    const auto &kf0 = m_keyframes[idx];
    const auto &kf1 = m_keyframes[idx + 1];
    const double t =
        static_cast<double>(tick - kf0.tick) / static_cast<double>(kf1.tick - kf0.tick);

    QList<double> result;
    result.reserve(n);
    double sum = 0;
    for (int i = 0; i < n - 1; i++) {
        const double w = kf0.weights[i] + t * (kf1.weights[i] - kf0.weights[i]);
        result.append(w);
        sum += w;
    }
    result.append(1.0 - sum);
    return result;
}

void SpeakerMixEditorView::drawStackedArea(QPainter *painter) const {
    const double viewWidth = visibleRect().width();
    const double viewHeight = visibleRect().height();
    const int n = m_speakers.size();

    if (viewWidth <= 0 || viewHeight <= 0)
        return;

    const double areaTop = kPadding;
    const double areaHeight = viewHeight - 2 * kPadding;

    QList<QPainterPath> fillPaths;
    QList<QPainterPath> borderPaths;
    QList<QList<double>> splitEdges;
    fillPaths.reserve(n);
    borderPaths.reserve(n);
    splitEdges.reserve(n - 1);
    for (int i = 0; i < n; i++)
        fillPaths.append(QPainterPath());
    for (int i = 0; i < n; i++)
        borderPaths.append(QPainterPath());
    for (int i = 0; i < n - 1; i++) {
        splitEdges.append(QList<double>());
    }

    const double step = 1.0;
    const int sampleCount = static_cast<int>(viewWidth / step) + 1;

    for (int s = 0; s <= sampleCount; s++) {
        const double localX = s * step;
        const double sceneX = localX + visibleRect().left();
        const double tick = sceneXToTick(sceneX);
        const auto weights = interpolateWeights(tick);

        double cumulative = 0;
        for (int i = 0; i < n - 1; i++) {
            cumulative += weights[i];
            splitEdges[i].append(areaTop + areaHeight * cumulative);
        }
    }

    for (int i = 0; i < n; i++) {
        QPainterPath &path = fillPaths[i];

        const auto topYAt = [&](int sample) {
            return (i == 0) ? areaTop : splitEdges[i - 1][sample];
        };
        const auto bottomYAt = [&](int sample) {
            return (i == n - 1) ? (areaTop + areaHeight) : splitEdges[i][sample];
        };

        path.moveTo(0, topYAt(0));
        for (int s = 0; s <= sampleCount; s++) {
            const double localX = s * step;
            path.lineTo(localX, topYAt(s));
        }

        for (int s = sampleCount; s >= 0; s--) {
            const double localX = s * step;
            path.lineTo(localX, bottomYAt(s));
        }

        path.closeSubpath();
    }

    for (int i = 0; i < n; i++) {
        QPainterPath &border = borderPaths[i];
        const auto borderYAt = [&](int sample) {
            return (i == 0) ? areaTop : splitEdges[i - 1][sample];
        };

        border.moveTo(0, borderYAt(0));
        for (int s = 1; s <= sampleCount; s++) {
            const double localX = s * step;
            border.lineTo(localX, borderYAt(s));
        }
    }

    for (int i = 0; i < n; i++) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_speakers[i].fillColor);
        painter->drawPath(fillPaths[i]);
    }

    painter->setBrush(Qt::NoBrush);
    for (int i = 0; i < n; i++) {
        QColor borderColor = m_speakers[i].color;
        borderColor.setAlpha(220);
        painter->setPen(QPen(borderColor, 1.5));
        painter->drawPath(borderPaths[i]);
    }
}

void SpeakerMixEditorView::drawKeyframeDots(QPainter *painter) const {
    const double viewHeight = visibleRect().height();
    const int n = m_speakers.size();
    const double areaTop = kPadding;
    const double areaHeight = viewHeight - 2 * kPadding;

    for (auto &kf : m_keyframes) {
        const double localX = tickToItemX(kf.tick);

        if (localX < -kDotRadius || localX > visibleRect().width() + kDotRadius)
            continue;

        const int kfIndex = &kf - &m_keyframes.first();
        const bool isKfSelected = (kfIndex == m_state.selectedKeyframeIndex) ||
                                  m_state.selectedKeyframeIndices.contains(kfIndex);

        if (kf.tick != 0) {
            QColor keyFrameColor;
            if (kfIndex == m_state.hoveredKeyframeIndex)
                keyFrameColor = QColor(220, 220, 220, 160);
            else
                keyFrameColor = QColor(220, 220, 220, 80);
            painter->setPen(QPen(keyFrameColor, 1.5));
            painter->drawLine(QPointF(localX, areaTop), QPointF(localX, areaTop + areaHeight));
        }

        const auto weights = interpolateWeights(kf.tick);

        double cumulative = 0;
        for (int i = 0; i < n - 1; i++) {
            cumulative += weights[i];
            const double y = areaTop + areaHeight * cumulative;

            const bool isSelected =
                isKfSelected || (kfIndex == m_state.selectedKeyframeIndex &&
                                 i == m_state.selectedSplitIndex);
            const int speakerIndex = i + 1;

            QColor strokeColor = isSelected ? QColor(255, 255, 255) : m_speakers[speakerIndex].color;
            strokeColor.setAlpha(isSelected ? 255 : 220);
            QColor fillColor = m_speakers[speakerIndex].dotFillColor;
            painter->setPen(QPen(strokeColor, 1.5));
            painter->setBrush(fillColor);
            painter->drawEllipse(QPointF(localX, y), kDotRadius, kDotRadius);
        }
    }
}

void SpeakerMixEditorView::drawSelectionRect(QPainter *painter) const {
    if (!m_state.selecting)
        return;

    const auto rect = m_state.selectionRect;
    if (rect.width() <= 0)
        return;

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(155, 186, 255, 40));
    painter->drawRect(rect);

    painter->setPen(QPen(QColor(155, 186, 255, 200), 1.0));
    painter->drawLine(QPointF(rect.left(), rect.top()), QPointF(rect.left(), rect.bottom()));
    painter->drawLine(QPointF(rect.right(), rect.top()), QPointF(rect.right(), rect.bottom()));
}

SpeakerMixHitResult SpeakerMixEditorView::hitTest(const QPointF &itemPos) const {
    const double viewHeight = visibleRect().height();
    const int n = m_speakers.size();
    const double areaTop = kPadding;
    const double areaHeight = viewHeight - 2 * kPadding;

    for (int ki = 0; ki < m_keyframes.size(); ki++) {
        const auto &kf = m_keyframes[ki];
        const double localX = tickToItemX(kf.tick);
        const double dx = itemPos.x() - localX;

        if (std::abs(dx) > kHitRadius)
            continue;

        const auto weights = interpolateWeights(kf.tick);
        double cumulative = 0;
        bool nearSplitPoint = false;
        for (int i = 0; i < n - 1; i++) {
            cumulative += weights[i];
            const double y = areaTop + areaHeight * cumulative;
            const double dy = itemPos.y() - y;
            if (std::abs(dy) <= kHitRadius) {
                nearSplitPoint = true;
                return {ki, i};
            }
        }

        if (!nearSplitPoint)
            return {ki, -1};
    }

    return {};
}

double SpeakerMixEditorView::cumWeightAtSplit(const SpeakerMixKeyframe &kf, int splitIndex) const {
    double cum = 0;
    for (int i = 0; i <= splitIndex; i++)
        cum += kf.weights[i];
    return cum;
}

double SpeakerMixEditorView::cumWeightFromItemY(double itemY) const {
    const double viewHeight = visibleRect().height();
    const double areaHeight = viewHeight - 2 * kPadding;
    return (itemY - kPadding) / areaHeight;
}

double SpeakerMixEditorView::cumWeightToItemY(double cumWeight) const {
    const double viewHeight = visibleRect().height();
    const double areaHeight = viewHeight - 2 * kPadding;
    return kPadding + areaHeight * cumWeight;
}

void SpeakerMixEditorView::updateHover(const QPointF &itemPos) {
    const auto hit = hitTest(itemPos);
    if (hit.keyframeIndex != m_state.hoveredKeyframeIndex || hit.splitIndex != m_state.hoveredSplitIndex) {
        m_state.hoveredKeyframeIndex = hit.keyframeIndex;
        m_state.hoveredSplitIndex = hit.splitIndex;

        if (hit.keyframeIndex >= 0) {
            if (hit.splitIndex >= 0)
                setCursor(Qt::SizeVerCursor);
            else
                setCursor(Qt::SizeHorCursor);

            const auto weights = interpolateWeights(m_keyframes[hit.keyframeIndex].tick);
            QStringList parts;
            for (int i = 0; i < m_speakers.size(); i++)
                parts.append(QString("%1: %2%")
                                 .arg(m_speakers[i].name)
                                 .arg(weights[i] * 100, 0, 'f', 1));
            setToolTip(parts.join("\n"));
        } else {
            setCursor(Qt::ArrowCursor);
            setToolTip(QString());
        }
    }
}

void SpeakerMixEditorView::startIntervalSelection(const QPointF &itemPos) {
    m_state.selecting = true;
    m_state.selectionStartPos = itemPos;
    m_state.selectionRect = QRectF(itemPos.x(), 0, 0, visibleRect().height());
    m_state.selectedKeyframeIndices.clear();
}

void SpeakerMixEditorView::updateIntervalSelection(const QPointF &itemPos) {
    const double left = std::min(m_state.selectionStartPos.x(), itemPos.x());
    const double right = std::max(m_state.selectionStartPos.x(), itemPos.x());
    m_state.selectionRect = QRectF(left, 0, right - left, visibleRect().height());

    m_state.selectedKeyframeIndices.clear();

    for (int ki = 0; ki < m_keyframes.size(); ki++) {
        const double localX = tickToItemX(m_keyframes[ki].tick);
        if (localX >= left && localX <= right)
            m_state.selectedKeyframeIndices.append(ki);
    }
}

void SpeakerMixEditorView::endIntervalSelection() {
    m_state.selecting = false;
    m_state.selectionRect = QRectF();
}

void SpeakerMixEditorView::startDrag(const QPointF &scenePos) {
    m_state.dragging = false;
    m_state.dragStartScenePos = scenePos;
    m_state.altDrag = false;

    if (m_state.selectedKeyframeIndex >= 0) {
        m_state.dragStartWeights = m_keyframes[m_state.selectedKeyframeIndex];
        m_state.dragStartTick = m_keyframes[m_state.selectedKeyframeIndex].tick;
        m_state.dragSplitIndex = m_state.selectedSplitIndex;
    }
}

void SpeakerMixEditorView::updateDrag(const QPointF &scenePos) {
    if (m_state.selectedKeyframeIndex < 0)
        return;

    const auto delta = scenePos - m_state.dragStartScenePos;
    if (!m_state.dragging) {
        if (std::abs(delta.x()) > kDragThreshold || std::abs(delta.y()) > kDragThreshold)
            m_state.dragging = true;
        else
            return;
    }

    const int ki = m_state.selectedKeyframeIndex;
    auto &kf = m_keyframes[ki];
    const int si = m_state.dragSplitIndex;

    if (si < 0) {
        if (m_state.dragStartTick == 0)
            return;

        const double newSceneX = m_state.dragStartScenePos.x() + delta.x();
        int newTick = static_cast<int>(sceneXToTick(newSceneX));

        const int prevTick = (ki > 0) ? m_keyframes[ki - 1].tick + 1 : 1;
        const int nextTick = (ki < m_keyframes.size() - 1) ? m_keyframes[ki + 1].tick - 1
                                                           : std::numeric_limits<int>::max();
        newTick = std::clamp(newTick, prevTick, nextTick);
        kf.tick = newTick;
        return;
    }

    const auto *view = scene()->views().isEmpty() ? nullptr : scene()->views().first();
    const bool altHeld = view ? (QApplication::keyboardModifiers() & Qt::AltModifier) : false;
    m_state.altDrag = altHeld;

    const int n = m_speakers.size();
    const double deltaItemY = delta.y();

    if (altHeld) {
        const double oldCum = cumWeightAtSplit(m_state.dragStartWeights, si);
        const double newCum = cumWeightFromItemY(cumWeightToItemY(oldCum) + deltaItemY);
        const double clampedCum = std::clamp(newCum, 0.0, 1.0);

        if (oldCum > 0 && oldCum < 1.0) {
            const double scaleLeft = (oldCum > 0) ? clampedCum / oldCum : 1.0;
            const double scaleRight =
                (1.0 - oldCum > 0) ? (1.0 - clampedCum) / (1.0 - oldCum) : 1.0;

            for (int i = 0; i <= si; i++)
                kf.weights[i] = m_state.dragStartWeights.weights[i] * scaleLeft;
            for (int i = si + 1; i < n - 1; i++)
                kf.weights[i] = m_state.dragStartWeights.weights[i] * scaleRight;
        }
    } else {
        const double oldCum = cumWeightAtSplit(m_state.dragStartWeights, si);
        const double newCum = cumWeightFromItemY(cumWeightToItemY(oldCum) + deltaItemY);

        double prevCumBound = (si > 0) ? cumWeightAtSplit(m_state.dragStartWeights, si - 1) : 0.0;
        double nextCumBound = (si < n - 2) ? cumWeightAtSplit(m_state.dragStartWeights, si + 1)
                                           : 1.0;

        double clampedCum = std::clamp(newCum, prevCumBound + 0.01, nextCumBound - 0.01);

        double deltaCum = clampedCum - oldCum;
        kf.weights[si] = m_state.dragStartWeights.weights[si] + deltaCum;
        if (si + 1 < n - 1)
            kf.weights[si + 1] = m_state.dragStartWeights.weights[si + 1] - deltaCum;
    }
}

void SpeakerMixEditorView::endDrag() {
    m_state.dragging = false;
    m_state.dragSplitIndex = -1;
}

void SpeakerMixEditorView::addKeyframeAt(int tick) {
    if (tick == 0)
        return;

    const auto weights = interpolateWeights(tick);
    SpeakerMixKeyframe kf;
    kf.tick = tick;
    for (int i = 0; i < m_speakers.size() - 1; i++)
        kf.weights.append(weights[i]);

    auto it = std::lower_bound(m_keyframes.begin(), m_keyframes.end(), tick,
                               [](const SpeakerMixKeyframe &kf, int t) { return kf.tick < t; });
    int insertIndex = static_cast<int>(it - m_keyframes.begin());
    m_keyframes.insert(it, kf);

    m_state.selectedKeyframeIndex = insertIndex;
    m_state.selectedSplitIndex = 0;
    m_state.selectedKeyframeIndices.clear();
}

void SpeakerMixEditorView::deleteSelectedKeyframe() {
    if (!m_state.selectedKeyframeIndices.isEmpty()) {
        auto indices = m_state.selectedKeyframeIndices;
        std::sort(indices.begin(), indices.end(), std::greater<int>());
        for (int ki : indices) {
            if (ki >= 0 && ki < m_keyframes.size() && m_keyframes[ki].tick != 0)
                m_keyframes.removeAt(ki);
        }
        m_state.selectedKeyframeIndices.clear();
        m_state.selectedKeyframeIndex = -1;
        m_state.selectedSplitIndex = -1;
        return;
    }

    if (m_state.selectedKeyframeIndex < 0)
        return;

    if (m_keyframes[m_state.selectedKeyframeIndex].tick == 0)
        return;

    m_keyframes.removeAt(m_state.selectedKeyframeIndex);

    m_state.selectedKeyframeIndex = -1;
    m_state.selectedSplitIndex = -1;
}
