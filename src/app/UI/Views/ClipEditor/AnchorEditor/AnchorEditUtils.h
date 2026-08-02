#ifndef ANCHOREDITUTILS_H
#define ANCHOREDITUTILS_H

#include <QList>

class AnchorCurve;
class Curve;
class DrawCurve;

namespace AnchorEditor {
    [[nodiscard]] QList<Curve *> replaceAnchors(const QList<Curve *> &existing,
                                                const QList<AnchorCurve *> &replacementAnchors);
    [[nodiscard]] QList<Curve *> replaceDrawCurves(const QList<Curve *> &existing,
                                                   const QList<DrawCurve *> &replacementDrawCurves);
}

#endif // ANCHOREDITUTILS_H
