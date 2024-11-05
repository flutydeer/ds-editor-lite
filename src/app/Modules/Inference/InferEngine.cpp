//
// Created by fluty on 24-9-25.
//

#include "InferEngine.h"

#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/InitInferEngineTask.h"

#include <dsonnxinfer/AcousticInference.h>
#include <dsonnxinfer/DurationInference.h>
#include <dsonnxinfer/PitchInference.h>
#include <dsonnxinfer/VarianceInference.h>

#include "Utils/BasePitchCurve.h"
#include "Utils/DmlUtils.h"
#include "Utils/Log.h"

#include <QDebug>

namespace DS = dsonnxinfer;

static void loggerCallbackDs(int level, const char *category, const char *msg) {
    switch (level) {
        case DS::LOGGING_LEVEL_FATAL:
            Log::f(category, msg);
            break;
        case DS::LOGGING_LEVEL_ERROR:
            Log::e(category, msg);
            break;
        case DS::LOGGING_LEVEL_WARNING:
            Log::w(category, msg);
            break;
        case DS::LOGGING_LEVEL_INFO:
            Log::i(category, msg);
            break;
        case DS::LOGGING_LEVEL_DEBUG:
            Log::d(category, msg);
            break;
        default:
            break;
    }
}

InferEngine::InferEngine() {
    m_env.setLoggerCallback(loggerCallbackDs);

    auto initTask = new InitInferEngineTask;
    connect(initTask, &Task::finished, this, [=] {
        taskManager->removeTask(initTask);
        if (initTask->success)
            appStatus->inferEngineEnvStatus = AppStatus::ModuleStatus::Ready;
        else
            appStatus->inferEngineEnvStatus = AppStatus::ModuleStatus::Error;
        delete initTask;
    });
    taskManager->addAndStartTask(initTask);
    appStatus->inferEngineEnvStatus = AppStatus::ModuleStatus::Loading;
}

InferEngine::~InferEngine() {
    dispose();
}

bool InferEngine::initialized() {
    QMutexLocker lock(&m_mutex);
    return m_initialized;
}

// void InferenceEngine::loadConfig(const QString &path) {
//     m_configLoaded = false;
//     auto task = new LoadInferConfigTask(path);
//     connect(task, &Task::finished, this, [=] {
//         taskManager->removeTask(task);
//         if (task->success) {
//             m_configLoaded = true;
//             m_configPath = path;
//         }
//         delete task;
//     });
//     taskManager->addAndStartTask(task);
// }

bool InferEngine::initialize(QString &error) {
    std::string errorMessage;

    std::filesystem::path ortPath =
#ifdef _WIN32
        "onnxruntime";
#elif defined(__APPLE__)
        "../Frameworks/libonnxruntime.dylib";
#else
        "../lib/libonnxruntime.so";
#endif
    const auto &gpuDeviceList = DmlUtils::getDirectXGPUs();
    // Load environment (must do this before inference)
    auto ep = DS::EP_CPU;
    if (appOptions->inference()->executionProvider == "CPU")
        ep = DS::EP_CPU;
    else if (appOptions->inference()->executionProvider == "DirectML") {
        if (gpuDeviceList.empty())
            qCritical() << "InferEngine: Unable to find GPU device.";
        ep = DS::EP_DirectML;
    }
    if (!m_env.load(ortPath, ep, &errorMessage)) {
        qCritical() << "Failed to load environment:" << errorMessage;
        error += errorMessage;
        m_initialized = false;
        return false;
    }

    const auto selectGpuIndex = appOptions->inference()->selectedGpuIndex < gpuDeviceList.size()
                                    ? gpuDeviceList[appOptions->inference()->selectedGpuIndex].index
                                    : gpuDeviceList.first().index;
    m_env.setDeviceIndex(selectGpuIndex);
    m_env.setDefaultSteps(appOptions->inference()->samplingSteps);
    m_env.setDefaultDepth(appOptions->inference()->depth);
    m_initialized = true;
    qInfo() << "Successfully loaded environment. Execution provider: DirectML";
    return true;
}

bool InferEngine::runLoadConfig(const QString &path) {
    if (path.isNull() || path.isEmpty()) {
        qWarning() << "Config path is null or empty";
        return false;
    }
    QMutexLocker lock(&m_mutex);
    if (path == m_configPath) {
        qInfo() << "Already loaded config";
        return m_configLoaded;
    }

    if (!m_initialized) {
        qFatal() << "runLoadConfig: Environment is not initialized!";
        return false;
    }

    // Load models
    m_configLoaded = false;
    bool loadDsConfigOk;

    std::string dsConfigPath = path.toStdString();
    auto dsConfig = DS::DsConfig::fromYAML(dsConfigPath, &loadDsConfigOk);

    auto dsVocoderConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dsvocoder" / "vocoder.yaml";
    auto dsVocoderConfig = DS::DsVocoderConfig::fromYAML(dsVocoderConfigPath, &loadDsConfigOk);

    auto durConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dsdur" / "dsconfig.yaml";
    auto durConfig = DS::DsDurConfig::fromYAML(durConfigPath, &loadDsConfigOk);

    auto pitchConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dspitch" / "dsconfig.yaml";
    auto pitchConfig = DS::DsPitchConfig::fromYAML(pitchConfigPath, &loadDsConfigOk);
    // bool expr = pitchConfig.features & DS::kfParamExpr;
    // qDebug() << "expr" << expr;

    auto varianceConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dsvariance" / "dsconfig.yaml";
    auto varianceConfig = DS::DsVarianceConfig::fromYAML(varianceConfigPath, &loadDsConfigOk);

    if (!loadDsConfigOk) {
        qCritical() << "Failed to load config:" << path;
        return false;
    }

    m_durationInfer = new DS::DurationInference(durConfig);
    m_durationInfer->open();

    m_pitchInfer = new DS::PitchInference(pitchConfig);
    m_pitchInfer->open();

    m_varianceInfer = new DS::VarianceInference(varianceConfig);
    m_varianceInfer->open();

    m_acousticInfer = new DS::AcousticInference(dsConfig, dsVocoderConfig);
    m_acousticInfer->open();

    qInfo() << "Successfully loaded config";
    m_configLoaded = true;
    m_configPath = path;
    return true;
}

bool InferEngine::inferDuration(const QString &input, QString &output, QString &error) const {
    if (!m_initialized) {
        qCritical() << "inferDuration: Environment is not initialized";
        return false;
    }
    if (!m_configLoaded) {
        qCritical() << "inferDuration: Config is not loaded";
    }
    DS::Status s;
    auto segment = DS::Segment::fromJson(input.toStdString(), &s);
    if (!s.isOk()) {
        error = QString::fromStdString(s.msg);
        return false;
    }

    // Run duration inference. The segment will be modified in-place.
    if (!m_durationInfer->runInPlace(segment, &s)) {
        qCritical() << "Failed to run duration inference: " << s.msg;
        return false;
    }

    output = QString::fromStdString(segment.toJson());
    return true;
}

bool InferEngine::inferPitch(const QString &input, QString &output, QString &error) const {
    if (!m_initialized) {
        qCritical() << "inferPitch: Environment is not initialized";
        return false;
    }
    if (!m_configLoaded) {
        qCritical() << "inferPitch: Config is not loaded";
    }
    DS::Status s;
    auto segment = DS::Segment::fromJson(input.toStdString(), &s);
    if (!s.isOk()) {
        error = QString::fromStdString(s.msg);
        return false;
    }

    // constexpr double timestep = 0.01161;
    //
    // const BasePitchCurve pitchCurve(segment);
    //
    // std::vector<double> pitchPoints = pitchCurve.GetPitchPoints(timestep);
    //
    // auto &[tag, sample_curve, retake_start, retake_end] = segment.parameters["pitch"];
    // sample_curve.samples.resize(pitchPoints.size());
    // std::copy(pitchPoints.begin(), pitchPoints.end(), sample_curve.samples.begin());
    // tag = "pitch";
    // sample_curve.timestep = timestep;
    // retake_start = 0;
    // retake_end = pitchPoints.size();
    //
    // qDebug() << "Pitch Points:" << pitchPoints;

    // Run pitch inference. The segment will be modified in-place.
    if (!m_pitchInfer->runInPlace(segment, &s)) {
        qCritical() << "Failed to run pitch inference: " << s.msg;
        return false;
    }

    output = QString::fromStdString(segment.toJson());
    return true;
}

bool InferEngine::inferVariance(const QString &input, QString &output, QString &error) const {
    if (!m_initialized) {
        qCritical() << "inferVariance: Environment is not initialized";
        return false;
    }
    if (!m_configLoaded) {
        qCritical() << "inferVariance: Config is not loaded";
    }
    DS::Status s;
    auto segment = DS::Segment::fromJson(input.toStdString(), &s);
    if (!s.isOk()) {
        error = QString::fromStdString(s.msg);
        return false;
    }
    // Run variance inference. The segment will be modified in-place.
    if (!m_varianceInfer->runInPlace(segment, &s)) {
        qDebug() << "Failed to run variance inference: " << s.msg << '\n';
        return false;
        // return RESULT_INFERENCE_FAILED;
    }
    output = QString::fromStdString(segment.toJson());
    return true;
}

bool InferEngine::inferAcoustic(const QString &input, const QString &outputPath,
                                QString &error) const {
    if (!m_initialized) {
        qCritical() << "inferAcoustic: Environment is not initialized";
        return false;
    }
    if (!m_configLoaded) {
        qCritical() << "inferAcoustic: Config is not loaded";
    }
    DS::Status s;
    auto segment = DS::Segment::fromJson(input.toStdString(), &s);
    if (!s.isOk()) {
        error = QString::fromStdString(s.msg);
        return false;
    }
    // Run acoustic inference and export audio.
    if (!m_acousticInfer->runAndSaveAudio(segment, outputPath.toStdString(), &s)) {
        qDebug() << "Failed to run acoustic inference: " << s.msg << '\n';
        return false;
        // return RESULT_INFERENCE_FAILED;
    }
    return true;
}

void InferEngine::terminateInferDurationAsync() const {
    qInfo() << "terminateInferDurationAsync";
    if (m_durationInfer)
        m_durationInfer->terminate();
}

void InferEngine::terminateInferPitchAsync() const {
    qInfo() << "terminateInferPitchAsync";
    if (m_pitchInfer)
        m_pitchInfer->terminate();
}

void InferEngine::terminateInferVarianceAsync() const {
    qInfo() << "terminateInferVarianceAsync";
    if (m_varianceInfer)
        m_varianceInfer->terminate();
}

void InferEngine::terminateInferAcousticAsync() const {
    qInfo() << "terminateInferAcousticAsync";
    if (m_acousticInfer)
        m_acousticInfer->terminate();
}

void InferEngine::dispose() {
    delete m_durationInfer;
    m_durationInfer = nullptr;
    delete m_pitchInfer;
    m_pitchInfer = nullptr;
    delete m_varianceInfer;
    m_varianceInfer = nullptr;
    delete m_acousticInfer;
    m_acousticInfer = nullptr;
}

QString InferEngine::configPath() {
    QMutexLocker lock(&m_mutex);
    return m_configPath;
}

bool InferEngine::configLoaded() {
    QMutexLocker lock(&m_mutex);
    return m_configLoaded;
}
