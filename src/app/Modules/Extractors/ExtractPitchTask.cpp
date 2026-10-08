#include "ExtractPitchTask.h"

#include <cmath>

#include <QtMath>

#include <otter/Api/F0/1/F0ApiL1.h>

#include "Modules/Inference/Models/InferParamCurve.h"
#include "Modules/Inference/Utils/PitchSampling.h"

namespace F0 = otter::Api::F0::L1;

ExtractPitchTask::ExtractPitchTask(Input input) : ExtractTask(std::move(input)) {
    TaskStatus status;
    status.title = tr("Extract Pitch");
    status.message = tr("Pending infer: %1").arg(displayPath());
    setStatus(status);
}

void ExtractPitchTask::runTask() {
    runAnalysis<F0::F0Executive, F0::F0Schema>(
        tr("Pitch"),
        [](F0::F0Executive &f0, const Span &span, const auto &progress) {
            F0::F0StartInput input;
            input.audio.sampleRate = span.sampleRate;
            input.audio.channelCount = 1;
            input.audio.samples.assign(span.samples.begin() + span.begin,
                                       span.samples.begin() + span.end);
            input.audio.startTime = span.startTime;
            input.progress = progress;
            return f0.start(input);
        },
        [this](const F0::F0Result &curve) {
            // The analyzer reports which frames are voiced. If interpolateUnvoiced is enabled, an
            // unvoiced frame contains an interpolated frequency rather than zero, and treating it
            // as pitch would produce a curve where the singer was silent. If the analyzer reports
            // no voicing, a zero frequency indicates an unvoiced frame.
            const bool hasVoicing = curve.voiced.size() == curve.f0.size();
            QList<double> values;
            values.reserve(static_cast<qsizetype>(curve.f0.size()));
            for (std::size_t frame = 0; frame < curve.f0.size(); ++frame) {
                const bool voiced = hasVoicing ? curve.voiced[frame] != 0 : curve.f0[frame] > 0;
                values.append(voiced ? freqToMidi(curve.f0[frame]) : 0.0);
            }
            auto placed =
                placeOnTimeline(values, curve.startTime * 1000.0, curve.interval * 1000.0);
            if (!placed.values.isEmpty()) {
                result.append(std::move(placed));
            }
        });
    if (!success()) {
        result.clear();
    }
}

double ExtractPitchTask::freqToMidi(const double frequency) {
    // Zero indicates a frame without pitch and is preserved, because consumers of the curve treat
    // zero as an unvoiced frame rather than as a note.
    return frequency > 0 ? 69 + 12 * std::log2(frequency / 440.0) : 0;
}

ExtractPitchTask::ResultSegment ExtractPitchTask::placeOnTimeline(const QList<double> &values,
                                                                  const double startMs,
                                                                  const double intervalMs) const {
    ResultSegment result;
    if (values.isEmpty() || intervalMs <= 0)
        return result;

    QList<double> sourcePositions;
    sourcePositions.reserve(values.size());
    for (qsizetype i = 0; i < values.size(); ++i) {
        sourcePositions.append(m_input.audioMaterialOriginMs + startMs + i * intervalMs);
    }

    const double overlapStartMs = qMax(sourcePositions.first(), m_input.audioVisibleStartMs);
    const double overlapEndMs = qMin(sourcePositions.last(), m_input.audioVisibleEndMs);
    if (overlapEndMs < overlapStartMs)
        return result;

    // Every point is converted through the timeline rather than by applying a single tempo to the
    // whole span. If the tempo changes, a single conversion causes a drift that grows with the
    // take length and misaligns the curve with the corresponding notes.
    const double firstGlobalTick = m_input.timeline.msToTick(overlapStartMs);
    const double lastGlobalTick = m_input.timeline.msToTick(overlapEndMs);
    constexpr int step = kParamCurveStepTicks;
    const int firstLocalGrid =
        qCeil((firstGlobalTick - m_input.singingClipStartTick) / static_cast<double>(step)) * step;
    const int lastLocalGrid =
        qFloor((lastGlobalTick - m_input.singingClipStartTick) / static_cast<double>(step)) * step;
    if (lastLocalGrid < firstLocalGrid)
        return result;

    QList<double> targetPositions;
    targetPositions.reserve((lastLocalGrid - firstLocalGrid) / step + 1);
    for (int localTick = firstLocalGrid; localTick <= lastLocalGrid; localTick += step) {
        targetPositions.append(m_input.timeline.tickToMs(m_input.singingClipStartTick + localTick));
    }

    result.globalStartTick = m_input.singingClipStartTick + firstLocalGrid;
    result.values = PitchSampling::resample(values, sourcePositions, targetPositions);
    return result;
}
