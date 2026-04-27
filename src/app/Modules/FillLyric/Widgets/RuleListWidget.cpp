#include "RuleListWidget.h"

#include <QDropEvent>

namespace FillLyric
{
    RuleListWidget::RuleListWidget(QWidget *parent) : QListWidget(parent) {
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
        setDropIndicatorShown(true);
        setDragDropOverwriteMode(false);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setObjectName("RuleListWidget");
    }

    void RuleListWidget::dropEvent(QDropEvent *event) {
        // Remember the dragged item's row before drop
        const int fromRow = currentRow();
        QListWidget::dropEvent(event);
        // After InternalMove, item widgets are lost — caller must rebuild them.
        emit orderChanged();
    }
} // namespace FillLyric
