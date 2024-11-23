//
// Created by fluty on 24-11-13.
//

#include "ExtractPitchTask.h"

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Utils/DmlGpuUtils.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"

#include <rmvpe-infer/Rmvpe.h>
#include <QDebug>
#include <QThread>
#include <utility>

ExtractPitchTask::ExtractPitchTask(Input input) : ExtractTask(std::move(input)) {
    TaskStatus status;
    status.title = tr("Extract Pitch");
    status.message = tr("Pending infer: %1").arg(m_input.audioPath);
    setStatus(status);

    if (!inferEngine->initialized()) {
        m_errorCode = ErrorCode::InferEngineNotLoaded;
        m_errorMessage = tr("Inference engine is not loaded");
        qCritical().noquote() << errorMessage();
        return;
    }

    auto rmvpePath = appOptions->general()->rmvpePath;
    const std::filesystem::path modelPath = rmvpePath
#ifdef _WIN32
        .toStdWString();
#else
        .toStdString();
#endif

    if (modelPath.empty() || !exists(modelPath) || is_directory(modelPath)) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("Invalid RMVPE model path: ") + rmvpePath;
        qCritical().noquote() << errorMessage();
        return;
    }

    const auto getCurrentGpuIndex = []() {
        auto selectedGpu = DmlGpuUtils::getGpuByIdString(appOptions->inference()->selectedGpuId);
        if (selectedGpu.index < 0) {
            selectedGpu = DmlGpuUtils::getRecommendedGpu();
        }
        return selectedGpu.index;
    };

    const int device_id = getCurrentGpuIndex();

    const auto rmProvider = appOptions->inference()->executionProvider == "DirectML"
                                ? Rmvpe::ExecutionProvider::DML
                                : Rmvpe::ExecutionProvider::CPU;

    // TODO:: forced on cpu
    m_rmvpe = std::make_unique<Rmvpe::Rmvpe>(modelPath, Rmvpe::ExecutionProvider::CPU, 0);
    if (!m_rmvpe || !m_rmvpe->is_open()) {
        m_errorCode = ErrorCode::ModelNotLoaded;
        m_errorMessage = tr("Failed to create RMVPE session. Make sure onnx model is valid.");
        qCritical().noquote() << errorMessage();
        return;
    }
}

void ExtractPitchTask::runTask() {
    auto newStatus = status();
    newStatus.message = tr("Running inference: %1").arg(m_input.audioPath);
    setStatus(newStatus);

    if (!m_rmvpe || !m_rmvpe->is_open()) {
        qCritical().noquote() << errorMessage();
        return;
    }

    constexpr float threshold = 0.03f;

    std::vector<Rmvpe::RmvpeRes> rmvpe_res;
    std::string msg;

#ifdef Q_OS_WIN
    const std::filesystem::path wavPath = m_input.audioPath.toStdWString();
#else
    const std::filesystem::path wavPath = m_input.audioPath.toStdString();
#endif

    const bool runSuccess = m_rmvpe->get_f0(wavPath, threshold, rmvpe_res, msg, [=](int progress) {
        auto progressStatus = status();
        progressStatus.progress = progress;
        setStatus(progressStatus);
    });

    if (runSuccess) {
        m_errorCode = ErrorCode::Success;
        m_errorMessage = tr("Successfully extracted pitch.");
        qDebug() << "midi output:";
        for (const auto &[offset, f0, uv] : rmvpe_res) {
            const auto midi = freqToMidi(f0);
            QList<double> values;
            for (const auto &value : midi)
                values.append(value);
            result.append({offset, processOutput(values)});
        }
    } else {
        m_errorCode = ErrorCode::ModelRunFailed;
        m_errorMessage = tr("RMVPE model run failed. Reason: ") + QString::fromStdString(msg);
        qCritical().noquote() << "Error:" << errorMessage();
    }
}

void ExtractPitchTask::terminate() {
    if (!m_rmvpe) {
        return;
    }
    m_rmvpe->terminate();
    m_errorCode = ErrorCode::Terminated;
    m_errorMessage = tr("Task terminated.");
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

QList<double> ExtractPitchTask::processOutput(const QList<double> &values) const {
    auto tickToSec = [&](const double &tick) { return tick * 60 / m_input.tempo / 480; };
    constexpr auto interval = 0.01;
    const auto newInterval = tickToSec(5);
    return MathUtils::resample(values, interval, newInterval);
}