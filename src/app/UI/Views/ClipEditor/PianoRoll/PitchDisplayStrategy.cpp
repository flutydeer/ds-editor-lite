#include "PitchDisplayStrategy.h"

#include "EditPitchAnchorHandler.h"

#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>

#include <QSet>

#include <algorithm>

namespace {
    void appendNodeCoverage(QList<PitchDisplayInterval> &result,
                            const QList<AnchorNode *> &nodes) {
        if (nodes.size() < 2)
            return;
        result.append({static_cast<double>(nodes.first()->pos()),
                       static_cast<double>(nodes.last()->pos())});
    }

    QList<AnchorNode *> sourceNodesForDisplay(AnchorCurve *curve,
                                               const AnchorOverlayState &state) {
        auto nodes = curve->nodes().toList();
        if (!state.dragging)
            return nodes;

        for (const auto &info : state.dragNodeInfos) {
            if (info.sourceCurve == curve && info.targetCurve)
                nodes.removeOne(info.node);
        }
        return nodes;
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

QList<PitchDisplayInterval>
PitchDisplayStrategy::drawCurveCoverage(const QList<DrawCurve *> &curves) {
    QList<PitchDisplayInterval> result;
    for (const auto *curve : curves) {
        if (!curve || curve->values().size() < 2)
            continue;
        result.append({static_cast<double>(curve->localStart()),
                       static_cast<double>(curve->localEndTick())});
    }
    return normalized(result);
}

QList<PitchDisplayInterval>
PitchDisplayStrategy::anchorCoverage(const AnchorOverlayState &state) {
    QList<PitchDisplayInterval> result;
    for (auto *curve : state.visibleCurves) {
        if (curve)
            appendNodeCoverage(result, sourceNodesForDisplay(curve, state));
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
                    auto it = std::lower_bound(
                        nodes.begin(), nodes.end(), info.node,
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
