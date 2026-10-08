#include "ExtractTask.h"

#include <QDebug>
#include <QFile>

#include <TalcsFormat/AudioFormatIO.h>

#include <otter/Analysis/AnalysisError.h>

std::optional<SynthrtEngine::AnalyzerLease> ExtractTask::createAnalyzer(const QString &kind) {
    auto newStatus = status();
    newStatus.message = tr("Loading model, please wait...");
    newStatus.isIndetermine = true;
    setStatus(newStatus);

    auto created = SynthrtEngine::instance().createAnalyzer(m_input.analyzer);
    if (!created) {
        fail(ErrorCode::ModelNotLoaded, tr("%1 analyzer unavailable: ").arg(kind) +
                                            QString::fromStdString(created.error().toString()));
        return std::nullopt;
    }
    return created.take();
}

QString ExtractTask::displayPath() const {
    return m_input.displayAudioPath.isEmpty() ? m_input.audioPath : m_input.displayAudioPath;
}

std::optional<Extractors::PreparedAudio> ExtractTask::prepareAudio(const int sampleRate) {
    auto newStatus = status();
    newStatus.message = tr("Running inference: %1").arg(displayPath());
    newStatus.isIndetermine = false;
    newStatus.maximum = 100;
    newStatus.progress = 0;
    setStatus(newStatus);

    QFile file(m_input.audioPath);
    // The stream must be open before the format IO wraps it. talcs rejects a stream that is not
    // open and reports the rejection as a failure to open the audio file, regardless of whether
    // the file exists.
    if (!file.open(QIODevice::ReadOnly)) {
        fail(ErrorCode::ModelRunFailed, tr("Failed to open the audio file"));
        return std::nullopt;
    }
    talcs::AudioFormatIO io(&file);
    QString audioError;
    const auto cancelled = [this] { return isTerminateRequested(); };
    // The region is specified on the file's timeline, on which prepareAudio() reads it and from
    // which the analyzer's startTime is measured. The project times in the input are shifted back
    // by the material origin here and shifted forward again when the result is placed.
    auto prepared = Extractors::prepareAudio(
        &io, m_input.audioVisibleStartMs - m_input.audioMaterialOriginMs,
        m_input.audioVisibleEndMs - m_input.audioMaterialOriginMs, sampleRate, cancelled,
        audioError);
    if (!prepared) {
        if (isTerminateRequested()) {
            terminateTask();
        } else {
            fail(ErrorCode::ModelRunFailed, audioError);
        }
        return std::nullopt;
    }
    return prepared;
}

std::vector<Extractors::SampleSpan>
    ExtractTask::sliceAudio(const Extractors::PreparedAudio &prepared,
                            const double maxSegmentDuration) const {
    // Slicing is performed by the host. The analyzer declares only the maximum span length it
    // accepts. The cut positions depend on the audio content and are chosen by the host, not by
    // the analyzer.
    const Extractors::SlicingProfile slicer;
    return slicer.slice(prepared.samples, prepared.sampleRate, maxSegmentDuration);
}

ExtractTask::Span ExtractTask::spanAt(const Extractors::PreparedAudio &prepared,
                                      const Extractors::SampleSpan &slice) {
    // The start time is measured in seconds from the start of the file, so that the result can be
    // placed on the project timeline without passing project information to the analyzer. The
    // slicer uses signed offsets and Span uses unsigned offsets. The conversion is explicit
    // because sample spans from the slicer are never negative, an invariant the compiler cannot
    // verify.
    return Span{prepared.samples, prepared.sampleRate, static_cast<std::size_t>(slice.begin),
                static_cast<std::size_t>(slice.end),
                prepared.startMs / 1000.0 +
                    static_cast<double>(slice.begin) / prepared.sampleRate};
}

std::function<bool(double)> ExtractTask::progressFor(const std::size_t index,
                                                     const std::size_t count,
                                                     const QString &audio) {
    const auto base = static_cast<double>(index) / static_cast<double>(count);
    const auto share = 1.0 / static_cast<double>(count);
    // The host cuts the region into passages, and nothing else says how many calls the analyzer
    // received: one long passage and six short ones look the same to the user. Every report
    // therefore names its passage. The text is composed from the path rather than by appending to
    // the message in flight, because appending would accumulate one suffix per report.
    const auto named = count > 1 ? tr("Running inference: %1 (passage %2 of %3)")
                                       .arg(audio)
                                       .arg(static_cast<qlonglong>(index + 1))
                                       .arg(static_cast<qlonglong>(count))
                                 : status().message;
    return [this, base, share, named](double fraction) {
        auto progressStatus = status();
        progressStatus.progress = static_cast<int>((base + fraction * share) * 100);
        progressStatus.message = named;
        setStatus(progressStatus);
        return !isTerminateRequested();
    };
}

void ExtractTask::failSpan(const QString &kind, const srt::Error &error) {
    // otter reports a stopped execution as AnalysisError::Cancelled. The analyzer also stops if
    // the progress callback returns false, so both the error code and the termination flag are
    // checked, and a cancellation is never reported as a failure.
    if (isTerminateRequested() || error.code() == otter::AnalysisError::Cancelled) {
        terminateTask();
        return;
    }
    fail(ErrorCode::ModelRunFailed, tr("The %1 analyzer failed. Reason: ").arg(kind.toLower()) +
                                        QString::fromStdString(error.toString()));
}

void ExtractTask::fail(const ErrorCode code, const QString &message) {
    m_errorCode = code;
    m_errorMessage = message;
    qCritical().noquote() << "Error:" << message;
}

void ExtractTask::terminateTask() {
    m_errorCode = ErrorCode::Terminated;
    m_errorMessage = tr("Task terminated.");
}

ExtractTask::ActiveAnalyzer::ActiveAnalyzer(ExtractTask &task,
                                            otter::AnalysisExecutive &executive)
    : m_task(task) {
    QMutexLocker locker(&m_task.m_analyzerMutex);
    m_task.m_analyzer = &executive;
}

ExtractTask::ActiveAnalyzer::~ActiveAnalyzer() {
    // Cleared under the mutex before the executive is destroyed, so that terminate() never stops
    // a destroyed executive.
    QMutexLocker locker(&m_task.m_analyzerMutex);
    m_task.m_analyzer = nullptr;
}
