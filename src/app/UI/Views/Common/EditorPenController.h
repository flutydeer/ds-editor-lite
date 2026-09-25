#ifndef EDITORPENCONTROLLER_H
#define EDITORPENCONTROLLER_H

#include "EditorPenStroke.h"
#include "EditorPenTarget.h"

#include <QCursor>
#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QPointer>

class EditorPenHoverWatcher;
class QTabletEvent;
class QWidget;

// Binds the headless stroke tracker to a real widget.
//
// A stylus is a mouse as far as most of this application is concerned, and for
// the plain tip it stays that way: the tablet event is left unaccepted and Qt
// synthesizes the mouse events itself (with the pen device, tagged
// Qt::MouseEventNotSynthesized), which reproduces today's behaviour exactly —
// double click, hover tips, cursor shapes and all. Only the two inputs the
// platform maps wrongly are taken over:
//
//   eraser tip    -> the platform reports button=Left buttons=Left, exactly
//                    like the tip, so without help it draws instead of erasing
//   barrel button -> the platform reports it as the right button, so a drag
//                    cancels out into a context menu
//
// Once a stroke is claimed, the pen layer owns it end to end: accepting the
// tablet event stops Qt from synthesizing anything for it
// (QGuiApplicationPrivate::processTabletEvent only synthesizes for events that
// were *not* accepted), and every further sample is translated here until the
// pen leaves the glass.
//
// Stroke boundaries come from the contact state and never from
// TabletPress/TabletRelease: pressing the barrel button mid-stroke makes the
// platform emit extra, contradictory press/release pairs that say nothing
// about the tip (see EditorPenStroke).
//
// See docs/design/touch-and-pen-input-design.md §5.
class EditorPenController final : public QObject {
    Q_OBJECT

public:
    EditorPenController(EditorPenTarget *target, QWidget *widget,
                        QWidget *eventTarget = nullptr, QObject *parent = nullptr);
    ~EditorPenController() override;

    // Call from the widget's event()/viewportEvent() override. Returns true
    // when the event was consumed and must not reach the base class.
    bool handleEvent(QEvent *event);

    // Drop everything in flight (hide/deactivate/tool change).
    void cancel();

    [[nodiscard]] bool isStrokeActive() const;

    // The cursor a widget shows while the pen hovers with an erase hint. Built
    // from the toolbar's eraser icon, cached process wide.
    [[nodiscard]] static QCursor eraseCursor();
    // Is there an erase hint at all: the pen is in range with an eraser end
    // pointed at the glass, and this tool has something it could erase. The
    // tool half keeps the cursor honest about what a stroke would do.
    //
    // Static and free of any widget, because the hover cursor belongs to the
    // views: they own their own cursor shape and have to be able to ask the
    // question from inside their own hover handling, where the answer decides
    // between the eraser and whatever cursor they would show otherwise (see
    // docs/design/touch-and-pen-input-design.md §5.4-7).
    [[nodiscard]] static bool eraseHintFor(EditorPenEraser action);
    // The pen is presenting an eraser at all — inverted tip in range, or the
    // barrel button held — whatever the armed tool makes of it. Hover handling
    // needs both halves of the question: the tool's answer decides between the
    // eraser cursor and the refusal cursor.
    [[nodiscard]] static bool eraseHintActive();
    // The pen is presenting an eraser and this tool has nothing it could erase,
    // so the stroke is swallowed whole (EditorPenPolicy answered Unsupported)
    // and hover must not promise anything either: no split marker, no anchor
    // preview, and a cursor that says no.
    [[nodiscard]] static bool eraseHintRefused(EditorPenEraser action);
    // The same question for this widget's armed tool.
    [[nodiscard]] bool showsEraseHint() const;

    // The pen input probe. Shares the developer option with the touch probe,
    // because both answer the same question: what did the platform actually
    // send, and what did the editor do with it.
    [[nodiscard]] static bool isProbeEnabled();

private:
    bool handleTabletEvent(QTabletEvent *event);
    // Synthetic mouse translation of a stroke intent.
    void dispatch(const EditorPenStroke::Intents &intents);
    // One probe line per intent. Only written while the developer option is on.
    static void reportIntents(const EditorPenStroke::Intents &intents);
    void sendSyntheticMouse(QEvent::Type type, const QPointF &position, Qt::MouseButton button,
                            Qt::MouseButtons buttons);
    // A side-button click is a context menu, and the menu event has to be
    // posted rather than sent: the menu runs a nested event loop.
    void postContextMenu(const QPointF &position);

    [[nodiscard]] QWidget *cursorWidget() const;
    void refreshHoverCursor(const QPointF &position);
    void clearHoverCursor();

    [[nodiscard]] bool penEraserAvailable() const;
    // The pen's barrel button is down, or was a moment ago — both are close
    // enough to the right-click the platform derives from it.
    [[nodiscard]] bool penBarrelActive() const;
    [[nodiscard]] static EditorPenStroke::Sample sampleFrom(const QTabletEvent *event);

    EditorPenTarget *m_target;
    QPointer<QWidget> m_widget;
    QPointer<QWidget> m_eventTarget;
    const QPointingDevice *m_device = nullptr;

    EditorPenStroke m_stroke;
    EditorPenHoverWatcher *m_hover = nullptr;

    // A synthetic left-button stream is in flight, so it has to be balanced
    // with EditorPointer::endPenStream().
    bool m_streamActive = false;
    // EditorPointer's erase intent is set, and must be cleared.
    bool m_eraseIntentActive = false;

    // The tip is on the glass in a stroke Qt is synthesizing for us. A barrel
    // press in the middle of that stroke is noise and is swallowed; the stroke
    // itself stays a tip stroke.
    bool m_foreignContact = false;
    // A swallowed barrel press of that stroke, waiting for its release.
    bool m_foreignBarrelNoise = false;
    // A claimed stroke that ended on a move already saw the lift; the
    // platform's own release still follows and has to be swallowed with it.
    bool m_trailingRelease = false;
    // The platform has a right-click of its own for the barrel button: it emits
    // the whole legacy mouse sequence for it (measured with the TouchProbe) and
    // Qt turns its WM_CONTEXTMENU into a context menu event of its own, at
    // wherever the pen happens to be — which is not where the user asked for a
    // menu. This layer has already ruled on the stroke that produced it, so the
    // platform's copy is dropped. Armed at the barrel press — the platform's
    // message is generated at the *release*, so arming any later would be
    // arming after the event it is meant to catch — and spent on it.
    bool m_platformMenuPending = false;
    // When the pen's barrel button was last seen down, from the watcher (during
    // hover, where no Qt event exists at all) or from a tablet sample. This is
    // the second way to recognise the platform's menu: the one it sends for a
    // barrel stroke the *popup* swallowed, so this layer never saw the press
    // and has nothing armed.
    qint64 m_barrelDownMs = -1;

    QPointF m_lastHoverPosition;
    qint64 m_lastHoverMs = -1;
    bool m_hoverCursorApplied = false;

    QElapsedTimer m_clock;
};

#endif // EDITORPENCONTROLLER_H
