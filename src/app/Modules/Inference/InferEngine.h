//
// Created by fluty on 24-9-25.
//

#ifndef INFERENGINE_H
#define INFERENGINE_H

#define inferEngine InferEngine::instance()

#include <memory>
#include <mutex>

#include <lite/Core/Singleton.h>
#include <lite/ProjectModel/AppModel/SingerIdentifier.h>

#include <QHash>
#include <QReadWriteLock>
#include <QObject>

#include <synthrt/Core/Core/Runtime.h>
#include <diffsinger/Session/ModelSetHandle.h>

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

    // Kicks off asynchronous initialization; called by the owner after
    // construction.
    void startInitialization();

private:
    friend class InitInferEngineTask;
    friend class InferDurationTask;
    friend class InferPitchTask;
    friend class InferVarianceTask;
    friend class InferAcousticTask;
    bool initialize(QString &error);
    void dispose();

    mutable QReadWriteLock m_engineRwLock;
    std::once_flag m_initFlag{};
    bool m_initialized = false;
    bool m_disposed = false;
    InferEnginePaths m_paths;

    // Cache of ModelSetHandle per SingerIdentifier. Uses weak_ptr so handles
    // can be released when all task references are gone, but consecutive/parallel
    // tasks for the same singer can reuse the underlying ModelSet (and its
    // cached ONNX sessions) instead of recreating them each time.
    // This restores the caching behavior of the old SingerModelSession map
    // (m_singerSessions) that was lost during the VoicebankSession migration.
    mutable std::mutex m_singerHandlesMutex;
    mutable QHash<SingerIdentifier, std::weak_ptr<ds::session::ModelSetHandle>> m_singerHandles;
};


#endif // INFERENGINE_H
