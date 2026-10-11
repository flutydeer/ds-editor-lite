#include "AnchorEditUtils.h"

#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/Curve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <algorithm>

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

AnchorEditor::AnchorInsertionLayout
    AnchorEditor::anchorInsertionLayout(const QList<AnchorNode *> &nodes, const int position) {
    const auto it =
        std::lower_bound(nodes.cbegin(), nodes.cend(), position,
                         [](const AnchorNode *node, int tick) { return node->pos() < tick; });
    const auto index = it - nodes.cbegin();
    AnchorInsertionLayout result{index, AnchorNode::Hermite, std::nullopt};
    if (index == nodes.size()) {
        result.interpolation = AnchorNode::None;
        if (index > 0) {
            // Terminal tails inherit the preceding segment's interpolation when extended.
            auto mode = nodes.at(index - 1)->interpMode();
            if (mode == AnchorNode::None)
                mode = index > 1 ? nodes.at(index - 2)->interpMode() : AnchorNode::Hermite;
            result.previousInterpolation = mode;
        }
    } else if (index > 0) {
        result.interpolation = nodes.at(index - 1)->interpMode();
    }
    return result;
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
        if (curve &&
            (curve->values().size() >= 2 || isExistingSinglePointDrawCurve(*curve, existing))) {
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
