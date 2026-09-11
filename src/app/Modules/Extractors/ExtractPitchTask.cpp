#include "ExtractPitchTask.h"

#include "AnalysisAudio.h"
#include "AudioSlicer.h"

#include <cmath>
#include <utility>

#include <QDebug>
#include <QMutexLocker>
#include <QScopeGuard>

#include <TalcsFormat/AudioFormatIO.h>

#include <QFile>

#include <otter/Api/F0/1/F0ApiL1.h>

#include <lite/Support/MathUtils.h>
#include <lite/Support/StringUtils.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

namespace F0 = otter::Api::F0::L1;

ExtractPitchTask::ExtractPitchTask(Input input) : ExtractTask(std::move(input)) {
    TaskStatus status;
    status.title = tr("Extract Pitch");
    status.message = tr("Pending infer: %1")
                         .arg(m_input.displayAudioPath.isEmpty() ? m_input.audioPath
                                                                : m_input.displayAudioPath);
    setStatus(status);
}

void ExtractPitchTask::runTask() {
    const auto terminateTask = [this] {
        m_errorCode = ErrorCode::Terminated;
        m_errorMessage = tr("Task terminated.");
    };

    auto newStatus = status();
    newStatus.message = tr("Loading model, please wait...");
    newStatus.isIndetermine = true;
    setStatus(newStatus);

    // The analyzer is named rather than found on disk: it is a contribution of an installed
    // package, and which one to use is a choice the person made and the project stored. A path
    // would move the day the package is reinstalled somewhere else.
    auto created = SynthrtEngine::instance().createAnalyzer(m_input.modelPath);
    if (!created) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("Pitch analyzer unavailable: ") +
                         QString::fromStdString(created.error().toString());
        qCritical().noquote() << errorMessage();
        return;
    }
    auto analyzer = created.take();
    auto *f0 = dynamic_cast<F0::F0Executive *>(analyzer.get());
    if (f0 == nullptr) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("The chosen analyzer does not produce a pitch curve");
        qCritical().noquote() << errorMessage();
        return;
    }
    {
        QMutexLocker locker(&m_analyzerMutex);
        m_analyzer = analyzer.get();
    }
    const auto clearAnalyzer = qScopeGuard([this] {
        QMutexLocker locker(&m_analyzerMutex);
        m_analyzer = nullptr;
    });

    if (isTerminateRequested()) {
        terminateTask();
        return;
    }

    // What the analyzer needs the audio to be. Read from its declaration rather than assumed:
    // the rate and the longest span it accepts differ between analyzers, and this is the only
    // place that can honour both.
    const auto *spec = SynthrtEngine::instance().analyzerSpec(m_input.modelPath);
    const auto *schema = spec ? spec->exports()->as<F0::F0Schema>() : nullptr;
    if (schema == nullptr || schema->sampleRate <= 0) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("The chosen analyzer does not declare an input format");
        qCritical().noquote() << errorMessage();
        return;
    }

    newStatus = status();
    newStatus.message = tr("Running inference: %1")
                            .arg(m_input.displayAudioPath.isEmpty() ? m_input.audioPath
                                                                   : m_input.displayAudioPath);
    newStatus.isIndetermine = false;
    newStatus.maximum = 100;
    newStatus.progress = 0;
    setStatus(newStatus);

    QFile file(m_input.audioPath);
    talcs::AudioFormatIO io(&file);
    QString audioError;
    const auto cancelled = [this] { return isTerminateRequested(); };
    auto prepared = Extractors::prepareAudio(&io, m_input.audioVisibleStartMs,
                                             m_input.audioVisibleEndMs, schema->sampleRate,
                                             cancelled, audioError);
    if (!prepared) {
        if (isTerminateRequested()) {
            terminateTask();
            return;
        }
        m_errorCode = ErrorCode::ModelRunFailed;
        m_errorMessage = audioError;
        qCritical().noquote() << "Error:" << errorMessage();
        return;
    }

    // Slicing is the host's job. The analyzer states the longest span it accepts and nothing
    // more; where to cut is a judgement about this audio, and an analyzer that sliced for itself
    // would be making it on the host's behalf.
    const Extractors::SlicingProfile slicer;
    const auto spans = slicer.slice(prepared->samples, prepared->sampleRate,
                                    schema->maxSegmentDuration);

    m_errorCode = ErrorCode::Success;
    m_errorMessage = tr("Successfully extracted pitch.");
    for (qsizetype index = 0; index < static_cast<qsizetype>(spans.size()); ++index) {
        if (isTerminateRequested()) {
            terminateTask();
            result.clear();
            return;
        }
        const auto &span = spans[index];

        F0::F0StartInput input;
        input.audio.sampleRate = prepared->sampleRate;
        input.audio.channelCount = 1;
        input.audio.samples.assign(prepared->samples.begin() + span.begin,
                                   prepared->samples.begin() + span.end);
        // Seconds from the start of the file, so that what comes back can be put where it
        // belongs without the analyzer being told anything about the project.
        input.audio.startTime =
            prepared->startMs / 1000.0 + static_cast<double>(span.begin) / prepared->sampleRate;
        const auto base = static_cast<double>(index) / static_cast<double>(spans.size());
        const auto share = 1.0 / static_cast<double>(spans.size());
        input.progress = [this, base = base, share = share](double fraction) {
            auto progressStatus = status();
            progressStatus.progress = static_cast<int>((base + fraction * share) * 100);
            setStatus(progressStatus);
            return !isTerminateRequested();
        };

        auto analysed = f0->start(input);
        if (!analysed) {
            if (isTerminateRequested()) {
                terminateTask();
                result.clear();
                return;
            }
            m_errorCode = ErrorCode::ModelRunFailed;
            m_errorMessage = tr("The pitch analyzer failed. Reason: ") +
                             QString::fromStdString(analysed.error().toString());
            qCritical().noquote() << "Error:" << errorMessage();
            return;
        }

        const auto curve = analysed.take();
        QList<double> values;
        values.reserve(static_cast<qsizetype>(curve->f0.size()));
        for (const auto frequency : curve->f0) {
            values.append(freqToMidi(frequency));
        }
        auto placed = placeOnTimeline(values, curve->startTime * 1000.0, curve->interval * 1000.0);
        if (!placed.values.isEmpty()) {
            result.append(std::move(placed));
        }
    }
}

void ExtractPitchTask::terminate() {
    ExtractTask::terminate();

    otter::AnalysisExecutive *analyzer = nullptr;
    {
        QMutexLocker locker(&m_analyzerMutex);
        analyzer = m_analyzer;
    }
    if (analyzer) {
        (void) analyzer->stop();
    }
}

double ExtractPitchTask::freqToMidi(const double frequency) {
    // Zero means the frame carried no pitch, and stays zero: the curve's consumers read it as
    // "nothing here" rather than as a note.
    return frequency > 0 ? 69 + 12 * std::log2(frequency / 440.0) : 0;
}

ExtractPitchTask::ResultSegment
    ExtractPitchTask::placeOnTimeline(const QList<double> &values, const double startMs,
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
