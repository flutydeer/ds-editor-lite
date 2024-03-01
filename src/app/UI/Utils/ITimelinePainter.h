//
// Created by fluty on 2024/2/3.
//

#ifndef TIMELINEPAINTER_H
#define TIMELINEPAINTER_H

class QPainter;

class ITimelinePainter {
public:
    void setPixelsPerQuarterNote(int px);
    virtual void setTimeSignature(int numerator, int denominator);
    virtual void setQuantize(int quantize);

protected:
    void drawTimeline(QPainter *painter, double startTick, double endTick, double rectWidth);
    [[nodiscard]] int pixelsPerQuarterNote() const;
    virtual void drawBar(QPainter *painter, int tick, int bar) = 0;
    virtual void drawBeat(QPainter *painter, int tick, int bar, int beat) = 0;
    virtual void drawEighth(QPainter *painter, int tick) = 0;
    virtual ~ITimelinePainter() = default;

private:
    int m_minimumSpacing = 24;
    int m_pixelsPerQuarterNote = 64;
    int m_numerator = 4;
    int m_denominator = 4;
    int m_quantize = 16;
};



#endif // TIMELINEPAINTER_H
