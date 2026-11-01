#include "TimelineLineEmitter.h"

#include <QImage>
#include <QPainter>

#include <algorithm>

namespace {
    QColor blendColors(const QColor &from, const QColor &to, double ratio) {
        ratio = std::clamp(ratio, 0.0, 1.0);
        return QColor(static_cast<int>(from.red() + (to.red() - from.red()) * ratio),
                      static_cast<int>(from.green() + (to.green() - from.green()) * ratio),
                      static_cast<int>(from.blue() + (to.blue() - from.blue()) * ratio));
    }

    QColor withPainterOpacity(const QColor &source, const QPainter *painter) {
        auto color = source;
        color.setAlphaF(color.alphaF() * painter->opacity());
        return color;
    }
}

void TimelineLineEmitter::emitLines(const Timeline &timeline, const int quantize,
                                    const double startTick, const double endTick,
                                    const double width, const QColor &barColor,
                                    const QColor &beatColor, const QColor &commonColor,
                                    Callback callback) {
    setTimeline(timeline);
    setQuantize(quantize);
    m_barColor = barColor;
    m_beatColor = beatColor;
    m_commonColor = commonColor;
    m_callback = std::move(callback);

    // drawTimeline only needs the painter for opacity, so a 1x1 target is
    // enough and avoids allocating a real surface per frame
    QImage target(1, 1, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&target);
    drawTimeline(&painter, startTick, endTick, width);
}

int TimelineLineEmitter::gridStep(const Timeline &timeline, const int quantize,
                                  const double ticksPerPixel, const int atTick) {
    setTimeline(timeline);
    setQuantize(quantize);
    return logicalGridStepForScale(ticksPerPixel, atTick);
}

void TimelineLineEmitter::drawBar(QPainter *painter, const int tick, int) {
    if (m_callback)
        m_callback(tick, withPainterOpacity(m_barColor, painter));
}

void TimelineLineEmitter::drawBeat(QPainter *painter, const int tick, int, int) {
    if (m_callback)
        m_callback(tick, withPainterOpacity(m_beatColor, painter));
}

void TimelineLineEmitter::drawSubdivision(QPainter *painter, const int tick, const int level,
                                          const int levelCount) {
    if (!m_callback)
        return;
    const auto ratio = levelCount > 1 ? static_cast<double>(level) / (levelCount - 1) : 0.0;
    m_callback(tick, withPainterOpacity(blendColors(m_beatColor, m_commonColor, ratio), painter));
}
