//
// Created by fluty on 2024/7/11.
//

#include "TrackListView.h"

#include "Controller/TrackController.h"
#include "TrackControlView.h"
#include "TracksGraphicsView.h"
#include "Global/TracksEditorGlobal.h"

#include <QDragMoveEvent>
#include <QDropEvent>
#include <QScrollBar>
#include <QScroller>
#include <QTimer>
#include <QWheelEvent>

TrackListView::TrackListView(QWidget *parent) : QListWidget(parent) {
    setObjectName("TrackListWidget");
    setFixedWidth(TracksEditorGlobal::trackListWidth);
    setViewMode(ListMode);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(ScrollPerPixel);
    setSelectionMode(SingleSelection);
    QScroller::grabGesture(this, QScroller::TouchGesture);

    // Enable drag and drop for track reordering
    setDragEnabled(true);
    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);

    // Enable auto-scroll during drag operations
    setAutoScroll(true);
    setAutoScrollMargin(50);
}

void TrackListView::setGraphicsView(TracksGraphicsView *view) {
    m_view = view;
}

void TrackListView::mousePressEvent(QMouseEvent *event) {
    // Check if the click is in the drag area (track index label)
    m_canStartDrag = isInDragArea(event->pos());
    QListWidget::mousePressEvent(event);
    event->ignore();
}

void TrackListView::mouseMoveEvent(QMouseEvent *event) {
    // Only allow drag if the press started in the drag area
    if (!m_canStartDrag) {
        // Prevent drag by not calling base class when not in drag area
        event->ignore();
        return;
    }
    QListWidget::mouseMoveEvent(event);
}

void TrackListView::wheelEvent(QWheelEvent *event) {
    if (!m_view)
        return;

    const auto modifiers = event->modifiers();
    if (modifiers == Qt::AltModifier)
        m_view->onWheelVerScale(event);
    else if (modifiers == Qt::NoModifier)
        m_view->onWheelVerScroll(event);
}

void TrackListView::dragMoveEvent(QDragMoveEvent *event) {
    QListWidget::dragMoveEvent(event);
}

void TrackListView::startDrag(Qt::DropActions supportedActions) {
    // Save scroll position before drag starts
    m_scrollPosBeforeDrag = verticalScrollBar()->value();
    QListWidget::startDrag(supportedActions);
}

void TrackListView::dropEvent(QDropEvent *event) {
    const auto selectedItems = selectedIndexes();
    if (selectedItems.isEmpty()) {
        event->ignore();
        return;
    }

    const auto dragRow = selectedItems.first().row();
    auto dropRow = indexAt(event->position().toPoint()).row();
    const auto dropIndicator = dropIndicatorPosition();

    if (dropRow < 0) {
        dropRow = count();
    } else if (dropIndicator == BelowItem) {
        dropRow++;
    } else if (dropIndicator == AboveItem && dragRow < dropRow) {
        dropRow++;
    }

    if (dragRow == dropRow || (dragRow + 1 == dropRow && dragRow < count() - 1)) {
        event->ignore();
        return;
    }

    // Save current scroll position before the move operation
    const int currentScrollPos = verticalScrollBar()->value();

    event->setDropAction(Qt::IgnoreAction);
    event->accept();

    TrackController::onMoveTrack(dragRow, dropRow);

    // Restore scroll position after the model update
    QTimer::singleShot(0, this, [this, currentScrollPos]() {
        verticalScrollBar()->setValue(currentScrollPos);
    });
}

bool TrackListView::isInDragArea(const QPoint &pos) const {
    // Get the item at the position
    const auto listItem = itemAt(pos);
    if (!listItem)
        return false;

    // Get the widget for this item
    const auto widget = itemWidget(listItem);
    if (!widget)
        return false;

    // Check if it's a TrackControlView and if the position is in its drag area
    const auto trackControl = qobject_cast<TrackControlView *>(widget);
    if (!trackControl)
        return false;

    // Convert position to widget coordinates
    const QPoint widgetPos = widget->mapFrom(viewport(), pos);
    return trackControl->isInDragArea(widgetPos);
}
