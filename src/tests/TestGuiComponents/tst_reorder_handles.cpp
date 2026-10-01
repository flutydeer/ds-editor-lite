#include "tst_gui_components.h"

// ItemViewReorderController turns a gesture on the drag handle into a drag
// request - and only a gesture on the handle. A real QDrag cannot run here
// (offscreen has no drag-and-drop), so this asserts the controller's contract,
// which is what the views hook their startDrag() guard to.
#include <lite/GUI/Controls/DragHandle.h>
#include <lite/GUI/Controls/ItemViewReorderController.h>
#include <lite/GUI/Controls/PathListWidget.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMouseEvent>
#include <QWidget>

#include <QtTest/QTest>

namespace {
    /// A row widget with a handle on the left, like the FillLyric rule rows.
    QWidget *makeRow(DragHandle **handleOut) {
        auto *row = new QWidget;
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *handle = new DragHandle;
        handle->setSquareSize(16);
        layout->addWidget(handle);
        layout->addWidget(new QWidget, 1);
        *handleOut = handle;
        return row;
    }

    void addHandleRow(QListWidget *list, DragHandle **handleOut) {
        auto *row = makeRow(handleOut);
        auto *item = new QListWidgetItem;
        item->setSizeHint(QSize(300, 34));
        list->addItem(item);
        list->setItemWidget(item, row);
    }

    /// Delivers a mouse event to \p target with its position given in the
    /// coordinate space of \p reference (the view's viewport). The controller
    /// reads globalPosition and maps it back into the viewport, so driving the
    /// test from viewport coordinates keeps it independent of how the row
    /// widgets happen to be laid out - and mirrors what a replayed touch does.
    void sendMouseAt(QWidget *target, QWidget *reference, QEvent::Type type,
                     const QPoint &referencePos, Qt::MouseButton button, Qt::MouseButtons buttons) {
        const QPoint global = reference->mapToGlobal(referencePos);
        QMouseEvent event(type, QPointF(target->mapFromGlobal(global)), QPointF(global), button,
                          buttons, Qt::NoModifier);
        QApplication::sendEvent(target, &event);
    }
} // namespace

void GuiComponentTests::reorderHandlesTrackRebuiltRowsAndIgnoreBodyDrags() {
    // Read only after the application exists, and use the controller's own
    // threshold so the test crosses it by a fixed margin.
    const int dragDistance = QApplication::startDragDistance() + 5;

    // --- a handle child widget starts the drag ---
    QListWidget list;
    list.resize(320, 240);
    DragHandle *handle = nullptr;
    addHandleRow(&list, &handle);
    list.show();
    QApplication::processEvents();

    const QRect firstItemRect = list.visualItemRect(list.item(0));
    QVERIFY2((!firstItemRect.isEmpty()), "the list laid out its rows");
    const QPoint pressViewport = handle->mapTo(list.viewport(), handle->rect().center());

    auto *controller = new ItemViewReorderController(&list);
    QList<int> requested;
    bool reenterOnce = true;
    QObject::connect(
        controller, &ItemViewReorderController::dragRequested,
        [&requested, &reenterOnce, &list, handle, pressViewport, dragDistance](const int row) {
            requested.append(row);
            // A replayed touch move arrives while the drag is up;
            // the controller must not start a second drag.
            if (reenterOnce) {
                reenterOnce = false;
                sendMouseAt(handle, list.viewport(), QEvent::MouseMove,
                            pressViewport + QPoint(dragDistance + 40, 0), Qt::NoButton,
                            Qt::LeftButton);
            }
        });

    sendMouseAt(handle, list.viewport(), QEvent::MouseButtonPress, pressViewport, Qt::LeftButton,
                Qt::LeftButton);
    sendMouseAt(handle, list.viewport(), QEvent::MouseMove, pressViewport + QPoint(dragDistance, 0),
                Qt::NoButton, Qt::LeftButton);
    sendMouseAt(handle, list.viewport(), QEvent::MouseButtonRelease, pressViewport, Qt::LeftButton,
                Qt::NoButton);
    QVERIFY2((requested == QList<int>{0}), "handle gesture requests a drag of its own row");

    // --- the rest of the row does not ---
    requested.clear();
    auto *row = list.itemWidget(list.item(0));
    sendMouseAt(row, list.viewport(), QEvent::MouseButtonPress, pressViewport, Qt::LeftButton,
                Qt::LeftButton);
    sendMouseAt(row, list.viewport(), QEvent::MouseMove, pressViewport + QPoint(0, dragDistance),
                Qt::NoButton, Qt::LeftButton);
    sendMouseAt(row, list.viewport(), QEvent::MouseButtonRelease, pressViewport, Qt::LeftButton,
                Qt::NoButton);
    QVERIFY2((requested.isEmpty()), "a gesture on the row body requests nothing");

    // --- rebuilt rows are picked up again ---
    // The FillLyric lists recreate every item widget after an InternalMove, so
    // the controller has to re-scan instead of keeping a row-to-handle map.
    requested.clear();
    list.clear();
    DragHandle *second = nullptr;
    addHandleRow(&list, &second);
    QApplication::processEvents(); // runs the queued handle refresh
    const QPoint secondPress = second->mapTo(list.viewport(), second->rect().center());
    sendMouseAt(second, list.viewport(), QEvent::MouseButtonPress, secondPress, Qt::LeftButton,
                Qt::LeftButton);
    sendMouseAt(second, list.viewport(), QEvent::MouseMove, secondPress + QPoint(dragDistance, 0),
                Qt::NoButton, Qt::LeftButton);
    QVERIFY2((requested == QList<int>{0}), "a rebuilt row's handle still requests a drag");

    // --- a delegate-painted grip goes through the view's hit test ---
    QListWidget painted;
    painted.resize(320, 240);
    painted.addItem(QStringLiteral("a"));
    painted.addItem(QStringLiteral("b"));
    painted.show();
    QApplication::processEvents();

    const QRect paintedRect = painted.visualRect(painted.model()->index(1, 0));
    QVERIFY2((!paintedRect.isEmpty()), "the painted list laid out its rows");

    constexpr int kGutter = 30;
    auto *paintedController = new ItemViewReorderController(&painted);
    paintedController->setHandleHitTest([&painted](const QPoint &pos, int *row) {
        const QModelIndex index = painted.indexAt(pos);
        if (!index.isValid())
            return false;
        if (pos.x() - painted.visualRect(index).left() > kGutter)
            return false;
        if (row)
            *row = index.row();
        return true;
    });

    QList<int> paintedRequested;
    QObject::connect(paintedController, &ItemViewReorderController::dragRequested,
                     [&paintedRequested](const int row) { paintedRequested.append(row); });

    auto *paintedViewport = painted.viewport();
    const QPoint onGrip(paintedRect.left() + 5, paintedRect.center().y());
    sendMouseAt(paintedViewport, paintedViewport, QEvent::MouseButtonPress, onGrip, Qt::LeftButton,
                Qt::LeftButton);
    sendMouseAt(paintedViewport, paintedViewport, QEvent::MouseMove,
                onGrip + QPoint(0, dragDistance), Qt::NoButton, Qt::LeftButton);
    sendMouseAt(paintedViewport, paintedViewport, QEvent::MouseButtonRelease, onGrip,
                Qt::LeftButton, Qt::NoButton);
    QVERIFY2((paintedRequested == QList<int>{1}), "a grip hit requests a drag of the hit row");

    paintedRequested.clear();
    const QPoint offGrip(paintedRect.right() - 5, paintedRect.center().y());
    sendMouseAt(paintedViewport, paintedViewport, QEvent::MouseButtonPress, offGrip, Qt::LeftButton,
                Qt::LeftButton);
    sendMouseAt(paintedViewport, paintedViewport, QEvent::MouseMove,
                offGrip + QPoint(0, dragDistance), Qt::NoButton, Qt::LeftButton);
    sendMouseAt(paintedViewport, paintedViewport, QEvent::MouseButtonRelease, offGrip,
                Qt::LeftButton, Qt::NoButton);
    QVERIFY2((paintedRequested.isEmpty()), "a drag outside the grip requests nothing");

    // --- the production list wires its own hit test ---
    // The two cases above drive the controller directly, so they cannot catch a
    // host that forgets to connect the painted grip to it. PathListWidget does
    // its own wiring; this goes through the real widget for exactly that reason.
    PathListWidget pathList;
    pathList.resize(320, 240);
    pathList.setDragEnabled(true);
    pathList.setDragDropMode(QAbstractItemView::InternalMove);
    pathList.addItem(QStringLiteral("a"));
    pathList.addItem(QStringLiteral("b"));
    pathList.show();
    QApplication::processEvents();

    auto *pathController = pathList.findChild<ItemViewReorderController *>();
    QVERIFY2((pathController != nullptr), "the path list owns a reorder controller");
    if (pathController) {
        // Cut the view loose from its own handler: that one calls startDrag()
        // and would run a real modal QDrag, which offscreen cannot complete. The
        // wiring is what is under test, not the drag itself.
        QObject::disconnect(pathController, nullptr, &pathList, nullptr);
        QList<int> pathRequested;
        QObject::connect(pathController, &ItemViewReorderController::dragRequested,
                         [&pathRequested](const int row) { pathRequested.append(row); });

        auto *pathViewport = pathList.viewport();
        const QRect pathRow = pathList.visualRect(pathList.model()->index(1, 0));
        QVERIFY2((!pathRow.isEmpty()), "the path list laid out its rows");

        const QPoint onPathGrip(pathRow.left() + 5, pathRow.center().y());
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseButtonPress, onPathGrip,
                    Qt::LeftButton, Qt::LeftButton);
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseMove,
                    onPathGrip + QPoint(0, dragDistance), Qt::NoButton, Qt::LeftButton);
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseButtonRelease, onPathGrip,
                    Qt::LeftButton, Qt::NoButton);
        QVERIFY2((pathRequested == QList<int>{1}), "the path list's own grip starts a reorder");

        pathRequested.clear();
        const QPoint onPathBody(pathRow.right() - 5, pathRow.center().y());
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseButtonPress, onPathBody,
                    Qt::LeftButton, Qt::LeftButton);
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseMove,
                    onPathBody + QPoint(0, dragDistance), Qt::NoButton, Qt::LeftButton);
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseButtonRelease, onPathBody,
                    Qt::LeftButton, Qt::NoButton);
        QVERIFY2((pathRequested.isEmpty()), "the path list's row body starts no reorder");
    }
}
