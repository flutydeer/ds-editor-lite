#ifndef EDITORWHEELCONTROLLER_H
#define EDITORWHEELCONTROLLER_H

#include <QElapsedTimer>
#include <QPointF>
#include <QTimer>

#include <lite/GUI/Controls/WheelInputController.h>

class EditorTouchTarget;
class EditorViewportController;
class QNativeGestureEvent;
class QWidget;
class QWheelEvent;

// Wheel and precision-touchpad input for the editor views.
//
// Discrete mouse wheels go through WheelInputController: stepping, remainder
// accumulation and (when the appearance animation switch is on) animated
// jumps. A precision touchpad is handled on top of that: its pixelDelta wheel
// events pan the viewport instantly, the pinch arrives as native gesture
// events, and a flick that ends with ScrollEnd glides on with the same decay
// the two-finger touch pan uses. When the system itself keeps scrolling
// (ScrollMomentum) only its increments are applied, so the two inertias never
// stack.
//
// The pan and pinch strokes are applied through the view's EditorTouchTarget,
// which is the same surface the two-finger touch gestures drive; both paths
// therefore share the two-axis zoom and its anchors.
class EditorWheelController final {
public:
    // RHI editors: the controller owns the wheel input and binds its scroll
    // and zoom targets to the viewport controller. `touchTarget` receives the
    // touchpad pan and pinch strokes.
    EditorWheelController(EditorViewportController *viewport, EditorTouchTarget *touchTarget,
                          QWidget *widget);
    // Legacy TimeGraphicsView family: the view owns its WheelInputController
    // and its wheel routing; this controller adds the precision-touchpad
    // behavior on top of that input.
    EditorWheelController(EditorTouchTarget *touchTarget, WheelInputController &viewInput,
                          QWidget *widget);

    bool handleWheel(QWheelEvent *event);
    bool handleNativeGesture(QNativeGestureEvent *event);
    bool horizontalScale(QWheelEvent *event);
    bool verticalScale(QWheelEvent *event);
    bool horizontalScroll(QWheelEvent *event);
    bool verticalScroll(QWheelEvent *event);
    void stop();

private:
    // Touchpad bookkeeping shared by every wheel entry point. Input other than
    // the current stroke cancels a glide. ScrollUpdate keeps that stroke's
    // velocity so ScrollEnd can glide with it. `resolvedAction` limits the pan
    // velocity and the glide to scroll strokes: a zoom stream (Ctrl/Alt on the
    // touchpad) must not leave a pan behind when the flick ends.
    void noteWheelInput(const QWheelEvent *event, WheelInputController::Action resolvedAction);
    void trackPanVelocity(const QWheelEvent *event);
    void startPanGlide();
    // `forgetVelocity` drops the speed estimate. A ScrollUpdate of the stroke
    // that is still being measured passes false, so ScrollEnd can glide with
    // the speed those updates accumulated.
    void stopPanGlide(bool forgetVelocity = true);
    void startZoomGlide();
    void stopZoomGlide();
    void onGlideFrame();
    [[nodiscard]] qint64 now() const;

    EditorViewportController *m_viewport;
    EditorTouchTarget *m_touchTarget;
    QWidget *m_widget;
    WheelInputController m_ownedInput;
    WheelInputController *m_input = nullptr;

    QElapsedTimer m_clock;
    QTimer m_glideTimer;
    bool m_panGliding = false;
    QPointF m_panVelocity; // px/s, in panTouchViewportBy direction
    qint64 m_glideLastMs = 0;
    // The system is gliding on its own (ScrollMomentum); its increments are
    // applied but no glide of our own may be started on top of them.
    bool m_systemMomentum = false;
    bool m_zoomGliding = false;
    double m_zoomRate = 0.0; // ln(factor) per second, smoothed
    QPointF m_zoomAnchor;
    qint64 m_zoomLastMs = 0;
};

#endif // EDITORWHEELCONTROLLER_H
