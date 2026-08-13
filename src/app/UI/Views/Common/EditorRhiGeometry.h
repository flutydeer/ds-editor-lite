#ifndef EDITORRHIGEOMETRY_H
#define EDITORRHIGEOMETRY_H

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QVector>

struct EditorRhiSolidVertex {
    float x = 0.0f;
    float y = 0.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
    float coverage = 1.0f;
};

namespace EditorRhiGeometry {
    void appendRect(QVector<EditorRhiSolidVertex> &vertices, const QRectF &physicalRect,
                    const QColor &color, float coverage = 1.0f);
    void appendClippedTriangles(QVector<EditorRhiSolidVertex> &vertices,
                                const QVector<EditorRhiSolidVertex> &triangles,
                                const QRectF &physicalClipRect);
    void appendRoundedRect(QVector<EditorRhiSolidVertex> &vertices, const QRectF &physicalRect,
                           double radius, const QColor &color);
    void appendAntialiasedCircle(QVector<EditorRhiSolidVertex> &vertices,
                                 const QPointF &physicalCenter, double physicalRadius,
                                 const QColor &color);
    void appendTopRoundedRect(QVector<EditorRhiSolidVertex> &vertices, const QRectF &physicalRect,
                              double radius, const QColor &color);
    void appendRoundedRectStroke(QVector<EditorRhiSolidVertex> &vertices,
                                 const QRectF &physicalRect, double radius, double width,
                                 const QColor &color, double feather = 1.0);
    void appendPixelAlignedVerticalLine(QVector<EditorRhiSolidVertex> &vertices, double physicalX,
                                        double top, double bottom, const QColor &color);
    void appendPixelAlignedHorizontalLine(QVector<EditorRhiSolidVertex> &vertices, double physicalY,
                                          double left, double right, const QColor &color);
    void appendAntialiasedVerticalLine(QVector<EditorRhiSolidVertex> &vertices, double physicalX,
                                       double top, double bottom, double physicalWidth,
                                       const QColor &color, double physicalCameraX = 0.0);
    void appendAntialiasedHorizontalLine(QVector<EditorRhiSolidVertex> &vertices, double physicalY,
                                         double left, double right, double physicalWidth,
                                         const QColor &color, double physicalCameraY = 0.0);
    void appendAntialiasedStroke(QVector<EditorRhiSolidVertex> &vertices,
                                 const QVector<QPointF> &physicalPoints, double width,
                                 const QColor &color, double feather = 1.0,
                                 double miterLimit = 3.0);
    void appendAntialiasedHairline(QVector<EditorRhiSolidVertex> &vertices,
                                   const QVector<QPointF> &physicalPoints, const QColor &color,
                                   double feather = 1.0, double miterLimit = 3.0);
    void appendAntialiasedWaveform(QVector<EditorRhiSolidVertex> &vertices,
                                   const QVector<QPointF> &physicalTop,
                                   const QVector<QPointF> &physicalBottom,
                                   const QRectF &physicalClipRect, const QColor &color,
                                   double feather = 1.0);
}

#endif // EDITORRHIGEOMETRY_H
