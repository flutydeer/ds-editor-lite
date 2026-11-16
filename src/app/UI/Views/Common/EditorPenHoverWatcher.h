#ifndef EDITORPENHOVERWATCHER_H
#define EDITORPENHOVERWATCHER_H

#include <QObject>

class QTabletEvent;

// Publishes pen state while the pen is in range without contact. Windows QPA omits
// hover barrel and inversion flags, so the Windows watcher reads GetPointerPenInfo()
// from native messages and leaves those messages available to Qt.
//
// The process-wide watcher emits changed() so each editor can update its hover cursor
// even if a barrel press produces no Qt tablet event.
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

    // Feed a hover tablet event. Backends other than Windows QPA carry the barrel
    // button and the inverted flag on the hover event itself (see the note in
    // observeHoverEvent), so the generic implementation reads them from here;
    // the Windows native implementation ignores this and uses the raw messages.
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
