//
// Created by fluty on 24-11-13.
//

#include "ExtractPitchTask.h"

#include "Model/AppOptions/AppOptions.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"

#include <rmvpe-infer/Rmvpe.h>
#include <QDebug>
#include <QThread>

ExtractPitchTask::ExtractPitchTask(const Input &input) : m_input(input) {
    TaskStatus status;
    status.title = tr("Extract Pitch");
    status.message = tr("Pending infer: %1").arg(m_input.audioPath);
    setStatus(status);
}

const ExtractPitchTask::Input &ExtractPitchTask::input() const {
    return m_input;
}

void ExtractPitchTask::runTask() {
    auto newStatus = status();
    newStatus.message = tr("Running inference: %1").arg(m_input.audioPath);
    setStatus(newStatus);

    const std::filesystem::path modelPath = "";
    Q_ASSERT(!modelPath.empty());
#ifdef Q_OS_WIN
    const std::filesystem::path wavPath = m_input.audioPath.toStdWString();
#else
    const std::filesystem::path wavPath = m_input.audioPath.toStdString();
#endif

    const int device_id = appOptions->inference()->selectedGpuIndex;

    const auto rmProvider = appOptions->inference()->executionProvider == "DirectML"
                                ? Rmvpe::ExecutionProvider::DML
                                : Rmvpe::ExecutionProvider::CPU;

    const Rmvpe::Rmvpe rmvpe(modelPath, rmProvider, device_id);
    constexpr float threshold = 0.03f;

    std::vector<float> f0;
    std::vector<bool> uv;
    std::string msg;

    success = rmvpe.get_f0(wavPath, threshold, f0, uv, msg, [=](int progress) {
        auto progressStatus = status();
        progressStatus.progress = progress;
        setStatus(progressStatus);
    });

    if (success) {
        qDebug() << "midi output:";
        const auto midi = freqToMidi(f0);
        QList<double> values;
        for (const auto &value : midi)
            values.append(value);
        processOutput(values);
    } else {
        qCritical() << "Error: " << msg;
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

void ExtractPitchTask::processOutput(const QList<double> &values) {
    auto tickToSec = [&](const double &tick) { return tick * 60 / m_input.tempo / 480; };
    auto interval = 0.01;
    auto newInterval = tickToSec(5);
    result = MathUtils::resample(values, interval, newInterval);
}