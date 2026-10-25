#ifndef INFERENGINE_H
#define INFERENGINE_H

#define inferEngine InferEngine::instance()

#include <memory>
#include <mutex>

#include <lite/Core/Singleton.h>
#include <lite/ProjectModel/AppModel/SingerIdentifier.h>

#include <QReadWriteLock>
#include <QObject>
#include <QSet>
#include <QThreadPool>
#include <QTimer>

#include <cstdint>

#include <synthrt/Core/SynthUnit.h>

#include <lite/SynthrtEngine/SingerPipeline.h>

#include "SingerSessionCache.h"

class GenericInferModel;
class InferParam;

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
    bool initialized() const;
    bool isAboutToQuit() const noexcept;
    QString configPath() const;
    QString singerProviderPath() const;
    QString inferenceDriverPath() const;
    QString inferenceRuntimePath() const;
    QString inferenceInterpreterPath() const;

    /// Retain a singer pipeline across refreshes and track its catalog generation.
    class SingerPipelineLease final {
    public:
        SingerPipelineLease(std::shared_ptr<lite::synthrt::SingerPipeline> pipeline,
                            std::uint64_t generation);

        SingerPipelineLease(const SingerPipelineLease &) = delete;
        SingerPipelineLease &operator=(const SingerPipelineLease &) = delete;

        lite::synthrt::SingerPipeline *pipeline() const noexcept;

        bool isStale() const;

    private:
        std::shared_ptr<lite::synthrt::SingerPipeline> m_pipeline;
        std::uint64_t m_generation;
    };

    std::shared_ptr<SingerPipelineLease>
        acquireSingerSession(const SingerIdentifier &identifier) const;
    void retainSingerSessions(const QSet<SingerIdentifier> &identifiers);

    // Kicks off asynchronous initialization; called by the owner after
    // construction.
    void startInitialization();

private:
    using SingerSessionHandleList = SingerSessionCache<SingerPipelineLease>::HandleList;

    friend class InitInferEngineTask;
    friend class InferDurationTask;
    friend class InferPitchTask;
    friend class InferVarianceTask;
    friend class InferAcousticTask;
    bool initialize(QString &error);
    void dispose();
    void evictSingerSessions();
    void releaseSingerSessionsAsync(SingerSessionHandleList handles);
    void releaseDeselectedSingerSessionsAsync(SingerSessionHandleList handles);

    /// Snapshot AppOptions on the application thread before worker initialization.
    struct StartOptions {
        QString executionProvider;
        QString selectedGpuId;
        QStringList packageSearchPaths;
    };

    mutable QReadWriteLock m_engineRwLock;
    StartOptions m_startOptions;
    std::once_flag m_initFlag{};
    bool m_initialized = false;
    bool m_disposed = false;
    InferEnginePaths m_paths;

    std::mutex m_singerSessionSelectionMutex;
    mutable SingerSessionCache<SingerPipelineLease> m_singerSessions;
    QTimer m_singerSessionEvictionTimer;
    QThreadPool m_singerSessionReleasePool;
};


#endif // INFERENGINE_H
