#ifndef INTERVALSELECTHANDLER_H
#define INTERVALSELECTHANDLER_H

#include "PianoRollEditHandler.h"

class IntervalSelectHandler : public PianoRollEditHandler {
public:
    IntervalSelectHandler();
    ~IntervalSelectHandler() override;

    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;
    bool mouseReleaseEvent(QMouseEvent *event) override;
};

#endif // INTERVALSELECTHANDLER_H
