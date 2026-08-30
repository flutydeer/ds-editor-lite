#include "WaveformRenderUtils.h"

#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QTransform>
#include <QtMath>

#include <cmath>

namespace WaveformRenderUtils {

    double mapAmplitude(const double value, const AmplitudeScale scale) {
        if (scale == AmplitudeScale::Linear || value == 0.0)
            return value;

        constexpr double strength = 15.0;
        static const double normalization = 1.0 / std::log1p(strength);
        return std::copysign(std::log1p(strength * std::abs(value)) * normalization, value);
    }

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

    void renderWaveform(QPainter *painter, const QColor &color, const Mode mode,
                        const SampledWaveform &waveform) {
        if (waveform.geometry == Geometry::FilledPeaks ||
            waveform.geometry == Geometry::VerticalPeaks) {
            renderWaveform(painter, color,
                           waveform.geometry == Geometry::FilledPeaks ? mode : LineMode,
                           waveform.peaks);
            return;
        }

        if (waveform.geometry != Geometry::Curve || waveform.curve.isEmpty())
            return;

        painter->setRenderHint(QPainter::Antialiasing, true);
        QPen pen(color);
        pen.setWidthF(0.0);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        QPainterPath path;
        path.moveTo(waveform.curve.constFirst());
        for (auto index = 1; index < waveform.curve.size(); ++index)
            path.lineTo(waveform.curve[index]);
        painter->drawPath(path);

        if (waveform.sampleDots.isEmpty())
            return;
        painter->setBrush(color);
        painter->setPen(Qt::NoPen);
        for (const auto &point : waveform.sampleDots)
            painter->drawEllipse(point, waveform.sampleDotRadius, waveform.sampleDotRadius);
    }

    void renderWaveform(QPainter *painter, const QColor &color, const Mode mode,
                        const SampledWaveform &waveform, const QTransform &transform) {
        if (transform.isIdentity()) {
            renderWaveform(painter, color, mode, waveform);
            return;
        }

        SampledWaveform mapped;
        mapped.geometry = waveform.geometry;
        mapped.peaks.reserve(waveform.peaks.size());
        for (const auto &point : waveform.peaks) {
            const auto minimum = transform.map(QPointF(point.x, point.yMin));
            const auto maximum = transform.map(QPointF(point.x, point.yMax));
            mapped.peaks.append({minimum.x(), minimum.y(), maximum.y()});
        }
        mapped.curve.reserve(waveform.curve.size());
        for (const auto &point : waveform.curve)
            mapped.curve.append(transform.map(point));
        mapped.sampleDots.reserve(waveform.sampleDots.size());
        for (const auto &point : waveform.sampleDots)
            mapped.sampleDots.append(transform.map(point));
        const auto origin = transform.map(QPointF());
        mapped.sampleDotRadius =
            QLineF(origin, transform.map(QPointF(waveform.sampleDotRadius, 0.0))).length();
        renderWaveform(painter, color, mode, mapped);
    }

} // namespace WaveformRenderUtils
