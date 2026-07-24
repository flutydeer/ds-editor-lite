#ifndef WAVEFORMRENDERUTILS_H
#define WAVEFORMRENDERUTILS_H

#include <QColor>
#include <QVector>

class QPainter;

namespace WaveformRenderUtils {

enum Mode {
    LineMode,
    FilledMode,
};

struct PeakPoint {
    double x;
    double yMin;
    double yMax;
};

/// Render a waveform from a vector of per-pixel peak points.
/// Supports two modes:
///   - LineMode:   vertical lines (fast, original behaviour)
///   - FilledMode: filled polygon + zero-dynamic horizontal lines (nicer, slower)
/// The caller is responsible for setting up the painter's pen/brush/antialiasing
/// before calling this function — this function manages its own rendering state.
void renderWaveform(QPainter *painter, const QColor &color, Mode mode,
                    const QVector<PeakPoint> &peaks);

} // namespace WaveformRenderUtils

#endif // WAVEFORMRENDERUTILS_H