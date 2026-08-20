#include "AnchorEditUtils.h"

#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/Curve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>

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
        if (curve && curve->values().size() >= 2)
            result.append(new DrawCurve(*curve));
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
