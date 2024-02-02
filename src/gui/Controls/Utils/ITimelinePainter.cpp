//
// Created by fluty on 2024/2/3.
//

#include "ITimelinePainter.h"

void ITimelinePainter::drawTimeline(QPainter *painter, double startTick, double endTick,
                                    int numerator, int denominator, int pixelsPerQuarterNote, double scaleX) {
    bool canDrawEighthLine = 240 * scaleX * pixelsPerQuarterNote / 480 >= m_minimumSpacing;
    bool canDrawQuarterLine = scaleX * pixelsPerQuarterNote >= m_minimumSpacing;
    int barTicks = 1920 * numerator / denominator;
    int beatTicks = 1920 / denominator;
    int oneBarLength = barTicks * scaleX * pixelsPerQuarterNote / 480;
    bool canDrawWholeLine = oneBarLength >= m_minimumSpacing;

    auto drawBarBeatEighthLines = [&] {
        auto prevLineTick = static_cast<int>(startTick / 240) * 240; // 1/8 quantize
        for (int i = prevLineTick; i <= endTick; i += 240) {
            auto bar = i / barTicks + 1;
            auto beat = (i % barTicks) / beatTicks + 1;
            if (i % barTicks == 0) { // bar
                drawBar(painter,i, bar);
            } else if (i % beatTicks == 0 && canDrawQuarterLine) {
                drawBeat(painter, i, bar, beat);
            } else if (canDrawEighthLine) {
                drawEighth(painter, i);
            }
        }
    };

    auto drawBars = [&] {
        int drawBarHopSize = 1; // bar
        int curLineSpacing = oneBarLength;
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