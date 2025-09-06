//
// Created by fluty on 24-9-25.
//

#ifndef INFERENGINE_H
#define INFERENGINE_H

#define inferEngine InferEngine::instance()

#include "Utils/Singleton.h"
#include "Models/SingerIdentifier.h"
#include "InferenceLoader.h"

#include <QHash>
#include <QMutex>
#include <QReadWriteLock>
#include <QObject>

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Core/PackageRef.h>
#include <synthrt/Core/NamedObject.h>
#include <synthrt/SVS/Inference.h>
#include <synthrt/SVS/InferenceContrib.h>

class GenericInferModel;
class InferParam;

namespace srt {
    class SingerSpec;
}

struct InferEnginePaths {
    QString config;
    QString singerProvider;
    QString inferenceDriver;
    QString inferenceRuntime;
    QString inferenceInterpreter;
};

class InferEngine final : public QObject {
    Q_OBJECT

private:
    explicit InferEngine(QObject *parent = nullptr);
    ~InferEngine() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(InferEngine)
    Q_DISABLE_COPY_MOVE(InferEngine)

public:
    bool initialized();
    // void loadConfig(const QString &path);
    QString configPath();
    QString singerProviderPath();
    QString inferenceDriverPath();
    QString inferenceRuntimePath();
    QString inferenceInterpreterPath();
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

    struct InferenceSet {
        srt::NO<srt::Inference> duration, pitch, variance, acoustic, vocoder;
    };

    bool initialize(QString &error);
    bool loadPackage(const std::filesystem::path &packagePath, bool noLoad, srt::PackageRef &outPackage);
    bool loadPackage(const QString &packagePath, bool noLoad, srt::PackageRef &outPackage);
    bool loadPackageAndAllSingers(const QString &packagePath, srt::PackageRef &outPackage);
    void loadAllSingersFromPackage(const srt::PackageRef &package);
    static srt::SingerSpec *findSingerForPackage(const srt::PackageRef &package, const QString &singerId);
    static srt::SingerSpec *findSingerForPackage(const srt::PackageRef &package, std::string_view singerId);
    std::shared_ptr<InferenceLoader> findLoaderForSinger(const SingerIdentifier &identifier) const;
#if false
    bool loadInferences(const QString &path);
#endif
    bool loadInferencesForSinger(const SingerIdentifier &identifier);
    void terminateInferDurationAll() const;
    void terminateInferPitchAll() const;
    void terminateInferVarianceAll() const;
    void terminateInferAcousticAll() const;
    void dispose();
    // Provides mutable access to the SynthUnit. Restricted to friends and member functions.
    srt::SynthUnit &synthUnit();

    QMutex m_mutex;
    bool m_initialized = false;

    srt::SynthUnit m_su;

    mutable QReadWriteLock m_loaderRwLock, m_inferenceRwLock;
    QHash<SingerIdentifier, std::shared_ptr<InferenceLoader>> m_loaders;
    QHash<SingerIdentifier, InferenceSet> m_inferences;
    InferEnginePaths m_paths;
};


#endif // INFERENGINE_H
