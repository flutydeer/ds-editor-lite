#include "EditorWheelController.h"

#include "EditorTouchTarget.h"
#include "EditorViewportController.h"

#include <QInputDevice>
#include <QNativeGestureEvent>
#include <QObject>
#include <QWidget>
#include <QWheelEvent>

#include <cmath>

namespace {
    // The glide constants the two-finger touch pan uses (EditorTouchController):
    // frame interval, exponential decay per second, and the pan speed below
    // which a glide stops. The touchpad shares them, so every finger-free
    // glide has the same feel.
    constexpr int glideIntervalMs = 16;
    constexpr double glideDecayPerSecond = 3.6;
    constexpr double panGlideStopSpeed = 20.0;
    // A pinch slower than this is a deliberate zoom, not a flick to glide; a
    // glide whose rate has decayed below this has effectively stopped.
    constexpr double zoomGlideMinRate = 0.05;
    constexpr double zoomGlideStopRate = 0.02;
    // Weight kept on the previous velocity estimate, the same smoothing the
    // touch pan uses.
    constexpr double velocitySmoothing = 0.55;
} // namespace

EditorWheelController::EditorWheelController(EditorViewportController *viewport,
                                             EditorTouchTarget *touchTarget, QWidget *widget)
    : m_viewport(viewport), m_touchTarget(touchTarget), m_widget(widget),
      m_input(&m_ownedInput) {
    const auto installScrollTarget = [this](const Qt::Orientation orientation,
                                            const double viewportFraction) {
        m_input->setScrollTarget(
            orientation,
            {
                .value =
                    [this, orientation] {
                        return orientation == Qt::Horizontal ? m_viewport->horizontalOffset()
                                                             : m_viewport->verticalOffset();
                    },
                .setValue =
                    [this, orientation](const double value) {
                        auto offset = m_viewport->offset();
                        if (orientation == Qt::Horizontal)
                            offset.setX(value);
                        else
                            offset.setY(value);
                        m_viewport->setOffset(offset);
                    },
                .boundedValue =
                    [this, orientation](const double value) {
                        return static_cast<double>(qBound(
                            0, qRound(value), qRound(m_viewport->maximumOffset(orientation))));
                    },
                .step =
                    [this, orientation, viewportFraction] {
                        const auto size = m_viewport->viewportSize();
                        return (orientation == Qt::Horizontal ? size.width() : size.height()) *
                               viewportFraction;
                    },
                .canScroll = [] { return true; },
            });
    };
    installScrollTarget(Qt::Horizontal, 0.2);
    installScrollTarget(Qt::Vertical, 0.15);

    m_input->setZoomTarget(Qt::Horizontal,
                           {
                               .value = [this] { return m_viewport->horizontalScale(); },
                               .setValueAt =
                                   [this](const double value, const double anchor) {
                                       m_viewport->setScale(
                                           value, m_viewport->verticalScale(),
                                           {anchor, m_viewport->viewportSize().height() * 0.5});
                                   },
                               .boundedValue =
                                   [this](const double value) {
                                       return m_viewport->boundedScale(Qt::Horizontal, value);
                                   },
                               .step = 0.4,
                           });
    m_input->setZoomTarget(Qt::Vertical,
                           {
                               .value = [this] { return m_viewport->verticalScale(); },
                               .setValueAt =
                                   [this](const double value, const double anchor) {
                                       m_viewport->setScale(
                                           m_viewport->horizontalScale(), value,
                                           {m_viewport->viewportSize().width() * 0.5, anchor});
                                   },
                               .boundedValue =
                                   [this](const double value) {
                                       return m_viewport->boundedScale(Qt::Vertical, value);
                                   },
                               .step = 0.3,
                           });

    m_clock.start();
    m_glideTimer.setInterval(glideIntervalMs);
    QObject::connect(&m_glideTimer, &QTimer::timeout, [this] { onGlideFrame(); });
}

EditorWheelController::EditorWheelController(EditorTouchTarget *touchTarget,
                                             WheelInputController &viewInput, QWidget *widget)
    : m_viewport(nullptr), m_touchTarget(touchTarget), m_widget(widget), m_input(&viewInput) {
    m_clock.start();
    m_glideTimer.setInterval(glideIntervalMs);
    QObject::connect(&m_glideTimer, &QTimer::timeout, [this] { onGlideFrame(); });
}

bool EditorWheelController::handleWheel(QWheelEvent *event) {
    noteWheelInput(event, m_input->resolveAction(event, WheelInputController::Action::Automatic));
    return m_input->handleWheel(event);
}

bool EditorWheelController::handleNativeGesture(QNativeGestureEvent *event) {
    if (!event || !m_touchTarget || !m_widget)
        return false;
    switch (event->gestureType()) {
        case Qt::BeginNativeGesture:
            // A pinch takes the viewport over: glides and wheel-driven motion
            // stop, the same handoff a two-finger touch gesture performs.
            stopPanGlide();
            stopZoomGlide();
            m_touchTarget->stopTouchViewportAnimation();
            m_zoomRate = 0.0;
            m_zoomLastMs = now();
            return true;
        case Qt::ZoomNativeGesture: {
            const auto factor = event->value() + 1.0;
            if (factor <= 0.0)
                return true;
            // Both axes scale, anchored at the gesture position's x and y —
            // the same factors and anchor the two-finger pinch uses.
            const auto anchor = m_widget->mapFromGlobal(event->globalPosition().toPoint());
            m_touchTarget->zoomTouchViewportBy(factor, factor, anchor);
            m_zoomAnchor = anchor;
            const auto timestamp = now();
            const auto dt = static_cast<double>(timestamp - m_zoomLastMs) / 1000.0;
            m_zoomLastMs = timestamp;
            if (dt > 0.0) {
                const auto instant = std::log(factor) / dt;
                m_zoomRate = m_zoomRate * velocitySmoothing + instant * (1.0 - velocitySmoothing);
            }
            return true;
        }
        case Qt::EndNativeGesture:
            // A fast pinch keeps settling for a short while, with the same
            // decay as the pan glide; anchored where the fingers left.
            if (std::abs(m_zoomRate) >= zoomGlideMinRate)
                startZoomGlide();
            return true;
        default:
            return false;
    }
}

bool EditorWheelController::horizontalScale(QWheelEvent *event) {
    noteWheelInput(event, WheelInputController::Action::HorizontalZoom);
    return m_input->handleWheel(event, WheelInputController::Action::HorizontalZoom, Qt::Vertical);
}

bool EditorWheelController::verticalScale(QWheelEvent *event) {
    noteWheelInput(event, WheelInputController::Action::VerticalZoom);
    return m_input->handleWheel(event, WheelInputController::Action::VerticalZoom, Qt::Vertical);
}

bool EditorWheelController::horizontalScroll(QWheelEvent *event) {
    noteWheelInput(event, WheelInputController::Action::HorizontalScroll);
    const auto sourceAxis = event->modifiers() == Qt::ShiftModifier ? Qt::Vertical : Qt::Horizontal;
    return m_input->handleWheel(event, WheelInputController::Action::HorizontalScroll, sourceAxis);
}

bool EditorWheelController::verticalScroll(QWheelEvent *event) {
    noteWheelInput(event, WheelInputController::Action::VerticalScroll);
    return m_input->handleWheel(event, WheelInputController::Action::VerticalScroll, Qt::Vertical);
}

void EditorWheelController::stop() {
    stopPanGlide();
    stopZoomGlide();
    m_input->stop();
}

void EditorWheelController::noteWheelInput(const QWheelEvent *event,
                                           const WheelInputController::Action resolvedAction) {
    // Anything that is not the touchpad stroke in progress cancels both
    // glides. The stroke itself has to keep the velocity its ScrollUpdate
    // events accumulate: clearing it on the way in makes ScrollEnd start a
    // glide at zero, and the first frame then stops immediately.
    stopZoomGlide();
    if (event->deviceType() != QInputDevice::DeviceType::TouchPad) {
        stopPanGlide();
        return;
    }
    // Only a scroll stroke may pan: a zoom stream resolves to a zoom action,
    // and recording its pixelDelta would start a pan glide the moment the
    // zoom flick ends, scrolling the viewport the zoom never asked for.
    const bool panStroke = resolvedAction == WheelInputController::Action::HorizontalScroll ||
                           resolvedAction == WheelInputController::Action::VerticalScroll;
    switch (event->phase()) {
        case Qt::ScrollBegin:
            stopPanGlide();
            m_systemMomentum = false;
            m_glideLastMs = now();
            break;
        case Qt::ScrollUpdate:
            // A glide still running here is a previous stroke (Begin was
            // skipped). Drop its speed. Otherwise keep the samples from this
            // stroke.
            stopPanGlide(m_panGliding);
            if (panStroke)
                trackPanVelocity(event);
            break;
        case Qt::ScrollMomentum:
            // The system is gliding. Forget our estimate so ScrollEnd does
            // not start a second one on top of those increments.
            stopPanGlide();
            m_systemMomentum = true;
            break;
        case Qt::ScrollEnd:
            if (panStroke && !m_systemMomentum)
                startPanGlide();
            m_systemMomentum = false;
            break;
        case Qt::NoScrollPhase:
            stopPanGlide();
            break;
    }
}

void EditorWheelController::trackPanVelocity(const QWheelEvent *event) {
    if (event->pixelDelta().isNull())
        return;
    const auto timestamp = now();
    const auto dt = static_cast<double>(timestamp - m_glideLastMs) / 1000.0;
    m_glideLastMs = timestamp;
    if (dt <= 0.0)
        return;
    const QPointF instant(event->pixelDelta().x() / dt, event->pixelDelta().y() / dt);
    m_panVelocity = m_panVelocity * velocitySmoothing + instant * (1.0 - velocitySmoothing);
}

void EditorWheelController::startPanGlide() {
    if (!m_touchTarget || !m_widget)
        return;
    // No separate start threshold: a speed already below the stop threshold
    // ends the glide on its first frame.
    m_panGliding = true;
    m_glideLastMs = now();
    m_glideTimer.start();
}

void EditorWheelController::stopPanGlide(const bool forgetVelocity) {
    m_panGliding = false;
    if (forgetVelocity)
        m_panVelocity = {};
    if (!m_zoomGliding)
        m_glideTimer.stop();
}

void EditorWheelController::startZoomGlide() {
    m_zoomGliding = true;
    m_glideLastMs = now();
    m_glideTimer.start();
}

void EditorWheelController::stopZoomGlide() {
    m_zoomGliding = false;
    m_zoomRate = 0.0;
    if (!m_panGliding)
        m_glideTimer.stop();
}

void EditorWheelController::onGlideFrame() {
    if (!m_widget || !m_widget->isVisible()) {
        stopPanGlide();
        stopZoomGlide();
        return;
    }
    const auto timestamp = now();
    const auto dt = static_cast<double>(timestamp - m_glideLastMs) / 1000.0;
    m_glideLastMs = timestamp;
    if (dt <= 0.0)
        return;

    if (m_panGliding) {
        m_touchTarget->panTouchViewportBy(m_panVelocity * dt);
        m_panVelocity *= std::exp(-glideDecayPerSecond * dt);
        if (std::hypot(m_panVelocity.x(), m_panVelocity.y()) < panGlideStopSpeed)
            stopPanGlide();
    }
    if (m_zoomGliding) {
        const auto factor = std::exp(m_zoomRate * dt);
        m_touchTarget->zoomTouchViewportBy(factor, factor, m_zoomAnchor);
        m_zoomRate *= std::exp(-glideDecayPerSecond * dt);
        if (std::abs(m_zoomRate) < zoomGlideStopRate)
            stopZoomGlide();
    }
}

qint64 EditorWheelController::now() const {
    return m_clock.elapsed();
}
