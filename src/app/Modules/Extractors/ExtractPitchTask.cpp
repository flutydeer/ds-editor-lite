#include "ExtractPitchTask.h"

#include <cmath>

#include <otter/Api/F0/1/F0ApiL1.h>

#include <lite/Support/MathUtils.h>

namespace F0 = otter::Api::F0::L1;

ExtractPitchTask::ExtractPitchTask(Input input) : ExtractTask(std::move(input)) {
    TaskStatus status;
    status.title = tr("Extract Pitch");
    status.message =
        tr("Pending infer: %1")
            .arg(m_input.displayAudioPath.isEmpty() ? m_input.audioPath : m_input.displayAudioPath);
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
            // The analyser says which frames are voiced. With interpolateUnvoiced on, an unvoiced
            // frame carries an interpolated frequency rather than zero, and reading that as a
            // pitch would draw a line where the singer was silent. An analyser that reports no
            // voicing is read the old way, zero meaning unvoiced.
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
    // Zero means the frame carried no pitch, and stays zero: the curve's consumers read it as
    // "nothing here" rather than as a note.
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

    // Every point is converted through the timeline rather than one tempo being applied to the
    // whole span. On a piece whose tempo changes, a single conversion drifts further the longer
    // the take runs, and the curve ends up somewhere else than the notes it describes.
    const double firstGlobalTick = m_input.timeline.msToTick(overlapStartMs);
    const double lastGlobalTick = m_input.timeline.msToTick(overlapEndMs);
    const int firstLocalGrid = qCeil((firstGlobalTick - m_input.singingClipStartTick) / 5.0) * 5;
    const int lastLocalGrid = qFloor((lastGlobalTick - m_input.singingClipStartTick) / 5.0) * 5;
    if (lastLocalGrid < firstLocalGrid)
        return result;

    QList<double> targetPositions;
    targetPositions.reserve((lastLocalGrid - firstLocalGrid) / 5 + 1);
    for (int localTick = firstLocalGrid; localTick <= lastLocalGrid; localTick += 5) {
        targetPositions.append(m_input.timeline.tickToMs(m_input.singingClipStartTick + localTick));
    }

    result.globalStartTick = m_input.singingClipStartTick + firstLocalGrid;
    result.values = MathUtils::resample(values, sourcePositions, targetPositions);
    return result;
}
