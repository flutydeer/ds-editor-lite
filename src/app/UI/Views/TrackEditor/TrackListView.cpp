#include "TrackListView.h"

#include "Controller/TrackController.h"
#include "TrackControlView.h"
#include "Global/TracksEditorGlobal.h"
#include "TrackAppendSlotView.h"
#include "UI/Views/Common/EditorWheelUtils.h"

#include <QDrag>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QPainter>
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
    setDropIndicatorShown(false);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);

    m_dropIndicator = new QWidget(viewport());
    m_dropIndicator->setObjectName("trackDropIndicator");
    m_dropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_dropIndicator->setAttribute(Qt::WA_StyledBackground);
    m_dropIndicator->setAutoFillBackground(true);
    m_dropIndicator->hide();

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

    const auto destination = to > from ? to + 1 : to;
    return model()->moveRow({}, from, {}, destination);
}

void TrackListView::setAppendSlotHeight(const int height) {
    if (!m_appendSlotItem)
        return;
    m_appendSlotItem->setSizeHint(QSize(0, height));
    doItemsLayout();
}

void TrackListView::mousePressEvent(QMouseEvent *event) {
    // Check if the click is in the drag area (track index label)
    m_dragStartPosition = event->pos();
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

    if (!setDropInsertionIndex(dropInsertionIndex(event->position().toPoint()))) {
        event->ignore();
        return;
    }

    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void TrackListView::dragLeaveEvent(QDragLeaveEvent *event) {
    QListWidget::dragLeaveEvent(event);
    clearDropIndicator();
}

void TrackListView::startDrag(Qt::DropActions supportedActions) {
    // Save scroll position before drag starts
    m_scrollPosBeforeDrag = verticalScrollBar()->value();

    const auto indexes = selectedIndexes();
    if (indexes.isEmpty())
        return;

    auto *mimeData = model()->mimeData(indexes);
    if (!mimeData)
        return;

    auto *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    m_dragRow = indexes.first().row();
    if (const auto *dragItem = item(m_dragRow)) {
        const auto rowRect = visualItemRect(dragItem);
        const auto rowPixmap = viewport()->grab(rowRect);
        QPixmap dragPixmap(rowPixmap.size());
        dragPixmap.setDevicePixelRatio(rowPixmap.devicePixelRatio());
        dragPixmap.fill(Qt::transparent);
        {
            QPainter painter(&dragPixmap);
            painter.setOpacity(0.65);
            painter.drawPixmap(QPoint{}, rowPixmap);
        }
        drag->setPixmap(dragPixmap);
        drag->setHotSpot(m_dragStartPosition - rowRect.topLeft());
    }
    drag->exec(supportedActions & ~Qt::CopyAction, Qt::MoveAction);
    m_dragRow = -1;
    clearDropIndicator();
}

void TrackListView::dropEvent(QDropEvent *event) {
    const auto dropRow = dropInsertionIndex(event->position().toPoint());
    const auto moved = moveDraggedTrack(dropRow);
    clearDropIndicator();

    if (!moved) {
        event->ignore();
        return;
    }

    event->setDropAction(Qt::MoveAction);
    event->accept();
}

int TrackListView::dropInsertionIndex(const QPoint &pos) const {
    const auto count = trackCount();
    if (m_dragRow < 0 || count == 0)
        return -1;

    const auto targetItem = itemAt(pos);
    if (!targetItem) {
        const auto lastTrackRect = visualItemRect(item(count - 1));
        return pos.y() >= lastTrackRect.center().y() ? count : -1;
    }

    const auto targetRow = row(targetItem);
    if (targetRow == count)
        return count;
    if (targetRow < 0 || targetRow > count)
        return -1;

    const auto targetRect = visualItemRect(targetItem);
    return pos.y() < targetRect.center().y() ? targetRow : targetRow + 1;
}

bool TrackListView::setDropInsertionIndex(const int insertionIndex) {
    if (!isValidDropInsertionIndex(insertionIndex)) {
        clearDropIndicator();
        return false;
    }

    updateDropIndicator(insertionIndex);
    return true;
}

bool TrackListView::isValidDropInsertionIndex(const int insertionIndex) const {
    return m_dragRow >= 0 && m_dragRow < trackCount() && insertionIndex >= 0 &&
           insertionIndex <= trackCount() && insertionIndex != m_dragRow &&
           insertionIndex != m_dragRow + 1;
}

bool TrackListView::moveDraggedTrack(const int insertionIndex) {
    if (!isValidDropInsertionIndex(insertionIndex))
        return false;

    const auto dragRow = m_dragRow;
    const auto currentScrollPos = verticalScrollBar()->value();
    // Moving downward removes the source row first, so the destination index shifts up by one.
    const auto finalRow = insertionIndex > dragRow ? insertionIndex - 1 : insertionIndex;

    TrackController::onMoveTrack(dragRow, insertionIndex);

    QTimer::singleShot(0, this, [this, currentScrollPos, finalRow]() {
        setCurrentRow(finalRow);
        verticalScrollBar()->setValue(currentScrollPos);
    });
    return true;
}

void TrackListView::updateDropIndicator(const int insertionIndex) {
    const auto count = trackCount();
    if (!m_dropIndicator || insertionIndex < 0 || insertionIndex > count || count == 0) {
        clearDropIndicator();
        return;
    }

    const auto boundary = insertionIndex == count
                              ? visualItemRect(item(count - 1)).bottom() + 1
                              : visualItemRect(item(insertionIndex)).top();
    constexpr auto indicatorHeight = 2;
    const auto top = qBound(0, boundary - indicatorHeight / 2,
                            viewport()->height() - indicatorHeight);
    m_dropIndicator->setGeometry(0, top, viewport()->width(), indicatorHeight);
    m_dropIndicator->raise();
    m_dropIndicator->show();
}

void TrackListView::clearDropIndicator() {
    if (m_dropIndicator)
        m_dropIndicator->hide();
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
