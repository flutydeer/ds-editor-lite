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

// Routes tablet events for a widget. For plain-tip input, Qt synthesizes mouse events
// tagged Qt::MouseEventNotSynthesized with the pen device. Eraser-tip input would
// otherwise appear as a left click, and the barrel button as a right click.
//
// Accepting a claimed stroke suppresses Qt's mouse synthesis. Its boundaries follow
// tip contact because barrel changes can emit extra TabletPress/TabletRelease pairs
// mid-stroke. See EditorPenStroke and docs/design/touch-and-pen-input-design.md.
class EditorPenController final : public QObject {
    Q_OBJECT

public:
    EditorPenController(EditorPenTarget *target, QWidget *widget,
                        QWidget *eventTarget = nullptr, QObject *parent = nullptr);
    ~EditorPenController() override;

    // Call from the widget's event()/viewportEvent() override. Returns true
    // when the event was consumed and must not reach the base class.
    bool handleEvent(QEvent *event);

    // The pen left the digitizer range with a stroke claimed and its end frame
    // never reached the widget: release the stroke in flight, exactly like a
    // mouse release outside the window. Never opens a menu.
    void cancel();

    // On interruption, discard a claimed stroke through abortPenEraseStroke().
    // A synthetic release would commit the in-flight erase.
    void interrupt();

    [[nodiscard]] bool isStrokeActive() const;

    // The cursor a widget shows while the pen hovers with an erase hint. Built
    // from the toolbar's eraser icon, cached process wide.
    [[nodiscard]] static QCursor eraseCursor();
    // Views use this to show the eraser cursor when pen hover and the armed tool allow it.
    [[nodiscard]] static bool eraseHintFor(EditorPenEraser action);
    // Detect the inverted tip in range or the held barrel button independently of the tool.
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
    // A platform context-menu event can follow either a recent or current barrel press.
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
    // Arm suppression on the barrel press, before Windows can emit WM_CONTEXTMENU on
    // release. The controller already decides whether and where to post a menu.
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
