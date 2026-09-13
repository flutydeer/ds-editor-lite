#ifndef EXTRACTTASK_H
#define EXTRACTTASK_H

#include <utility>

#include <QDebug>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QScopeGuard>
#include <QString>

#include <TalcsFormat/AudioFormatIO.h>

#include <otter/Analysis/AnalysisExecutive.h>

#include <lite/MusicBase/Timeline.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Tasking/Task.h>

#include "AnalysisAudio.h"
#include "AudioSlicer.h"

/// What the two extraction tasks share: naming the analyser, reading what it needs, preparing
/// and slicing the audio, and driving one span after another with progress and cancellation.
///
/// A task supplies the contract's types and two callables: one that builds the span's input
/// and one that takes the span's result. The pitch and MIDI tasks were once two copies of this
/// with those two places different, and a bug fixed in one had to be fixed in the other.
class ExtractTask : public Task {
    Q_OBJECT

public:
    enum class ErrorCode {
        UnknownError = -2,
        Terminated = -1,
        Success = 0,
        InferEngineNotLoaded = 1,
        ModelNotLoaded = 2,
        ModelRunFailed = 3,
    };

    struct Input {
        int singingClipId = -1;
        int audioClipId = -1;
        QString audioPath;
        QString displayAudioPath;
        /// The analyzer to run, named <package>:analysis/<contribution>.
        QString analyzer;

        /// Which language to transcribe as, when the analyzer distinguishes them. Empty leaves
        /// the choice to the analyzer, which is what a monolingual one does anyway.
        QString language;
        Timeline timeline;
        int singingClipStartTick = 0;
        int audioClipStartTick = 0;
        int audioClipLengthTick = 0;
        double audioMaterialOriginMs = 0;
        double audioVisibleStartMs = 0;
        double audioVisibleEndMs = 0;
    };

    explicit ExtractTask(Input input) : m_input(std::move(input)) {
    }

    const Input &input() const {
        return m_input;
    }

    ErrorCode errorCode() const {
        return m_errorCode;
    }

    QString errorMessage() const {
        return m_errorMessage;
    }

    bool success() const {
        return m_errorCode == ErrorCode::Success;
    }

    void terminate() override {
        Task::terminate();
        // Held across the call: runAnalysis() clears the pointer under this mutex before the
        // executive is destroyed, so a stop that finds the pointer set is talking to a live
        // object.
        QMutexLocker locker(&m_analyzerMutex);
        if (m_analyzer) {
            (void) m_analyzer->stop();
        }
    }

protected:
    /// One span of the prepared audio, in seconds from the start of the file.
    struct Span {
        const std::vector<float> &samples;
        int sampleRate;
        std::size_t begin;
        std::size_t end;
        double startTime;
    };

    /// Runs the analyser named by the input over the visible audio, span by span.
    ///
    /// \a Executive is the contract's executive type and \a Schema its exports type; both must
    /// carry \c sampleRate and \c maxSegmentDuration. \a start(executive, span, progress) runs
    /// one span and returns the contract's result, and \a place(result) files it. The error code
    /// and message are set here for every failure; a task only fills in what it produced.
    template <class Executive, class Schema, class Start, class Place>
    void runAnalysis(const QString &kind, Start start, Place place) {
        const auto terminateTask = [this] {
            m_errorCode = ErrorCode::Terminated;
            m_errorMessage = tr("Task terminated.");
        };
        const auto displayPath =
            m_input.displayAudioPath.isEmpty() ? m_input.audioPath : m_input.displayAudioPath;

        auto newStatus = status();
        newStatus.message = tr("Loading model, please wait...");
        newStatus.isIndetermine = true;
        setStatus(newStatus);

        // The analyzer is named rather than found on disk: it is a contribution of an installed
        // package, and which one to use is a choice the person made and the project stored. The
        // lease keeps the analyser's package loaded until this function returns, so a rescan
        // landing mid extraction cannot pull the declaration out from under it.
        auto created = SynthrtEngine::instance().createAnalyzer(m_input.analyzer);
        if (!created) {
            m_errorCode = ErrorCode::ModelNotLoaded;
            m_errorMessage = tr("%1 analyzer unavailable: ").arg(kind) +
                             QString::fromStdString(created.error().toString());
            qCritical().noquote() << errorMessage();
            return;
        }
        auto lease = created.take();
        auto *executive = dynamic_cast<Executive *>(lease.executive.get());
        if (executive == nullptr) {
            m_errorCode = ErrorCode::ModelNotLoaded;
            m_errorMessage = tr("The chosen analyzer does not answer the %1 contract").arg(kind);
            qCritical().noquote() << errorMessage();
            return;
        }
        {
            QMutexLocker locker(&m_analyzerMutex);
            m_analyzer = lease.executive.get();
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
        const auto *exports = lease.spec->exports();
        const auto *schema = exports ? exports->template as<Schema>() : nullptr;
        if (schema == nullptr || schema->sampleRate <= 0) {
            m_errorCode = ErrorCode::ModelNotLoaded;
            m_errorMessage = tr("The chosen analyzer does not declare an input format");
            qCritical().noquote() << errorMessage();
            return;
        }

        newStatus = status();
        newStatus.message = tr("Running inference: %1").arg(displayPath);
        newStatus.isIndetermine = false;
        newStatus.maximum = 100;
        newStatus.progress = 0;
        setStatus(newStatus);

        QFile file(m_input.audioPath);
        talcs::AudioFormatIO io(&file);
        QString audioError;
        const auto cancelled = [this] { return isTerminateRequested(); };
        // The region is named on the file's own timeline, which is where prepareAudio() reads it
        // and where the analyser's startTime is measured from. The project times the input
        // carries are moved back by the material origin here, and forward again when the result
        // is placed.
        auto prepared = Extractors::prepareAudio(
            &io, m_input.audioVisibleStartMs - m_input.audioMaterialOriginMs,
            m_input.audioVisibleEndMs - m_input.audioMaterialOriginMs, schema->sampleRate,
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
        // more; where to cut is a judgement about this audio, and an analyzer that sliced for
        // itself would be making it on the host's behalf.
        const Extractors::SlicingProfile slicer;
        const auto spans =
            slicer.slice(prepared->samples, prepared->sampleRate, schema->maxSegmentDuration);

        m_errorCode = ErrorCode::Success;
        m_errorMessage = tr("Successfully extracted %1.").arg(kind.toLower());
        for (qsizetype index = 0; index < static_cast<qsizetype>(spans.size()); ++index) {
            if (isTerminateRequested()) {
                terminateTask();
                return;
            }
            const auto &slice = spans[index];
            // Seconds from the start of the file, so that what comes back can be put where it
            // belongs without the analyzer being told anything about the project.
            const Span span{prepared->samples, prepared->sampleRate, slice.begin, slice.end,
                            prepared->startMs / 1000.0 +
                                static_cast<double>(slice.begin) / prepared->sampleRate};
            const auto base = static_cast<double>(index) / static_cast<double>(spans.size());
            const auto share = 1.0 / static_cast<double>(spans.size());
            const auto progress = [this, base, share](double fraction) {
                auto progressStatus = status();
                progressStatus.progress = static_cast<int>((base + fraction * share) * 100);
                setStatus(progressStatus);
                return !isTerminateRequested();
            };

            auto analysed = start(*executive, span, progress);
            if (!analysed) {
                if (isTerminateRequested()) {
                    terminateTask();
                    return;
                }
                m_errorCode = ErrorCode::ModelRunFailed;
                m_errorMessage = tr("The %1 analyzer failed. Reason: ").arg(kind.toLower()) +
                                 QString::fromStdString(analysed.error().toString());
                qCritical().noquote() << "Error:" << errorMessage();
                return;
            }
            place(*analysed.take());
        }
    }

    Input m_input;
    ErrorCode m_errorCode = ErrorCode::UnknownError;
    QString m_errorMessage;

private:
    mutable QMutex m_analyzerMutex;
    otter::AnalysisExecutive *m_analyzer = nullptr;
};

#endif // EXTRACTTASK_H
