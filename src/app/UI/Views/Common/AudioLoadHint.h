#ifndef AUDIOLOADHINT_H
#define AUDIOLOADHINT_H

#include <QPointF>
#include <QRectF>
#include <QVector>

#include <QtGlobal>

// Shape and timing of the rotating ring drawn before the loading text of an audio clip. Both
// render backends read the numbers from here, so the QGraphics scene and the RHI canvas show the
// same hint instead of two hand-tuned copies that drift apart.
namespace AudioLoadHint {
    // Ring diameter in logical pixels, kept a little smaller than the hint line height
    inline constexpr double ringDiameter = 12.0;

    // Stroke width in logical pixels: thin enough to read as a status hint rather than as a
    // button. On a 200% display this lands on four physical pixels.
    inline constexpr double ringStrokeWidth = 2.0;

    // Gap between the ring and the text
    inline constexpr double ringTextGap = 5.0;

    // Alpha the static track behind the moving arc is drawn with
    inline constexpr double trackAlpha = 0.28;

    // Where the ring goes and where the text starts, as one block centred inside labelRect.
    // Everything is in the caller's unit: the RHI backend passes physical pixels by handing in
    // scale == dpr, the QGraphics backend passes logical ones with scale == 1.
    struct Layout {
        QPointF ringCenter;
        double ringRadius = 0.0;
        double textLeft = 0.0;
    };

    [[nodiscard]] Layout layout(const QRectF &labelRect, double textWidth, double scale);

    // Milliseconds since the first call. One clock for every clip and both backends, so all rings
    // turn in step instead of each starting at its own phase.
    [[nodiscard]] qint64 clockMs();

    // Head of the moving arc, in degrees clockwise from twelve o'clock
    [[nodiscard]] double headDegrees(qint64 elapsedMs);
    // Angular length of the moving arc
    [[nodiscard]] double sweepDegrees(qint64 elapsedMs);

    // Polyline along the middle of the stroke, from the tail to the head. Both backends stroke this
    // same line, so the arc has one shape in the painted path and in the GPU path.
    [[nodiscard]] QVector<QPointF> arcCenterline(const QPointF &center, double radius,
                                                 qint64 elapsedMs);
    // The full circle the moving arc runs on
    [[nodiscard]] QVector<QPointF> trackCenterline(const QPointF &center, double radius);
}

#endif // AUDIOLOADHINT_H
