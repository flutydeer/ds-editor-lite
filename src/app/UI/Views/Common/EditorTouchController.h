#ifndef EDITORTOUCHCONTROLLER_H
#define EDITORTOUCHCONTROLLER_H

#include "EditorTouchGesture.h"
#include "EditorTouchTarget.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QObject>
#include <QPointF>
#include <QPointer>

class QContextMenuEvent;
class QMouseEvent;
class QPointingDevice;
class QTimer;
class QTouchEvent;
class QWidget;

// Binds the headless gesture machine to a real widget.
//
// Single-finger gestures are translated into synthetic QMouseEvents that carry
// the originating touch device, so every existing mouse-driven interaction
// state machine keeps working untouched. Two-finger gestures never reach the
// interaction layer at all: they drive the viewport directly through
// EditorTouchTarget.
//
// Pen input is deliberately not handled here. A stylus is a mouse as far as
// this application is concerned, and letting the tablet events fall through
// makes Qt synthesize the mouse events for us (with the stylus device and
// Qt::MouseEventNotSynthesized, see QGuiApplicationPrivate::processTabletEvent).
//
// The long press context menu is left to the platform wherever the platform
// has one. Windows draws its own press-and-hold feedback and raises the menu
// when the finger leaves the glass, and matching that timing by hand is not
// worth it: our own menu would open mid-hold and then be closed by the right
// button press Windows emits on release. A fallback posts the menu ourselves
// only when no platform menu shows up.
//
// The controller also has to *swallow* foreign pointer events. Accepting a
// QTouchEvent stops Qt from synthesizing mouse events, but it does nothing
// about the ones the operating system generates on its own: Windows promotes
// the primary touch point to legacy mouse messages, which Qt forwards tagged
// Qt::MouseEventSynthesizedBySystem. Without swallowing them every finger drag
// runs twice, and even a two-finger gesture drags content because the promoted
// primary point keeps driving the interaction layer.
class EditorTouchController final : public QObject {
    Q_OBJECT

public:
    EditorTouchController(EditorTouchTarget *target, QWidget *widget,
                          QWidget *eventTarget = nullptr, QObject *parent = nullptr);
    ~EditorTouchController() override;

    // Call from the widget's event()/viewportEvent() override. Returns true
    // when the event was consumed and must not reach the base class.
    bool handleEvent(QEvent *event);

    // Drop everything in flight (hide/deactivate/edit mode change).
    void cancel();

    [[nodiscard]] bool isGestureActive() const;

    // The multi-touch gesture master switch (Appearance options). When off,
    // touch events are left alone and Qt synthesizes plain mouse events.
    [[nodiscard]] static bool isEnabled();

private:
    bool handleTouchEvent(QTouchEvent *event);
    // True when the event was produced from touch by someone other than this
    // controller (the OS, or Qt's own fallback synthesis) and must not reach
    // the interaction layer.
    bool swallowForeignMouseEvent(QMouseEvent *event);
    // Decides whether a context menu may reach the widget. Only a long press
    // that we resolved into a menu is allowed through while touch owns the
    // interaction.
    bool filterContextMenuEvent(QContextMenuEvent *event);
    // Is a context menu arriving right now attributable to a touch gesture of
    // ours? True during a gesture and for a short grace period after it, which
    // is when the platform raises its own press-and-hold menu.
    [[nodiscard]] bool touchOwnsContextMenu() const;
    void armContextMenuFallback(const QPointF &position);
    void cancelContextMenuFallback();
    void dispatch(const EditorTouchGesture::Events &events);
    void onSingleBegin(const EditorTouchGesture::Event &event);
    void onSingleMove(const EditorTouchGesture::Event &event);
    void onSingleEnd(const EditorTouchGesture::Event &event);
    void onSingleCancel();
    void onLongPress(const EditorTouchGesture::Event &event);
    void finishStream();

    void sendSyntheticMouse(QEvent::Type type, const QPointF &position, Qt::MouseButton button,
                            Qt::MouseButtons buttons);
    void postContextMenu(const QPointF &position);

    void armLongPressTimer();
    void disarmLongPressTimer();

    void startInertia(const QPointF &velocity);
    void stopInertia();
    void onInertiaFrame();

    [[nodiscard]] qint64 now() const;

    EditorTouchTarget *m_target;
    QPointer<QWidget> m_widget;
    QPointer<QWidget> m_eventTarget;
    const QPointingDevice *m_device = nullptr;

    EditorTouchGesture m_gesture;
    QElapsedTimer m_clock;
    QTimer *m_longPressTimer;
    QTimer *m_inertiaTimer;
    QTimer *m_contextMenuFallbackTimer;

    // Single-finger stream state.
    bool m_syntheticStreamActive = false;
    // A held press over blank canvas becomes a rubber band, but only once the
    // finger actually travels. Until then the synthetic press is held back, so
    // that a press and hold which never moves stays a context menu instead of
    // silently clearing the selection with a zero-sized rubber band.
    bool m_pressDeferred = false;
    QPointF m_deferredPressPosition;
    bool m_panStreamActive = false;
    QPointF m_lastStreamPosition;
    QPointF m_panVelocity;
    qint64 m_panTimestamp = 0;

    QPointF m_inertiaVelocity;
    qint64 m_inertiaTimestamp = 0;
    // Where a consumed long press happened, kept until the platform delivers
    // its own context menu or the fallback timer posts ours.
    QPointF m_pendingContextMenuPosition;
    // A long press resolved into "open a menu here", so the next context menu
    // event is ours and must be let through.
    bool m_contextMenuExpected = false;
    // When the last touch event arrived, so that a context menu can be traced
    // back to a finger even after the gesture has ended.
    qint64 m_lastTouchActivityMs = -1;
};

#endif // EDITORTOUCHCONTROLLER_H
