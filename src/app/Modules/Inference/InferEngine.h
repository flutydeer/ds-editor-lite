#ifndef INFERENGINE_H
#define INFERENGINE_H

#define inferEngine InferEngine::instance()

#include <atomic>
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

// Immutable startup snapshot captured on the application thread. The
// background initialization task reads only this value, never AppOptions or
// other GUI-owned state, and never publishes a correction over a setting the
// user changed while initialization was running.
struct InferEngineInitInput {
    QString executionProvider;
    int selectedGpuIndex = 0;
    QString selectedGpuId;
    QStringList packageSearchPaths;
    quint64 generation = 0;
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
    void releaseDeselectedSingerSessionsAsync(SingerSessionHandleList handles,
                                              quint64 generation);
    quint64 publishSingerSelection(const QSet<SingerIdentifier> &identifiers);

    // m_engineRwLock protects only the short publication of m_paths. It must
    // never be held while enumerating GPUs, loading plugins or creating an ORT
    // session: the GUI reads those same state flags during startup.
    mutable QReadWriteLock m_engineRwLock;
    std::once_flag m_initFlag{};
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_disposed{false};
    InferEnginePaths m_paths;
    InferEngineInitInput m_initInput;

    std::mutex m_singerSessionSelectionMutex;
    QSet<SingerIdentifier> m_latestSingerSelection;
    std::atomic<quint64> m_selectionGeneration{0};
    std::atomic<bool> m_sessionMaintenanceScheduled{false};
    mutable SingerSessionCache<ds::session::ModelSetHandle> m_singerSessions;
    QTimer m_singerSessionEvictionTimer;
    QThreadPool m_singerSessionReleasePool;
};


#endif // INFERENGINE_H
