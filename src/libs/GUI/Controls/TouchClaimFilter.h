#ifndef DSEDITORLITE_TOUCHCLAIMFILTER_H
#define DSEDITORLITE_TOUCHCLAIMFILTER_H

#include <QObject>
#include <QPointF>
#include <QPointer>

#include <functional>

class QWidget;

/// Lets a drag-owning widget keep its gesture inside a touch-kinetic scroll
/// area: the filter claims the widget's whole rect (the widget itself starts
/// accepting touch events) and replays each touch point as a left-button mouse
/// event, so the widget's existing mouse drag logic runs unchanged. Accepting
/// the touch keeps the stream away from the ancestor viewport's QScroller and
/// stops Qt from synthesizing additional mouse events for the widget.
///
/// An optional hit test narrows the claim to a sub-region of the target, for
/// widgets that own only part of themselves - a list whose handle is painted
/// into the item rect, for instance. A touch outside the hit test is left
/// alone: it keeps flowing to the ancestor scroller, and taps still reach the
/// widget through Qt's own touch-to-mouse synthesis.
class TouchClaimFilter : public QObject {
    Q_OBJECT

public:
    /// Returns true when a touch at \p localPos should be claimed by the target.
    using HitTest = std::function<bool(const QPointF &localPos)>;
    /// Called when the system takes a claimed stream away (QEvent::TouchCancel),
    /// right before the release replay that closes the stream. The target uses
    /// it to drop what the release would otherwise commit - a reorder mid-drag,
    /// for one - while still letting the replay reset its pressed state.
    using CancelNotice = std::function<void()>;

    /// Installs the filter on \p target and enables touch delivery on it.
    /// Idempotent; the filter lives as long as the target. An empty \p hitTest
    /// claims the whole rect, which is what every plain drag target wants.
    static void install(QWidget *target, HitTest hitTest = {},
                        CancelNotice cancelNotice = {});

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    TouchClaimFilter(QWidget *target, HitTest hitTest, CancelNotice cancelNotice);

    void stopAncestorScroller() const;

    QPointer<QWidget> m_target;
    HitTest m_hitTest;
    CancelNotice m_cancelNotice;
    bool m_pressed = false;
    // Set when the hit test turned the current stream away, so its remaining
    // events are passed through instead of being consumed.
    bool m_rejected = false;
};

#endif // DSEDITORLITE_TOUCHCLAIMFILTER_H
