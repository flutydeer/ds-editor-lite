#include <lite/GUI/Controls/PathListWidget.h>

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

PathListWidget::PathListWidget(QWidget *parent) : QListWidget(parent) {
    setAcceptDrops(true);
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
