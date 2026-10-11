#include <lite/GUI/Controls/PathListWidget.h>

#include <lite/GUI/Controls/ItemViewReorderController.h>
#include <lite/GUI/Controls/PathItemDelegate.h>
#include <lite/GUI/Controls/SmoothScroller.h>

#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QModelIndex>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>

namespace {
    bool hasDirectoryInMimeData(const QMimeData *mimeData) {
        if (!mimeData || !mimeData->hasUrls()) {
            return false;
        }
        const auto &urls = mimeData->urls();
        if (urls.isEmpty()) {
            return false;
        }
        // NOLINTNEXTLINE(*-use-anyofallof)
        for (const auto &url : std::as_const(urls)) {
            QString path = url.toLocalFile();
            if (QFileInfo info(path); info.exists() && info.isDir()) {
                return true;
            }
        }
        return false;
    }

    bool getPathFromMimeData(const QMimeData *mimeData, QStringList *outPaths) {
        if (!mimeData || !mimeData->hasUrls()) {
            return false;
        }
        const auto &urls = mimeData->urls();
        if (urls.isEmpty()) {
            return false;
        }
        QStringList paths;
        for (const auto &url : std::as_const(urls)) {
            QString path = url.toLocalFile();
            if (QFileInfo info(path); info.exists() && info.isDir()) {
                paths.append(QDir::toNativeSeparators(path));
            }
        }
        if (!paths.isEmpty()) {
            if (outPaths) {
                *outPaths = std::move(paths);
            }
            return true;
        }
        return false;
    }
}

PathListWidget::PathListWidget(QWidget *parent)
    : QListWidget(parent), m_reorder(new ItemViewReorderController(this)) {
    setAcceptDrops(true);
    setItemDelegate(new PathItemDelegate(this));

    // The grip is painted, not a widget, so the controller cannot recognise it
    // on its own - without this the press is never attributed to a handle, no
    // drag is ever armed, and the startDrag() guard below then swallows every
    // drag the base class would have started.
    m_reorder->setHandleHitTest(
        [this](const QPoint &viewportPos, int *row) { return handleHitTest(viewportPos, row); });

    // The grip is painted into the row, so the claim sits on the viewport and
    // the hit test decides: a touch on the grip is replayed as a mouse press and
    // starts a reorder, while a touch anywhere else falls through and scrolls
    // the page the list lives in.
    SmoothScroller::installClaim(
        viewport(), [this](const QPointF &pos) { return handleHitTest(pos.toPoint(), nullptr); });

    connect(m_reorder, &ItemViewReorderController::dragRequested, this, [this](const int row) {
        setCurrentIndex(model()->index(row, 0));
        startDrag(Qt::MoveAction);
    });
}

bool PathListWidget::handleHitTest(const QPoint &viewportPos, int *row) const {
    const QModelIndex index = indexAt(viewportPos);
    if (!index.isValid())
        return false;
    if (!PathItemDelegate::handleRect(visualRect(index)).contains(viewportPos))
        return false;
    if (row)
        *row = index.row();
    return true;
}

void PathListWidget::startDrag(const Qt::DropActions supportedActions) {
    // Only a drag the grip asked for may run. The base class would otherwise
    // start one from anywhere on the row, and dragEnabled() cannot be cleared to
    // stop it: that drops the view out of InternalMove and turns the drop into a
    // duplicate instead of a reorder.
    if (!m_reorder->consumeDragArm())
        return;
    QListWidget::startDrag(supportedActions);
}

void PathListWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    const QModelIndex idx = indexAt(event->pos());
    if (!idx.isValid()) {
        emit doubleClickedEmpty(event->pos());
    } else {
        QListWidget::mouseDoubleClickEvent(event);
    }
}

void PathListWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (hasDirectoryInMimeData(event->mimeData())) {
        if (event->possibleActions() & Qt::CopyAction) {
            event->setDropAction(Qt::CopyAction);
            event->accept();
        }
        return;
    }
    QListWidget::dragEnterEvent(event);
}

void PathListWidget::dropEvent(QDropEvent *event) {
    QStringList paths;
    if (getPathFromMimeData(event->mimeData(), &paths)) {
        // Adding search paths must not move the directories in the drag source.
        if (event->possibleActions() & Qt::CopyAction) {
            event->setDropAction(Qt::CopyAction);
            event->accept();
            Q_EMIT itemsDropped(paths);
        }
        return;
    }
    QListWidget::dropEvent(event);
}
