//
// Created by fluty on 24-9-25.
//

#ifndef INFERENGINE_H
#define INFERENGINE_H

#define inferEngine InferEngine::instance()

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

class InferEngine final : public QObject, public Singleton<InferEngine> {
    Q_OBJECT

public:
    InferEngine();
    ~InferEngine() override;
    InferEngine(InferEngine const &) = delete;

    bool initialized();
    // void loadConfig(const QString &path);
    QString configPath();
    bool configLoaded();

private:
    friend class InitInferEngineTask;
    friend class LoadInferConfigTask;
    friend class InferDurationTask;
    friend class InferPitchTask;
    friend class InferVarianceTask;
    friend class InferAcousticTask;

    bool initialize(QString &error);
    bool runLoadConfig(const QString &path);
    bool inferDuration(const QString &input, QString &output, QString &error) const;
    bool inferPitch(const QString &input, QString &output, QString &error) const;
    bool inferVariance(const QString &input, QString &output, QString &error) const;
    bool inferAcoustic(const QString &input, const QString &outputPath, QString &error) const;
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


#endif // INFERENGINE_H
