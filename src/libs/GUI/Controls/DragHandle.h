#ifndef DSEDITORLITE_DRAGHANDLE_H
#define DSEDITORLITE_DRAGHANDLE_H

#include "IconLabel.h"

/// The reorder grip of a drag-sortable list row.
///
/// Owns everything a handle needs to be recognisable and usable: the grip
/// icon, the size-all cursor, and - crucially - the touch claim, so a finger
/// that goes down on the handle reorders the row instead of scrolling the list
/// or the page the list sits in. Lists recognise handles by this type instead of
/// sniffing for a cursor shape, which is what kept the previous handle
/// implementation fragile.
///
/// It does not implement the drag: ItemViewReorderController watches the
/// press-and-move on the handle and asks the view to start the drag.
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
