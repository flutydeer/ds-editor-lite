#ifndef WAVEFORMRHIUTILS_H
#define WAVEFORMRHIUTILS_H

#include "UI/Utils/WaveformRenderUtils.h"

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QVector>

struct EditorRhiSolidVertex;

// RHI counterpart of WaveformRenderUtils::renderWaveform. The QGraphics
// backends paint the sampled waveform with QPainter, the RHI backends have to
// build the same shapes as vertex triangles, so both renderers share one
// sampler and only differ in how the geometry is emitted.
namespace WaveformRhiUtils {
    // `physicalRect` is the waveform area in device pixels, `physicalCameraOffset`
    // is the offset the vertex shader subtracts from every vertex.
    void appendWaveform(QVector<EditorRhiSolidVertex> &vertices,
                        const WaveformRenderUtils::SampledWaveform &waveform,
                        const QRectF &physicalRect, const QColor &color, double devicePixelRatio,
                        const QPointF &physicalCameraOffset);
}

#endif // WAVEFORMRHIUTILS_H
