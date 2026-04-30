#ifndef PIANOROLLEDITHANDLER_H
#define PIANOROLLEDITHANDLER_H

#include <QtGlobal>

class QMouseEvent;
class QHoverEvent;
class QContextMenuEvent;
class PianoRollGraphicsView;
class PianoRollGraphicsViewPrivate;

class PianoRollEditHandler {
public:
    virtual ~PianoRollEditHandler() = default;

    void setContext(PianoRollGraphicsView *view, PianoRollGraphicsViewPrivate *d);

    virtual void activate() {}
    virtual void deactivate() {}

    virtual bool mousePressEvent(QMouseEvent *event) { Q_UNUSED(event); return true; }
    virtual bool mouseMoveEvent(QMouseEvent *event) { Q_UNUSED(event); return true; }
    virtual bool mouseReleaseEvent(QMouseEvent *event) { Q_UNUSED(event); return true; }
    virtual void mouseDoubleClickEvent(QMouseEvent *event) { Q_UNUSED(event); }
    virtual void hoverMoveEvent(QHoverEvent *event) { Q_UNUSED(event); }
    virtual void contextMenuEvent(QContextMenuEvent *event) { Q_UNUSED(event); }

    virtual void commit() {}
    virtual void discard() {}

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
