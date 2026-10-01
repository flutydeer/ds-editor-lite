#include "RuleListWidget.h"

#include <QDropEvent>

#include <lite/GUI/Controls/ItemViewReorderController.h>
#include <lite/GUI/Controls/SmoothScroller.h>

namespace FillLyric {
    RuleListWidget::RuleListWidget(QWidget *parent)
        : QListWidget(parent), m_reorder(new ItemViewReorderController(this)) {
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
        setDropIndicatorShown(true);
        setDragDropOverwriteMode(false);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setObjectName("RuleListWidget");

        // Touch scrolling, like the other lists in the app. Row drags start at
        // the handle instead (see startDrag below), so a finger on the row
        // scrolls this list rather than reordering it.
        auto *smoothScroller = new SmoothScroller(this);
        smoothScroller->attachTo(this);

        connect(m_reorder, &ItemViewReorderController::dragRequested, this, [this](const int row) {
            setCurrentRow(row);
            startDrag(Qt::MoveAction);
        });
    }

    void RuleListWidget::startDrag(const Qt::DropActions supportedActions) {
        // Only a drag the handle asked for may run. The base class would
        // otherwise start one from anywhere on the row and fight the scroll.
        if (!m_reorder->consumeDragArm())
            return;
        QListWidget::startDrag(supportedActions);
    }

    void RuleListWidget::dropEvent(QDropEvent *event) {
        QListWidget::dropEvent(event);
        // After InternalMove, item widgets are lost — caller must rebuild them.
        emit orderChanged();
    }
} // namespace FillLyric
