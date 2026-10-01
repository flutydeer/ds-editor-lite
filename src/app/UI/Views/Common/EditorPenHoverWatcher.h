#ifndef EDITORPENHOVERWATCHER_H
#define EDITORPENHOVERWATCHER_H

#include <QObject>

class QTabletEvent;

// Pen state during the hover phase: the pen is in range but not touching the
// glass. This is the one piece of the pen layer that cannot stay platform
// neutral.
//
// Windows throws the two flags away before Qt ever builds a tablet event —
// qwindowspointerhandler.cpp only reads the barrel flag while the tip is in
// contact:
//
//     if (pointerInContact && penInfo->penFlags & PEN_FLAG_BARREL)
//         mouseButtons = Qt::RightButton; // Either left or right, not both
//
// so no pure-Qt application can see either the barrel button or an inverted
// pen during hover; OneNote's lasso cursor proves the state is there, and it
// is only reachable through GetPointerPenInfo(). Hence the shim: the interface
// is platform free, and native types appear only in the Windows implementation
// below.
//
// The watcher is process-wide on purpose. It sees the state change the instant
// it happens — pressing the barrel button while hovering produces *no* Qt
// event at all — and pushes the change out through changed() so each editor
// widget can keep its cursor in sync. It only ever reads messages; the Qt
// handler that follows must still see them untouched.
class EditorPenHoverWatcher : public QObject {
    Q_OBJECT

public:
    struct State {
        // The pen is in range of the digitiser.
        bool inRange = false;
        // Inverted tip, i.e. the eraser.
        bool inverted = false;
        // The barrel (side) button is held.
        bool sideButton = false;
    };

    ~EditorPenHoverWatcher() override;

    [[nodiscard]] State state() const;
    // Is there an erase hint to show: one of the two ends of the pen that
    // erases is pointing at the screen.
    [[nodiscard]] bool eraseHint() const;

    // The process-wide shim. Created on first use, never destroyed, inert
    // until an editor widget asks for it.
    [[nodiscard]] static EditorPenHoverWatcher *instance();
    // The shim only if something already created it. Queries that must not
    // install platform machinery as a side effect use this one.
    [[nodiscard]] static EditorPenHoverWatcher *existing();

    // Feed a hover tablet event. Platforms other than Windows carry the barrel
    // button and the inverted flag on the hover event itself (see the note in
    // observeHoverEvent), so the generic implementation reads them from here;
    // the Windows implementation ignores this and uses the raw messages.
    virtual void observeHoverEvent(const QTabletEvent *event);

    // Tablet proximity events are delivered to the application object, not to
    // a widget, so there is no controller in the path to notice them. The
    // watcher filters them itself, on every platform.
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    // Emitted whenever the published state actually moved, so a cursor update
    // is never driven by the message rate (about 260 hover messages a second).
    void changed();

protected:
    explicit EditorPenHoverWatcher(QObject *parent);

    void setState(const State &state);
    void clear();

private:
    State m_state;
};

#endif // EDITORPENHOVERWATCHER_H
