//
// Created by fluty on 24-11-13.
//

#include "ExtractPitchTask.h"

#include "ExtractorUtils.h"

#include "AppContext.h"
#include "Model/AppOptions/AppOptions.h"
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Support/StringUtils.h>
#include <lite/Support/Linq.h>
#include <lite/Support/MathUtils.h>

#include <synthrt/Core/Plugin/PluginFactory.h>
#include <synthrt/Extract/PitchExtractorPlugin.h>

#include <QDebug>
#include <QMutexLocker>
#include <QScopeGuard>
#include <utility>
#include "Global/AppGlobal.h"

ExtractPitchTask::ExtractPitchTask(Input input) : ExtractTask(std::move(input)) {
    TaskStatus status;
    status.title = tr("Extract Pitch");
    status.message = tr("Pending infer: %1").arg(m_input.audioPath);
    setStatus(status);
}

void ExtractPitchTask::runTask() {
    const auto terminateTask = [this] {
        m_errorCode = ErrorCode::Terminated;
        m_errorMessage = tr("Task terminated.");
    };

    // 1. Load model in background thread (avoid blocking UI)
    auto newStatus = status();
    newStatus.message = tr("Loading model, please wait...");
    newStatus.isIndetermine = true;
    setStatus(newStatus);

    auto *synthrtEngine = AppContext::instance<SynthrtEngine>();
    auto runtimeLease = synthrtEngine ? synthrtEngine->acquirePitchExtractionOperation()
                                      : SynthrtEngine::RuntimeOperationLease{};
    if (!runtimeLease) {
        m_errorCode = ErrorCode::InferEngineNotLoaded;
        m_errorMessage = tr("Pitch extraction is not available");
        qCritical().noquote() << errorMessage();
        return;
    }
    if (isTerminateRequested()) {
        terminateTask();
        return;
    }

    const auto rmvpePath = appOptions->general()->rmvpePath;
    const auto modelPath = StringUtils::qstr_to_path(rmvpePath);

    if (modelPath.empty() || !exists(modelPath) || is_directory(modelPath)) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("Invalid RMVPE model path: ") + rmvpePath;
        qCritical().noquote() << errorMessage();
        return;
    }

    // Obtain the rmvpe PitchExtractor plugin and create an extractor instance.
    auto &runtime = runtimeLease.runtime();
    auto *plugins = runtime.services().get<srt::core::PluginFactory>();
    if (!plugins) {
        m_errorCode = ErrorCode::InferEngineNotLoaded;
        m_errorMessage = tr("PluginFactory is not available");
        qCritical().noquote() << errorMessage();
        return;
    }

    auto *rmvpePlugin = plugins->plugin<srt::extract::PitchExtractorPlugin>("rmvpe");
    if (!rmvpePlugin) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("RMVPE PitchExtractor plugin not found");
        qCritical().noquote() << errorMessage();
        return;
    }

    auto extractorExp = rmvpePlugin->createExtractor(&runtime);
    if (!extractorExp) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        const auto reason = QString::fromUtf8(extractorExp.error().message());
        m_errorMessage = tr("Failed to create RMVPE extractor: ") + reason;
        qCritical().noquote() << errorMessage();
        return;
    }
    auto extractor = extractorExp.take();
    {
        QMutexLocker locker(&m_extractorMutex);
        m_extractor = extractor;
    }
    const auto clearExtractor = qScopeGuard([this] {
        QMutexLocker locker(&m_extractorMutex);
        m_extractor.reset();
    });

    if (isTerminateRequested()) {
        terminateTask();
        return;
    }

    if (auto exp = extractor->open(modelPath); !exp) {
        if (isTerminateRequested()) {
            terminateTask();
            return;
        }
        m_errorCode = ErrorCode::ModelNotLoaded;
        const auto reason = QString::fromUtf8(exp.error().message());
        m_errorMessage = tr("Failed to create RMVPE session: ") + reason;
        qCritical().noquote() << errorMessage();
        QMutexLocker locker(&m_extractorMutex);
        m_extractor.reset();
        return;
    }

    if (isTerminateRequested()) {
        terminateTask();
        return;
    }

    // 2. Run inference
    newStatus = status();
    newStatus.message = tr("Running inference: %1").arg(m_input.audioPath);
    newStatus.isIndetermine = false;
    newStatus.maximum = 100;
    newStatus.progress = 0;
    setStatus(newStatus);

    QString decodeError;
    auto audio = ExtractorUtils::decodeAudio(
        m_input.audioPath, [this] { return isTerminateRequested(); }, decodeError);
    if (!audio) {
        if (isTerminateRequested()) {
            terminateTask();
            return;
        }
        m_errorCode = ErrorCode::ModelRunFailed;
        m_errorMessage = decodeError;
        qCritical().noquote() << "Error:" << errorMessage();
        return;
    }

    // Extract pitch (the extractor resamples internally to the model's required format).
    auto resultExp =
        extractor->extract(audio->buffer, audio->sampleRate, [this](const int progress) {
            auto progressStatus = status();
            progressStatus.progress = progress;
            setStatus(progressStatus);
        });

    if (isTerminateRequested()) {
        terminateTask();
        return;
    }

    if (resultExp) {
        m_errorCode = ErrorCode::Success;
        m_errorMessage = tr("Successfully extracted pitch.");
        auto pitchResult = resultExp.take();
        for (const auto &frame : pitchResult.frames) {
            if (isTerminateRequested()) {
                terminateTask();
                result.clear();
                return;
            }
            const auto midi = freqToMidi(frame.f0);
            QList<double> values;
            for (const auto &value : midi)
                values.append(value);
            auto processed = processOutput(values, frame.offset);
            if (isTerminateRequested()) {
                terminateTask();
                result.clear();
                return;
            }
            if (!processed.values.isEmpty())
                result.append(std::move(processed));
        }
    } else {
        m_errorCode = ErrorCode::ModelRunFailed;
        m_errorMessage =
            tr("RMVPE model run failed. Reason: ") + QString::fromUtf8(resultExp.error().message());
        qCritical().noquote() << "Error:" << errorMessage();
    }
}

void ExtractPitchTask::terminate() {
    ExtractTask::terminate();

    srt::core::NO<srt::extract::PitchExtractor> extractor;
    {
        QMutexLocker locker(&m_extractorMutex);
        extractor = m_extractor;
    }
    if (extractor) {
        extractor->terminate();
    }
}

std::vector<float> ExtractPitchTask::freqToMidi(const std::vector<float> &frequencies) {
    std::vector<float> midiPitches;

    for (const float f : frequencies) {
        if (f > 0) {
            float midiPitch = 69 + 12 * std::log2(f / 440.0f);
            midiPitches.push_back(midiPitch);
        } else {
            midiPitches.push_back(0);
        }
    }

    return midiPitches;
}

ExtractPitchTask::ResultSegment ExtractPitchTask::processOutput(const QList<double> &values,
                                                                const double frameOffsetMs) const {
    ResultSegment result;
    if (values.isEmpty())
        return result;

    constexpr double intervalMs = 10.0;
    QList<double> sourcePositions;
    sourcePositions.reserve(values.size());
    for (qsizetype i = 0; i < values.size(); ++i) {
        sourcePositions.append(m_input.audioMaterialOriginMs + frameOffsetMs + i * intervalMs);
    }

    const double overlapStartMs = qMax(sourcePositions.first(), m_input.audioVisibleStartMs);
    const double overlapEndMs = qMin(sourcePositions.last(), m_input.audioVisibleEndMs);
    if (overlapEndMs < overlapStartMs)
        return result;

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
