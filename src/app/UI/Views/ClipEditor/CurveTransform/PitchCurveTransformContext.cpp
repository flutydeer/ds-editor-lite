#include "PitchCurveTransformContext.h"

#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/Models/InferInputBase.h"
#include "Modules/Inference/Utils/BasePitchCurve.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>

namespace {
    int alignedAtOrAfter(const int tick) {
        const auto remainder = ((tick % CurveTransform::SampleStep) + CurveTransform::SampleStep) %
                               CurveTransform::SampleStep;
        return remainder == 0 ? tick : tick + CurveTransform::SampleStep - remainder;
    }

    int alignedAtOrBefore(const int tick) {
        const auto remainder = ((tick % CurveTransform::SampleStep) + CurveTransform::SampleStep) %
                               CurveTransform::SampleStep;
        return tick - remainder;
    }
}

void CurveTransform::PitchContext::rebuild(SingingClip *clip) {
    if (!clip) {
        clear();
        return;
    }
    if (m_clip != clip) {
        clear();
        m_clip = clip;
    }
    if (m_valid && m_clipRevision == clip->inferenceRevision())
        return;

    m_clipRevision = clip->inferenceRevision();
    m_partitions.clear();
    m_pieces.clear();
    QHash<int, CurveCache> nextCurveCache;
    const auto timeline = appModel->timeline();
    for (const auto *piece : clip->pieces()) {
        if (!piece || piece->notes.isEmpty())
            continue;
        const auto input = InferControllerHelper::buildInferBaseInput(*piece, piece->identifier);
        const auto words = InferTaskHelper::buildWords(input, true);
        auto notes = BasePitchCurve::inputNotes(words);
        std::shared_ptr<BasePitchCurve> curve;
        if (const auto cached = m_curveCache.constFind(piece->id());
            cached != m_curveCache.cend() && cached->notes == notes) {
            curve = cached->curve;
        } else {
            curve = std::make_shared<BasePitchCurve>(notes);
        }
        nextCurveCache.insert(piece->id(), {std::move(notes), curve});
        if (curve->isEmpty())
            continue;

        const auto start = alignedAtOrAfter(piece->localStartTick(timeline));
        const auto end = alignedAtOrBefore(piece->localEndTick(timeline) - 1);
        if (end < start)
            continue;
        const Interval interval{start, end};
        m_partitions.append(interval);
        m_pieces.append({interval, timeline.tickToMs(input.pieceStartTick), std::move(curve)});
    }
    m_curveCache = std::move(nextCurveCache);
    m_valid = true;
}

void CurveTransform::PitchContext::invalidate() {
    m_valid = false;
}

void CurveTransform::PitchContext::clear() {
    m_clip.clear();
    m_clipRevision = 0;
    m_valid = false;
    m_partitions.clear();
    m_pieces.clear();
    m_curveCache.clear();
}

const QList<CurveTransform::Interval> &CurveTransform::PitchContext::partitions() const {
    return m_partitions;
}

std::optional<double> CurveTransform::PitchContext::baselineAtTick(const int localTick) const {
    if (!m_clip)
        return std::nullopt;
    for (const auto &piece : m_pieces) {
        if (localTick < piece.interval.startTick)
            break;
        if (localTick > piece.interval.endTick)
            continue;
        const auto globalTick = m_clip->start() + localTick;
        const auto seconds = (appModel->tickToMs(globalTick) - piece.originMs) / 1000.0;
        return piece.curve->SemitoneValueAt(seconds) * 100.0;
    }
    return std::nullopt;
}
