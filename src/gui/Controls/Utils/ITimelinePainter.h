//
// Created by fluty on 2024/2/3.
//

#ifndef TIMELINEPAINTER_H
#define TIMELINEPAINTER_H

#include <QPainter>

class ITimelinePainter {
public:
    void drawTimeline(QPainter *painter, double startTick, double endTick, int numerator,
                      int denominator, int pixelsPerQuarterNote, double scaleX);
    virtual void drawBar(QPainter *painter, int tick, int bar) = 0;
    virtual void drawBeat(QPainter *painter, int tick, int bar, int beat) = 0;
    virtual void drawEighth(QPainter *painter, int tick) = 0;
    virtual ~ITimelinePainter() = default;

private:
    int m_minimumSpacing = 24;
};



#endif // TIMELINEPAINTER_H
