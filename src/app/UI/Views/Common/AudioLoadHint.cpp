#include "AudioLoadHint.h"

#include <QElapsedTimer>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace AudioLoadHint {
    namespace {
        // The moving arc spins at the same rate as the ProgressIndicator control, so a loading clip
        // and a running task in the task view read as the same kind of activity
        constexpr double speedDegreesPerSecond = 270.0;
        constexpr double minSweepDegrees = 30.0;
        constexpr double maxSweepDegrees = 90.0;
        constexpr double pulseHertz = 0.2;
        // One segment every few degrees keeps the small circle round without flooding the vertex
        // buffer of the RHI backend
        constexpr double degreesPerSegment = 6.0;
        constexpr int trackSegments = 60;

        [[nodiscard]] QPointF pointAt(const QPointF &center, const double radius,
                                      const double degreesClockwise) {
            const auto radians = degreesClockwise * std::numbers::pi / 180.0;
            return center + QPointF(radius * std::sin(radians), -radius * std::cos(radians));
        }
    }

    Layout layout(const QRectF &labelRect, const double textWidth, const double scale) {
        const auto diameter = ringDiameter * scale;
        const auto gap = ringTextGap * scale;
        const auto blockWidth = diameter + gap + textWidth;
        const auto left = labelRect.center().x() - blockWidth * 0.5;
        return {
            .ringCenter = QPointF(left + diameter * 0.5, labelRect.center().y()),
            .ringRadius = (diameter - ringStrokeWidth * scale) * 0.5,
            .textLeft = left + diameter + gap,
        };
    }

    qint64 clockMs() {
        static const QElapsedTimer timer = [] {
            QElapsedTimer started;
            started.start();
            return started;
        }();
        return timer.elapsed();
    }

    double headDegrees(const qint64 elapsedMs) {
        const auto seconds = static_cast<double>(elapsedMs) / 1000.0;
        return std::fmod(seconds * speedDegreesPerSecond, 360.0);
    }

    double sweepDegrees(const qint64 elapsedMs) {
        const auto seconds = static_cast<double>(elapsedMs) / 1000.0;
        return minSweepDegrees +
               (maxSweepDegrees - minSweepDegrees) *
                   (0.5 + 0.5 * std::sin(seconds * pulseHertz * 2.0 * std::numbers::pi));
    }

    QVector<QPointF> arcCenterline(const QPointF &center, const double radius,
                                   const qint64 elapsedMs) {
        const auto sweep = sweepDegrees(elapsedMs);
        const auto head = headDegrees(elapsedMs);
        const auto segments = std::max(2, static_cast<int>(std::ceil(sweep / degreesPerSegment)));
        QVector<QPointF> points;
        points.reserve(segments + 1);
        for (int index = 0; index <= segments; ++index) {
            const auto offset = sweep * static_cast<double>(index) / static_cast<double>(segments);
            points.append(pointAt(center, radius, head - sweep + offset));
        }
        return points;
    }

    QVector<QPointF> trackCenterline(const QPointF &center, const double radius) {
        QVector<QPointF> points;
        points.reserve(trackSegments + 1);
        for (int index = 0; index <= trackSegments; ++index) {
            points.append(
                pointAt(center, radius,
                        360.0 * static_cast<double>(index) / static_cast<double>(trackSegments)));
        }
        return points;
    }
}
