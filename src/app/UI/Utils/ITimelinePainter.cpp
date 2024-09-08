//
// Created by fluty on 2024/2/3.
//

#include "ITimelinePainter.h"

void ITimelinePainter::drawTimeline(QPainter *painter, double startTick, double endTick,
                                    double rectWidth) {
    auto ticksPerPixel = (endTick - startTick) / rectWidth;

    bool canDrawEighthLine = m_quantize >= 8 && 240 / ticksPerPixel >= m_minimumSpacing;
    bool canDrawQuarterLine = 480 / ticksPerPixel >= m_minimumSpacing;
    int barTicks = 1920 * m_numerator / m_denominator;
    int beatTicks = 1920 / m_denominator;
    auto oneBarLength = barTicks / ticksPerPixel;
    bool canDrawWholeLine = oneBarLength >= m_minimumSpacing;

    auto drawBarBeatEighthLines = [&] {
        auto prevLineTick = static_cast<int>(startTick / 240) * 240; // 1/8 quantize
        for (int i = prevLineTick; i <= endTick; i += 240) {
            auto bar = i / barTicks + 1;
            auto beat = (i % barTicks) / beatTicks + 1;
            if (i % barTicks == 0) { // bar
                drawBar(painter, i, bar);
            } else if (i % beatTicks == 0 && canDrawQuarterLine) {
                drawBeat(painter, i, bar, beat);
            } else if (canDrawEighthLine) {
                drawEighth(painter, i);
            }
        }
    };

    auto drawBars = [&] {
        int drawBarHopSize = 1; // bar
        auto curLineSpacing = oneBarLength;
        while (curLineSpacing < m_minimumSpacing) {
            drawBarHopSize *= 2;
            curLineSpacing *= 2;
        }
        auto prevLineTick =
            static_cast<int>(startTick / barTicks / drawBarHopSize) * barTicks * drawBarHopSize;
        for (int i = prevLineTick; i <= endTick; i += barTicks * drawBarHopSize) {
            auto bar = i / barTicks + 1;
            drawBar(painter, i, bar);
        }
    };

    if (!canDrawWholeLine)
        drawBars();
    else
        drawBarBeatEighthLines();
}

int ITimelinePainter::pixelsPerQuarterNote() const {
    return m_pixelsPerQuarterNote;
}

void ITimelinePainter::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
}

void ITimelinePainter::setTimeSignature(int numerator, int denominator) {
    m_numerator = numerator;
    m_denominator = denominator;
}

void ITimelinePainter::setQuantize(int quantize) {
    m_quantize = quantize;
}

int ITimelinePainter::denominator() const {
    return m_denominator;
}