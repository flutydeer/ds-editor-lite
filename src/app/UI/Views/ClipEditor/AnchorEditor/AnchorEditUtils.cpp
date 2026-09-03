#include "AnchorEditUtils.h"

#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/Curve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>

namespace {
    bool isExistingSinglePointDrawCurve(const DrawCurve &replacement,
                                        const QList<Curve *> &existing) {
        for (const auto *curve : existing) {
            if (!curve || curve->type() != Curve::Draw)
                continue;
            const auto *draw = static_cast<const DrawCurve *>(curve);
            if (draw->values().size() == 1 && *draw == replacement)
                return true;
        }
        return false;
    }
}

bool AnchorEditor::isCompleteAnchorCurve(const AnchorCurve *curve) {
    return curve && curve->nodes().toList().size() >= 2;
}

QList<Curve *> AnchorEditor::replaceAnchors(const QList<Curve *> &existing,
                                            const QList<AnchorCurve *> &replacementAnchors) {
    QList<Curve *> result;
    for (const auto *curve : existing) {
        if (curve && curve->type() == Curve::Draw)
            result.append(new DrawCurve(*static_cast<const DrawCurve *>(curve)));
    }
    for (const auto *curve : replacementAnchors) {
        if (isCompleteAnchorCurve(curve))
            result.append(new AnchorCurve(*curve));
    }
    return result;
}

QList<Curve *> AnchorEditor::replaceDrawCurves(const QList<Curve *> &existing,
                                               const QList<DrawCurve *> &replacementDrawCurves) {
    QList<Curve *> result;
    for (const auto *curve : replacementDrawCurves) {
        if (curve && (curve->values().size() >= 2 ||
                      isExistingSinglePointDrawCurve(*curve, existing))) {
            result.append(new DrawCurve(*curve));
        }
    }
    for (const auto *curve : existing) {
        if (curve && curve->type() == Curve::Anchor) {
            const auto *anchor = static_cast<const AnchorCurve *>(curve);
            if (isCompleteAnchorCurve(anchor))
                result.append(new AnchorCurve(*anchor));
        }
    }
    return result;
}
