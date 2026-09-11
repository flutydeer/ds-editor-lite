#include "ExtractMidiTask.h"

#include "AnalysisAudio.h"
#include "AudioSlicer.h"

#include <utility>

#include <QDebug>
#include <QFile>
#include <QMutexLocker>
#include <QScopeGuard>

#include <TalcsFormat/AudioFormatIO.h>

#include <otter/Api/Note/1/NoteApiL1.h>

#include <lite/Support/StringUtils.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

namespace Note = otter::Api::Note::L1;

ExtractMidiTask::ExtractMidiTask(Input input) : ExtractTask(std::move(input)) {
    TaskStatus status;
    status.title = tr("Extract MIDI");
    status.message = tr("Pending infer: %1")
                         .arg(m_input.displayAudioPath.isEmpty() ? m_input.audioPath
                                                                : m_input.displayAudioPath);
    setStatus(status);
}

void ExtractMidiTask::runTask() {
    const auto terminateTask = [this] {
        m_errorCode = ErrorCode::Terminated;
        m_errorMessage = tr("Task terminated.");
    };

    auto newStatus = status();
    newStatus.message = tr("Loading model, please wait...");
    newStatus.isIndetermine = true;
    setStatus(newStatus);

    auto created = SynthrtEngine::instance().createAnalyzer(m_input.analyzer);
    if (!created) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("Note analyzer unavailable: ") +
                         QString::fromStdString(created.error().toString());
        qCritical().noquote() << errorMessage();
        return;
    }
    auto analyzer = created.take();
    auto *notes = dynamic_cast<Note::NoteExecutive *>(analyzer.get());
    if (notes == nullptr) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("The chosen analyzer does not transcribe notes");
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

    const auto *spec = SynthrtEngine::instance().analyzerSpec(m_input.analyzer);
    const auto *schema = spec ? spec->exports()->as<Note::NoteSchema>() : nullptr;
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

    const Extractors::SlicingProfile slicer;
    const auto spans = slicer.slice(prepared->samples, prepared->sampleRate,
                                    schema->maxSegmentDuration);

    m_errorCode = ErrorCode::Success;
    m_errorMessage = tr("Successfully extracted midi.");
    for (qsizetype index = 0; index < static_cast<qsizetype>(spans.size()); ++index) {
        if (isTerminateRequested()) {
            terminateTask();
            result.clear();
            return;
        }
        const auto &span = spans[index];

        Note::NoteStartInput input;
        input.audio.sampleRate = prepared->sampleRate;
        input.audio.channelCount = 1;
        input.audio.samples.assign(prepared->samples.begin() + span.begin,
                                   prepared->samples.begin() + span.end);
        input.audio.startTime =
            prepared->startMs / 1000.0 + static_cast<double>(span.begin) / prepared->sampleRate;
        if (!m_input.language.isEmpty()) {
            input.language = m_input.language.toStdString();
        }
        const auto base = static_cast<double>(index) / static_cast<double>(spans.size());
        const auto share = 1.0 / static_cast<double>(spans.size());
        input.progress = [this, base = base, share = share](double fraction) {
            auto progressStatus = status();
            progressStatus.progress = static_cast<int>((base + fraction * share) * 100);
            setStatus(progressStatus);
            return !isTerminateRequested();
        };

        auto transcribed = notes->start(input);
        if (!transcribed) {
            if (isTerminateRequested()) {
                terminateTask();
                result.clear();
                return;
            }
            m_errorCode = ErrorCode::ModelRunFailed;
            m_errorMessage = tr("The note analyzer failed. Reason: ") +
                             QString::fromStdString(transcribed.error().toString());
            qCritical().noquote() << "Error:" << errorMessage();
            return;
        }

        // Seconds to ticks, note by note through the timeline. The older line asked the timeline
        // for one tempo and applied it to the whole take, which drifts further the longer the
        // take runs on a piece whose tempo changes -- and the notes then sit next to the audio
        // rather than on it.
        const auto transcription = transcribed.take();
        result.reserve(result.size() + transcription->notes.size());
        for (const auto &note : transcription->notes) {
            const auto startMs = m_input.audioMaterialOriginMs + note.start * 1000.0;
            const auto endMs = startMs + note.duration * 1000.0;
            const auto startTick = m_input.timeline.msToTick(startMs);
            const auto endTick = m_input.timeline.msToTick(endMs);
            result.push_back({note.key, static_cast<int>(qRound(startTick)),
                              static_cast<int>(qRound(endTick - startTick))});
        }
    }
}

void ExtractMidiTask::terminate() {
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
