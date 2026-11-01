#include "WaveformRhiUtils.h"

#include "UI/Views/Common/EditorRhiGeometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace WaveformRhiUtils {
    void appendWaveform(QVector<EditorRhiSolidVertex> &vertices,
                        const WaveformRenderUtils::SampledWaveform &waveform,
                        const QRectF &physicalRect, const QColor &color,
                        const double devicePixelRatio, const QPointF &physicalCameraOffset) {
        if (waveform.geometry == WaveformRenderUtils::Geometry::None || physicalRect.isEmpty())
            return;

        const auto physicalPoint = [devicePixelRatio](const QPointF &point) {
            return point * devicePixelRatio + QPointF(0.0, -0.5);
        };
        QVector<EditorRhiSolidVertex> waveformVertices;
        auto writtenDirectly = false;
        if (waveform.geometry == WaveformRenderUtils::Geometry::FilledPeaks) {
            QVector<QPointF> top;
            QVector<QPointF> bottom;
            top.reserve(waveform.peaks.size());
            bottom.reserve(waveform.peaks.size());
            for (const auto &point : waveform.peaks) {
                top.append(physicalPoint({point.x, point.yMax}));
                bottom.append(physicalPoint({point.x, point.yMin}));
            }
            EditorRhiGeometry::appendAntialiasedWaveform(waveformVertices, top, bottom,
                                                         physicalRect, color, 0.75);

            // Zero-dynamic sections are drawn as a flat centre line so silent
            // parts stay visible, mirroring the QPainter renderer
            auto flatRunStart = -1;
            for (auto index = 0; index < waveform.peaks.size(); ++index) {
                const auto &point = waveform.peaks[index];
                const auto flat = std::abs(point.yMax - point.yMin) < 0.5;
                if (flat && flatRunStart < 0) {
                    flatRunStart = index;
                } else if (!flat && flatRunStart >= 0) {
                    const auto runStart = physicalPoint(
                        {waveform.peaks[flatRunStart].x, waveform.peaks[flatRunStart].yMax});
                    const auto runEnd = physicalPoint(
                        {waveform.peaks[index - 1].x, waveform.peaks[index - 1].yMax});
                    EditorRhiGeometry::appendPixelAlignedHorizontalLine(
                        waveformVertices, runStart.y(), runStart.x(), runEnd.x(), color);
                    flatRunStart = -1;
                }
            }
            if (flatRunStart >= 0) {
                const auto runStart =
                    physicalPoint({waveform.peaks[flatRunStart].x, waveform.peaks[flatRunStart].yMax});
                const auto runEnd = physicalPoint(
                    {waveform.peaks.constLast().x, waveform.peaks.constLast().yMax});
                EditorRhiGeometry::appendPixelAlignedHorizontalLine(waveformVertices, runStart.y(),
                                                                    runStart.x(), runEnd.x(), color);
            }
        } else if (waveform.geometry == WaveformRenderUtils::Geometry::VerticalPeaks) {
            auto cosmeticLineOffset = 0.0;
            if (!waveform.peaks.isEmpty()) {
                const auto firstPhysicalX = physicalPoint({waveform.peaks.constFirst().x, 0.0}).x();
                const auto firstScreenX = firstPhysicalX - physicalCameraOffset.x();
                const auto pixelPhase = firstScreenX - std::floor(firstScreenX);
                // Preserve QPainter's aliased cosmetic-pen phase after the RHI camera transform.
                cosmeticLineOffset = pixelPhase < 0.5 ? -0.5 : 0.0;
            }
            for (const auto &point : waveform.peaks) {
                const auto top = physicalPoint({point.x, std::min(point.yMin, point.yMax)});
                const auto bottom = physicalPoint({point.x, std::max(point.yMin, point.yMax)});
                const auto lineTop = top.y() - 0.5;
                const auto lineBottom = bottom.y() + 0.5;
                const auto lineX = std::floor(top.x() - physicalCameraOffset.x()) +
                                   physicalCameraOffset.x() + cosmeticLineOffset;
                EditorRhiGeometry::appendRect(
                    waveformVertices,
                    QRectF(lineX, lineTop, 1.0, std::max(1.0, lineBottom - lineTop)), color);
            }
        } else if (waveform.geometry == WaveformRenderUtils::Geometry::Curve) {
            QVector<QPointF> curve;
            curve.reserve(waveform.curve.size());
            QVector<QPointF> dotCenters;
            dotCenters.reserve(waveform.sampleDots.size());
            auto minimumX = std::numeric_limits<double>::max();
            auto maximumX = std::numeric_limits<double>::lowest();
            auto minimumY = std::numeric_limits<double>::max();
            auto maximumY = std::numeric_limits<double>::lowest();
            const auto appendPoint = [&](QVector<QPointF> &points, const QPointF &point) {
                const auto converted = physicalPoint(point);
                points.append(converted);
                minimumX = std::min(minimumX, converted.x());
                maximumX = std::max(maximumX, converted.x());
                minimumY = std::min(minimumY, converted.y());
                maximumY = std::max(maximumY, converted.y());
            };
            for (const auto &point : waveform.curve)
                appendPoint(curve, point);
            for (const auto &point : waveform.sampleDots)
                appendPoint(dotCenters, point);
            const auto radius = waveform.sampleDotRadius * devicePixelRatio;
            const auto hasPoints = !curve.isEmpty() || !dotCenters.isEmpty();
            // Hairline joins use a 3x miter limit; sample dots add a 0.75 px feather.
            const auto geometryMargin = std::max(3.0, radius + 0.75);
            writtenDirectly = hasPoints && minimumX - geometryMargin >= physicalRect.left() &&
                              maximumX + geometryMargin <= physicalRect.right() &&
                              minimumY - geometryMargin >= physicalRect.top() &&
                              maximumY + geometryMargin <= physicalRect.bottom();
            auto &targetVertices = writtenDirectly ? vertices : waveformVertices;
            EditorRhiGeometry::appendAntialiasedHairline(targetVertices, curve, color);
            for (const auto &center : dotCenters)
                EditorRhiGeometry::appendAntialiasedCircle(targetVertices, center, radius, color);
        }
        if (!writtenDirectly)
            EditorRhiGeometry::appendClippedTriangles(vertices, waveformVertices, physicalRect);
    }
}
