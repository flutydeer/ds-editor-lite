#ifndef DS_EDITOR_LITE_OVERLAPPINGHANDLERESOLVER_H
#define DS_EDITOR_LITE_OVERLAPPINGHANDLERESOLVER_H

#include <QVector>

namespace OverlappingHandleResolver {
    int resolve(const QVector<double> &positions, int handleIndex, double dragDelta);
}

#endif // DS_EDITOR_LITE_OVERLAPPINGHANDLERESOLVER_H
