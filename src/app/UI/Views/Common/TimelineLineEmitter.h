#ifndef TIMELINELINEEMITTER_H
#define TIMELINELINEEMITTER_H

#include "UI/Utils/ITimelinePainter.h"

#include <QColor>

#include <functional>

// Turns the shared timeline grid walk into callbacks that a render backend can
// turn into its own primitives. The RHI editors cannot reuse TimeGridView
// because they build vertex buffers instead of painting, so they drive
// ITimelinePainter through this adapter.
class TimelineLineEmitter final : public ITimelinePainter {
public:
    using Callback = std::function<void(int tick, const QColor &color)>;

    // Reports every visible bar, beat and subdivision line in tick order
    void emitLines(const Timeline &timeline, int quantize, double startTick, double endTick,
                   double width, const QColor &barColor, const QColor &beatColor,
                   const QColor &commonColor, Callback callback);
    // Grid step used by snapping code, so the snapped positions match the lines
    // that are actually drawn at the current zoom
    [[nodiscard]] int gridStep(const Timeline &timeline, int quantize, double ticksPerPixel,
                               int atTick);

private:
    void drawBar(QPainter *painter, int tick, int bar) override;
    void drawBeat(QPainter *painter, int tick, int bar, int beat) override;
    void drawSubdivision(QPainter *painter, int tick, int level, int levelCount) override;

    QColor m_barColor;
    QColor m_beatColor;
    QColor m_commonColor;
    Callback m_callback;
};

#endif // TIMELINELINEEMITTER_H
