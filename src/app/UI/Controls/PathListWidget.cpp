#include "PathListWidget.h"

#include <QModelIndex>
#include <QMouseEvent>

PathListWidget::PathListWidget(QWidget *parent) : QListWidget(parent) {
}

void PathListWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    const QModelIndex idx = indexAt(event->pos());
    if (!idx.isValid()) {
        emit doubleClickedEmpty(event->pos());
    } else {
        QListWidget::mouseDoubleClickEvent(event);
    }
}