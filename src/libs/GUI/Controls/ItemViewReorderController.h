#ifndef DSEDITORLITE_ITEMVIEWREORDERCONTROLLER_H
#define DSEDITORLITE_ITEMVIEWREORDERCONTROLLER_H

#include <QObject>
#include <QPoint>
#include <QPointer>

#include <functional>

class QAbstractItemView;
class DragHandle;

/// Turns "press on the drag handle, then move" into a row drag.
///
/// Rows are reordered by dragging, but on a touch screen - and inside a page
/// that scrolls - a drag starting anywhere on the row fights the scroll: the row
/// moves and the page scrolls at the same time. This controller confines the
/// drag to the handle so the rest of the row stays scrollable.
///
/// Two handle shapes are supported:
///  - a DragHandle child widget inside the row (item widgets), recognised by
///    type and resolved to a row through indexAt();
///  - a grip painted by the view's own delegate into the item rect, described by
///    a hit test the view supplies.
///
/// The drag itself is started by the view: the controller only decides when.
/// It emits dragRequested(), and the view's startDrag() override admits exactly
/// that one call through consumeDragArm(). That is how the base class's own
/// mouse-drag initiation is suppressed without touching dragEnabled() - clearing
/// dragEnabled would drop the view out of InternalMove, after which the drop
/// falls back to the proposed action and the reorder turns into a duplicate.
class ItemViewReorderController : public QObject {
    Q_OBJECT

public:
    /// Identifies the handle under a viewport position, reporting its row.
    using HandleHitTest = std::function<bool(const QPoint &viewportPos, int *row)>;

    /// Creates a controller for \p view. The controller is parented to \p view
    /// unless \p parent says otherwise, so it lives exactly as long as the view.
    explicit ItemViewReorderController(QAbstractItemView *view, QObject *parent = nullptr);

    /// Identifies delegate-painted handles. Required for that shape, unused for
    /// child-widget handles.
    void setHandleHitTest(HandleHitTest hitTest);
    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;

    /// Re-scans the row widgets for DragHandle children. Cheap and idempotent.
    void refreshHandles();

    /// Takes the pending arm, if any. The view's startDrag() override must ask
    /// this before deferring to the base implementation.
    [[nodiscard]] bool consumeDragArm();

Q_SIGNALS:
    /// Emitted when a handle gesture crossed the drag threshold for \p row.
    void dragRequested(int row);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Resolves the handle under \p watched at \p viewportPos, reporting its row.
    [[nodiscard]] bool pressIsOnHandle(const QObject *watched, const QPoint &viewportPos,
                                       int *row) const;
    void scheduleHandleRefresh();
    void beginDrag();

    QPointer<QAbstractItemView> m_view;
    HandleHitTest m_hitTest;
    bool m_enabled = true;
    bool m_refreshPending = false;
    // True while the current press is one the handle owns, so the per-mode
    // consume decisions below stay consistent across press/move/release.
    bool m_pressedOnChildHandle = false;
    bool m_pressed = false;
    bool m_dragActive = false;
    bool m_armPending = false;
    int m_row = -1;
    QPoint m_pressPos;
};

#endif // DSEDITORLITE_ITEMVIEWREORDERCONTROLLER_H
