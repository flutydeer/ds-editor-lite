//
// Created by fluty on 24-9-25.
//

#include "InferenceEngine.h"

#include "InitInferEngineTask.h"
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
            appStatus->inferenceEngineStatus = AppStatus::ModuleStatus::Ready;
        else
            appStatus->inferenceEngineStatus = AppStatus::ModuleStatus::Error;
        delete initTask;
    });
    taskManager->addAndStartTask(initTask);
    appStatus->inferenceEngineStatus = AppStatus::ModuleStatus::Loading;
    return;

    m_env.setDeviceIndex(0);
    m_env.setDefaultSteps(20);
    m_env.setDefaultDepth(1.0f);

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
}

bool InferenceEngine::initialize(QString &error) {
    std::string errorMessage;

    // Load environment (must do this before inference)
    if (!m_env.load("onnxruntime", EP_DirectML, &errorMessage)) {
        qCritical() << "Failed to load environment:" << errorMessage;
        error += errorMessage;
        m_initialized = false;
        return false;
    }
    qInfo() << "Successfully loaded environment. Execution provider: DirectML";
    m_initialized = true;
    return true;
}

bool InferenceEngine::initialized() {
    QMutexLocker lock(&m_mutex);
    return m_initialized;
}

bool InferenceEngine::loadConfig(const QString &path) {
    m_configPath = path;
    return true;
}

QString InferenceEngine::config() {
    QMutexLocker lock(&m_mutex);
    return m_configPath;
}
