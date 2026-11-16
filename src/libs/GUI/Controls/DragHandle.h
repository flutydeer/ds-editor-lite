#ifndef DSEDITORLITE_DRAGHANDLE_H
#define DSEDITORLITE_DRAGHANDLE_H

#include "IconLabel.h"

/// The reorder grip of a drag-sortable list row.
///
/// The handle claims touch presses so a finger drag reorders the row. The list recognizes
/// the handle by type, and ItemViewReorderController starts the drag through the view.
class DragHandle : public IconLabel {
    Q_OBJECT

public:
    explicit DragHandle(QWidget *parent = nullptr);

    /// Turns dragging from this handle on or off. A disabled handle keeps the
    /// grip visible but greys it out and lets touches fall through to the
    /// surrounding scroller.
    void setDragEnabled(bool enabled);
    [[nodiscard]] bool isDragEnabled() const;

private:
    bool m_dragEnabled = true;
};

#endif // DSEDITORLITE_DRAGHANDLE_H
