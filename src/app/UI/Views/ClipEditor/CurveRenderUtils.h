#ifndef CURVERENDERUTILS_H
#define CURVERENDERUTILS_H

#include <QVector>

class DrawCurve;

struct CurveRenderSamples {
    QVector<int> pointIndices;
    int lastVisitedIndex = -1;
};

namespace CurveRenderUtils {
    [[nodiscard]] CurveRenderSamples sampleCurve(const DrawCurve &curve, double startTick,
                                                 double endTick, double pixelsPerTick,
                                                 double minimumPointDistance);
}

#endif // CURVERENDERUTILS_H
