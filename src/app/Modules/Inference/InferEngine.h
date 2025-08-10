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

struct InferEnginePaths {
    QString config;
    QString singerProvider;
    QString inferenceDriver;
    QString inferenceRuntime;
    QString inferenceInterpreter;
};

class InferEngine final : public QObject, public Singleton<InferEngine> {
    Q_OBJECT

public:
    InferEngine();
    ~InferEngine() override;

    Q_DISABLE_COPY_MOVE(InferEngine)

    bool initialized();
    // void loadConfig(const QString &path);
    QString configPath();
    QString singerProviderPath();
    QString inferenceDriverPath();
    QString inferenceRuntimePath();
    QString inferenceInterpreterPath();
    bool configLoaded();
    // Returns a const reference to the SynthUnit. Intended for public, read-only access.
    const srt::SynthUnit &constSynthUnit() const;

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
    friend class ExtractMidiTask;
    friend class ExtractPitchTask;
    friend class PackageManager;

    bool initialize(QString &error);
    bool loadPackage(const std::filesystem::path &packagePath, bool metadataOnly, srt::PackageRef &outPackage);
    bool loadPackage(const QString &packagePath, bool metadataOnly, srt::PackageRef &outPackage);
    bool loadInferences(const QString &path);
    bool inferDuration(const GenericInferModel &model, std::vector<double> &outDuration, QString &error) const;
    bool inferPitch(const GenericInferModel &model, InferParam &outPitch, QString &error) const;
    bool inferVariance(const GenericInferModel &model, QList<InferParam> &outParams, QString &error) const;
    bool inferAcoustic(const GenericInferModel &model, const QString &outputPath, QString &error) const;
    void terminateInferDurationAsync() const;
    void terminateInferPitchAsync() const;
    void terminateInferVarianceAsync() const;
    void terminateInferAcousticAsync() const;
    void dispose();
    // Provides mutable access to the SynthUnit. Restricted to friends and member functions.
    srt::SynthUnit &synthUnit();

    QMutex m_mutex;
    bool m_initialized = false;
    bool m_configLoaded = false;

    srt::SynthUnit m_su;

    InferEnginePaths m_paths;

    struct InferenceSet {
        srt::NO<srt::Inference> duration, pitch, variance, acoustic, vocoder;
    };

    struct PackageContext {
        srt::ScopedPackageRef pkg;
        InferenceSet inference;
    };

    PackageContext m_pkgCtx;
    std::unordered_map<std::string, std::shared_ptr<PackageContext>> m_pkgCtxs;
};


#endif // INFERENGINE_H
