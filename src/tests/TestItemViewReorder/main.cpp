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

#include <cstdio>

namespace {
    int g_failures = 0;

    void check(const bool ok, const char *what) {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok)
            ++g_failures;
    }

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

int main(int argc, char **argv) {
    QApplication app(argc, argv);
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
    check(!firstItemRect.isEmpty(), "the list laid out its rows");
    const QPoint pressViewport = firstItemRect.center();

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
    check(requested == QList<int>{0}, "handle gesture requests a drag of its own row");
    check(requested.size() == 1, "a replayed move during the drag does not nest another drag");

    // --- the rest of the row does not ---
    requested.clear();
    auto *row = list.itemWidget(list.item(0));
    sendMouseAt(row, list.viewport(), QEvent::MouseButtonPress, pressViewport, Qt::LeftButton,
                Qt::LeftButton);
    sendMouseAt(row, list.viewport(), QEvent::MouseMove, pressViewport + QPoint(0, dragDistance),
                Qt::NoButton, Qt::LeftButton);
    sendMouseAt(row, list.viewport(), QEvent::MouseButtonRelease, pressViewport, Qt::LeftButton,
                Qt::NoButton);
    check(requested.isEmpty(), "a gesture on the row body requests nothing");

    // --- rebuilt rows are picked up again ---
    // The FillLyric lists recreate every item widget after an InternalMove, so
    // the controller has to re-scan instead of keeping a row-to-handle map.
    requested.clear();
    list.clear();
    DragHandle *second = nullptr;
    addHandleRow(&list, &second);
    QApplication::processEvents(); // runs the queued handle refresh
    const QPoint secondPress = list.visualItemRect(list.item(0)).center();
    sendMouseAt(second, list.viewport(), QEvent::MouseButtonPress, secondPress, Qt::LeftButton,
                Qt::LeftButton);
    sendMouseAt(second, list.viewport(), QEvent::MouseMove, secondPress + QPoint(dragDistance, 0),
                Qt::NoButton, Qt::LeftButton);
    check(requested == QList<int>{0}, "a rebuilt row's handle still requests a drag");

    // --- a delegate-painted grip goes through the view's hit test ---
    QListWidget painted;
    painted.resize(320, 240);
    painted.addItem(QStringLiteral("a"));
    painted.addItem(QStringLiteral("b"));
    painted.show();
    QApplication::processEvents();

    const QRect paintedRect = painted.visualRect(painted.model()->index(1, 0));
    check(!paintedRect.isEmpty(), "the painted list laid out its rows");

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
    check(paintedRequested == QList<int>{1}, "a grip hit requests a drag of the hit row");

    paintedRequested.clear();
    const QPoint offGrip(paintedRect.right() - 5, paintedRect.center().y());
    sendMouseAt(paintedViewport, paintedViewport, QEvent::MouseButtonPress, offGrip, Qt::LeftButton,
                Qt::LeftButton);
    sendMouseAt(paintedViewport, paintedViewport, QEvent::MouseMove,
                offGrip + QPoint(0, dragDistance), Qt::NoButton, Qt::LeftButton);
    sendMouseAt(paintedViewport, paintedViewport, QEvent::MouseButtonRelease, offGrip,
                Qt::LeftButton, Qt::NoButton);
    check(paintedRequested.isEmpty(), "a drag outside the grip requests nothing");

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
    check(pathController != nullptr, "the path list owns a reorder controller");
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
        check(!pathRow.isEmpty(), "the path list laid out its rows");

        const QPoint onPathGrip(pathRow.left() + 5, pathRow.center().y());
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseButtonPress, onPathGrip,
                    Qt::LeftButton, Qt::LeftButton);
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseMove,
                    onPathGrip + QPoint(0, dragDistance), Qt::NoButton, Qt::LeftButton);
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseButtonRelease, onPathGrip,
                    Qt::LeftButton, Qt::NoButton);
        check(pathRequested == QList<int>{1}, "the path list's own grip starts a reorder");

        pathRequested.clear();
        const QPoint onPathBody(pathRow.right() - 5, pathRow.center().y());
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseButtonPress, onPathBody,
                    Qt::LeftButton, Qt::LeftButton);
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseMove,
                    onPathBody + QPoint(0, dragDistance), Qt::NoButton, Qt::LeftButton);
        sendMouseAt(pathViewport, pathViewport, QEvent::MouseButtonRelease, onPathBody,
                    Qt::LeftButton, Qt::NoButton);
        check(pathRequested.isEmpty(), "the path list's row body starts no reorder");
    }

    // --- a stray move after the drag must not re-arm it ---
    // The platform's drag loop swallows the release, so the drag returning is
    // what ends the gesture. With the press left latched, the next move over the
    // view reads as a handle press that crossed the threshold and starts a drag
    // of the row that was just dropped - which is what made every entry drag the
    // one that had been reordered.
    QListWidget stray;
    stray.resize(320, 240);
    // The production lists track the mouse on their viewport (SmoothScroller),
    // and Qt discards a button-less move for a widget that does not - without
    // this the stray move below would never be delivered.
    stray.viewport()->setMouseTracking(true);
    DragHandle *strayHandle = nullptr;
    addHandleRow(&stray, &strayHandle);
    DragHandle *straySecondHandle = nullptr;
    addHandleRow(&stray, &straySecondHandle);
    stray.show();
    QApplication::processEvents();

    auto *strayController = new ItemViewReorderController(&stray);
    QList<int> strayRequested;
    QObject::connect(strayController, &ItemViewReorderController::dragRequested,
                     [&strayRequested](const int row) { strayRequested.append(row); });

    const QPoint strayPress = stray.visualItemRect(stray.item(0)).center();
    sendMouseAt(strayHandle, stray.viewport(), QEvent::MouseButtonPress, strayPress, Qt::LeftButton,
                Qt::LeftButton);
    sendMouseAt(strayHandle, stray.viewport(), QEvent::MouseMove,
                strayPress + QPoint(dragDistance, 0), Qt::NoButton, Qt::LeftButton);
    check(strayRequested == QList<int>{0}, "the stray-test gesture requests a drag of its row");

    // No release follows: the real drag loop never delivers one. The stray
    // move is aimed at the viewport, not the handle: the platform delivers a
    // button-less move to the deepest widget that tracks the mouse, and Qt
    // discards it for widgets that do not (handles do not track) - in the real
    // view the move reaches the handle's controller through the viewport.
    sendMouseAt(stray.viewport(), stray.viewport(), QEvent::MouseMove,
                strayPress + QPoint(dragDistance + 40, 0), Qt::NoButton, Qt::NoButton);
    check(strayRequested == QList<int>{0}, "a stray move after the drag does not re-arm it");

    std::printf("%s (%d failure(s))\n", g_failures == 0 ? "ALL OK" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
