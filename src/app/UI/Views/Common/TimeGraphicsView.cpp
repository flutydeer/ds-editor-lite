#include "TimeGraphicsView.h"

#include <QApplication>
#include <QCursor>
#include <QHideEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QWheelEvent>

#include <cmath>

#include "TimeGraphicsScene.h"
#include "TimeGridView.h"
#include "TimeIndicatorView.h"
#include "EditorWheelUtils.h"
#include "Controller/PlaybackController.h"
#include "UI/Views/Common/AutoPageTurnUtils.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/AppOptions/AppOptions.h"
#include "Global/AppGlobal.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/GUI/Controls/OverlayScrollBar.h>

static inline bool isDirectManipulationEnabled() {
#if defined(WITH_DIRECT_MANIPULATION)
    return appOptions->appearance()->enableDirectManipulation;
#else
    return false;
#endif
}

TimeGraphicsView::TimeGraphicsView(TimeGraphicsScene *scene, bool showLastPlaybackPosition,
                                   QWidget *parent)
    : QGraphicsView(parent), m_scene(scene) {
    setRenderHint(QPainter::Antialiasing);
    // Full viewport repaint: with partial updates the fast-scrubbing playhead
    // indicator leaves ghost lines behind (the antialiased 1px ink extends
    // past Qt's computed damage rect, so old positions never get erased).
    // TODO: rework to repaint only the indicator's swept span (a route via
    // scene->invalidate() was tried and did not clear the ghosts) or use
    // SmartViewportUpdate with a custom damage tracking, to drop CPU cost.
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
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

    m_scaleXAnimation.setTargetObject(this);
    m_scaleXAnimation.setPropertyName("scaleX");
    m_scaleXAnimation.setEasingCurve(QEasingCurve::OutCubic);

    m_scaleYAnimation.setTargetObject(this);
    m_scaleYAnimation.setPropertyName("scaleY");
    m_scaleYAnimation.setEasingCurve(QEasingCurve::OutCubic);

    m_hBarAnimation.setTargetObject(this);
    m_hBarAnimation.setPropertyName("horizontalScrollBarValue");
    m_hBarAnimation.setEasingCurve(QEasingCurve::OutCubic);

    m_vBarAnimation.setTargetObject(this);
    m_vBarAnimation.setPropertyName("verticalScrollBarValue");
    m_vBarAnimation.setEasingCurve(QEasingCurve::OutCubic);

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

    m_scenePlayPosIndicator = new TimeIndicatorView;
    m_scenePlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    QPen curPlayPosPen;
    curPlayPosPen.setWidth(1);
    curPlayPosPen.setColor(m_playPosIndicatorColor);
    m_scenePlayPosIndicator->setPen(curPlayPosPen);
    m_scene->addTimeIndicator(m_scenePlayPosIndicator);

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

    m_positionThrottle.setSingleShot(true);
    m_positionThrottle.setInterval(33);
    connect(&m_positionThrottle, &QTimer::timeout, this, [this] {
        double tick = m_pendingPosition;
        m_playbackPosition = tick;
        if (m_scenePlayPosIndicator != nullptr)
            m_scenePlayPosIndicator->setPosition(tick);

        if (!m_autoTurnPage || !m_autoPageTurnAvailable ||
            appStatus->currentEditObject != AppStatus::EditObjectType::None)
            return;
        // Do not auto turn pages while edge auto scroll drives the viewport
        if (isEdgeAutoScrollActive())
            return;

        auto viewWidth = viewport()->width();
        auto hBarValue = horizontalBarValue();
        auto targetEndTick = sceneXToTick(hBarValue + viewWidth) + m_offset;
        auto tickRange = targetEndTick - sceneXToTick(hBarValue) - m_offset;

        if (m_playbackPosition > targetEndTick) {
            if (m_playbackPosition > targetEndTick + tickRange)
                setViewportStartTick(m_playbackPosition);
            else
                pageAdd();
        } else if (m_playbackPosition < startTick())
            setViewportStartTick(m_playbackPosition);
    });

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
    if ((!m_logicalHorizontalBarValue.has_value() && !m_logicalVerticalBarValue.has_value()) ||
        scaleX() <= 0 || scaleY() <= 0)
        return rect;

    const auto horizontalValue = m_logicalHorizontalBarValue.value_or(horizontalBarValue());
    const auto verticalValue = m_logicalVerticalBarValue.value_or(verticalBarValue());
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
    emit visibleRectChanged(visibleRect());
}

void TimeGraphicsView::onWheelHorScale(QWheelEvent *event) {
    auto cursorPos = event->position().toPoint();
    auto scenePos = mapToScene(cursorPos);

    const auto deltaY = EditorWheelUtils::wheelDelta(event, Qt::Vertical);

    auto targetScaleX = scaleX();
    if (deltaY > 0)
        targetScaleX = scaleX() * (1 + m_hZoomingStep * deltaY / 120);
    else if (deltaY < 0)
        targetScaleX = scaleX() / (1 + m_hZoomingStep * -deltaY / 120);

    if (targetScaleX > m_scaleXMax)
        targetScaleX = m_scaleXMax;

    auto scaledSceneWidth = sceneRect().width() * (targetScaleX / scaleX());
    if (scaledSceneWidth < viewport()->width()) {
        auto targetSceneWidth = viewport()->width();
        targetScaleX = targetSceneWidth / (sceneRect().width() / scaleX());
    }

    auto ratio = targetScaleX / scaleX();
    auto targetSceneX = scenePos.x() * ratio;
    auto targetValue = qRound(targetSceneX - cursorPos.x());
    if (isDirectManipulationEnabled() || !isMouseEventFromWheel(event)) {
        setScaleX(targetScaleX);
        setHorizontalBarValue(targetValue);
    } else {
        m_scaleXAnimation.stop();
        m_scaleXAnimation.setStartValue(scaleX());
        m_scaleXAnimation.setEndValue(targetScaleX);
        m_scaleXAnimation.start();

        horizontalBarAnimateTo(targetValue);
    }
}

void TimeGraphicsView::onWheelVerScale(QWheelEvent *event) {
    auto cursorPos = event->position().toPoint();
    auto scenePos = mapToScene(cursorPos);

    const auto deltaX = EditorWheelUtils::wheelDelta(event, Qt::Horizontal);
    auto targetScaleY = scaleY();
    if (deltaX > 0)
        targetScaleY = scaleY() * (1 + m_vZoomingStep * deltaX / 120);
    else if (deltaX < 0)
        targetScaleY = scaleY() / (1 + m_vZoomingStep * -deltaX / 120);

    if (targetScaleY < m_scaleYMin)
        targetScaleY = m_scaleYMin;
    else if (targetScaleY > m_scaleYMax)
        targetScaleY = m_scaleYMax;

    auto scaledSceneHeight = sceneRect().height() * (targetScaleY / scaleY());
    if (m_ensureSceneFillViewY && scaledSceneHeight < viewport()->height()) {
        auto targetSceneHeight = viewport()->height();
        targetScaleY = targetSceneHeight / (sceneRect().height() / scaleY());
    }

    auto ratio = targetScaleY / scaleY();
    auto targetSceneY = scenePos.y() * ratio;
    auto targetValue = qRound(targetSceneY - cursorPos.y());
    if (isDirectManipulationEnabled() || !isMouseEventFromWheel(event)) {
        setScaleY(targetScaleY);
        setVerticalBarValue(targetValue);
    } else {
        m_scaleYAnimation.stop();
        m_scaleYAnimation.setStartValue(scaleY());
        m_scaleYAnimation.setEndValue(targetScaleY);
        m_scaleYAnimation.start();

        verticalBarAnimateTo(targetValue);
    }
}

void TimeGraphicsView::onWheelHorScroll(QWheelEvent *event) {
    const auto fromWheel = isMouseEventFromWheel(event);
    auto startValue = horizontalBarValue();
    auto endValue = EditorWheelUtils::scrollTarget(startValue, viewport()->width(), 0.2, event,
                                                   EditorWheelUtils::horizontalScrollAxis(event));
    if (isDirectManipulationEnabled() || !fromWheel)
        setHorizontalBarValue(endValue);
    else {
        horizontalBarAnimateTo(endValue);
    }
}

void TimeGraphicsView::onWheelVerScroll(QWheelEvent *event) {
    const auto fromWheel = isMouseEventFromWheel(event);
    const auto startValue = verticalBarValue();
    const auto endValue = EditorWheelUtils::scrollTarget(startValue, viewport()->height(), 0.15,
                                                         event, Qt::Vertical);
    if (isDirectManipulationEnabled() || !fromWheel) {
        setVerticalBarValue(endValue);
    } else {
        verticalBarAnimateTo(endValue);
    }
}

void TimeGraphicsView::adjustScaleXToFillView() {
    if (sceneRect().width() < viewport()->width()) {
        auto targetSceneWidth = viewport()->width();
        auto targetScaleX = targetSceneWidth / (sceneRect().width() / scaleX());
        setScaleX(targetScaleX);
        qDebug() << "Scene width < viewport width, adjust scaleX to" << targetScaleX;
    }
}

void TimeGraphicsView::adjustScaleYToFillView() {
    if (sceneRect().height() < viewport()->height()) {
        auto targetSceneHeight = viewport()->height();
        auto targetScaleY = targetSceneHeight / (sceneRect().height() / scaleY());
        setScaleY(targetScaleY);
        qDebug() << "Scene height < viewport height, adjust scaleY to" << targetScaleY;
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
    // Touchpad smooth zooming
    if (event->type() == QEvent::NativeGesture) {
        auto gestureEvent = static_cast<QNativeGestureEvent *>(event);

        if (gestureEvent->gestureType() == Qt::ZoomNativeGesture) {
            stopViewportAnimations();
            auto cursorGlobalPos = gestureEvent->globalPosition().toPoint();
            auto cursorPos = mapFromGlobal(cursorGlobalPos);
            auto scenePos = mapToScene(cursorPos);

            auto multiplier = gestureEvent->value() + 1;

            // Prevent negative zoom factors
            if (multiplier <= 0) {
                return true;
            }

            auto targetScaleX = scaleX() * multiplier;

            targetScaleX = qMin(targetScaleX, scaleXMax());

            auto scaledSceneWidth = sceneRect().width() * (targetScaleX / scaleX());
            if (scaledSceneWidth < viewport()->width()) {
                auto targetSceneWidth = viewport()->width();
                targetScaleX = targetSceneWidth / (sceneRect().width() / scaleX());
            }

            auto ratio = targetScaleX / scaleX();
            auto targetSceneX = scenePos.x() * ratio;
            auto targetValue = qRound(targetSceneX - cursorPos.x());

            setScaleX(targetScaleX);
            setHorizontalBarValue(targetValue);

            return true;
        }
    }

    return QGraphicsView::event(event);
}

void TimeGraphicsView::wheelEvent(QWheelEvent *event) {
    stopViewportAnimations();
    if (event->modifiers() == Qt::ControlModifier) {
        onWheelHorScale(event);
    } else if (event->modifiers() == Qt::AltModifier) {
        onWheelVerScale(event);
    } else if (event->modifiers() == Qt::ShiftModifier) {
        onWheelHorScroll(event);
    } else if (event->modifiers() == Qt::NoModifier) {
        if (EditorWheelUtils::dominantAxis(event) == Qt::Horizontal)
            onWheelHorScroll(event);
        else
            onWheelVerScroll(event);
    }
    notifyVisibleRectChanged();
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
    if (m_edgeAutoScrollArmed)
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

bool TimeGraphicsView::isMouseEventFromWheel(QWheelEvent *event) {
    return m_wheelInputState.isMouseWheel(event);
}

void TimeGraphicsView::updateAnimationDuration() {
    constexpr int animationDurationBase = 250;
    const auto duration = getEffectiveAnimationTime(animationDurationBase, AnimationGlobal::Full);

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

    updateAnimation(m_scaleXAnimation, scaleX(),
                    [this](const QVariant &value) { setScaleX(value.toDouble()); });
    updateAnimation(m_scaleYAnimation, scaleY(),
                    [this](const QVariant &value) { setScaleY(value.toDouble()); });
    updateAnimation(m_hBarAnimation, horizontalBarValue(),
                    [this](const QVariant &value) { setHorizontalBarValue(value.toInt()); });
    updateAnimation(m_vBarAnimation, verticalBarValue(),
                    [this](const QVariant &value) { setVerticalBarValue(value.toInt()); });
}

void TimeGraphicsView::afterSetScale() {
    emit scaleChanged(scaleX(), scaleY());
}

void TimeGraphicsView::afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) {
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
    m_scenePlayPosIndicator->setOffset(tick);
    emit timeRangeChanged(startTick(), endTick());
}

void TimeGraphicsView::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
    m_scenePlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    m_sceneLastPlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
}

void TimeGraphicsView::setLeftMarginPx(int px) {
    m_scene->setLeftMarginPx(px);
    // Reposition the indicators and grid against the shifted origin, and let
    // the ruler/lanes resync their time range.
    m_scenePlayPosIndicator->setPosition(m_playbackPosition);
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
    if (!m_edgeAutoScrollArmed) {
        armEdgeAutoScroll(axes, pointerPos);
        return;
    }
    // Already armed (callers may re-arm on every move event): refresh the axes
    // and re-evaluate the hot zone, keeping the press position intact. This
    // also covers subclasses whose mouseMoveEvent returns before reaching the
    // base class implementation.
    m_edgeAutoScrollAxes = axes;
    updateEdgeAutoScrollState(pointerPos);
}

void TimeGraphicsView::armEdgeAutoScroll(const Qt::Orientations axes, const QPoint &pressPos) {
    m_edgeAutoScrollAxes = axes;
    m_edgeAutoScrollArmed = !!axes;
    m_edgeAutoScrollDistanceReached = false;
    m_edgeAutoScrollPressPos = pressPos;
}

void TimeGraphicsView::disarmEdgeAutoScroll() {
    m_edgeAutoScrollArmed = false;
    m_edgeAutoScrollDistanceReached = false;
    m_edgeAutoScroller.stop();
}

bool TimeGraphicsView::isEdgeAutoScrollActive() const {
    return m_edgeAutoScroller.isRunning();
}

void TimeGraphicsView::updateEdgeAutoScrollState(const QPoint &viewportPos) {
    if (!m_edgeAutoScrollArmed)
        return;

    if (!m_edgeAutoScrollDistanceReached) {
        const auto delta = viewportPos - m_edgeAutoScrollPressPos;
        if (delta.manhattanLength() < QApplication::startDragDistance())
            return;
        m_edgeAutoScrollDistanceReached = true;
    }

    const QRectF vpRect(QPointF(0, 0), viewport()->size());
    const auto v = EdgeAutoScroller::velocity(viewportPos, m_edgeAutoScrollPressPos, vpRect,
                                              m_edgeAutoScrollAxes, m_edgeAutoScroller.config());
    const bool inHotZone = !v.isNull();
    if (inHotZone && !m_edgeAutoScroller.isRunning()) {
        // Direct scroll bar writes must not fight the bar animations
        m_hBarAnimation.stop();
        m_vBarAnimation.stop();
        m_edgeAutoScroller.start();
    } else if (!inHotZone && m_edgeAutoScroller.isRunning()) {
        m_edgeAutoScroller.stop();
    }
}

void TimeGraphicsView::onEdgeAutoScrollTimerFrame(double dtMs) {
    // Safety net: stop if the mouse button was released without us seeing the
    // event (e.g. release outside the window swallowed by a popup).
    if (!m_edgeAutoScrollArmed || QGuiApplication::mouseButtons() == Qt::NoButton || !isVisible()) {
        disarmEdgeAutoScroll();
        return;
    }

    const auto pointerPos = QPointF(viewport()->mapFromGlobal(QCursor::pos()));
    const QRectF vpRect(QPointF(0, 0), viewport()->size());

    const auto step = m_edgeAutoScroller.computeStep(pointerPos, QPointF(m_edgeAutoScrollPressPos),
                                                     vpRect, m_edgeAutoScrollAxes, dtMs);
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
    m_pendingPosition = tick;
    if (!m_positionThrottle.isActive())
        m_positionThrottle.start();
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
    m_scaleXAnimation.stop();
    m_scaleYAnimation.stop();
    m_hBarAnimation.stop();
    m_vBarAnimation.stop();
    m_logicalHorizontalBarValue.reset();
    m_logicalVerticalBarValue.reset();
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
    if (m_scenePlayPosIndicator) {
        auto pen = m_scenePlayPosIndicator->pen();
        pen.setColor(color);
        m_scenePlayPosIndicator->setPen(pen);
        m_scenePlayPosIndicator->update();
    }
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
