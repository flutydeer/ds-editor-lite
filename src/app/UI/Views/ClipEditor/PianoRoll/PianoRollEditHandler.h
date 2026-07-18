#ifndef PIANOROLLEDITHANDLER_H
#define PIANOROLLEDITHANDLER_H

#include <QtGlobal>
#include <QtCore/qnamespace.h>

class QKeyEvent;
class QMouseEvent;
class QHoverEvent;
class QContextMenuEvent;
class QPoint;
class PianoRollGraphicsView;
class PianoRollGraphicsViewPrivate;

class PianoRollEditHandler {
public:
    virtual ~PianoRollEditHandler() = default;

    void setContext(PianoRollGraphicsView *view, PianoRollGraphicsViewPrivate *d);

    virtual void activate() {
    }

    virtual void deactivate() {
    }

    virtual bool mousePressEvent(QMouseEvent *event) {
        Q_UNUSED(event);
        return true;
    }

    virtual bool mouseMoveEvent(QMouseEvent *event) {
        Q_UNUSED(event);
        return true;
    }

    virtual bool mouseReleaseEvent(QMouseEvent *event) {
        Q_UNUSED(event);
        return true;
    }

    virtual void mouseDoubleClickEvent(QMouseEvent *event) {
        Q_UNUSED(event);
    }

    virtual void hoverEnterEvent(QHoverEvent *event) {
        Q_UNUSED(event);
    }

    virtual void hoverLeaveEvent(QHoverEvent *event) {
        Q_UNUSED(event);
    }

    virtual void hoverMoveEvent(QHoverEvent *event) {
        Q_UNUSED(event);
    }

    virtual void contextMenuEvent(QContextMenuEvent *event) {
        Q_UNUSED(event);
    }

    virtual bool keyPressEvent(QKeyEvent *event) {
        Q_UNUSED(event);
        return false;
    }

    virtual void commit() {
    }

    virtual void discard() {
    }

    // Edge auto scroll support. Return the axes the current drag may scroll
    // along ({} = auto scroll disabled) and continue the drag at the given
    // (clamped) viewport position on each scroll frame.
    [[nodiscard]] virtual Qt::Orientations edgeAutoScrollAxes() const {
        return {};
    }

    virtual void continueDragAt(const QPoint &viewportPos) {
        Q_UNUSED(viewportPos);
    }

protected:
    PianoRollGraphicsView *q = nullptr;
    PianoRollGraphicsViewPrivate *d = nullptr;
};

inline void PianoRollEditHandler::setContext(PianoRollGraphicsView *view,
                                             PianoRollGraphicsViewPrivate *dp) {
    q = view;
    d = dp;
}

#endif // PIANOROLLEDITHANDLER_H
