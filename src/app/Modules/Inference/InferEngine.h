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
    // Returns a const reference to the unit. Intended for public, read-only access.
    const srt::SynthUnit &constRuntime() const;

    /// One singer's pipeline, kept alive for as long as the lease is held.
    ///
    /// The pipeline itself belongs to SynthrtEngine, which builds it once and caches it; what this
    /// adds is the two things a cache needs and a raw pointer cannot give. Letting the last lease
    /// go tells the engine to drop the pipeline, which is what closes the five models -- the only
    /// thing here that costs real memory. And a lease knows the catalogue generation it was taken
    /// in, so a task still holding one after a voicebank rescan can tell that its pointer no
    /// longer means anything instead of following it.
    class SingerPipelineLease final {
    public:
        SingerPipelineLease(SingerIdentifier identifier, lite::synthrt::SingerPipeline *pipeline,
                            std::uint64_t generation);
        ~SingerPipelineLease();

        SingerPipelineLease(const SingerPipelineLease &) = delete;
        SingerPipelineLease &operator=(const SingerPipelineLease &) = delete;

        lite::synthrt::SingerPipeline *pipeline() const noexcept;

        /// True once the catalogue was republished under it. The cache drops such an entry and
        /// takes a fresh lease rather than handing out a pointer into released packages.
        bool isStale() const;

    private:
        SingerIdentifier m_identifier;
        lite::synthrt::SingerPipeline *m_pipeline;
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

    mutable QReadWriteLock m_engineRwLock;
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
