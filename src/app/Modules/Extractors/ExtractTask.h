#ifndef EXTRACTTASK_H
#define EXTRACTTASK_H

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <QMutex>
#include <QMutexLocker>
#include <QString>

#include <otter/Analysis/AnalysisExecutive.h>

#include <lite/MusicBase/Timeline.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Tasking/Task.h>

#include "AnalysisAudio.h"
#include "AudioSlicer.h"

/// Common base of the extraction tasks. It opens the selected analyzer, reads its input format,
/// prepares and slices the audio, and runs the spans in sequence with progress reporting and
/// cancellation.
///
/// A derived task supplies the contract types and two callables: one that builds the input of a
/// span and one that places the result of a span. The shared implementation prevents the pitch
/// and MIDI tasks from diverging, so that a fix applies to both.
class ExtractTask : public Task {
    Q_OBJECT

public:
    enum class ErrorCode {
        UnknownError = -2,
        Terminated = -1,
        Success = 0,
        ModelNotLoaded = 2,
        ModelRunFailed = 3,
    };

    struct Input {
        QString audioPath;
        QString displayAudioPath;
        /// Analyzer to run, identified as <package>:inference/<contribution>.
        QString analyzer;

        /// Transcription language, if the analyzer distinguishes languages. If empty, the
        /// analyzer selects the language; a monolingual analyzer behaves identically in both cases.
        QString language;
        Timeline timeline;
        int singingClipStartTick = 0;
        int audioClipStartTick = 0;
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
        // The mutex is held across the call. runAnalysis() clears the pointer under this mutex
        // before the executive is destroyed, so a non-null pointer refers to a live executive.
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

    /// Runs the analyzer selected by the input over the visible audio, span by span.
    ///
    /// \a Executive is the executive type of the contract and \a Schema its exports type. \a Schema
    /// must provide \c sampleRate and \c maxSegmentDuration. \a start(executive, span, progress)
    /// runs one span and returns the result of the contract, and \a place(result) stores the
    /// result. This function sets the error code and message for every failure; a derived task
    /// only stores the results.
    ///
    /// This template contains only the steps that depend on the contract types. All other steps
    /// are member functions in ExtractTask.cpp.
    template <class Executive, class Schema, class Start, class Place>
    void runAnalysis(const QString &kind, Start start, Place place) {
        auto lease = createAnalyzer(kind);
        if (!lease) {
            return;
        }
        auto *executive = dynamic_cast<Executive *>(lease->executive.get());
        if (executive == nullptr) {
            fail(ErrorCode::ModelNotLoaded,
                 tr("The chosen analyzer does not answer the %1 contract").arg(kind));
            return;
        }
        const ActiveAnalyzer active(*this, *lease->executive);
        if (isTerminateRequested()) {
            terminateTask();
            return;
        }

        // Input format of the analyzer, read from its declaration rather than assumed. The sample
        // rate and the maximum span length differ between analyzers.
        const auto *exports = lease->spec->exports();
        const auto *schema = exports ? exports->template as<Schema>() : nullptr;
        if (schema == nullptr || schema->sampleRate <= 0) {
            fail(ErrorCode::ModelNotLoaded,
                 tr("The chosen analyzer does not declare an input format"));
            return;
        }

        const auto prepared = prepareAudio(schema->sampleRate);
        if (!prepared) {
            return;
        }
        const auto spans = sliceAudio(*prepared, schema->maxSegmentDuration);

        m_errorCode = ErrorCode::Success;
        m_errorMessage = tr("Successfully extracted %1.").arg(kind.toLower());
        for (std::size_t index = 0; index < spans.size(); ++index) {
            if (isTerminateRequested()) {
                terminateTask();
                return;
            }
            const auto span = spanAt(*prepared, spans[index]);
            auto analysed = start(*executive, span, progressFor(index, spans.size()));
            if (!analysed) {
                failSpan(kind, analysed.error());
                return;
            }
            place(*analysed.take());
        }
    }

    /// Opens the analyzer selected by the input. Returns \c std::nullopt with the error set on
    /// failure.
    ///
    /// The analyzer is identified by name rather than by a path on disk because it is a
    /// contribution of an installed package, selected by the user and stored in the project. The
    /// lease keeps the package of the analyzer loaded while it is held, so a package rescan during
    /// extraction cannot unload the declaration.
    std::optional<SynthrtEngine::AnalyzerLease> createAnalyzer(const QString &kind);

    /// Decodes the visible region of the audio at \a sampleRate. Returns \c std::nullopt with the
    /// error or the termination set on failure.
    std::optional<Extractors::PreparedAudio> prepareAudio(int sampleRate);

    /// Splits the prepared audio into spans no longer than \a maxSegmentDuration seconds.
    std::vector<Extractors::SampleSpan> sliceAudio(const Extractors::PreparedAudio &prepared,
                                                   double maxSegmentDuration) const;

    /// Returns the span of \a prepared that \a slice selects, with its start in seconds from the
    /// start of the file.
    static Span spanAt(const Extractors::PreparedAudio &prepared,
                       const Extractors::SampleSpan &slice);

    /// Returns the progress callback for span \a index of \a count. The callback returns whether
    /// the analysis continues.
    std::function<bool(double)> progressFor(std::size_t index, std::size_t count);

    /// Records the failure of one span, or a termination if the execution was stopped.
    void failSpan(const QString &kind, const srt::Error &error);

    void fail(ErrorCode code, const QString &message);
    void terminateTask();

    /// Registers a running analyzer for terminate().
    class ActiveAnalyzer final {
    public:
        ActiveAnalyzer(ExtractTask &task, otter::AnalysisExecutive &executive);
        ~ActiveAnalyzer();

        ActiveAnalyzer(const ActiveAnalyzer &) = delete;
        ActiveAnalyzer &operator=(const ActiveAnalyzer &) = delete;

    private:
        ExtractTask &m_task;
    };

    Input m_input;
    ErrorCode m_errorCode = ErrorCode::UnknownError;
    QString m_errorMessage;

private:
    mutable QMutex m_analyzerMutex;
    otter::AnalysisExecutive *m_analyzer = nullptr;
};

#endif // EXTRACTTASK_H
