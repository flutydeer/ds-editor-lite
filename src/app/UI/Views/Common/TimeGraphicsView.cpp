#include "TimeGraphicsView.h"

#include <QApplication>
#include <QCursor>
#include <QHideEvent>
#include <QPainter>
#include <QScrollBar>
#include <QShowEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "TimeGraphicsScene.h"
#include "TimeGridView.h"
#include "TimeIndicatorView.h"
#include "EditorWheelController.h"
#include "Controller/PlaybackController.h"
#include "UI/Views/Common/AutoPageTurnUtils.h"
#include "Model/AppStatus/AppStatus.h"
#include "Global/AppGlobal.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/GUI/Controls/OverlayScrollBar.h>

TimeGraphicsView::TimeGraphicsView(TimeGraphicsScene *scene, bool showLastPlaybackPosition,
                                   QWidget *parent)
    : QGraphicsView(parent), m_scene(scene) {
    setRenderHint(QPainter::Antialiasing);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_Hover);
    setMinimumHeight(150);
    setAcceptDrops(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_hScrollBar = OverlayScrollBar::install(this, Qt::Horizontal);
    m_vScrollBar = OverlayScrollBar::install(this, Qt::Vertical);
    // The two bars are companions: when both are shown their tails recede so the
    // bottom-right corner stays clear instead of overlapping.
    m_hScrollBar->setCompanion(m_vScrollBar);
    m_vScrollBar->setCompanion(m_hScrollBar);
    // OverlayScrollBar visibility is controlled internally by rangeChanged; use
    // setRangeVisible(false) to keep it hidden (e.g. the parameter editor).

    m_hBarAnimation.setTargetObject(this);
    m_hBarAnimation.setPropertyName("horizontalScrollBarValue");
    m_hBarAnimation.setEasingCurve(QEasingCurve::OutCubic);

    m_vBarAnimation.setTargetObject(this);
    m_vBarAnimation.setPropertyName("verticalScrollBarValue");
    m_vBarAnimation.setEasingCurve(QEasingCurve::OutCubic);

    m_wheelInput.setDiscreteAnimationEnabled(isEditorWheelAnimationEnabled);
    const auto installScrollTarget = [this](const Qt::Orientation orientation,
                                            const double viewportFraction) {
        m_wheelInput.setScrollTarget(
            orientation, {
                             .value =
                                 [this, orientation] {
                                     return orientation == Qt::Horizontal ? horizontalBarValue()
                                                                          : verticalBarValue();
                                 },
                             .setValue =
                                 [this, orientation](const double value) {
                                     if (orientation == Qt::Horizontal)
                                         setHorizontalBarValue(qRound(value));
                                     else
                                         setVerticalBarValue(qRound(value));
                                 },
                             .boundedValue =
                                 [this, orientation](const double value) {
                                     const auto *bar = orientation == Qt::Horizontal
                                                           ? horizontalScrollBar()
                                                           : verticalScrollBar();
                                     return static_cast<double>(
                                         qBound(bar->minimum(), qRound(value), bar->maximum()));
                                 },
                             .step =
                                 [this, orientation, viewportFraction] {
                                     return (orientation == Qt::Horizontal ? viewport()->width()
                                                                           : viewport()->height()) *
                                            viewportFraction;
                                 },
                             .canScroll = [] { return true; },
                         });
    };
    installScrollTarget(Qt::Horizontal, 0.2);
    installScrollTarget(Qt::Vertical, 0.15);
    m_wheelInput.setZoomTarget(
        Qt::Horizontal,
        {
            .value = [this] { return scaleX(); },
            .setValueAt =
                [this](const double value, const double anchor) {
                    setScaleAt(Qt::Horizontal, value, anchor);
                },
            .boundedValue =
                [this](const double value) { return boundedScale(Qt::Horizontal, value); },
            .step = 0.4,
        });
    m_wheelInput.setZoomTarget(
        Qt::Vertical,
        {
            .value = [this] { return scaleY(); },
            .setValueAt = [this](const double value,
                                 const double anchor) { setScaleAt(Qt::Vertical, value, anchor); },
            .boundedValue =
                [this](const double value) { return boundedScale(Qt::Vertical, value); },
            .step = 0.3,
        });

    const auto clearLogicalViewport = [this] {
        if (m_hBarAnimation.state() == QAbstractAnimation::Running ||
            m_vBarAnimation.state() == QAbstractAnimation::Running)
            return;
        m_logicalHorizontalBarValue.reset();
        m_logicalVerticalBarValue.reset();
    };
    connect(&m_hBarAnimation, &QPropertyAnimation::finished, this, clearLogicalViewport);
    connect(&m_vBarAnimation, &QPropertyAnimation::finished, this, clearLogicalViewport);

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &TimeGraphicsView::notifyVisibleRectChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &TimeGraphicsView::notifyVisibleRectChanged);

    initializeAnimation();
    updateAnimationDuration();

    connect(this, &TimeGraphicsView::visibleRectChanged,
            [this](const QRectF &rect) { m_scene->setVisibleRect(rect); });
    connect(this, &TimeGraphicsView::scaleChanged,
            [this](double sx, double sy) { m_scene->setScaleXY(sx, sy); });
    connect(m_scene, &TimeGraphicsScene::baseSizeChanged, this,
            &TimeGraphicsView::adjustScaleXToFillView);

    m_sceneLastPlayPosIndicator = new TimeIndicatorView;
    m_sceneLastPlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    QPen lastPlayPosPen;
    lastPlayPosPen.setWidth(1);
    lastPlayPosPen.setColor(m_lastPlayPosIndicatorColor);
    lastPlayPosPen.setStyle(Qt::DashLine);
    m_sceneLastPlayPosIndicator->setPen(lastPlayPosPen);
    if (showLastPlaybackPosition)
        m_scene->addTimeIndicator(m_sceneLastPlayPosIndicator);

    setScene(m_scene);
    setEnsureSceneFillViewX(true);

    connect(this, &TimeGraphicsView::scaleChanged, this,
            [this] { emit timeRangeChanged(startTick(), endTick()); });
    connect(this, &TimeGraphicsView::visibleRectChanged, this,
            [this] { emit timeRangeChanged(startTick(), endTick()); });
    connect(this, &TimeGraphicsView::timeRangeChanged, this,
            [this] { updateAutoPageTurnAvailability(); });
    connect(appModel, &AppModel::timelineChanged, this,
            &TimeGraphicsView::updateAutoPageTurnAvailability);

    connect(&m_edgeAutoScroller, &EdgeAutoScroller::frame, this,
            &TimeGraphicsView::onEdgeAutoScrollTimerFrame);
    QTimer::singleShot(0, this, &TimeGraphicsView::updateAutoPageTurnAvailability);
}

TimeGraphicsScene *TimeGraphicsView::scene() {
    return m_scene;
}

void TimeGraphicsView::setGridItem(TimeGridView *item) {
    if (m_gridItem)
        m_scene->removeCommonItem(m_gridItem);
    m_gridItem = item;
    m_scene->addTimeGrid(item);
    m_gridItem->setOffset(m_offset);
    setBarLineColor(barLineColor());
    setBeatLineColor(beatLineColor());
    setCommonLineColor(commonLineColor());
}

void TimeGraphicsView::setSceneVisibility(bool on) {
    setVisible(on);
}

qreal TimeGraphicsView::scaleXMax() const {
    return m_scaleXMax;
}

void TimeGraphicsView::setScaleXMax(qreal max) {
    m_scaleXMax = max;
}

double TimeGraphicsView::scaleYMin() const {
    return m_scaleYMin;
}

void TimeGraphicsView::setScaleYMin(double min) {
    m_scaleYMin = min;
}

int TimeGraphicsView::horizontalBarValue() const {
    return horizontalScrollBar()->value();
}

void TimeGraphicsView::setHorizontalBarValue(const int value) {
    if (m_hBarAnimation.state() != QAbstractAnimation::Running)
        m_logicalHorizontalBarValue.reset();
    horizontalScrollBar()->setValue(value);
}

int TimeGraphicsView::verticalBarValue() const {
    return verticalScrollBar()->value();
}

void TimeGraphicsView::setVerticalBarValue(const int value) {
    if (m_vBarAnimation.state() != QAbstractAnimation::Running)
        m_logicalVerticalBarValue.reset();
    verticalScrollBar()->setValue(value);
}

void TimeGraphicsView::horizontalBarAnimateTo(int value) {
    m_hBarAnimation.stop();
    m_hBarAnimation.setStartValue(horizontalBarValue());
    m_hBarAnimation.setEndValue(value);
    m_hBarAnimation.start();
}

void TimeGraphicsView::verticalBarAnimateTo(int value) {
    m_vBarAnimation.stop();
    m_vBarAnimation.setStartValue(verticalBarValue());
    m_vBarAnimation.setEndValue(value);
    m_vBarAnimation.start();
}

void TimeGraphicsView::horizontalBarVBarAnimateTo(int hValue, int vValue) {
    m_hBarAnimation.stop();
    m_vBarAnimation.stop();

    m_hBarAnimation.setStartValue(horizontalBarValue());
    m_hBarAnimation.setEndValue(hValue);
    m_vBarAnimation.setStartValue(verticalBarValue());
    m_vBarAnimation.setEndValue(vValue);

    m_hBarAnimation.start();
    m_vBarAnimation.start();
}

QRectF TimeGraphicsView::visibleRect() const {
    auto viewportRect = viewport()->rect();
    auto leftTop = mapToScene(viewportRect.left(), viewportRect.top());
    auto rightBottom = mapToScene(viewportRect.width(), viewportRect.height());
    auto rect = QRectF(leftTop, rightBottom);
    return rect;
}

QRectF TimeGraphicsView::logicalVisibleRect() const {
    const auto rect = visibleRect();
    const auto wheelHorizontal = m_wheelInput.logicalScrollValue(Qt::Horizontal);
    const auto wheelVertical = m_wheelInput.logicalScrollValue(Qt::Vertical);
    if ((!m_logicalHorizontalBarValue.has_value() && !m_logicalVerticalBarValue.has_value() &&
         !wheelHorizontal.has_value() && !wheelVertical.has_value()) ||
        scaleX() <= 0 || scaleY() <= 0) {
        return rect;
    }

    const auto horizontalValue =
        m_logicalHorizontalBarValue.value_or(wheelHorizontal.value_or(horizontalBarValue()));
    const auto verticalValue =
        m_logicalVerticalBarValue.value_or(wheelVertical.value_or(verticalBarValue()));
    return rect.translated((horizontalValue - horizontalBarValue()) / scaleX(),
                           (verticalValue - verticalBarValue()) / scaleY());
}

void TimeGraphicsView::ensureSceneRectVisible(const QRectF &rect, const int xmargin,
                                              const int ymargin, const bool animated) {
    stopViewportAnimations();
    if (!animated || (m_hBarAnimation.duration() <= 0 && m_vBarAnimation.duration() <= 0)) {
        QGraphicsView::ensureVisible(rect, xmargin, ymargin);
        return;
    }

    const auto currentHorizontalValue = horizontalBarValue();
    const auto currentVerticalValue = verticalBarValue();
    QGraphicsView::ensureVisible(rect, xmargin, ymargin);
    const auto targetHorizontalValue = horizontalBarValue();
    const auto targetVerticalValue = verticalBarValue();
    setHorizontalBarValue(currentHorizontalValue);
    setVerticalBarValue(currentVerticalValue);

    m_logicalHorizontalBarValue = targetHorizontalValue;
    m_logicalVerticalBarValue = targetVerticalValue;
    if (targetHorizontalValue != currentHorizontalValue)
        horizontalBarAnimateTo(targetHorizontalValue);
    if (targetVerticalValue != currentVerticalValue)
        verticalBarAnimateTo(targetVerticalValue);
}

void TimeGraphicsView::setEnsureSceneFillViewX(bool on) {
    m_ensureSceneFillViewX = on;
}

void TimeGraphicsView::setEnsureSceneFillViewY(bool on) {
    m_ensureSceneFillViewY = on;
}

TimeGraphicsView::DragBehavior TimeGraphicsView::dragBehavior() const {
    return m_dragBehavior;
}

void TimeGraphicsView::setDragBehavior(DragBehavior dragBehaviour) {
    m_dragBehavior = dragBehaviour;
    // TODO: 实现手型拖动视图
}

void TimeGraphicsView::setScrollBarVisibility(Qt::Orientation orientation, bool visibility) {
    auto *bar = orientation == Qt::Horizontal ? m_hScrollBar : m_vScrollBar;
    if (bar)
        bar->setRangeVisible(visibility);
}

void TimeGraphicsView::notifyVisibleRectChanged() {
    viewport()->update();
    emit visibleRectChanged(visibleRect());
}

void TimeGraphicsView::onWheelHorScale(QWheelEvent *event) {
    stopProgrammaticViewportAnimations();
    m_wheelInput.handleWheel(event, WheelInputController::Action::HorizontalZoom, Qt::Vertical);
}

void TimeGraphicsView::onWheelVerScale(QWheelEvent *event) {
    stopProgrammaticViewportAnimations();
    m_wheelInput.handleWheel(event, WheelInputController::Action::VerticalZoom, Qt::Vertical);
}

void TimeGraphicsView::onWheelHorScroll(QWheelEvent *event) {
    stopProgrammaticViewportAnimations();
    const auto sourceAxis = event->modifiers() == Qt::ShiftModifier ? Qt::Vertical : Qt::Horizontal;
    m_wheelInput.handleWheel(event, WheelInputController::Action::HorizontalScroll, sourceAxis);
}

void TimeGraphicsView::onWheelVerScroll(QWheelEvent *event) {
    stopProgrammaticViewportAnimations();
    m_wheelInput.handleWheel(event, WheelInputController::Action::VerticalScroll, Qt::Vertical);
}

void TimeGraphicsView::adjustScaleXToFillView() {
    if (sceneRect().width() < viewport()->width()) {
        auto targetSceneWidth = viewport()->width();
        auto targetScaleX = targetSceneWidth / (sceneRect().width() / scaleX());
        setScaleX(targetScaleX);
    }
}

void TimeGraphicsView::adjustScaleYToFillView() {
    if (sceneRect().height() < viewport()->height()) {
        auto targetSceneHeight = viewport()->height();
        auto targetScaleY = targetSceneHeight / (sceneRect().height() / scaleY());
        setScaleY(targetScaleY);
    }
}

void TimeGraphicsView::dragEnterEvent(QDragEnterEvent *event) {
    QGraphicsView::dragEnterEvent(event);
    event->ignore();
}

void TimeGraphicsView::dragMoveEvent(QDragMoveEvent *event) {
    QGraphicsView::dragMoveEvent(event);
    event->ignore();
}

void TimeGraphicsView::dragLeaveEvent(QDragLeaveEvent *event) {
    QGraphicsView::dragLeaveEvent(event);
    event->ignore();
}

bool TimeGraphicsView::event(QEvent *event) {
    if (event->type() == QEvent::NativeGesture) {
        const auto *gestureEvent = static_cast<QNativeGestureEvent *>(event);
        if (gestureEvent->gestureType() == Qt::ZoomNativeGesture) {
            stopProgrammaticViewportAnimations();
            const auto factor = gestureEvent->value() + 1.0;
            if (factor > 0.0) {
                const auto anchor = mapFromGlobal(gestureEvent->globalPosition().toPoint()).x();
                m_wheelInput.zoomByFactor(Qt::Horizontal, factor, anchor);
            }
            return true;
        }
    }

    return QGraphicsView::event(event);
}

void TimeGraphicsView::wheelEvent(QWheelEvent *event) {
    stopProgrammaticViewportAnimations();
    if (!m_wheelInput.handleWheel(event))
        event->ignore();
}

void TimeGraphicsView::drawForeground(QPainter *painter, const QRectF &rect) {
    QGraphicsView::drawForeground(painter, rect);

    QPen pen(m_playPosIndicatorColor);
    pen.setWidthF(1.0);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen);
    const auto x = tickToSceneX(m_playbackPosition - m_offset);
    painter->drawLine(QLineF(x, rect.top(), x, rect.bottom()));
    painter->restore();
}

void TimeGraphicsView::resizeEvent(QResizeEvent *event) {
    if (scene()) {
        if (m_ensureSceneFillViewX) {
            adjustScaleXToFillView();
        } else if (m_ensureSceneFillViewY) {
            adjustScaleYToFillView();
        }
    }

    QGraphicsView::resizeEvent(event);
    emit sizeChanged(viewport()->size());
    notifyVisibleRectChanged();
}

void TimeGraphicsView::mousePressEvent(QMouseEvent *event) {
    stopViewportAnimations();

    const auto isSelect = m_dragBehavior == DragBehavior::RectSelect ||
                          m_dragBehavior == DragBehavior::IntervalSelect;
    if (isSelect && event->button() == Qt::LeftButton) {
        if (scene()) {
            m_isDraggingContent = true;
            if (m_dragBehavior == DragBehavior::RectSelect) {
                m_rubberBand.setSelectMode(RubberBandView::SelectMode::RectSelect);
                armEdgeAutoScroll(Qt::Horizontal | Qt::Vertical, event->pos());
            } else {
                m_rubberBand.setSelectMode(RubberBandView::SelectMode::BeamSelect);
                armEdgeAutoScroll(Qt::Horizontal, event->pos());
            }
            m_rubberBand.mouseDown(mapToScene(event->pos()));
            m_rubberBandAdded = false;
        }
    }
    QGraphicsView::mousePressEvent(event);
    event->ignore();
}

void TimeGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDraggingContent) {
        updateRubberBandSelection(mapToScene(event->pos()));
    }
    if (m_edgeAutoScroller.isDragArmed())
        updateEdgeAutoScrollState(event->pos());
    QGraphicsView::mouseMoveEvent(event);
}

void TimeGraphicsView::updateRubberBandSelection(const QPointF &scenePos) {
    if (!m_rubberBandAdded) {
        scene()->addCommonItem(&m_rubberBand);
        m_rubberBandAdded = true;
    }
    m_rubberBand.mouseMove(scenePos);
    QPainterPath path;
    path.addRect(QRectF(m_rubberBand.pos(), m_rubberBand.boundingRect().size()));
    scene()->setSelectionArea(path);
}

void TimeGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    if (m_isDraggingContent) {
        if (m_rubberBandAdded)
            scene()->removeCommonItem(&m_rubberBand);
        m_isDraggingContent = false;
    }
    disarmEdgeAutoScroll();
    QGraphicsView::mouseReleaseEvent(event);
}

void TimeGraphicsView::showEvent(QShowEvent *event) {
    QGraphicsView::showEvent(event);
    updateAutoPageTurnAvailability();
}

void TimeGraphicsView::hideEvent(QHideEvent *event) {
    QGraphicsView::hideEvent(event);
    updateAutoPageTurnAvailability();
}

void TimeGraphicsView::changeEvent(QEvent *event) {
    QGraphicsView::changeEvent(event);
    if (event->type() == QEvent::EnabledChange)
        updateAutoPageTurnAvailability();
}

void TimeGraphicsView::stopProgrammaticViewportAnimations() {
    m_hBarAnimation.stop();
    m_vBarAnimation.stop();
    m_logicalHorizontalBarValue.reset();
    m_logicalVerticalBarValue.reset();
}

double TimeGraphicsView::boundedScale(const Qt::Orientation orientation,
                                      const double requested) const {
    if (!std::isfinite(requested) || requested <= 0.0)
        return orientation == Qt::Horizontal ? scaleX() : scaleY();

    if (orientation == Qt::Horizontal) {
        auto minimum = 0.0001;
        if (m_ensureSceneFillViewX && scaleX() > 0.0 && sceneRect().width() > 0.0) {
            const auto unscaledWidth = sceneRect().width() / scaleX();
            if (unscaledWidth > 0.0)
                minimum = viewport()->width() / unscaledWidth;
        }
        return std::clamp(requested, std::min(minimum, m_scaleXMax), m_scaleXMax);
    }

    auto minimum = m_scaleYMin;
    if (m_ensureSceneFillViewY && scaleY() > 0.0 && sceneRect().height() > 0.0) {
        const auto unscaledHeight = sceneRect().height() / scaleY();
        if (unscaledHeight > 0.0)
            minimum = std::max(minimum, viewport()->height() / unscaledHeight);
    }
    return std::clamp(requested, std::min(minimum, m_scaleYMax), m_scaleYMax);
}

void TimeGraphicsView::setScaleAt(const Qt::Orientation orientation, const double value,
                                  const double anchor) {
    if (orientation == Qt::Horizontal) {
        const QPoint cursor(qRound(anchor), viewport()->height() / 2);
        const auto scenePosition = mapToScene(cursor);
        const auto ratio = value / scaleX();
        setScaleX(value);
        setHorizontalBarValue(qRound(scenePosition.x() * ratio - cursor.x()));
        return;
    }

    const QPoint cursor(viewport()->width() / 2, qRound(anchor));
    const auto scenePosition = mapToScene(cursor);
    const auto ratio = value / scaleY();
    setScaleY(value);
    setVerticalBarValue(qRound(scenePosition.y() * ratio - cursor.y()));
}

void TimeGraphicsView::updateAnimationDuration() {
    constexpr int animationDurationBase = 250;
    const auto duration = getEffectiveAnimationTime(animationDurationBase);

    auto updateAnimation = [duration](QPropertyAnimation &animation, const QVariant &currentValue,
                                      const auto &applyValue) {
        const auto running = animation.state() == QAbstractAnimation::Running;
        const auto endValue = animation.endValue();
        animation.stop();
        animation.setDuration(duration);
        if (!running)
            return;
        if (duration == 0) {
            applyValue(endValue);
            return;
        }
        animation.setStartValue(currentValue);
        animation.setEndValue(endValue);
        animation.start();
    };

    updateAnimation(m_hBarAnimation, horizontalBarValue(),
                    [this](const QVariant &value) { setHorizontalBarValue(value.toInt()); });
    updateAnimation(m_vBarAnimation, verticalBarValue(),
                    [this](const QVariant &value) { setVerticalBarValue(value.toInt()); });
}

void TimeGraphicsView::afterSetScale() {
    viewport()->update();
    emit scaleChanged(scaleX(), scaleY());
}

void TimeGraphicsView::afterSetAnimationEnabled(bool enabled) {
    updateAnimationDuration();
}

void TimeGraphicsView::afterSetTimeScale(double scale) {
    updateAnimationDuration();
}

double TimeGraphicsView::startTick() const {
    return sceneXToTick(visibleRect().left()) + m_offset;
}

double TimeGraphicsView::endTick() const {
    return sceneXToTick(visibleRect().right()) + m_offset;
}

void TimeGraphicsView::setOffset(int tick) {
    m_offset = tick;
    if (m_gridItem)
        m_gridItem->setOffset(tick);
    m_sceneLastPlayPosIndicator->setOffset(tick);
    viewport()->update();
    emit timeRangeChanged(startTick(), endTick());
}

void TimeGraphicsView::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
    m_sceneLastPlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    viewport()->update();
}

void TimeGraphicsView::setLeftMarginPx(int px) {
    m_scene->setLeftMarginPx(px);
    m_sceneLastPlayPosIndicator->setPosition(m_lastPlaybackPosition);
    if (m_gridItem)
        m_gridItem->update();
    notifyVisibleRectChanged();
}

void TimeGraphicsView::setAutoTurnPage(bool on) {
    m_autoTurnPage = on;
    if (m_autoTurnPage && m_autoPageTurnAvailable && m_playbackPosition > endTick())
        pageAdd();
}

void TimeGraphicsView::setSceneLength(int tick) {
    m_baseSceneLength = tick;
    m_scene->setSceneLength(m_baseSceneLength + m_sceneLengthExtension);
    updateAutoPageTurnAvailability();
}

void TimeGraphicsView::setSceneLengthExtension(int ticks) {
    if (m_sceneLengthExtension == ticks)
        return;
    m_sceneLengthExtension = ticks;
    m_scene->setSceneLength(m_baseSceneLength + m_sceneLengthExtension);
}

int TimeGraphicsView::sceneLengthExtension() const {
    return m_sceneLengthExtension;
}

void TimeGraphicsView::updateAutoPageTurnAvailability() {
    const bool available =
        AutoPageTurnUtils::isPageDurationAvailable(appModel->timeline(), startTick(), endTick());
    if (m_autoPageTurnAvailable != available) {
        m_autoPageTurnAvailable = available;
        emit autoPageTurnAvailabilityChanged(available);
    }
}

void TimeGraphicsView::armEdgeAutoScroll(Qt::Orientations axes) {
    const auto pointerPos = viewport()->mapFromGlobal(QCursor::pos());
    if (!m_edgeAutoScroller.isDragArmed()) {
        armEdgeAutoScroll(axes, pointerPos);
        return;
    }
    // Already armed (callers may re-arm on every move event): refresh the axes
    // and re-evaluate the hot zone, keeping the press position intact. This
    // also covers subclasses whose mouseMoveEvent returns before reaching the
    // base class implementation.
    m_edgeAutoScroller.setDragAxes(axes);
    updateEdgeAutoScrollState(pointerPos);
}

void TimeGraphicsView::armEdgeAutoScroll(const Qt::Orientations axes, const QPoint &pressPos) {
    m_edgeAutoScroller.prepareDrag(pressPos, axes);
}

void TimeGraphicsView::disarmEdgeAutoScroll() {
    m_edgeAutoScroller.stopDrag();
}

bool TimeGraphicsView::isEdgeAutoScrollActive() const {
    return m_edgeAutoScroller.isRunning();
}

void TimeGraphicsView::updateEdgeAutoScrollState(const QPoint &viewportPos) {
    const QRectF vpRect(QPointF(0, 0), viewport()->size());
    const auto wasRunning = m_edgeAutoScroller.isRunning();
    m_edgeAutoScroller.updateDragState(viewportPos, vpRect, QApplication::startDragDistance());
    if (!wasRunning && m_edgeAutoScroller.isRunning()) {
        // Direct scroll bar writes must not fight the bar animations
        m_hBarAnimation.stop();
        m_vBarAnimation.stop();
    }
}

void TimeGraphicsView::onEdgeAutoScrollTimerFrame(double dtMs) {
    // Safety net: stop if the mouse button was released without us seeing the
    // event (e.g. release outside the window swallowed by a popup).
    if (!m_edgeAutoScroller.isDragArmed() || QGuiApplication::mouseButtons() == Qt::NoButton ||
        !isVisible()) {
        disarmEdgeAutoScroll();
        return;
    }

    const auto pointerPos = QPointF(viewport()->mapFromGlobal(QCursor::pos()));
    const QRectF vpRect(QPointF(0, 0), viewport()->size());

    const auto step = m_edgeAutoScroller.computeDragStep(pointerPos, vpRect, dtMs);
    if (step.x() != 0)
        setHorizontalBarValue(horizontalBarValue() + step.x());
    if (step.y() != 0)
        setVerticalBarValue(verticalBarValue() + step.y());

    const auto clamped = EdgeAutoScroller::clampToRect(pointerPos, vpRect).toPoint();
    onEdgeAutoScrollFrame(clamped, QGuiApplication::queryKeyboardModifiers());

    // Stop the timer once the pointer left the hot zone (it may re-enter later)
    updateEdgeAutoScrollState(pointerPos.toPoint());
}

void TimeGraphicsView::onEdgeAutoScrollFrame(const QPoint &clampedViewportPos,
                                             Qt::KeyboardModifiers modifiers) {
    Q_UNUSED(modifiers);
    if (m_isDraggingContent && scene())
        updateRubberBandSelection(mapToScene(clampedViewportPos));
}

void TimeGraphicsView::setPlaybackPosition(double tick) {
    const auto oldTick = m_playbackPosition;
    m_playbackPosition = tick;

    const bool canTurnPage = m_autoTurnPage && m_autoPageTurnAvailable &&
                             appStatus->currentEditObject == AppStatus::EditObjectType::None &&
                             !isEdgeAutoScrollActive();
    if (canTurnPage) {
        const auto viewWidth = viewport()->width();
        const auto hBarValue = horizontalBarValue();
        const auto targetEndTick = sceneXToTick(hBarValue + viewWidth) + m_offset;
        const auto tickRange = targetEndTick - sceneXToTick(hBarValue) - m_offset;

        if (m_playbackPosition > targetEndTick) {
            if (m_playbackPosition > targetEndTick + tickRange)
                setViewportStartTick(m_playbackPosition);
            else
                pageAdd();
        } else if (m_playbackPosition < startTick()) {
            setViewportStartTick(m_playbackPosition);
        }
    }

    updatePlaybackIndicator(oldTick);
}

QRect TimeGraphicsView::playbackIndicatorViewportRect(double tick) const {
    if (!viewport())
        return {};

    const auto sceneX = tickToSceneX(tick - m_offset);
    const auto viewportX = viewportTransform().map(QPointF(sceneX, 0)).x();
    if (!std::isfinite(viewportX))
        return {};

    constexpr qreal updateMargin = 2.0;
    return QRectF(viewportX - updateMargin, 0, updateMargin * 2, viewport()->height())
        .toAlignedRect()
        .intersected(viewport()->rect());
}

void TimeGraphicsView::updatePlaybackIndicator(double oldTick) {
    const auto oldRect = playbackIndicatorViewportRect(oldTick);
    const auto newRect = playbackIndicatorViewportRect(m_playbackPosition);
    if (!oldRect.isEmpty())
        viewport()->update(oldRect);
    if (!newRect.isEmpty() && newRect != oldRect)
        viewport()->update(newRect);
}

void TimeGraphicsView::setLastPlaybackPosition(double tick) {
    m_lastPlaybackPosition = tick;
    if (m_sceneLastPlayPosIndicator != nullptr)
        m_sceneLastPlayPosIndicator->setPosition(tick);
}

void TimeGraphicsView::setViewportStartTick(double tick) {
    auto sceneX = qRound(tickToSceneX(tick - m_offset));
    setHorizontalBarValue(sceneX);
}

void TimeGraphicsView::setViewportCenterAtTick(double tick) {
    auto tickRange = endTick() - startTick();
    auto targetStart = tick - tickRange / 2;
    setViewportStartTick(targetStart);
}

bool TimeGraphicsView::setViewportScale(double horizontalScale, double verticalScale) {
    if (!std::isfinite(horizontalScale) || !std::isfinite(verticalScale) || horizontalScale <= 0 ||
        verticalScale <= 0) {
        return false;
    }

    stopViewportAnimations();

    double minimumHorizontalScale = 0.0001;
    if (m_ensureSceneFillViewX && scaleX() > 0 && sceneRect().width() > 0) {
        const auto unscaledSceneWidth = sceneRect().width() / scaleX();
        if (unscaledSceneWidth > 0)
            minimumHorizontalScale = viewport()->width() / unscaledSceneWidth;
    }

    auto minimumVerticalScale = m_scaleYMin;
    if (m_ensureSceneFillViewY && scaleY() > 0 && sceneRect().height() > 0) {
        const auto unscaledSceneHeight = sceneRect().height() / scaleY();
        if (unscaledSceneHeight > 0)
            minimumVerticalScale =
                qMax(minimumVerticalScale, viewport()->height() / unscaledSceneHeight);
    }

    const auto targetHorizontalScale =
        qMin(m_scaleXMax, qMax(minimumHorizontalScale, horizontalScale));
    const auto targetVerticalScale = qMin(m_scaleYMax, qMax(minimumVerticalScale, verticalScale));
    setScaleXY(targetHorizontalScale, targetVerticalScale);
    return true;
}

void TimeGraphicsView::stopViewportAnimations() {
    stopProgrammaticViewportAnimations();
    m_wheelInput.stop();
}

void TimeGraphicsView::pageAdd() {
    auto start = horizontalScrollBar()->value();
    auto end = start + horizontalScrollBar()->pageStep();
    setHorizontalBarValue(end);
}

double TimeGraphicsView::sceneXToTick(double pos) const {
    return AppGlobal::ticksPerQuarterNote * (pos - m_scene->leftMarginPx()) / scaleX() /
           m_pixelsPerQuarterNote;
}

double TimeGraphicsView::tickToSceneX(double tick) const {
    return tick * scaleX() * m_pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote +
           m_scene->leftMarginPx();
}

QColor TimeGraphicsView::barLineColor() const {
    return m_barLineColor;
}

void TimeGraphicsView::setBarLineColor(const QColor &color) {
    m_barLineColor = color;
    if (m_gridItem)
        m_gridItem->setBarLineColor(m_barLineColor);
}

QColor TimeGraphicsView::beatLineColor() const {
    return m_beatLineColor;
}

void TimeGraphicsView::setBeatLineColor(const QColor &color) {
    m_beatLineColor = color;
    if (m_gridItem)
        m_gridItem->setBeatLineColor(m_beatLineColor);
}

QColor TimeGraphicsView::commonLineColor() const {
    return m_commonLineColor;
}

void TimeGraphicsView::setCommonLineColor(const QColor &color) {
    m_commonLineColor = color;
    if (m_gridItem)
        m_gridItem->setCommonLineColor(m_commonLineColor);
}

QColor TimeGraphicsView::playPosIndicatorColor() const {
    return m_playPosIndicatorColor;
}

void TimeGraphicsView::setPlayPosIndicatorColor(const QColor &color) {
    if (m_playPosIndicatorColor == color)
        return;
    m_playPosIndicatorColor = color;
    updatePlaybackIndicator(m_playbackPosition);
}

QColor TimeGraphicsView::lastPlayPosIndicatorColor() const {
    return m_lastPlayPosIndicatorColor;
}

void TimeGraphicsView::setLastPlayPosIndicatorColor(const QColor &color) {
    if (m_lastPlayPosIndicatorColor == color)
        return;
    m_lastPlayPosIndicatorColor = color;
    if (m_sceneLastPlayPosIndicator) {
        auto pen = m_sceneLastPlayPosIndicator->pen();
        pen.setColor(color);
        m_sceneLastPlayPosIndicator->setPen(pen);
        m_sceneLastPlayPosIndicator->update();
    }
}

QColor TimeGraphicsView::rubberBandBorderColor() const {
    return m_rubberBandBorderColor;
}

void TimeGraphicsView::setRubberBandBorderColor(const QColor &color) {
    if (m_rubberBandBorderColor == color)
        return;
    m_rubberBandBorderColor = color;
    m_rubberBand.setBorderColor(color);
}

QColor TimeGraphicsView::rubberBandFillColor() const {
    return m_rubberBandFillColor;
}

void TimeGraphicsView::setRubberBandFillColor(const QColor &color) {
    if (m_rubberBandFillColor == color)
        return;
    m_rubberBandFillColor = color;
    m_rubberBand.setFillColor(color);
}
