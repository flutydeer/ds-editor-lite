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

/// Shared analyzer setup, audio preparation, slicing, progress, and cancellation.
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

        /// Transcription language; empty selects the analyzer's default.
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
        // Keep the executive alive throughout stop(); ActiveAnalyzer clears it under the same
        // mutex.
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

    /// Run visible audio spans using Schema::sampleRate and Schema::maxSegmentDuration.
    /// start(executive, span, progress) returns a result; place(result) stores or rejects it.
    /// Failures set the task error.
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
        const auto audio = displayPath();
        const auto spans = sliceAudio(*prepared, schema->maxSegmentDuration);

        m_errorCode = ErrorCode::Success;
        m_errorMessage = tr("Successfully extracted %1.").arg(kind.toLower());
        for (std::size_t index = 0; index < spans.size(); ++index) {
            if (isTerminateRequested()) {
                terminateTask();
                return;
            }
            const auto span = spanAt(*prepared, spans[index]);
            auto analysed = start(*executive, span, progressFor(index, spans.size(), audio));
            if (!analysed) {
                failSpan(kind, analysed.error());
                return;
            }
            place(*analysed.take());
            if (!success())
                return;
        }
    }

    /// Open the configured analyzer and retain its package. On failure, set the error and return no
    /// value.
    std::optional<SynthrtEngine::AnalyzerLease> createAnalyzer(const QString &kind);

    /// Display path when supplied, otherwise the audio file path.
    QString displayPath() const;

    /// Decode the visible region at sampleRate, setting failure or cancellation on error.
    std::optional<Extractors::PreparedAudio> prepareAudio(int sampleRate);

    std::vector<Extractors::SampleSpan> sliceAudio(const Extractors::PreparedAudio &prepared,
                                                   double maxSegmentDuration) const;

    /// Select a sample span with a file-relative start time in seconds.
    static Span spanAt(const Extractors::PreparedAudio &prepared,
                       const Extractors::SampleSpan &slice);

    /// Create a passage progress callback. Returning false requests cancellation.
    std::function<bool(double)> progressFor(std::size_t index, std::size_t count,
                                            const QString &audio);

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
