#ifndef DSEDITORLITE_COMBOPOPUTOUCHFILTER_H
#define DSEDITORLITE_COMBOPOPUTOUCHFILTER_H

#include <QObject>
#include <QPointer>
#include <QPointF>

class QComboBox;
class QEvent;
class QWidget;

/// Gives a combo box popup the touch semantics of a mobile picker: a tap
/// selects the item under the finger and closes the popup, a drag scrolls the
/// list with QScroller inertia, and releasing a drag never commits anything.
///
/// While a popup is active Qt does not deliver touch events into its widget
/// tree at all: QWidgetWindow ignores touch so the touch is synthesized into
/// mouse events instead (qwidgetwindow.cpp), and on Windows the OS adds its
/// own legacy mouse stream (source = MouseEventSynthesizedBySystem). A combo
/// popup therefore only ever sees touch-derived MOUSE events, whose press
/// highlights items and whose release runs QComboBoxPrivateContainer's
/// "commit and close" branch - any drag selected the item it ended on and
/// closed the popup before scrolling could engage, and a QScroller touch
/// gesture grabbed on the popup viewport never receives any input.
///
/// The filter on the popup view's viewport takes over the synthesized mouse
/// streams (source != MouseEventNotSynthesized - real mice and pens pass
/// through untouched):
///   - each press/move/release feeds QScroller::handleInput(), so a drag
///     scrolls with the same kinetic physics as the rest of the app,
///   - a stream that releases within the tap threshold is replayed as a real
///     left-button press + release pair, driving Qt's own activation and
///     popup close paths,
///   - everything else is consumed, so the item view never sees a
///     touch-derived release that would commit mid-scroll.
///
/// The threshold shares its value with the scroller's DragStartDistance and
/// both use QScroller's own measure ((delta / pixelPerMeter).manhattanLength(),
/// see qscroller.cpp), so a stream is either a tap or a scroll - no dead zone.
/// Side effect: the OS touch long-press context menu no longer appears inside
/// the popup; combo items have no context menu anyway.
class ComboPopupTouchFilter : public QObject {
    Q_OBJECT

public:
    /// Installs the popup touch behavior on \p combo, including the kinetic
    /// scroller if the popup does not carry one yet. Call after the final view
    /// is set (the filter binds to that view's viewport); idempotent. The
    /// filter lives as long as the combo's view.
    static void install(QComboBox *combo);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit ComboPopupTouchFilter(QWidget *viewport);

    void replayTap(const QPointF &position, const QPointF &globalPosition, ulong timestamp);

    QPointer<QWidget> m_viewport;
    // State of the touch-derived mouse stream in flight. A stream that ever
    // moved past the tap threshold belongs to scrolling (m_moved latches),
    // even if the finger wandered back before releasing.
    bool m_streaming = false;
    bool m_moved = false;
    QPointF m_pressPosition;
};

#endif // DSEDITORLITE_COMBOPOPUTOUCHFILTER_H
