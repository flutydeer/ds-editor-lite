#include "PitchDisplayStrategy.h"

#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditController.h"

#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>

#include <QSet>

#include <algorithm>

namespace {
    void appendNodeCoverage(QList<PitchDisplayInterval> &result, const QList<AnchorNode *> &nodes) {
        if (nodes.size() < 2)
            return;
        result.append(
            {static_cast<double>(nodes.first()->pos()), static_cast<double>(nodes.last()->pos())});
    }

    QList<PitchDisplayInterval> normalized(QList<PitchDisplayInterval> intervals) {
        for (auto &interval : intervals) {
            if (interval.endTick < interval.startTick)
                std::swap(interval.startTick, interval.endTick);
        }
        intervals.erase(std::remove_if(intervals.begin(), intervals.end(),
                                       [](const PitchDisplayInterval &interval) {
                                           return interval.endTick <= interval.startTick;
                                       }),
                        intervals.end());
        std::sort(intervals.begin(), intervals.end(),
                  [](const PitchDisplayInterval &left, const PitchDisplayInterval &right) {
                      if (left.startTick != right.startTick)
                          return left.startTick < right.startTick;
                      return left.endTick < right.endTick;
                  });

        QList<PitchDisplayInterval> result;
        for (const auto &interval : intervals) {
            if (result.isEmpty() || interval.startTick > result.last().endTick) {
                result.append(interval);
            } else {
                result.last().endTick = std::max(result.last().endTick, interval.endTick);
            }
        }
        return result;
    }
}

PitchDisplayStrategy::MergedCurveCache::~MergedCurveCache() {
    invalidate();
}

const QList<DrawCurve *> &
    PitchDisplayStrategy::MergedCurveCache::mergedCurves(const QList<DrawCurve *> &originalCurves,
                                                         const QList<DrawCurve *> &editedCurves) {
    if (!m_valid) {
        m_curves = AppModelUtils::mergeCurves(originalCurves, editedCurves);
        m_valid = true;
    }
    return m_curves;
}

void PitchDisplayStrategy::MergedCurveCache::invalidate() {
    qDeleteAll(m_curves);
    m_curves.clear();
    m_valid = false;
}

PitchDisplayMode PitchDisplayStrategy::displayModeForEditMode(
    const EditorViewGlobal::PianoRollEditMode editMode) {
    if (editMode == EditorViewGlobal::DrawPitch || editMode == EditorViewGlobal::ErasePitch ||
        editMode == EditorViewGlobal::FreezePitch) {
        return PitchDisplayMode::Draw;
    }
    if (editMode == EditorViewGlobal::EditPitchAnchor)
        return PitchDisplayMode::Anchor;
    return PitchDisplayMode::Final;
}

QList<PitchDisplayLayer>
    PitchDisplayStrategy::displayLayers(const PitchDisplayMode mode,
                                        const QList<DrawCurve *> &editedCurves,
                                        const QList<PitchDisplayInterval> &anchorCoverage) {
    if (mode == PitchDisplayMode::Final) {
        return {
            {PitchDisplayCurveSource::Merged,
             PitchDisplayColorRole::Edited,
             210, anchorCoverage,
             {}}
        };
    }
    if (mode == PitchDisplayMode::Draw) {
        const auto editedOrAnchorCoverage =
            combineCoverage(drawCurveCoverage(editedCurves), anchorCoverage);
        return {
            {PitchDisplayCurveSource::Original,
             PitchDisplayColorRole::Original,
             255, {},
             editedOrAnchorCoverage},
            {PitchDisplayCurveSource::Edited,
             PitchDisplayColorRole::Edited,
             230, {},
             anchorCoverage        },
        };
    }
    return {
        {PitchDisplayCurveSource::Merged, PitchDisplayColorRole::Edited, 80, {}, anchorCoverage}
    };
}

AnchorDisplayOpacity PitchDisplayStrategy::anchorOpacity(const PitchDisplayMode mode) {
    if (mode == PitchDisplayMode::Final)
        return {220, 180};
    if (mode == PitchDisplayMode::Draw)
        return {80, 60};
    return {255, 200};
}

int PitchDisplayStrategy::anchorPreviewAlpha() {
    return 128;
}

int PitchDisplayStrategy::anchorInteractionPreviewAlpha() {
    return 160;
}

int PitchDisplayStrategy::anchorSelectionFillAlpha() {
    return 64;
}

int PitchDisplayStrategy::anchorSelectionBorderAlpha() {
    return 200;
}

QList<PitchDisplayInterval>
    PitchDisplayStrategy::drawCurveCoverage(const QList<DrawCurve *> &curves) {
    QList<PitchDisplayInterval> result;
    for (const auto *curve : curves) {
        if (!curve || curve->values().size() < 2)
            continue;
        result.append(
            {static_cast<double>(curve->localStart()), static_cast<double>(curve->localEndTick())});
    }
    return normalized(result);
}

QList<AnchorNode *>
    PitchDisplayStrategy::anchorCurveNodes(AnchorCurve *curve,
                                           const AnchorEditor::AnchorOverlayState &state) {
    if (!curve)
        return {};
    auto nodes = curve->nodes().toList();
    if (!state.dragging)
        return nodes;

    for (const auto &info : state.dragNodeInfos) {
        if (info.sourceCurve == curve && info.targetCurve)
            nodes.removeOne(info.node);
    }
    return nodes;
}

QList<PitchDisplayInterval>
    PitchDisplayStrategy::anchorCoverage(const AnchorEditor::AnchorOverlayState &state) {
    QList<PitchDisplayInterval> result;
    for (auto *curve : state.visibleCurves) {
        if (curve)
            appendNodeCoverage(result, anchorCurveNodes(curve, state));
    }

    if (state.dragging) {
        QSet<AnchorCurve *> targets;
        for (const auto &info : state.dragNodeInfos) {
            if (info.targetCurve)
                targets.insert(info.targetCurve);
        }
        for (auto *target : targets) {
            auto nodes = target->nodes().toList();
            for (const auto &info : state.dragNodeInfos) {
                if (info.targetCurve == target) {
                    auto it = std::lower_bound(nodes.begin(), nodes.end(), info.node,
                                               [](const AnchorNode *left, const AnchorNode *right) {
                                                   return left->pos() < right->pos();
                                               });
                    nodes.insert(it, info.node);
                }
            }
            appendNodeCoverage(result, nodes);
        }
    }

    if (state.showMergePreview && state.currentCurve && state.mergeCandidateCurve) {
        auto nodes = state.currentCurve->nodes().toList();
        nodes.append(state.mergeCandidateCurve->nodes().toList());
        std::sort(nodes.begin(), nodes.end(), [](const AnchorNode *left, const AnchorNode *right) {
            return left->pos() < right->pos();
        });
        appendNodeCoverage(result, nodes);
    } else if (state.showPreview && state.previewCurve && state.cursorInView) {
        auto nodes = state.previewCurve->nodes().toList();
        AnchorNode previewNode(state.previewTick, 0);
        auto it = std::lower_bound(nodes.begin(), nodes.end(), &previewNode,
                                   [](const AnchorNode *left, const AnchorNode *right) {
                                       return left->pos() < right->pos();
                                   });
        nodes.insert(it, &previewNode);
        appendNodeCoverage(result, nodes);
    }

    return normalized(result);
}

QList<PitchDisplayInterval>
    PitchDisplayStrategy::combineCoverage(const QList<PitchDisplayInterval> &first,
                                          const QList<PitchDisplayInterval> &second) {
    auto result = first;
    result.append(second);
    return normalized(result);
}
