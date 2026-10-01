#ifndef TESTSUPPORT_POINTERINPUT_H
#define TESTSUPPORT_POINTERINPUT_H

#include <QCursor>
#include <QWidget>
#include <QWindow>
#include <QtTest/QTest>

namespace TestSupport {
    inline void hoverWidget(QWidget &surface, const QPoint &position) {
        const auto global = surface.mapToGlobal(position);
        // Cursor warping alone does not always produce a platform mouse move.
        QCursor::setPos(global);
        QCoreApplication::processEvents();
        auto *window = surface.window();
        QTest::mouseMove(window->windowHandle(), window->mapFromGlobal(global));
    }
}

#endif
