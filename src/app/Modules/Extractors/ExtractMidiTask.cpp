//
// Created by fluty on 24-11-13.
//

#include "ExtractMidiTask.h"

#include "ExtractorUtils.h"

#include "AppContext.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/SynthrtEngine/SynthrtEngine.h"
#include "Utils/StringUtils.h"

#include <synthrt/Core/Plugin/PluginFactory.h>
#include <synthrt/Extract/MidiExtractorPlugin.h>

#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QScopeGuard>
#include <utility>

ExtractMidiTask::ExtractMidiTask(Input input) : ExtractTask(std::move(input)) {
    TaskStatus status;
    status.title = tr("Extract Midi");
    status.message = tr("Pending infer: %1").arg(m_input.audioPath);
    setStatus(status);
}

void ExtractMidiTask::runTask() {
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
    auto runtimeLease = synthrtEngine ? synthrtEngine->acquireMidiExtractionOperation()
                                      : SynthrtEngine::RuntimeOperationLease{};
    if (!runtimeLease) {
        m_errorCode = ErrorCode::InferEngineNotLoaded;
        m_errorMessage = tr("MIDI extraction is not available");
        qCritical().noquote() << "Error:" << errorMessage();
        return;
    }
    if (isTerminateRequested()) {
        terminateTask();
        return;
    }

    const auto gameDir = appOptions->general()->gameDir;
    const auto modelPath = StringUtils::qstr_to_path(gameDir);

    if (modelPath.empty() || !exists(modelPath) || !is_directory(modelPath)) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("Invalid game model dir: ") +
                         (gameDir.isEmpty() ? QString("Dir is Empty.") : gameDir);
        qCritical().noquote() << "Error:" << errorMessage();
        return;
    }

    // Obtain the game MidiExtractor plugin and create an extractor instance.
    auto &runtime = runtimeLease.runtime();
    auto *plugins = runtime.services().get<srt::core::PluginFactory>();
    if (!plugins) {
        m_errorCode = ErrorCode::InferEngineNotLoaded;
        m_errorMessage = tr("PluginFactory is not available");
        qCritical().noquote() << "Error:" << errorMessage();
        return;
    }

    auto *gamePlugin = plugins->plugin<srt::extract::MidiExtractorPlugin>("game");
    if (!gamePlugin) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("game MidiExtractor plugin not found");
        qCritical().noquote() << "Error:" << errorMessage();
        return;
    }

    auto extractorExp = gamePlugin->createExtractor(&runtime);
    if (!extractorExp) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        const auto reason = QString::fromUtf8(extractorExp.error().message());
        m_errorMessage = tr("Failed to create game extractor: ") + reason;
        qCritical().noquote() << "Error:" << errorMessage();
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
        m_errorMessage = tr("Failed to create game session: ") + reason;
        qCritical().noquote() << "Error:" << errorMessage();
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

    // Configure extraction options (tempo from input; thresholds/language use defaults
    // that match the model's config.json — the plugin may override internally).
    srt::extract::MidiExtractOptions options;
    options.tempo = static_cast<float>(m_input.tempo);

    auto resultExp =
        extractor->extract(audio->buffer, audio->sampleRate, options, [this](const int progress) {
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
        m_errorMessage = tr("Successfully extracted midi.");
        auto midiResult = resultExp.take();
        result.reserve(midiResult.notes.size());
        for (const auto &note : midiResult.notes) {
            if (isTerminateRequested()) {
                terminateTask();
                result.clear();
                return;
            }
            result.push_back({note.note, note.start, note.duration});
        }
    } else {
        m_errorCode = ErrorCode::ModelRunFailed;
        m_errorMessage =
            tr("game model run failed. Reason: ") + QString::fromUtf8(resultExp.error().message());
        qCritical().noquote() << "Error:" << errorMessage();
    }
}

void ExtractMidiTask::terminate() {
    ExtractTask::terminate();

    srt::core::NO<srt::extract::MidiExtractor> extractor;
    {
        QMutexLocker locker(&m_extractorMutex);
        extractor = m_extractor;
    }
    if (extractor) {
        extractor->terminate();
    }
}
