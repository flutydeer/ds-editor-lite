//
// Created by fluty on 24-9-25.
//

#include "InferenceEngine.h"

#include "InitInferEngineTask.h"
#include "LoadInferConfigTask.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Task/TaskManager.h"

#include <dsonnxinfer/AcousticInference.h>
#include <dsonnxinfer/DurationInference.h>
#include <dsonnxinfer/PitchInference.h>
#include <dsonnxinfer/VarianceInference.h>

#include <QDebug>
#include <iostream>
#include <sstream>
#include <fstream>

InferenceEngine::InferenceEngine() {
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
    return;

    // Load input data (json format)
    std::ifstream dsFile(R"(C:\Users\fluty\Downloads\test_dsinfer_data_0.json)");
    if (!dsFile.is_open()) {
        qDebug() << "failed to open project file!\n";
        return;
        // return RESULT_PROJECT_LOAD_FAILED;
    }
    std::stringstream buffer;
    buffer << dsFile.rdbuf();
    std::string jsonData = buffer.str();

    Status s;

    // Initialize inference segment from json data
    Segment segment = Segment::fromJson(jsonData, &s);

    // Load models
    bool loadDsConfigOk;

    std::string dsConfigPath =
        R"(F:\Sound libraries\DiffSinger\OpenUtau\Singers\Junninghua_v1.4.0_DiffSinger_OpenUtau\dsconfig.yaml)";
    auto dsConfig = DsConfig::fromYAML(dsConfigPath, &loadDsConfigOk);

    auto dsVocoderConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dsvocoder" / "vocoder.yaml";
    auto dsVocoderConfig = DsVocoderConfig::fromYAML(dsVocoderConfigPath, &loadDsConfigOk);

    auto durConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dsdur" / "dsconfig.yaml";
    auto durConfig = DsDurConfig::fromYAML(durConfigPath, &loadDsConfigOk);

    auto pitchConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dspitch" / "dsconfig.yaml";
    auto pitchConfig = DsPitchConfig::fromYAML(pitchConfigPath, &loadDsConfigOk);

    auto varianceConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dsvariance" / "dsconfig.yaml";
    auto varianceConfig = DsVarianceConfig::fromYAML(varianceConfigPath, &loadDsConfigOk);

    if (!loadDsConfigOk) {
        s = {Status_ModelLoadError, "Failed to load config!"};
    }
    if (!s.isOk()) {
        return;
        // return RESULT_MODEL_LOAD_FAILED;
    }

    DurationInference durationInference(durConfig);
    durationInference.open();

    PitchInference pitchInference(pitchConfig);
    pitchInference.open();

    VarianceInference varianceInference(varianceConfig);
    varianceInference.open();

    AcousticInference acousticInference(dsConfig, dsVocoderConfig);
    acousticInference.open();

    auto trySaveSegment = [&](const Segment &currSegment, const std::string &filename) -> bool {
        auto newProject = segment.toJson(&s);
        if (!s.isOk()) {
            qDebug() << "Failed to save segment: " << s.msg << '\n';
            return false;
        }
        std::ofstream outFile(filename);
        outFile << newProject;
        outFile.close();
        return true;
    };

    trySaveSegment(segment, "result_original.json");
    // Run duration inference. The segment will be modified in-place.
    if (!durationInference.runInPlace(segment, &s)) {
        qDebug() << "Failed to run duration inference: " << s.msg << '\n';
        return;
        // return RESULT_INFERENCE_FAILED;
    }
    // Save the modified segment to JSON file
    if (!trySaveSegment(segment, "result_dur.json"))
        return;
    // return RESULT_PROJECT_SAVE_FAILED;

    // Run pitch inference. The segment will be modified in-place.
    if (!pitchInference.runInPlace(segment, &s)) {
        qDebug() << "Failed to run pitch inference: " << s.msg << '\n';
        return;
        // return RESULT_INFERENCE_FAILED;
    }
    if (!trySaveSegment(segment, "result_pitch.json"))
        return;
    // return RESULT_PROJECT_SAVE_FAILED;

    // Run variance inference. The segment will be modified in-place.
    if (!varianceInference.runInPlace(segment, &s)) {
        qDebug() << "Failed to run variance inference: " << s.msg << '\n';
        return;
        // return RESULT_INFERENCE_FAILED;
    }
    if (!trySaveSegment(segment, "result_variance.json"))
        return;
    // return RESULT_PROJECT_SAVE_FAILED;

    // Run acoustic inference and export audio.
    if (!acousticInference.runAndSaveAudio(segment, "test.wav", &s)) {
        qDebug() << "Failed to run acoustic inference: " << s.msg << '\n';
        return;
        // return RESULT_INFERENCE_FAILED;
    }

    durationInference.close();
    pitchInference.close();
    varianceInference.close();
    acousticInference.close();
}

InferenceEngine::~InferenceEngine() {
    dispose();
}

bool InferenceEngine::initialized() {
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

bool InferenceEngine::initialize(QString &error) {
    std::string errorMessage;

    // Load environment (must do this before inference)
    if (!m_env.load("onnxruntime", EP_DirectML, &errorMessage)) {
        qCritical() << "Failed to load environment:" << errorMessage;
        error += errorMessage;
        m_initialized = false;
        return false;
    }
    m_env.setDeviceIndex(0);
    m_env.setDefaultSteps(20);
    m_env.setDefaultDepth(1.0f);
    m_initialized = true;
    qInfo() << "Successfully loaded environment. Execution provider: DirectML";
    return true;
}

bool InferenceEngine::runLoadConfig(const QString &path) {
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
    auto dsConfig = DsConfig::fromYAML(dsConfigPath, &loadDsConfigOk);

    auto dsVocoderConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dsvocoder" / "vocoder.yaml";
    auto dsVocoderConfig = DsVocoderConfig::fromYAML(dsVocoderConfigPath, &loadDsConfigOk);

    auto durConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dsdur" / "dsconfig.yaml";
    auto durConfig = DsDurConfig::fromYAML(durConfigPath, &loadDsConfigOk);

    auto pitchConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dspitch" / "dsconfig.yaml";
    auto pitchConfig = DsPitchConfig::fromYAML(pitchConfigPath, &loadDsConfigOk);

    auto varianceConfigPath =
        std::filesystem::path(dsConfigPath).parent_path() / "dsvariance" / "dsconfig.yaml";
    auto varianceConfig = DsVarianceConfig::fromYAML(varianceConfigPath, &loadDsConfigOk);

    if (!loadDsConfigOk) {
        qCritical() << "Failed to load config";
        return false;
    }

    m_durationInfer = new DurationInference(durConfig);
    m_durationInfer->open();

    m_pitchInfer = new PitchInference(pitchConfig);
    m_pitchInfer->open();

    m_varianceInfer = new VarianceInference(varianceConfig);
    m_varianceInfer->open();

    m_acousticInfer = new AcousticInference(dsConfig, dsVocoderConfig);
    m_acousticInfer->open();

    qInfo() << "Successfully loaded config";
    m_configLoaded = true;
    m_configPath = path;
    return true;
}

bool InferenceEngine::inferDuration(const QString &input, QString &output, QString &error) const {
    if (!m_initialized) {
        qCritical() << "inferDuration: Environment is not initialized";
        return false;
    }
    if (!m_configLoaded) {
        qCritical() << "inferDuration: Config is not loaded";
    }
    Status s;
    auto segment = Segment::fromJson(input.toStdString(), &s);
    if (!s.isOk()) {
        error = QString::fromStdString(s.msg);
        return false;
    }

    // Run duration inference. The segment will be modified in-place.
    if (!m_durationInfer->runInPlace(segment, &s)) {
        qCritical() << "Failed to run duration inference: " << s.msg << '\n';
        return false;
    }

    output = QString::fromStdString(segment.toJson());
    return true;
}

void InferenceEngine::dispose() const {
    delete m_durationInfer;
    delete m_pitchInfer;
    delete m_varianceInfer;
    delete m_acousticInfer;
}

QString InferenceEngine::configPath() {
    QMutexLocker lock(&m_mutex);
    return m_configPath;
}

bool InferenceEngine::configLoaded() {
    QMutexLocker lock(&m_mutex);
    return m_configLoaded;
}
