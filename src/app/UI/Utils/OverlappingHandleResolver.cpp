#include "OverlappingHandleResolver.h"

#include <QtGlobal>

int OverlappingHandleResolver::resolve(const QVector<double> &positions, const int handleIndex,
                                       const double dragDelta) {
    if (handleIndex < 0 || handleIndex >= positions.size() || qFuzzyIsNull(dragDelta))
        return handleIndex;

    int first = handleIndex;
    while (first > 0 && qFuzzyCompare(positions.at(first - 1) + 1.0,
                                     positions.at(handleIndex) + 1.0))
        --first;

    int last = handleIndex;
    while (last + 1 < positions.size() &&
           qFuzzyCompare(positions.at(last + 1) + 1.0, positions.at(handleIndex) + 1.0))
        ++last;

    return dragDelta < 0 ? first : last;
}
