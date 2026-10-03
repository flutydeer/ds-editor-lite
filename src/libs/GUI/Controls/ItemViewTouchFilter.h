#ifndef ITEMVIEWTOUCHFILTER_H
#define ITEMVIEWTOUCHFILTER_H

#include <QObject>
#include <QPointF>
#include <QPointer>

class QAbstractItemView;
class QEvent;
class QWidget;

/// Keeps touch scrolling from mutating an item view's selection. On a
/// touch-kinetic viewport (see SmoothScroller) QScroller owns the real touch
/// stream, but Windows keeps synthesizing a mouse stream (BySystem legacy path
/// plus ByQt for the unclaimed press) that QScroller cannot stop; on an item
/// view the synthesized press lands on the item under the finger and changes
/// the selection before the drag even starts.
///
/// The filter swallows every touch-derived mouse event on the viewport and
/// replays a genuine-looking click only when a touch stream ends without
/// moving beyond the tap slop, so taps still select and activate while drags
/// scroll silently. Real mice and pens (NotSynthesized) pass through
/// untouched. Scrolling itself stays with QScroller's own touch recognizer.
class ItemViewTouchFilter : public QObject {
    Q_OBJECT

public:
    /// Installs the filter on \p view's viewport and aligns the scroller's
    /// DragStartDistance with the tap slop so a touch stream is either a tap or
    /// a scroll, never both. Idempotent; the filter lives as long as the view.
    static void install(QAbstractItemView *view);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit ItemViewTouchFilter(QWidget *viewport);
    void replayTap(const QPointF &position, const QPointF &globalPosition, ulong timestamp);

    QPointer<QWidget> m_viewport;
    bool m_streaming = false;
    bool m_moved = false;
    QPointF m_pressPosition;
};

#endif // ITEMVIEWTOUCHFILTER_H
