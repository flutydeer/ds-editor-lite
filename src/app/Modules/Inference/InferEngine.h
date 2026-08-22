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

#include <synthrt/Core/Core/Runtime.h>
#include <diffsinger/Session/ModelSetHandle.h>

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
    // Returns a const reference to the Runtime. Intended for public, read-only access.
    const srt::core::Runtime &constRuntime() const;
    // B1b: acquireSingerSession now returns a ModelSetHandle from
    // VoicebankSession::ensureModelSet() instead of a SingerModelSession.
    // The handle is the synthrt-side chokepoint for per-stage load/start/stop;
    // ActiveInference adapts it into the same {inference, importOptions} Model
    // shape the 4 DiffSinger tasks consume.
    std::shared_ptr<ds::session::ModelSetHandle>
        acquireSingerSession(const SingerIdentifier &identifier) const;
    void retainSingerSessions(const QSet<SingerIdentifier> &identifiers);

    // Kicks off asynchronous initialization; called by the owner after
    // construction.
    void startInitialization();

private:
    using SingerSessionHandleList = SingerSessionCache<ds::session::ModelSetHandle>::HandleList;

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
    mutable SingerSessionCache<ds::session::ModelSetHandle> m_singerSessions;
    QTimer m_singerSessionEvictionTimer;
    QThreadPool m_singerSessionReleasePool;
};


#endif // INFERENGINE_H
