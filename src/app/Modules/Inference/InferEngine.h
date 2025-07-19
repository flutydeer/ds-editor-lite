//
// Created by fluty on 24-9-25.
//

#ifndef INFERENGINE_H
#define INFERENGINE_H

#define inferEngine InferEngine::instance()

#include "Utils/Singleton.h"

#include <QMutex>
#include <QObject>

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Core/PackageRef.h>
#include <synthrt/Core/NamedObject.h>
#include <synthrt/SVS/Inference.h>
#include <synthrt/SVS/InferenceContrib.h>

class GenericInferModel;
class InferParam;

class InferEngine final : public QObject, public Singleton<InferEngine> {
    Q_OBJECT

public:
    InferEngine();
    ~InferEngine() override;

    Q_DISABLE_COPY_MOVE(InferEngine)

    bool initialized();
    // void loadConfig(const QString &path);
    QString configPath();
    bool configLoaded();

Q_SIGNALS:
    void cancelAllInferTasks();
    void recreateAllInferTasks();

private:
    friend class InitInferEngineTask;
    friend class LoadInferConfigTask;
    friend class InferDurationTask;
    friend class InferPitchTask;
    friend class InferVarianceTask;
    friend class InferAcousticTask;

    bool initialize(QString &error);
    bool loadPackage(const std::filesystem::path &packagePath, bool metadataOnly, srt::PackageRef &outPackage);
    bool loadPackage(const QString &packagePath, bool metadataOnly, srt::PackageRef &outPackage);
    bool runLoadConfig(const QString &path);
    bool inferDuration(const GenericInferModel &model, std::vector<double> &outDuration, QString &error) const;
    bool inferPitch(const GenericInferModel &model, InferParam &outPitch, QString &error) const;
    bool inferVariance(const GenericInferModel &model, QList<InferParam> &outParams, QString &error) const;
    bool inferAcoustic(const GenericInferModel &model, const QString &outputPath, QString &error) const;
    void terminateInferDurationAsync() const;
    void terminateInferPitchAsync() const;
    void terminateInferVarianceAsync() const;
    void terminateInferAcousticAsync() const;
    void dispose();

    QMutex m_mutex;
    bool m_initialized = false;
    bool m_configLoaded = false;

    srt::SynthUnit m_su;
    srt::ScopedPackageRef m_pkg;

    struct Inference {
        srt::NO<srt::InferenceImportOptions> options;
        srt::InferenceSpec *spec = nullptr;
        srt::NO<srt::Inference> session;
    };
    Inference m_duration, m_pitch, m_variance, m_acoustic, m_vocoder;

    QString m_configPath;
};


#endif // INFERENGINE_H
