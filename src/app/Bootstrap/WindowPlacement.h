#ifndef WINDOWPLACEMENT_H
#define WINDOWPLACEMENT_H

#include <QApplication>
#include <QCursor>
#include <QScreen>
#include <QWidget>

namespace WindowPlacement {

    // Centers the widget on the screen under the mouse cursor,
    // falling back to the primary screen.
    inline void centerOnScreenAtCursor(QWidget &widget) {
        auto scr = QApplication::screenAt(QCursor::pos());
        if (!scr) {
            scr = QApplication::primaryScreen();
        }
        if (scr) {
            auto availableRect = scr->availableGeometry();
            auto left = (availableRect.width() - widget.width()) / 2;
            auto top = (availableRect.height() - widget.height()) / 2;
            widget.move(left, top);
        }
    }

} // namespace WindowPlacement

#endif // WINDOWPLACEMENT_H
