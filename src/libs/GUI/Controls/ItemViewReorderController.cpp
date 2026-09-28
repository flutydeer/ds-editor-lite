#include <lite/GUI/Controls/ItemViewReorderController.h>

#include <lite/GUI/Controls/DragHandle.h>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QChildEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>

ItemViewReorderController::ItemViewReorderController(QAbstractItemView *view, QObject *parent)
    // A controller filters its view's events, so it has to die with it: callers
    // pass only the view, and leaving them unparented leaked one controller per
    // view built.
    : QObject(parent ? parent : static_cast<QObject *>(view)), m_view(view) {
    if (!m_view)
        return;
    m_view->installEventFilter(this);
    if (auto *viewport = m_view->viewport()) {
        viewport->installEventFilter(this);
        // setItemWidget() parents the row widget to the viewport, so a rebuilt
        // row tree shows up here - that is how the FillLyric lists recreate
        // their handles after an InternalMove.
        if (auto *model = m_view->model()) {
            connect(model, &QAbstractItemModel::rowsInserted, this,
                    &ItemViewReorderController::scheduleHandleRefresh);
            connect(model, &QAbstractItemModel::rowsRemoved, this,
                    &ItemViewReorderController::scheduleHandleRefresh);
            connect(model, &QAbstractItemModel::modelReset, this,
                    &ItemViewReorderController::scheduleHandleRefresh);
        }
    }
    // The rows are often built after the controller, so pick up whatever
    // already exists and let one event-loop pass catch the rest.
    refreshHandles();
    scheduleHandleRefresh();
}

void ItemViewReorderController::setHandleHitTest(HandleHitTest hitTest) {
    m_hitTest = std::move(hitTest);
}

void ItemViewReorderController::setEnabled(const bool enabled) {
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (!m_enabled) {
        m_pressed = false;
        m_pressedOnChildHandle = false;
        m_row = -1;
    }
}

bool ItemViewReorderController::isEnabled() const {
    return m_enabled;
}

void ItemViewReorderController::refreshHandles() {
    if (!m_view)
        return;
    // Re-installing the same filter object on its own target is a no-op in Qt,
    // so this needs no bookkeeping across rebuilds.
    const auto handles = m_view->findChildren<DragHandle *>();
    for (auto *handle : handles)
        handle->installEventFilter(this);
}

void ItemViewReorderController::scheduleHandleRefresh() {
    if (m_refreshPending || !m_view)
        return;
    m_refreshPending = true;
    // One pass per event-loop iteration: a page builds its whole row tree
    // synchronously, so a single queued sweep sees all of it.
    QTimer::singleShot(0, this, [this] {
        m_refreshPending = false;
        refreshHandles();
    });
}

bool ItemViewReorderController::consumeDragArm() {
    if (!m_armPending)
        return false;
    m_armPending = false;
    return true;
}

bool ItemViewReorderController::pressIsOnHandle(const QObject *watched, const QPoint &viewportPos,
                                                int *row) const {
    if (!m_view)
        return false;
    if (qobject_cast<const DragHandle *>(watched)) {
        const auto index = m_view->indexAt(viewportPos);
        if (!index.isValid())
            return false;
        *row = index.row();
        return true;
    }
    if (m_hitTest && watched == m_view->viewport())
        return m_hitTest(viewportPos, row);
    return false;
}

void ItemViewReorderController::beginDrag() {
    if (!m_view || m_row < 0 || m_dragActive)
        return;
    // QDrag::exec() runs a nested modal loop, and a replayed touch move can
    // re-enter this filter while the drag is up; without the latch the same
    // gesture would start a second drag and nest two modal loops.
    m_dragActive = true;
    m_armPending = true;
    Q_EMIT dragRequested(m_row);
    // Whatever happened, do not leave the view's startDrag() guard open.
    m_armPending = false;
    m_dragActive = false;
}

bool ItemViewReorderController::eventFilter(QObject *watched, QEvent *event) {
    if (!m_view)
        return QObject::eventFilter(watched, event);

    switch (event->type()) {
        case QEvent::ChildAdded:
        case QEvent::ChildRemoved:
            if (watched == m_view->viewport() || watched == m_view)
                scheduleHandleRefresh();
            break;
        case QEvent::MouseButtonPress: {
            if (!m_enabled)
                break;
            const auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() != Qt::LeftButton)
                break;
            // Child-widget handles report local coordinates, so go through the
            // global position; delegate-painted grips arrive on the viewport and
            // are already in viewport coordinates.
            const bool onChildHandle = qobject_cast<const DragHandle *>(watched) != nullptr;
            const auto *viewport = m_view->viewport();
            const QPoint viewportPos =
                onChildHandle ? viewport->mapFromGlobal(mouseEvent->globalPosition().toPoint())
                              : mouseEvent->position().toPoint();
            int row = -1;
            if (!pressIsOnHandle(watched, viewportPos, &row))
                break;
            m_pressed = true;
            m_pressedOnChildHandle = onChildHandle;
            m_row = row;
            m_pressPos = viewportPos;
            // A child handle swallows its own press, like the speaker mixer does,
            // so the row never enters the view's drag-select bookkeeping. A
            // delegate-painted grip lets it through: the press still has to
            // select the row and edit it on a double click.
            if (onChildHandle) {
                m_view->setCurrentIndex(m_view->model()->index(row, 0));
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if (!m_pressed)
                break;
            const auto *mouseEvent = static_cast<QMouseEvent *>(event);
            const auto *viewport = m_view->viewport();
            const QPoint globalPos = mouseEvent->globalPosition().toPoint();
            const QPoint viewportPos = m_pressedOnChildHandle ? viewport->mapFromGlobal(globalPos)
                                                              : mouseEvent->position().toPoint();
            if ((viewportPos - m_pressPos).manhattanLength() < QApplication::startDragDistance()) {
                if (m_pressedOnChildHandle)
                    return true;
                break;
            }
            beginDrag();
            return true;
        }
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick: {
            if (!m_pressed)
                break;
            const bool consumed = m_pressedOnChildHandle;
            m_pressed = false;
            m_pressedOnChildHandle = false;
            m_row = -1;
            if (consumed)
                return true;
            break;
        }
        default:
            break;
    }
    return QObject::eventFilter(watched, event);
}
