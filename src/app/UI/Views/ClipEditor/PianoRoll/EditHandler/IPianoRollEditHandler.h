//
// Created by fluty on 24-10-29.
//

#ifndef IPIANOROLLEDITHANDLER_H
#define IPIANOROLLEDITHANDLER_H

#include "Utils/Macros.h"

class PianoRollGraphicsView;
class QMouseEvent;
interface IPianoRollEditHandler {
    I_DECL(IPianoRollEditHandler)
    I_METHOD(void onMouseDown(QMouseEvent *event, PianoRollGraphicsView *view));
    I_METHOD(void onMouseMove(QMouseEvent *event, PianoRollGraphicsView *view));
    I_METHOD(void onMouseUp(QMouseEvent *event, PianoRollGraphicsView *view));
    I_METHOD(void onDiscard(PianoRollGraphicsView *view));
    I_METHOD(void onCommit(PianoRollGraphicsView *view));
};



#endif // IPIANOROLLEDITHANDLER_H
