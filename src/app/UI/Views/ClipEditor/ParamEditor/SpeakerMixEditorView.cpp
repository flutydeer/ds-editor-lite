//
// Created by fluty on 2026/5/4.
//

#include "SpeakerMixEditorView.h"

#include "UI/Controls/ToolTip.h"
#include "UI/Utils/SpeakerMixColorResolver.h"
#include "UI/Utils/ThemeManager.h"
#include "UI/Utils/SpeakerMixUtils.h"
#include "UI/Controls/Menu.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QApplication>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QPainter>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>

using SpeakerMixModel::normalizeSpeakerMixData;
using SpeakerMixModel::SingerSourceMode;
using SpeakerMixModel::SpeakerMixData;

namespace {
    QVector<double> toVector(const QList<double> &values) {
        QVector<double> result;
        result.reserve(values.size());
        for (const double value : values)
            result.append(value);
        return result;
    }

    QList<double> toList(const QVector<double> &values) {
        QList<double> result;
        result.reserve(values.size());
        for (const double value : values)
            result.append(value);
        return result;
    }
}

SpeakerMixEditorView::SpeakerMixEditorView() {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    setTransparentMouseEvents(false);
    setFlag(ItemIsFocusable);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this] { refreshThemeColors(); });
}

SpeakerMixEditorView::~SpeakerMixEditorView() {
    delete m_tooltip.data();
}

void SpeakerMixEditorView::setSpeakerMixData(const SpeakerMixData &data) {
    m_committedData = normalizeSpeakerMixData(data);
    syncWorkingFromCommitted();
}

void SpeakerMixEditorView::setReferenceSpeakers(const QList<SpeakerInfo> &speakers) {
    if (m_referenceSpeakers == speakers)
        return;

    m_referenceSpeakers = speakers;
    syncWorkingFromCommitted();
}

SpeakerMixData SpeakerMixEditorView::speakerMixData() const {
    return workingMixData();
}

SpeakerMixData SpeakerMixEditorView::committedMixData() const {
    return m_committedData;
}

SpeakerMixData SpeakerMixEditorView::workingMixData() const {
    SpeakerMixData result = m_committedData;
    if (m_editable) {
        result.dynamicKeyframes.clear();
        for (const auto &keyframe : m_keyframes) {
            SpeakerMixModel::SpeakerMixKeyframe modelKeyframe;
            modelKeyframe.tick = keyframe.tick;
            modelKeyframe.weights = toVector(keyframe.weights);
            result.dynamicKeyframes.append(modelKeyframe);
        }
    }
    return normalizeSpeakerMixData(result);
}

bool SpeakerMixEditorView::isEditable() const {
    return m_editable;
}

void SpeakerMixEditorView::commit() {
    if (!m_editable)
        return;

    const auto data = workingMixData();
    if (data == m_committedData)
        return;

    m_committedData = data;
    Q_EMIT speakerMixEdited(m_committedData);
}

void SpeakerMixEditorView::discard() {
    syncWorkingFromCommitted();
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

QColor SpeakerMixEditorView::textColor() const {
    return m_textColor;
}

void SpeakerMixEditorView::setTextColor(const QColor &color) {
    if (m_textColor == color)
        return;
    m_textColor = color;
    update();
}

QColor SpeakerMixEditorView::keyframeLineColor() const {
    return m_keyframeLineColor;
}

void SpeakerMixEditorView::setKeyframeLineColor(const QColor &color) {
    if (m_keyframeLineColor == color)
        return;
    m_keyframeLineColor = color;
    update();
}

QColor SpeakerMixEditorView::selectedDotColor() const {
    return m_selectedDotColor;
}

void SpeakerMixEditorView::setSelectedDotColor(const QColor &color) {
    if (m_selectedDotColor == color)
        return;
    m_selectedDotColor = color;
    update();
}

QColor SpeakerMixEditorView::selectionBorderColor() const {
    return m_selectionBorderColor;
}

void SpeakerMixEditorView::setSelectionBorderColor(const QColor &color) {
    if (m_selectionBorderColor == color)
        return;
    m_selectionBorderColor = color;
    update();
}

QColor SpeakerMixEditorView::selectionFillColor() const {
    return m_selectionFillColor;
}

void SpeakerMixEditorView::setSelectionFillColor(const QColor &color) {
    if (m_selectionFillColor == color)
        return;
    m_selectionFillColor = color;
    update();
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
    if (m_dynamicBypassed) {
        QColor bypassedTextColor = m_textColor;
        bypassedTextColor.setAlpha(180);
        painter->setPen(bypassedTextColor);
        painter->drawText(QRectF(8, 4, rect().width() - 16, 20), Qt::AlignRight | Qt::AlignTop,
                          tr("Bypassed"));
    }
}

void SpeakerMixEditorView::updateRectAndPos() {
    const auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}

void SpeakerMixEditorView::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (!m_editable) {
        event->ignore();
        return;
    }

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
    if (!m_editable) {
        event->ignore();
        return;
    }

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
    if (!m_editable) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (m_state.dragging || m_state.dragSplitIndex >= 0) {
            endDrag();
        } else if (m_state.selecting) {
            endIntervalSelection();
        }
    }
    update();
    event->accept();
}

void SpeakerMixEditorView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (!m_editable) {
        event->ignore();
        return;
    }

    if (event->button() != Qt::LeftButton)
        return;

    const auto itemPos = event->pos();
    const auto hit = hitTest(itemPos);
    if (hit.keyframeIndex < 0) {
        const double sceneX = itemPos.x() + pos().x();
        const int tick = static_cast<int>(sceneXToTick(sceneX));
        addKeyframeAt(tick);
        commit();
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
    if (!m_editable) {
        event->ignore();
        return;
    }

    if (event->key() == Qt::Key_Delete) {
        const auto before = workingMixData();
        deleteSelectedKeyframe();
        if (workingMixData() != before)
            commit();
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        discard();
        event->accept();
        return;
    }
    event->ignore();
}

void SpeakerMixEditorView::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    if (!m_editable)
        return;

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
        const auto before = workingMixData();
        deleteSelectedKeyframe();
        if (workingMixData() != before)
            commit();
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
        return toList(SpeakerMixUtils::storedWeightsToFull(toVector(kf.weights), n));
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
        const double w = kf0.weights.value(i) + t * (kf1.weights.value(i) - kf0.weights.value(i));
        result.append(w);
        sum += w;
    }
    result.append(1.0 - sum);
    return toList(SpeakerMixUtils::normalizeFullWeights(toVector(result)));
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
            QColor keyFrameColor = m_keyframeLineColor;
            keyFrameColor.setAlpha(kfIndex == m_state.hoveredKeyframeIndex ? 160 : 80);
            painter->setPen(QPen(keyFrameColor, 1.5));
            painter->drawLine(QPointF(localX, areaTop), QPointF(localX, areaTop + areaHeight));
        }

        const auto weights = interpolateWeights(kf.tick);
        const auto drawDot = [&](const int speakerIndex, const QPointF &center, const bool selected,
                                 const bool clipInnerBottom = false) {
            QColor centerColor = selected ? m_selectedDotColor : m_speakers[speakerIndex].color;
            centerColor.setAlpha(selected ? 255 : 220);

            painter->setPen(Qt::NoPen);
            painter->setBrush(m_speakers[speakerIndex].fillColor);
            painter->drawEllipse(center, kDotRadius, kDotRadius);

            painter->setBrush(centerColor);
            painter->drawEllipse(center, kInnerDotRadius, kInnerDotRadius);
        };

        drawDot(0, QPointF(localX, areaTop), isKfSelected, true);

        double cumulative = 0;
        for (int i = 0; i < n - 1; i++) {
            cumulative += weights[i];
            const double y = areaTop + areaHeight * cumulative;

            const bool isSelected = isKfSelected || (kfIndex == m_state.selectedKeyframeIndex &&
                                                     i == m_state.selectedSplitIndex);
            const int speakerIndex = i + 1;

            drawDot(speakerIndex, QPointF(localX, y), isSelected);
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
    painter->setBrush(m_selectionFillColor);
    painter->drawRect(rect);

    painter->setPen(QPen(m_selectionBorderColor, 1.0));
    painter->drawLine(QPointF(rect.left(), rect.top()), QPointF(rect.left(), rect.bottom()));
    painter->drawLine(QPointF(rect.right(), rect.top()), QPointF(rect.right(), rect.bottom()));
}

SpeakerMixHitResult SpeakerMixEditorView::hitTest(const QPointF &itemPos) const {
    const double viewHeight = visibleRect().height();
    const int n = m_speakers.size();
    const double areaTop = kPadding;
    const double areaHeight = viewHeight - 2 * kPadding;

    for (int ki = m_keyframes.size() - 1; ki >= 0; --ki) {
        const auto &kf = m_keyframes[ki];
        const double localX = tickToItemX(kf.tick);
        const double dx = itemPos.x() - localX;

        if (std::abs(dx) > kHitRadius)
            continue;

        const auto weights = interpolateWeights(kf.tick);
        double cumulative = 0;
        QVector<double> splitYs;
        splitYs.reserve(n - 1);
        for (int i = 0; i < n - 1; i++) {
            cumulative += weights[i];
            splitYs.append(areaTop + areaHeight * cumulative);
        }

        for (int i = splitYs.size() - 1; i >= 0; --i) {
            const double y = splitYs[i];
            const double dy = itemPos.y() - y;
            if (std::abs(dy) <= kHitRadius)
                return {ki, i};
        }

        if (std::abs(itemPos.y() - areaTop) <= kHitRadius)
            return {};

        return {ki, -1};
    }

    return {};
}

double SpeakerMixEditorView::cumWeightAtSplit(const SpeakerMixKeyframe &kf, int splitIndex) const {
    return SpeakerMixUtils::cumulativeWeightAtSplit(
        SpeakerMixUtils::storedWeightsToFull(toVector(kf.weights), m_speakers.size()), splitIndex);
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
    if (hit.keyframeIndex != m_state.hoveredKeyframeIndex ||
        hit.splitIndex != m_state.hoveredSplitIndex) {
        m_state.hoveredKeyframeIndex = hit.keyframeIndex;
        m_state.hoveredSplitIndex = hit.splitIndex;

        if (hit.keyframeIndex >= 0) {
            if (hit.splitIndex >= 0) {
                setCursor(Qt::SizeVerCursor);
                showSplitHoverToolTip();
            } else {
                setCursor(Qt::SizeHorCursor);
                hideSplitHoverToolTip();
            }
        } else {
            setCursor(Qt::ArrowCursor);
            hideSplitHoverToolTip();
        }
    } else if (hit.keyframeIndex >= 0 && hit.splitIndex >= 0 && m_state.dragSplitIndex < 0 &&
               m_tooltip) {
        const auto cursorPos = QCursor::pos();
        m_tooltip->move(cursorPos.x(), cursorPos.y());
    }
}

void SpeakerMixEditorView::showSplitHoverToolTip() {
    if (m_state.dragSplitIndex >= 0 || m_state.hoveredKeyframeIndex < 0 ||
        m_state.hoveredSplitIndex < 0)
        return;

    updateSplitToolTipContent(m_state.hoveredKeyframeIndex);
    auto *tooltip = ensureToolTip();
    tooltip->setWindowOpacity(1);
    const auto cursorPos = QCursor::pos();
    tooltip->move(cursorPos.x(), cursorPos.y());
    tooltip->show();
}

void SpeakerMixEditorView::hideSplitHoverToolTip() {
    if (!m_tooltip || m_state.dragSplitIndex >= 0)
        return;

    m_tooltip->setWindowOpacity(0);
    m_tooltip->hide();
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
        if (m_state.dragSplitIndex >= 0)
            showSplitDragToolTip();
    }
}

void SpeakerMixEditorView::updateDrag(const QPointF &scenePos) {
    if (m_state.selectedKeyframeIndex < 0)
        return;

    const auto delta = scenePos - m_state.dragStartScenePos;
    if (!m_state.dragging) {
        if (std::abs(delta.x()) > kDragThreshold || std::abs(delta.y()) > kDragThreshold)
            m_state.dragging = true;
        else {
            updateSplitDragToolTip();
            return;
        }
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

    const double deltaItemY = delta.y();
    const QVector<double> dragStartFullWeights = SpeakerMixUtils::storedWeightsToFull(
        toVector(m_state.dragStartWeights.weights), m_speakers.size());

    if (altHeld) {
        const double oldCum = SpeakerMixUtils::cumulativeWeightAtSplit(dragStartFullWeights, si);
        const double newCum = SpeakerMixUtils::snapCumulativeToPercent(
            cumWeightFromItemY(cumWeightToItemY(oldCum) + deltaItemY));
        kf.weights = toList(SpeakerMixUtils::fullWeightsToStored(
            SpeakerMixUtils::proportionalDragWeights(dragStartFullWeights, si, newCum)));
    } else {
        const double oldCum = SpeakerMixUtils::cumulativeWeightAtSplit(dragStartFullWeights, si);
        const double newCum = SpeakerMixUtils::snapCumulativeToPercent(
            cumWeightFromItemY(cumWeightToItemY(oldCum) + deltaItemY));
        kf.weights = toList(SpeakerMixUtils::fullWeightsToStored(
            SpeakerMixUtils::adjacentDragWeights(dragStartFullWeights, si, newCum)));
    }

    updateSplitDragToolTip();
}

void SpeakerMixEditorView::endDrag() {
    bool changed = false;
    if (m_state.selectedKeyframeIndex >= 0 && m_state.selectedKeyframeIndex < m_keyframes.size()) {
        const auto &keyframe = m_keyframes[m_state.selectedKeyframeIndex];
        changed = keyframe.tick != m_state.dragStartWeights.tick ||
                  keyframe.weights != m_state.dragStartWeights.weights;
    }
    m_state.dragging = false;
    m_state.dragSplitIndex = -1;
    hideSplitDragToolTip();
    if (changed)
        commit();
}

void SpeakerMixEditorView::showSplitDragToolTip() {
    updateSplitDragToolTip();
    auto *tooltip = ensureToolTip();
    tooltip->setWindowOpacity(1);
    const auto cursorPos = QCursor::pos();
    tooltip->move(cursorPos.x(), cursorPos.y());
    tooltip->show();
}

void SpeakerMixEditorView::updateSplitDragToolTip() {
    if (m_state.selectedKeyframeIndex < 0 || m_state.dragSplitIndex < 0 ||
        m_state.selectedKeyframeIndex >= m_keyframes.size())
        return;

    updateSplitToolTipContent(m_state.selectedKeyframeIndex);
    const auto cursorPos = QCursor::pos();
    m_tooltip->move(cursorPos.x(), cursorPos.y());
}

void SpeakerMixEditorView::hideSplitDragToolTip() {
    if (!m_tooltip)
        return;

    m_tooltip->setWindowOpacity(0);
    m_tooltip->hide();
    m_tooltip->deleteLater();
    m_tooltip = nullptr;
}

ToolTip *SpeakerMixEditorView::ensureToolTip() {
    if (!m_tooltip) {
        QWidget *parent = nullptr;
        if (scene() && !scene()->views().isEmpty())
            parent = scene()->views().first();
        if (!parent)
            parent = QApplication::activeWindow();
        m_tooltip = new ToolTip(QString(), parent);
    }
    return m_tooltip;
}

void SpeakerMixEditorView::updateSplitToolTipContent(const int keyframeIndex) {
    if (keyframeIndex < 0 || keyframeIndex >= m_keyframes.size())
        return;

    const auto fullWeights = SpeakerMixUtils::storedWeightsToFull(
        toVector(m_keyframes[keyframeIndex].weights), m_speakers.size());
    const auto displayValues = SpeakerMixUtils::fullWeightsToPercentages(fullWeights);

    QStringList titleParts;
    for (int i = 0; i < m_speakers.size(); ++i)
        titleParts.append(QString("%1: %2%").arg(m_speakers[i].name).arg(displayValues.value(i)));

    auto *tooltip = ensureToolTip();
    tooltip->setTitle(titleParts.join("\n"));
    tooltip->setMessage({});
}

void SpeakerMixEditorView::addKeyframeAt(int tick) {
    if (tick == 0)
        return;

    const auto weights = interpolateWeights(tick);
    SpeakerMixKeyframe kf;
    kf.tick = tick;
    kf.weights = toList(SpeakerMixUtils::fullWeightsToStored(toVector(weights)));

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

void SpeakerMixEditorView::refreshThemeColors() {
    for (int i = 0; i < m_speakers.size(); ++i) {
        if (i >= m_committedData.sources.size())
            break;
        const auto &speaker = m_committedData.sources[i].speaker;
        const auto colors =
            SpeakerMixColorResolver::colorsForSpeaker(speaker.id(), m_referenceSpeakers, i);
        m_speakers[i].color = colors.accent;
        m_speakers[i].fillColor = colors.areaFill;
        m_speakers[i].dotFillColor = colors.dotFill;
    }
    update();
    emit speakerColorsChanged();
}

void SpeakerMixEditorView::syncWorkingFromCommitted() {
    m_speakers.clear();
    m_keyframes.clear();
    clearInteractionState();
    hideSplitHoverToolTip();
    hideSplitDragToolTip();

    for (int i = 0; i < m_committedData.sources.size(); ++i) {
        const auto &speaker = m_committedData.sources[i].speaker;
        QString name = speaker.name();
        if (name.isEmpty())
            name = speaker.id();
        const auto colors =
            SpeakerMixColorResolver::colorsForSpeaker(speaker.id(), m_referenceSpeakers, i);
        m_speakers.append({name, colors.accent, colors.areaFill, colors.dotFill});
    }

    m_dynamicBypassed = SpeakerMixModel::isDynamicMixBypassed(m_committedData);
    m_editable = m_committedData.sources.size() >= 2 && !m_committedData.dynamicKeyframes.isEmpty();

    const auto appendKeyframe = [this](const SpeakerMixModel::SpeakerMixKeyframe &keyframe) {
        m_keyframes.append({keyframe.tick, toList(keyframe.weights)});
    };

    if (!m_committedData.dynamicKeyframes.isEmpty()) {
        for (const auto &keyframe : m_committedData.dynamicKeyframes)
            appendKeyframe(keyframe);
    } else if (m_committedData.sources.size() >= 2 && !m_committedData.fixedWeights.isEmpty()) {
        m_keyframes.append({0, toList(m_committedData.fixedWeights)});
    }

    setTransparentMouseEvents(!m_editable);
    update();
}

void SpeakerMixEditorView::clearInteractionState() {
    m_state = {};
}
