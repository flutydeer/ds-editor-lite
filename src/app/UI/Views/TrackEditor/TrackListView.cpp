#include "TrackListView.h"

#include "Controller/TrackController.h"
#include "TrackControlView.h"
#include "Global/TracksEditorGlobal.h"
#include "TrackAppendSlotView.h"
#include "UI/Views/Common/EditorWheelUtils.h"

#include <QDragMoveEvent>
#include <QDropEvent>
#include <QScrollBar>
#include <QScroller>
#include <QTimer>
#include <QWheelEvent>

TrackListView::TrackListView(QWidget *parent) : QListWidget(parent) {
    setObjectName("TrackListWidget");
    setViewMode(ListMode);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(ScrollPerPixel);
    setSelectionMode(SingleSelection);
    QScroller::grabGesture(this, QScroller::TouchGesture);
    EditorWheelUtils::forwardChildWheelEvents(viewport(), this,
                                              [this](QWheelEvent *event) { wheelEvent(event); });

    // Enable drag and drop for track reordering
    setDragEnabled(true);
    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);

    // Enable auto-scroll during drag operations
    setAutoScroll(true);
    setAutoScrollMargin(50);

    // Permanent append-slot placeholder row at the bottom of the list. It
    // mirrors the canvas append slot: disabled, non-selectable and
    // non-draggable, excluded from track count/ordering/selection.
    m_appendSlotItem = new QListWidgetItem;
    m_appendSlotItem->setFlags(Qt::NoItemFlags);
    m_appendSlotItem->setSizeHint(QSize(0, qRound(TracksEditorGlobal::trackHeight)));
    addItem(m_appendSlotItem);
    setItemWidget(m_appendSlotItem, new TrackAppendSlotView);
}

int TrackListView::trackCount() const {
    return m_appendSlotItem ? count() - 1 : count();
}

bool TrackListView::moveTrackRow(const int from, const int to) {
    if (from == to)
        return true;
    if (from < 0 || from >= trackCount() || to < 0 || to >= trackCount())
        return false;
    auto *item = takeItem(from);
    if (!item)
        return false;
    auto *widget = itemWidget(item);
    insertItem(to, item);
    if (widget)
        setItemWidget(item, widget);
    return true;
}

void TrackListView::setAppendSlotHeight(const int height) {
    if (!m_appendSlotItem)
        return;
    m_appendSlotItem->setSizeHint(QSize(0, height));
    doItemsLayout();
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
    const auto modifiers = event->modifiers();
    if (modifiers == Qt::AltModifier) {
        emit wheelVerScale(event);
        event->accept();
    } else if (modifiers == Qt::NoModifier) {
        emit wheelVerScroll(event);
        event->accept();
    } else {
        QListWidget::wheelEvent(event);
    }
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

    // The append-slot placeholder row is not a valid drop target; dropping on
    // it (or past the last real track) appends to the end of the list.
    if (dropRow < 0 || dropRow >= trackCount()) {
        dropRow = trackCount();
    } else if (dropIndicator == BelowItem) {
        dropRow++;
    } else if (dropIndicator == AboveItem && dragRow < dropRow) {
        dropRow++;
    }

    if (dragRow == dropRow || (dragRow + 1 == dropRow && dragRow < trackCount() - 1)) {
        event->ignore();
        return;
    }

    // Save current scroll position before the move operation
    const int currentScrollPos = verticalScrollBar()->value();
    // Moving downward removes the source row first, so the destination index shifts up by one.
    const auto finalRow = dropRow > dragRow ? dropRow - 1 : dropRow;

    event->setDropAction(Qt::IgnoreAction);
    event->accept();

    TrackController::onMoveTrack(dragRow, dropRow);

    // Restore scroll position after the model update
    QTimer::singleShot(0, this, [this, currentScrollPos, finalRow]() {
        setCurrentRow(finalRow);
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
