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

    virtual void mousePressEvent(QMouseEvent *event) { Q_UNUSED(event); }
    virtual void mouseMoveEvent(QMouseEvent *event) { Q_UNUSED(event); }
    virtual void mouseReleaseEvent(QMouseEvent *event) { Q_UNUSED(event); }
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
