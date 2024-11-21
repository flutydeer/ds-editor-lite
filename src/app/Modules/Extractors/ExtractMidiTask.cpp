//
// Created by fluty on 24-11-13.
//

#include "ExtractMidiTask.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Utils/DmlUtils.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"

#include <QDebug>
#include <QThread>
#include <utility>

ExtractMidiTask::ExtractMidiTask(Input input) : m_input(std::move(input)) {
    TaskStatus status;
    status.title = tr("Extract Midi");
    status.message = tr("Pending infer: %1").arg(m_input.audioPath);
    setStatus(status);

    const std::filesystem::path modelPath =
#ifdef _WIN32
        appOptions->general()->somePath.toStdWString();
#else
        appOptions->general()->somePath.toStdString();
#endif
    Q_ASSERT(!modelPath.empty());

    const auto getCurrentGpuIndex = []() {
        auto selectedGpu = DmlUtils::getGpuByIdString(appOptions->inference()->selectedGpuId);
        if (selectedGpu.index < 0) {
            selectedGpu = DmlUtils::getRecommendedGpu();
        }
        return selectedGpu.index;
    };

    const int device_id = getCurrentGpuIndex();

    const auto rmProvider = appOptions->inference()->executionProvider == "DirectML"
                                ? Some::ExecutionProvider::DML
                                : Some::ExecutionProvider::CPU;

    if (!inferEngine->initialized()) {
        return;
    }

    m_some = std::make_unique<Some::Some>(modelPath, rmProvider, device_id);
}

const ExtractMidiTask::Input &ExtractMidiTask::input() const {
    return m_input;
}

void ExtractMidiTask::runTask() {
    auto newStatus = status();
    newStatus.message = tr("Running inference: %1").arg(m_input.audioPath);
    setStatus(newStatus);

    if (!m_some) {
        success = false;
        qCritical() << "Error: Infer engine is not initialized!";
        return;
    }

    if (!m_some->is_open()) {
        success = false;
        qCritical() << "Error: Model is not loaded! Make sure the model exists and is valid.";
        return;
    }

    std::vector<Some::Midi> midis;
    std::string msg;

#ifdef Q_OS_WIN
    const std::filesystem::path wavPath = m_input.audioPath.toStdWString();
#else
    const std::filesystem::path wavPath = m_input.audioPath.toStdString();
#endif

    success = m_some->get_midi(wavPath, midis, appModel->tempo(), msg, [=](int progress) {
        auto progressStatus = status();
        progressStatus.progress = progress;
        setStatus(progressStatus);
    });

    if (success) {
        qDebug() << "midi output:";
        result = midis;
    } else {
        qCritical() << "Error: " << msg;
    }
}

void ExtractMidiTask::terminate() {
    if (!m_some) {
        return;
    }
    m_some->terminate();
}