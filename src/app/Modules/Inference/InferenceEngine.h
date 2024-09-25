//
// Created by fluty on 24-9-25.
//

#ifndef INFERENCEENGINE_H
#define INFERENCEENGINE_H

#define inferEngine InferenceEngine::instance()

#include "Utils/Singleton.h"

#include <QMutex>
#include <QObject>

#include <dsonnxinfer/Environment.h>

namespace dsonnxinfer {
    class AcousticInference;
    class VarianceInference;
    class PitchInference;
    class DurationInference;
}

using namespace dsonnxinfer;

class InferenceEngine final : public QObject, public Singleton<InferenceEngine> {
    Q_OBJECT

public:
    InferenceEngine();
    ~InferenceEngine() override;
    InferenceEngine(InferenceEngine const &) = delete;

    bool initialized();
    void loadConfig(const QString &path);
    QString configPath();
    bool configLoaded();

private:
    friend class InitInferEngineTask;
    friend class LoadInferConfigTask;
    friend class InferDurationTask;

    bool initialize(QString &error);
    bool runLoadConfig(const QString &path);
    bool inferDuration(const QString &input, QString &output, QString &error) const;
    void dispose() const;

    QMutex m_mutex;
    bool m_initialized = false;
    bool m_configLoaded = false;
    Environment m_env;
    DurationInference *m_durationInfer = nullptr;
    PitchInference *m_pitchInfer = nullptr;
    VarianceInference *m_varianceInfer = nullptr;
    AcousticInference *m_acousticInfer = nullptr;

    QString m_configPath;
};


#endif // INFERENCEENGINE_H
