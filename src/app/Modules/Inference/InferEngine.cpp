#include "InferEngine.h"

#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include <lite/Tasking/TaskManager.h>
#include "Modules/Inference/Models/GenericInferModel.h"
#include <lite/SynthrtEngine/SynthrtBootstrap.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include "ExecutionProvider.h"
#include "Tasks/InitInferEngineTask.h"

#include <synthrt/SVS/InferenceContrib.h>
#include <synthrt/SVS/SingerContrib.h>
#include <synthrt/Support/Logging.h>

#include <QCoreApplication>

#include "Utils/DmlGpuUtils.h"
#include <lite/Support/Log.h>
#include <lite/ADT/Expected.h>
#include <lite/Support/StringUtils.h>
#include <lite/Support/VersionUtils.h>

#include <QDebug>
#include <QDir>
#include <QReadWriteLock>
#include <QStandardPaths>
#include <QString>

#include <algorithm>
#include <chrono>

#include "Tasks/InferTaskCommon.h"
#include "Utils/CudaGpuUtils.h"

static void log_report_callback(const int level, const srt::LogContext &ctx,
                                const std::string_view &msg) {
    const QString message_qstr = QString::fromUtf8(msg.data(), msg.size());
    switch (level) {
        case srt::Logger::Fatal:
            Log::f(ctx.category, message_qstr);
            break;
        case srt::Logger::Critical:
            Log::e(ctx.category, message_qstr);
            break;
        case srt::Logger::Warning:
            Log::w(ctx.category, message_qstr);
            break;
        case srt::Logger::Information:
        case srt::Logger::Success:
            Log::i(ctx.category, message_qstr);
            break;
        case srt::Logger::Debug:
        default:
            Log::d(ctx.category, message_qstr);
            break;
    }
}

static constexpr int kSingerSessionScanIntervalMilliseconds = 10'000;

InferEngine::InferEngine(QObject *parent) : QObject(parent) {
    srt::Logger::setLogCallback(log_report_callback);
    m_singerSessionReleasePool.setMaxThreadCount(1);

    // The engine is not disposed on QCoreApplication::aboutToQuit. That signal is emitted when
    // the event loop exits, while inference and extraction tasks may still be running, and
    // shutting down the runtime under those tasks causes a crash. The destructor of AppContext
    // drains the task manager first and destroys this engine afterwards; only this order is safe.

    m_singerSessionEvictionTimer.setInterval(kSingerSessionScanIntervalMilliseconds);
    m_singerSessionEvictionTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_singerSessionEvictionTimer, &QTimer::timeout, this,
            &InferEngine::evictSingerSessions);
    m_singerSessionEvictionTimer.start();
}

InferEngine::~InferEngine() {
    dispose();
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(InferEngine)

void InferEngine::startInitialization() {
    std::call_once(m_initFlag, [this] {
        {
            QWriteLocker lock(&m_engineRwLock);
            m_startOptions.executionProvider = appOptions->inference()->executionProvider;
            m_startOptions.selectedGpuId = appOptions->inference()->selectedGpuId;
            m_startOptions.packageSearchPaths = appOptions->general()->packageSearchPaths;
        }
        const auto initTask = new InitInferEngineTask;
        connect(initTask, &Task::finished, this, [=] {
            taskManager->removeTask(initTask);
            QWriteLocker lock(&m_engineRwLock);
            if (m_disposed || SynthrtEngine::instance().isAboutToQuit()) {
                delete initTask;
                return;
            }

            const bool languageReady = initTask->success.load(std::memory_order_acquire);
            const bool runtimeReady = SynthrtEngine::instance().initialized();
            appStatus->inferEngineEnvStatus =
                runtimeReady ? AppStatus::ModuleStatus::Ready : AppStatus::ModuleStatus::Error;
            if (languageReady) {
                appStatus->languageModuleError = QString();
                appStatus->languageModuleStatus = AppStatus::ModuleStatus::Ready;
                // No warm-up is required. wolf loads a language on its first conversion and
                // keeps it loaded, so only the first conversion of each language is delayed.
            } else {
                appStatus->languageModuleError = initTask->errorMessage;
                appStatus->languageModuleStatus = AppStatus::ModuleStatus::Error;
            }
            delete initTask;
        });
        appStatus->inferEngineEnvStatus = AppStatus::ModuleStatus::Loading;
        appStatus->languageModuleStatus = AppStatus::ModuleStatus::Loading;
        appStatus->languageModuleError = QString();
        taskManager->addAndStartTask(initTask);
    });
}

bool InferEngine::initialized() const {
    QReadLocker lock(&m_engineRwLock);
    return m_initialized;
}

bool InferEngine::isAboutToQuit() const noexcept {
    return SynthrtEngine::instance().isAboutToQuit();
}

bool InferEngine::initialize(QString &error) {
    QWriteLocker lock(&m_engineRwLock);
    if (m_disposed) {
        error = "Application is about to quit.";
        return false;
    }
    if (m_initialized) {
        qDebug() << "InferEngine already initialized";
        return true;
    }

    // The options are read from the copy made by startInitialization() on the application thread.
    const auto options = m_startOptions;
    const auto persistedProvider = options.executionProvider;
    const auto requestedProvider = ExecutionProviderUtils::fromString(persistedProvider);
    const auto gpuDeviceList = requestedProvider ? [&requestedProvider]() -> QList<GpuInfo> {
        if (*requestedProvider == ExecutionProvider::DirectML)
            return DmlGpuUtils::getGpuList();
        if (*requestedProvider == ExecutionProvider::Cuda)
            return CudaGpuUtils::getGpuList();
        return {};
    }()
        : QList<GpuInfo>{};

    // A provider this machine cannot actually run must never survive here:
    // failing the whole engine initialization would also stall the voicebank
    // scan, which waits on the synthrt session (PackageManager::initialize).
    const auto resolution =
        ExecutionProviderUtils::resolve(persistedProvider, {.gpuFound = !gpuDeviceList.isEmpty()});
    const auto ep = ExecutionProviderUtils::toString(resolution.provider);
    // Publish before SynthrtEngine::initialize(): every inference task and the
    // DirectML serialization guard read this once the engine reports ready.
    ExecutionProviderUtils::setEffective(resolution.provider);
    if (resolution.changed) {
        qWarning().noquote() << QStringLiteral(
                                    "InferEngine: execution provider '%1' is unusable (%2); "
                                    "falling back to '%3'.")
                                    .arg(persistedProvider, resolution.reason, ep);
        // Persist the correction and publish it from the application thread:
        // initialize() runs on the initialization task thread, while AppOptions
        // and AppStatus are owned by the application thread.
        QMetaObject::invokeMethod(
            qApp,
            [unavailableProvider = persistedProvider, ep] {
                appOptions->inference()->executionProvider = ep;
                appOptions->inference()->selectedGpuIndex = -1;
                appOptions->inference()->selectedGpuId.clear();
                appOptions->saveAndNotify(AppOptionsGlobal::Inference);
                appStatus->unavailableExecutionProvider = unavailableProvider;
            },
            Qt::QueuedConnection);
    }

    const auto [index, description, deviceId, memory] = [&resolution, &options]() -> GpuInfo {
        if (resolution.provider == ExecutionProvider::DirectML) {
            auto selectedGpu_ =
                DmlGpuUtils::getGpuByPciDeviceVendorIdString(options.selectedGpuId);
            if (selectedGpu_.index < 0) {
                qInfo() << "Auto selecting GPU";
                selectedGpu_ = DmlGpuUtils::getRecommendedGpu();
            } else {
                qInfo() << "Selecting GPU";
            }
            return selectedGpu_;
        }
        if (resolution.provider == ExecutionProvider::Cuda) {
            auto selectedGpu_ = CudaGpuUtils::getGpuByUuid(options.selectedGpuId);
            if (selectedGpu_.index < 0) {
                qInfo() << "Auto selecting GPU";
                selectedGpu_ = CudaGpuUtils::getRecommendedGpu();
            } else {
                qInfo() << "Selecting GPU";
            }
            return selectedGpu_;
        }
        return {};
    }();

    if (isAboutToQuit()) {
        error = "Application is about to quit.";
        return false;
    }

    const auto pluginRootDir = SynthrtEngine::defaultPluginRoot();

    QStringList packagePathsQt;
    for (const auto &pathQt : options.packageSearchPaths) {
        const auto path = StringUtils::qstr_to_path(pathQt);
        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        const bool isDirectory = exists && std::filesystem::is_directory(path, error);
        if (error || !isDirectory) {
            qWarning().noquote() << QStringLiteral(
                                        "Skipping inaccessible package search path '%1': %2")
                                        .arg(pathQt, error ? QString::fromStdString(error.message())
                                                           : QStringLiteral("not a directory"));
            continue;
        }
        packagePathsQt.append(pathQt);
    }
    // Search paths for dependencies, which differ from the directories of the voicebank scan. A
    // language package that a voicebank declares as a dependency is resolved through these paths.
    const QStringList languagePackagePaths{
        StringUtils::path_to_qstr(SynthrtEngine::defaultLanguagePackagePath())};
    // Each runtime flavor is deployed in its own subdirectory, because the CUDA payload contains
    // DLLs with the same names as the DirectML payload. The CUDA execution provider therefore
    // uses its own subdirectory, and every other provider uses the flavor deployed at the runtime
    // root. The runtime gate verifies this layout at build and install time
    // (cmake/OrtRuntimeGate.cmake).
    const auto runtimePath = resolution.provider == ExecutionProvider::Cuda
                                 ? SynthrtEngine::defaultCudaRuntimePath()
                                 : SynthrtEngine::defaultRuntimePath();
    if (!SynthrtEngine::instance().initialize(packagePathsQt, languagePackagePaths, ep, index,
                                              SynthrtEngine::defaultPluginRoot(), runtimePath)) {
        error = QStringLiteral("Failed to initialize SynthrtEngine");
        return false;
    }

    // These paths are displayed on the settings page. They are read from the list that the engine
    // registered, so that the displayed paths are the searched paths.
    const auto directoriesOf = [&pluginRootDir](const std::string &category) {
        QStringList result;
        for (const auto &entry : lite::synthrt::pluginCategories(pluginRootDir)) {
            if (entry.category == category) {
                for (const auto &directory : entry.directories) {
                    result.append(StringUtils::path_to_qstr(directory));
                }
            }
        }
        return result.join(QStringLiteral("; "));
    };
    m_paths.singerProvider = directoriesOf(srt::SingerCategory::NAME);
    m_paths.inferenceDriver =
        StringUtils::path_to_qstr(lite::synthrt::driverDirectory(pluginRootDir));
    m_paths.inferenceInterpreter = directoriesOf(srt::InferenceCategory::NAME);
    m_paths.inferenceRuntime = StringUtils::path_to_qstr(runtimePath);

    if (ExecutionProviderUtils::requiresGpu(resolution.provider)) {
        qInfo().noquote() << QStringLiteral("GPU: %1, Device ID: %2, Memory: %3")
                                 .arg(description)
                                 .arg(deviceId)
                                 .arg(memory);
    }

    m_initialized = true;
    qInfo().noquote() << "Successfully initialized InferEngine. Execution provider:" << ep;
    return true;
}

InferEngine::SingerPipelineLease::SingerPipelineLease(
    std::shared_ptr<lite::synthrt::SingerPipeline> pipeline, std::uint64_t generation)
    : m_pipeline(std::move(pipeline)), m_generation(generation) {
}

lite::synthrt::SingerPipeline *InferEngine::SingerPipelineLease::pipeline() const noexcept {
    return m_pipeline.get();
}

bool InferEngine::SingerPipelineLease::isStale() const {
    return SynthrtEngine::instance().catalogGeneration() != m_generation;
}

std::shared_ptr<InferEngine::SingerPipelineLease>
    InferEngine::acquireSingerSession(const SingerIdentifier &identifier) const {
    // The engines are queried instead of AppStatus, because this function runs on inference task
    // threads and AppStatus belongs to the application thread. InferEngine is initialized only
    // after SynthrtEngine, which is the condition that the Ready status reports.
    if (!initialized() || !SynthrtEngine::instance().initialized()) {
        qCritical() << "acquireSingerSession: inference runtime is not ready" << identifier;
        return {};
    }
    return m_singerSessions.acquire(identifier, [&identifier] {
        // The generation is read before the pipeline. A rescan that completes in between
        // therefore marks the lease stale, and the cache replaces the lease instead of keeping a
        // pipeline of the previous scan.
        const auto generation = SynthrtEngine::instance().catalogGeneration();
        auto built = SynthrtEngine::instance().pipelineFor(identifier);
        if (!built) {
            qCritical().noquote().nospace()
                << "acquireSingerSession: could not build the pipeline for " << identifier << ": "
                << QString::fromStdString(built.error().toString());
            return std::shared_ptr<SingerPipelineLease>{};
        }
        return std::make_shared<SingerPipelineLease>(built.take(), generation);
    });
}

void InferEngine::retainSingerSessions(const QSet<SingerIdentifier> &identifiers) {
    QReadLocker lock(&m_engineRwLock);
    std::lock_guard selectionLock(m_singerSessionSelectionMutex);
    auto result = m_singerSessions.retainOnly(identifiers);
    if (!m_initialized || m_disposed) {
        if (result.released > 0) {
            qInfo() << "Singer session retention dropped" << result.released
                    << "deselected resident entries";
        }
        releaseSingerSessionsAsync(std::move(result.handles));
        return;
    }
    releaseDeselectedSingerSessionsAsync(std::move(result.handles));
}

void InferEngine::releaseSingerSessionsAsync(SingerSessionHandleList handles) {
    if (handles.empty()) {
        return;
    }
    m_singerSessionReleasePool.start([handles = std::move(handles)]() mutable {
        InferDirectMLSerializationGuard dmlGuard;
        handles.clear();
    });
}

void InferEngine::releaseDeselectedSingerSessionsAsync(SingerSessionHandleList handles) {
    // Releasing the leases is sufficient; loaded packages are not unloaded. A loaded package
    // consists of its declarations and imports, which occupy negligible memory. The five models,
    // which are the expensive resources, are released with the pipeline of the lease.
    const auto releasedSessions = handles.size();
    m_singerSessionReleasePool.start([handles = std::move(handles), releasedSessions]() mutable {
        InferDirectMLSerializationGuard dmlGuard;
        handles.clear();
        if (releasedSessions > 0) {
            qInfo().noquote().nospace()
                << "Singer session retention updated: dropped=" << releasedSessions;
        }
    });
}

void InferEngine::evictSingerSessions() {
    QReadLocker lock(&m_engineRwLock);
    if (!m_initialized || m_disposed) {
        return;
    }

    const auto option = appOptions->inference();
    const auto capacity = static_cast<std::size_t>(
        InferenceOption::normalizeSingerSessionCacheCapacity(option->singerSessionCacheCapacity));
    const auto idleTimeout =
        std::chrono::seconds(InferenceOption::normalizeSingerSessionIdleTimeoutSeconds(
            option->singerSessionIdleTimeoutSeconds));
    auto result = m_singerSessions.evict(capacity, idleTimeout);
    if (result.idle > 0 || result.capacity > 0) {
        qInfo().noquote().nospace()
            << "Singer session retention scan: idleReleased=" << result.idle
            << ", lruReleased=" << result.capacity << ", capacity=" << capacity
            << ", idleTimeoutSeconds=" << idleTimeout.count();
        releaseSingerSessionsAsync(std::move(result.handles));
    }
}

void InferEngine::dispose() {
    QWriteLocker lock(&m_engineRwLock);
    if (m_disposed) {
        return;
    }
    m_singerSessionEvictionTimer.stop();
    m_disposed = true;
    m_initialized = false;
    qDebug() << "dispose InferEngine inference sessions";
    SingerSessionHandleList handles;
    {
        std::lock_guard selectionLock(m_singerSessionSelectionMutex);
        handles = m_singerSessions.clear();
    }
    releaseSingerSessionsAsync(std::move(handles));
    m_singerSessionReleasePool.waitForDone();
    SynthrtEngine::instance().shutdown();
}

QString InferEngine::configPath() const {
    QReadLocker lock(&m_engineRwLock);
    return m_paths.config;
}

QString InferEngine::singerProviderPath() const {
    QReadLocker lock(&m_engineRwLock);
    return m_paths.singerProvider;
}

QString InferEngine::inferenceDriverPath() const {
    QReadLocker lock(&m_engineRwLock);
    return m_paths.inferenceDriver;
}

QString InferEngine::inferenceRuntimePath() const {
    QReadLocker lock(&m_engineRwLock);
    return m_paths.inferenceRuntime;
}

QString InferEngine::inferenceInterpreterPath() const {
    QReadLocker lock(&m_engineRwLock);
    return m_paths.inferenceInterpreter;
}
