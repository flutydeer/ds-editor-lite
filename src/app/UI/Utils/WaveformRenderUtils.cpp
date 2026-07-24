#include "WaveformRenderUtils.h"

#include <QLineF>
#include <QPainter>
#include <QPolygonF>
#include <QtMath>

#include <cmath>

namespace WaveformRenderUtils {

void renderWaveform(QPainter *painter, const QColor &color, const Mode mode,
                    const QVector<PeakPoint> &peaks) {
    if (peaks.isEmpty())
        return;

    if (mode == FilledMode) {
        // --- Filled polygon rendering ---
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);

        // Build polygon: top edge (max) left→right, then bottom edge (min) right→left
        QPolygonF polygon;
        polygon.reserve(peaks.size() * 2 + 1);
        for (const auto &p : peaks)
            polygon << QPointF(p.x, p.yMax);
        for (int i = peaks.size() - 1; i >= 0; i--)
            polygon << QPointF(peaks[i].x, peaks[i].yMin);
        polygon << polygon.first();
        painter->drawPolygon(polygon);

        // Zero-dynamic sections: draw horizontal lines where min ≈ max
        // so silent/constant sections are visible as a thin centre line
        QPen linePen;
        linePen.setColor(color);
        linePen.setWidthF(0);
        painter->setPen(linePen);
        painter->setBrush(Qt::NoPen);
        painter->setRenderHint(QPainter::Antialiasing, false);

        int runStart = -1;
        for (int i = 0; i < peaks.size(); i++) {
            const bool isFlat = std::abs(peaks[i].yMax - peaks[i].yMin) < 0.5;
            if (isFlat && runStart < 0)
                runStart = i;
            else if (!isFlat && runStart >= 0) {
                painter->drawLine(QPointF(peaks[runStart].x, peaks[runStart].yMax),
                                  QPointF(peaks[i - 1].x, peaks[i - 1].yMax));
                runStart = -1;
            }
        }
        if (runStart >= 0) {
            painter->drawLine(QPointF(peaks[runStart].x, peaks[runStart].yMax),
                              QPointF(peaks.last().x, peaks.last().yMax));
        }
    } else {
        // --- Line (vertical strokes) rendering ---
        painter->setRenderHint(QPainter::Antialiasing, false);
        QPen pen;
        pen.setColor(color);
        pen.setWidthF(0);
        painter->setPen(pen);
        painter->setBrush(Qt::NoPen);

        QVector<QLineF> lines;
        lines.reserve(peaks.size());
        for (const auto &p : peaks)
            lines.append(QLineF(p.x, p.yMin, p.x, p.yMax));
        painter->drawLines(lines);
    }
}

} // namespace WaveformRenderUtils