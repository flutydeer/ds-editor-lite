//
// Created by fluty on 2024/2/3.
//

#ifndef TIMELINEPAINTER_H
#define TIMELINEPAINTER_H

#include <lite/MusicBase/Timeline.h>

class QPainter;

// Shared grid painting skeleton for the ruler and the piano roll background.
// Iterates measure by measure over the project timeline so that bar, beat and
// subdivision lines stay correct when the time signature changes mid-project.
class ITimelinePainter {
public:
    void setPixelsPerQuarterNote(int px);
    void setTimeline(const Timeline &timeline);
    virtual void setQuantize(int quantize);

protected:
    void drawTimeline(QPainter *painter, double startTick, double endTick, double rectWidth);
    // The logical grid step near `atTick` (the measure there decides beat and
    // bar lengths). Used by snapping code to match the visible grid density.
    [[nodiscard]] int logicalGridStepForScale(double ticksPerPixel, int atTick = 0) const;
    [[nodiscard]] int pixelsPerQuarterNote() const;
    [[nodiscard]] const Timeline &timeline() const;
    virtual void drawBar(QPainter *painter, int tick, int bar) = 0;
    virtual void drawBeat(QPainter *painter, int tick, int bar, int beat) = 0;
    virtual void drawSubdivision(QPainter *painter, int tick, int level, int levelCount) = 0;
    virtual ~ITimelinePainter() = default;

private:
    Timeline m_timeline;
    int m_minimumSpacing = 24;
    int m_pixelsPerQuarterNote = 64;
    int m_quantize = 16;
};



#endif // TIMELINEPAINTER_H
