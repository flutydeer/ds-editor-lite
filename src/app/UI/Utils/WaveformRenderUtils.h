#ifndef WAVEFORMRENDERUTILS_H
#define WAVEFORMRENDERUTILS_H

#include <QColor>
#include <QPointF>
#include <QVector>

class QPainter;
class QTransform;

namespace WaveformRenderUtils {

    enum Mode {
        LineMode,
        FilledMode,
    };

    enum class Geometry { None, FilledPeaks, VerticalPeaks, Curve };

    enum class AmplitudeScale { Linear, Logarithmic };

    struct PeakPoint {
        double x;
        double yMin;
        double yMax;
    };

    struct SampledWaveform {
        Geometry geometry = Geometry::None;
        QVector<PeakPoint> peaks;
        QVector<QPointF> curve;
        QVector<QPointF> sampleDots;
        double sampleDotRadius = 0.0;
    };

    [[nodiscard]] double mapAmplitude(double value, AmplitudeScale scale);

    /// Render a waveform from a vector of per-pixel peak points.
    /// Supports two modes:
    ///   - LineMode:   vertical lines (fast, original behaviour)
    ///   - FilledMode: filled polygon + zero-dynamic horizontal lines (nicer, slower)
    /// The caller is responsible for setting up the painter's pen/brush/antialiasing
    /// before calling this function — this function manages its own rendering state.
    void renderWaveform(QPainter *painter, const QColor &color, Mode mode,
                        const QVector<PeakPoint> &peaks);

    void renderWaveform(QPainter *painter, const QColor &color, Mode mode,
                        const SampledWaveform &waveform);
    void renderWaveform(QPainter *painter, const QColor &color, Mode mode,
                        const SampledWaveform &waveform, const QTransform &transform);

} // namespace WaveformRenderUtils

#endif // WAVEFORMRENDERUTILS_H
