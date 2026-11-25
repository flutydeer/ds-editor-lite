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
    // Sample positions are in logical scene pixels. Subtract logicalOrigin before tessellation.
    // The clip rect and the shader's camera offset are in the resulting physical coordinates.
    void appendWaveform(QVector<EditorRhiSolidVertex> &vertices,
                        const WaveformRenderUtils::SampledWaveform &waveform,
                        const QRectF &physicalRect, const QColor &color, double devicePixelRatio,
                        const QPointF &physicalCameraOffset, const QPointF &logicalOrigin = {});
}

#endif // WAVEFORMRHIUTILS_H
