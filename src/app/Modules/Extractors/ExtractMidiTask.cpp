//
// Created by fluty on 24-11-13.
//

#include "ExtractMidiTask.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Utils/DmlGpuUtils.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"

#include <QDebug>
#include <QThread>
#include <utility>

ExtractMidiTask::ExtractMidiTask(Input input) : ExtractTask(std::move(input)) {
    TaskStatus status;
    status.title = tr("Extract Midi");
    status.message = tr("Pending infer: %1").arg(m_input.audioPath);
    setStatus(status);

    if (!inferEngine->initialized()) {
        m_errorCode = ErrorCode::InferEngineNotLoaded;
        m_errorMessage = tr("Inference engine is not loaded");
        qCritical().noquote() << "Error:" << errorMessage();
        return;
    }

    auto somePath = appOptions->general()->somePath;
    const std::filesystem::path modelPath = somePath
#ifdef _WIN32
        .toStdWString();
#else
        .toStdString();
#endif

    if (modelPath.empty() || !exists(modelPath) || is_directory(modelPath)) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("Invalid SOME model path: ") + somePath;
        qCritical().noquote() << "Error:" << errorMessage();
        return;
    }

    const auto getCurrentGpuIndex = []() {
        auto selectedGpu = DmlGpuUtils::getGpuByPciDeviceVendorIdString(appOptions->inference()->selectedGpuId);
        if (selectedGpu.index < 0) {
            selectedGpu = DmlGpuUtils::getRecommendedGpu();
        }
        return selectedGpu.index;
    };

    const int device_id = getCurrentGpuIndex();

    const auto rmProvider = appOptions->inference()->executionProvider == "DirectML"
                                ? Some::ExecutionProvider::DML
                                : Some::ExecutionProvider::CPU;

    m_some = std::make_unique<Some::Some>(modelPath, rmProvider, device_id);
    if (!m_some || !m_some->is_open()) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("Failed to create SOME session. Make sure onnx model is valid.");
        qCritical().noquote() << errorMessage();
        return;
    }
}

void ExtractMidiTask::runTask() {
    auto newStatus = status();
    newStatus.message = tr("Running inference: %1").arg(m_input.audioPath);
    setStatus(newStatus);

    if (!m_some || !m_some->is_open()) {
        qCritical().noquote() << errorMessage();
        return;
    }

    std::vector<Some::Midi> midis;
    std::string msg;

#ifdef Q_OS_WIN
    const std::filesystem::path wavPath = m_input.audioPath.toStdWString();
#else
    const std::filesystem::path wavPath = m_input.audioPath.toStdString();
#endif

    const bool runSuccess = m_some->get_midi(wavPath, midis, appModel->tempo(), msg, [=](int progress) {
        auto progressStatus = status();
        progressStatus.progress = progress;
        setStatus(progressStatus);
    });

    if (runSuccess) {
        m_errorCode = ErrorCode::Success;
        m_errorMessage = tr("Successfully extracted midi.");
        qDebug() << "midi output:";
        result = midis;
    } else {
        m_errorCode = ErrorCode::ModelRunFailed;
        m_errorMessage = tr("SOME model run failed. Reason: ") + QString::fromStdString(msg);
        qCritical().noquote() << "Error:" << errorMessage();
    }
}

void ExtractMidiTask::terminate() {
    if (!m_some) {
        return;
    }
    m_some->terminate();
    m_errorCode = ErrorCode::Terminated;
    m_errorMessage = tr("Task terminated.");
}