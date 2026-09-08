#include "EditorTouchController.h"

#include "EditorPointerUtils.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppOptions/Options/AppearanceOption.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QTimer>
#include <QTouchEvent>
#include <QWidget>

#include <cmath>

namespace {
    // Inertia glide after a flick. Exponential decay is frame-rate independent,
    // which matters because the timer interval is only a hint.
    constexpr int inertiaIntervalMs = 16;
    constexpr double inertiaDecayPerSecond = 3.6;
    constexpr double inertiaStopSpeed = 20.0;
    // Weight kept on the previous estimate when tracking a single-finger pan.
    constexpr double panVelocitySmoothing = 0.55;
    // How long after the last touch point a mouse-reason context menu is still
    // assumed to be the platform's press-and-hold emulation. Windows raises it
    // around 500 ms, a little after our own long press has already fired.
    constexpr qint64 foreignContextMenuGraceMs = 1500;
}

EditorTouchController::EditorTouchController(EditorTouchTarget *target, QWidget *widget,
                                             QWidget *eventTarget, QObject *parent)
    : QObject(parent ? parent : widget), m_target(target), m_widget(widget),
      m_eventTarget(eventTarget ? eventTarget : widget),
      m_longPressTimer(new QTimer(this)), m_inertiaTimer(new QTimer(this)) {
    m_clock.start();

    m_longPressTimer->setSingleShot(true);
    connect(m_longPressTimer, &QTimer::timeout, this,
            [this] { dispatch(m_gesture.longPressTimeout(now())); });

    m_inertiaTimer->setInterval(inertiaIntervalMs);
    connect(m_inertiaTimer, &QTimer::timeout, this, &EditorTouchController::onInertiaFrame);
}

EditorTouchController::~EditorTouchController() {
    // Never leave the global touch-stream counter unbalanced.
    if (m_syntheticStreamActive)
        EditorPointer::endTouchStream();
}

bool EditorTouchController::isEnabled() {
    return appOptions->appearance()->enableTouchGestures;
}

bool EditorTouchController::isGestureActive() const {
    return m_gesture.phase() != EditorTouchGesture::Phase::Idle;
}

qint64 EditorTouchController::now() const {
    return m_clock.elapsed();
}

bool EditorTouchController::handleEvent(QEvent *event) {
    switch (event->type()) {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
            return handleTouchEvent(static_cast<QTouchEvent *>(event));
        case QEvent::TouchCancel:
            if (!isGestureActive())
                return false;
            cancel();
            event->accept();
            return true;
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
            return swallowForeignMouseEvent(static_cast<QMouseEvent *>(event));
        case QEvent::ContextMenu:
            return swallowForeignContextMenu(static_cast<QContextMenuEvent *>(event));
        default:
            return false;
    }
}

bool EditorTouchController::swallowForeignMouseEvent(QMouseEvent *event) {
    if (!m_target || !isEnabled())
        return false;
    // Our own synthetic events, a real mouse and a stylus all report
    // Qt::MouseEventNotSynthesized. Anything else was made from a touch we are
    // already handling ourselves, so it is a duplicate.
    if (event->source() == Qt::MouseEventNotSynthesized)
        return false;
    event->accept();
    return true;
}

bool EditorTouchController::swallowForeignContextMenu(QContextMenuEvent *event) {
    if (!m_target || !isEnabled())
        return false;
    // Our own long press posts the menu with the Other reason, and a keyboard
    // menu key must always get through.
    if (event->reason() != QContextMenuEvent::Mouse)
        return false;
    if (!isGestureActive() && now() - m_lastTouchTimestamp > foreignContextMenuGraceMs)
        return false;
    event->accept();
    return true;
}

bool EditorTouchController::handleTouchEvent(QTouchEvent *event) {
    if (!m_target || !m_widget)
        return false;
    // Leaving TouchBegin unaccepted is what makes Qt fall back to its own
    // touch-to-mouse synthesis, which is exactly the behaviour we want when the
    // feature is switched off.
    if (!isEnabled()) {
        if (isGestureActive())
            cancel();
        return false;
    }

    m_device = event->pointingDevice();
    const auto timestamp = now();
    m_lastTouchTimestamp = timestamp;
    for (const auto &point : event->points()) {
        const auto position = point.position();
        switch (point.state()) {
            case QEventPoint::State::Pressed:
                dispatch(m_gesture.pressed(point.id(), position, timestamp));
                break;
            case QEventPoint::State::Updated:
                dispatch(m_gesture.moved(point.id(), position, timestamp));
                break;
            case QEventPoint::State::Released:
                dispatch(m_gesture.released(point.id(), position, timestamp));
                break;
            case QEventPoint::State::Stationary:
            case QEventPoint::State::Unknown:
                break;
        }
    }
    dispatch(m_gesture.flushNavigation(timestamp));

    if (m_gesture.longPressDeadline() != 0)
        armLongPressTimer();
    else
        disarmLongPressTimer();

    event->accept();
    return true;
}

void EditorTouchController::armLongPressTimer() {
    const auto remaining = m_gesture.longPressDeadline() - now();
    if (remaining <= 0) {
        dispatch(m_gesture.longPressTimeout(now()));
        return;
    }
    m_longPressTimer->start(static_cast<int>(remaining));
}

void EditorTouchController::disarmLongPressTimer() {
    m_longPressTimer->stop();
}

void EditorTouchController::dispatch(const EditorTouchGesture::Events &events) {
    for (const auto &event : events) {
        switch (event.type) {
            case EditorTouchGesture::Event::Type::SingleBegin:
                onSingleBegin(event);
                break;
            case EditorTouchGesture::Event::Type::SingleMove:
                onSingleMove(event);
                break;
            case EditorTouchGesture::Event::Type::SingleEnd:
                onSingleEnd(event);
                break;
            case EditorTouchGesture::Event::Type::SingleCancel:
                onSingleCancel();
                break;
            case EditorTouchGesture::Event::Type::LongPress:
                onLongPress(event);
                break;
            case EditorTouchGesture::Event::Type::NavigationBegin:
                stopInertia();
                m_target->stopTouchViewportAnimation();
                break;
            case EditorTouchGesture::Event::Type::NavigationUpdate:
                if (!event.panDelta.isNull())
                    m_target->panTouchViewportBy(event.panDelta);
                if (event.horizontalFactor != 1.0 || event.verticalFactor != 1.0) {
                    m_target->zoomTouchViewportBy(event.horizontalFactor, event.verticalFactor,
                                                  event.anchor);
                }
                break;
            case EditorTouchGesture::Event::Type::NavigationEnd:
                startInertia(event.velocity);
                break;
        }
    }
}

void EditorTouchController::onSingleBegin(const EditorTouchGesture::Event &event) {
    stopInertia();
    m_target->stopTouchViewportAnimation();
    m_lastStreamPosition = event.position;
    m_panVelocity = {};
    m_panTimestamp = now();

    // A held press always means "select", never "create": it is the gesture
    // that reaches rubber band / interval selection when the plain drag is
    // already taken by direct manipulation.
    // A tap always goes straight to the interaction layer: it must select or
    // deselect, never create content and never scroll.
    auto action = EditorTouchTarget::BlankDragAction::SyntheticMouse;
    if (!event.tap && !event.fromLongPress && !m_target->touchHitsContent(event.position))
        action = m_target->touchBlankDragAction();

    if (action == EditorTouchTarget::BlankDragAction::Pan) {
        m_panStreamActive = true;
        return;
    }
    if (action == EditorTouchTarget::BlankDragAction::DirectManipulation) {
        m_target->beginTouchDirectManipulation();
        m_directManipulationActive = true;
    }

    m_syntheticStreamActive = true;
    EditorPointer::beginTouchStream();
    sendSyntheticMouse(QEvent::MouseButtonPress, event.position, Qt::LeftButton, Qt::LeftButton);
    if (event.doubleTap) {
        sendSyntheticMouse(QEvent::MouseButtonDblClick, event.position, Qt::LeftButton,
                           Qt::LeftButton);
    }
}

void EditorTouchController::onSingleMove(const EditorTouchGesture::Event &event) {
    const auto delta = event.position - m_lastStreamPosition;
    if (m_panStreamActive) {
        const auto timestamp = now();
        const auto dt = static_cast<double>(timestamp - m_panTimestamp) / 1000.0;
        if (dt > 0.0) {
            const QPointF instant(delta.x() / dt, delta.y() / dt);
            m_panVelocity = m_panVelocity * panVelocitySmoothing +
                            instant * (1.0 - panVelocitySmoothing);
            m_panTimestamp = timestamp;
        }
        m_lastStreamPosition = event.position;
        m_target->panTouchViewportBy(delta);
        return;
    }
    m_lastStreamPosition = event.position;
    if (m_syntheticStreamActive)
        sendSyntheticMouse(QEvent::MouseMove, event.position, Qt::NoButton, Qt::LeftButton);
}

void EditorTouchController::onSingleEnd(const EditorTouchGesture::Event &event) {
    if (m_panStreamActive) {
        m_panStreamActive = false;
        startInertia(m_panVelocity);
        return;
    }
    if (m_syntheticStreamActive)
        sendSyntheticMouse(QEvent::MouseButtonRelease, event.position, Qt::LeftButton,
                           Qt::NoButton);
    finishStream();
}

void EditorTouchController::onSingleCancel() {
    if (m_panStreamActive) {
        m_panStreamActive = false;
        return;
    }
    if (m_syntheticStreamActive)
        m_target->cancelTouchPointerInteraction();
    finishStream();
}

void EditorTouchController::finishStream() {
    if (m_directManipulationActive) {
        m_target->endTouchDirectManipulation();
        m_directManipulationActive = false;
    }
    if (m_syntheticStreamActive) {
        m_syntheticStreamActive = false;
        EditorPointer::endTouchStream();
    }
}

void EditorTouchController::onLongPress(const EditorTouchGesture::Event &event) {
    if (m_target->touchHitsContent(event.position)) {
        postContextMenu(event.position);
        dispatch(m_gesture.confirmLongPress(false, now()));
        return;
    }
    // Blank area: a held press starts the selection drag straight away, so the
    // rubber band shows up under the finger instead of waiting for a release.
    dispatch(m_gesture.confirmLongPress(true, now()));
}

void EditorTouchController::sendSyntheticMouse(const QEvent::Type type, const QPointF &position,
                                               const Qt::MouseButton button,
                                               const Qt::MouseButtons buttons) {
    auto *target = m_eventTarget ? m_eventTarget.data() : m_widget.data();
    if (!target)
        return;
    const auto global = target->mapToGlobal(position);
    // The touch device travels with the event: that is how the rest of the
    // codebase recognizes a finger-driven mouse stream (EditorPointer).
    QMouseEvent event(type, position, position, global, button, buttons,
                      QApplication::keyboardModifiers(),
                      m_device ? m_device : QPointingDevice::primaryPointingDevice());
    QCoreApplication::sendEvent(target, &event);
}

void EditorTouchController::postContextMenu(const QPointF &position) {
    auto *target = m_eventTarget ? m_eventTarget.data() : m_widget.data();
    if (!target)
        return;
    const auto global = target->mapToGlobal(position).toPoint();
    // Posted, never sent: context menus run a nested event loop through exec(),
    // and doing that from inside touch delivery wedges the gesture stream.
    QCoreApplication::postEvent(
        target, new QContextMenuEvent(QContextMenuEvent::Other, position.toPoint(), global,
                                      QApplication::keyboardModifiers()));
}

void EditorTouchController::startInertia(const QPointF &velocity) {
    const auto speed = std::hypot(velocity.x(), velocity.y());
    if (speed < m_gesture.config().inertiaMinVelocityPxPerSec) {
        stopInertia();
        return;
    }
    m_inertiaVelocity = velocity;
    m_inertiaTimestamp = now();
    m_inertiaTimer->start();
}

void EditorTouchController::stopInertia() {
    m_inertiaTimer->stop();
    m_inertiaVelocity = {};
}

void EditorTouchController::onInertiaFrame() {
    if (!m_target || !m_widget || !m_widget->isVisible()) {
        stopInertia();
        return;
    }
    const auto timestamp = now();
    const auto dt = static_cast<double>(timestamp - m_inertiaTimestamp) / 1000.0;
    m_inertiaTimestamp = timestamp;
    if (dt <= 0.0)
        return;

    m_target->panTouchViewportBy(m_inertiaVelocity * dt);
    m_inertiaVelocity *= std::exp(-inertiaDecayPerSecond * dt);
    if (std::hypot(m_inertiaVelocity.x(), m_inertiaVelocity.y()) < inertiaStopSpeed)
        stopInertia();
}

void EditorTouchController::cancel() {
    disarmLongPressTimer();
    stopInertia();
    const auto events = m_gesture.cancelled();
    for (const auto &event : events) {
        if (event.type == EditorTouchGesture::Event::Type::SingleCancel)
            onSingleCancel();
    }
    m_panStreamActive = false;
    if (m_syntheticStreamActive) {
        m_target->cancelTouchPointerInteraction();
        finishStream();
    }
}
