#include "InferEngine.h"

#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include <lite/Tasking/TaskManager.h>
#include "Modules/Inference/Models/GenericInferModel.h"
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include "ExecutionProvider.h"
#include "Tasks/InitInferEngineTask.h"

#include <synthrt/Core/Support/Logging.h>

#include <QCoreApplication>
#include <QScopeGuard>

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

static void log_report_callback(const int level, const srt::core::LogContext &ctx,
                                const std::string_view &msg) {
    const QString message_qstr = QString::fromUtf8(msg.data(), msg.size());
    switch (level) {
        case srt::core::Logger::Fatal:
            Log::f(ctx.category, message_qstr);
            break;
        case srt::core::Logger::Critical:
            Log::e(ctx.category, message_qstr);
            break;
        case srt::core::Logger::Warning:
            Log::w(ctx.category, message_qstr);
            break;
        case srt::core::Logger::Information:
        case srt::core::Logger::Success:
            Log::i(ctx.category, message_qstr);
            break;
        case srt::core::Logger::Debug:
        default:
            Log::d(ctx.category, message_qstr);
            break;
    }
}

static bool packageIsSelected(const ds::session::LoadedVoicebankInfo &package,
                              const QSet<SingerIdentifier> &identifiers) {
    const auto packageId = QString::fromStdString(package.packageId);
    const auto packageVersion =
        QVersionNumber::fromString(QString::fromStdString(package.version.toString()));
    return std::any_of(identifiers.cbegin(), identifiers.cend(),
                       [&packageId, &packageVersion](const SingerIdentifier &identifier) {
                           return identifier.packageId == packageId &&
                                  QVersionNumber::compare(identifier.packageVersion,
                                                          packageVersion) == 0;
                       });
}

static constexpr int kSingerSessionScanIntervalMilliseconds = 10'000;

InferEngine::InferEngine(QObject *parent) : QObject(parent) {
    srt::core::Logger::setLogCallback(log_report_callback);
    m_singerSessionReleasePool.setMaxThreadCount(1);

    // Prevent crash on app exit
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
            &InferEngine::dispose);

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
        // Capture all GUI-owned inputs here, on the application thread, before
        // the task starts. The worker reads only this immutable snapshot.
        const auto *inference = appOptions->inference();
        m_initInput.executionProvider = inference->executionProvider;
        m_initInput.selectedGpuIndex = inference->selectedGpuIndex;
        m_initInput.selectedGpuId = inference->selectedGpuId;
        m_initInput.packageSearchPaths = appOptions->general()->packageSearchPaths;
        m_initInput.generation = 1;

        const auto initTask = new InitInferEngineTask;
        connect(
            initTask, &Task::finished, this,
            [this, initTask] {
                // Deliberately lock-free. The completion callback runs on the
                // application thread; taking the engine write lock here would
                // stall the GUI behind any long-running background work.
                // Never publish a stale Ready once shutdown has started.
                if (m_disposed.load(std::memory_order_acquire) ||
                    SynthrtEngine::instance().isAboutToQuit()) {
                    return;
                }

                const bool languageReady = initTask->success.load(std::memory_order_acquire);
                const bool runtimeReady = SynthrtEngine::instance().runtimeInitialized();
                appStatus->inferEngineEnvStatus =
                    runtimeReady ? AppStatus::ModuleStatus::Ready : AppStatus::ModuleStatus::Error;
                if (languageReady) {
                    appStatus->languageModuleError = QString();
                    appStatus->languageModuleStatus = AppStatus::ModuleStatus::Ready;
                    // Warm up G2P models in the background so the first
                    // FillLyric / inference conversion does not stall. The engine
                    // was initialized with deferLanguageModels=true; this kicks
                    // off the deferred Stage 2 load on a worker thread, matching
                    // how PackageManager scans packages asynchronously.
                    QThreadPool::globalInstance()->start(
                        [] { SynthrtEngine::instance().warmUpLanguageModels(); });
                } else {
                    appStatus->languageModuleError = initTask->errorMessage;
                    appStatus->languageModuleStatus = AppStatus::ModuleStatus::Error;
                }
            },
            Qt::QueuedConnection);
        // Cleanup must survive engine teardown while completion is still queued.
        connect(
            initTask, &Task::finished, initTask,
            [initTask] {
                taskManager->removeTask(initTask);
                initTask->deleteLater();
            },
            Qt::QueuedConnection);
        appStatus->inferEngineEnvStatus = AppStatus::ModuleStatus::Loading;
        appStatus->languageModuleStatus = AppStatus::ModuleStatus::Loading;
        appStatus->languageModuleError = QString();
        taskManager->addAndStartTask(initTask);
    });
}

bool InferEngine::initialized() const {
    return m_initialized.load(std::memory_order_acquire);
}

bool InferEngine::isAboutToQuit() const noexcept {
    return SynthrtEngine::instance().isAboutToQuit();
}

bool InferEngine::initialize(QString &error) {
    // Deliberately no long write lock here. GPU enumeration, plugin loading and
    // ORT session creation can block for seconds; holding m_engineRwLock across
    // them makes the GUI's initialized()/path reads stall before the first
    // window appears. Only the final path publication takes a short write lock.
    const auto finishAttempt = qScopeGuard([] {
        SynthrtEngine::instance().completeInitializationAttempt();
    });
    if (m_disposed.load(std::memory_order_acquire)) {
        error = "Application is about to quit.";
        return false;
    }
    if (m_initialized.load(std::memory_order_acquire)) {
        qDebug() << "InferEngine already initialized";
        return true;
    }

    const auto persistedProvider = m_initInput.executionProvider;
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
            [unavailableProvider = persistedProvider, ep,
             expectedGpuId = m_initInput.selectedGpuId] {
                // The fallback is computed from the startup snapshot. If the
                // user changed the provider or GPU while initialization was
                // running, their newer choice wins: an old callback must never
                // overwrite it. The diagnostic is published in that case only.
                auto *inference = appOptions->inference();
                if (inference->executionProvider != unavailableProvider ||
                    inference->selectedGpuId != expectedGpuId) {
                    appStatus->unavailableExecutionProvider = unavailableProvider;
                    return;
                }
                inference->executionProvider = ep;
                inference->selectedGpuIndex = -1;
                inference->selectedGpuId.clear();
                appOptions->saveAndNotify(AppOptionsGlobal::Inference);
                appStatus->unavailableExecutionProvider = unavailableProvider;
            },
            Qt::QueuedConnection);
    }

    // Copy the immutable snapshot field into a local so the GPU selection
    // lambda does not need to capture this.
    const auto selectedGpuId = m_initInput.selectedGpuId;
    const auto [index, description, deviceId, memory] =
        [&resolution, &selectedGpuId]() -> GpuInfo {
        if (resolution.provider == ExecutionProvider::DirectML) {
            auto selectedGpu_ = DmlGpuUtils::getGpuByPciDeviceVendorIdString(selectedGpuId);
            if (selectedGpu_.index < 0) {
                qInfo() << "Auto selecting GPU";
                selectedGpu_ = DmlGpuUtils::getRecommendedGpu();
            } else {
                qInfo() << "Selecting GPU";
            }
            return selectedGpu_;
        }
        if (resolution.provider == ExecutionProvider::Cuda) {
            auto selectedGpu_ = CudaGpuUtils::getGpuByUuid(selectedGpuId);
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

    const auto pluginRootDir = SynthrtEngine::pluginRoot();
    const auto diffsingerPluginDir = pluginRootDir / "diffsinger";
    const auto singerProviderDir = diffsingerPluginDir / "singerproviders";
    const auto inferenceDriverDir = pluginRootDir / "srt-driver" / "inferencedrivers";
    const auto inferenceInterpreterDir = diffsingerPluginDir / "inferenceinterpreters";

    QStringList packagePathsQt;
    for (const auto &pathQt : m_initInput.packageSearchPaths) {
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
    const auto g2pPackageDir = pluginRootDir / "srt-g2p" / "G2pPackages";
    const QStringList g2pPackagePaths{StringUtils::path_to_qstr(g2pPackageDir)};
    // Defer G2P model loading to first use so startup stays fast: the G2P
    // convert tasks call VoicebankSession::ensureLanguageReady() first, which
    // triggers LanguageService::initializeModels() (idempotent + mutex-guarded
    // in synthrt) on demand. Nothing in this method reads G2P outputs.
    if (!SynthrtEngine::instance().initialize(packagePathsQt, g2pPackagePaths, ep, index,
                                              /*deferLanguageModels=*/true)) {
        error = QStringLiteral("Failed to initialize SynthrtEngine");
        return false;
    }

    const auto runtimeDir = inferenceDriverDir / "srt-onnxdriver" / "runtimes" / "onnx" /
                            (resolution.provider == ExecutionProvider::Cuda ? "cuda" : "default");
    // Short publication only. No GPU/ORT work happens under this lock.
    {
        QWriteLocker lock(&m_engineRwLock);
        m_paths.singerProvider = StringUtils::path_to_qstr(singerProviderDir);
        m_paths.inferenceDriver = StringUtils::path_to_qstr(inferenceDriverDir);
        m_paths.inferenceInterpreter = StringUtils::path_to_qstr(inferenceInterpreterDir);
        m_paths.inferenceRuntime = StringUtils::path_to_qstr(runtimeDir);
    }

    if (ExecutionProviderUtils::requiresGpu(resolution.provider)) {
        qInfo().noquote() << QStringLiteral("GPU: %1, Device ID: %2, Memory: %3")
                                 .arg(description)
                                 .arg(deviceId)
                                 .arg(memory);
    }

    {
        // Atomic with dispose(): a background task that resumes after shutdown
        // must not publish Ready again.
        std::lock_guard stateLock(m_stateMutex);
        if (m_disposed.load(std::memory_order_acquire)) {
            error = "Application is about to quit.";
            return false;
        }
        m_initialized.store(true, std::memory_order_release);
    }
    qInfo().noquote() << "Successfully initialized InferEngine. Execution provider:" << ep;
    return true;
}

std::shared_ptr<ds::session::ModelSetHandle>
    InferEngine::acquireSingerSession(const SingerIdentifier &identifier) const {
    if (appStatus->inferEngineEnvStatus != AppStatus::ModuleStatus::Ready || !initialized()) {
        qCritical() << "acquireSingerSession: inference runtime is not ready" << identifier;
        return {};
    }
    return m_singerSessions.acquire(identifier, [&identifier] {
        auto &session = SynthrtEngine::instance().session();
        auto exp = session.ensureModelSet(identifier);
        if (!exp) {
            if (exp.error().code() != srt::core::ErrorCode::StaleModelSet) {
                qCritical().noquote().nospace()
                    << "acquireSingerSession: ensureModelSet failed for " << identifier << ": "
                    << QString::fromUtf8(exp.error().messageWithLocation());
                return std::shared_ptr<ds::session::ModelSetHandle>{};
            }
            qWarning() << "acquireSingerSession: StaleModelSet for" << identifier
                       << "; retrying ensureModelSet once";
            exp = session.ensureModelSet(identifier);
            if (!exp) {
                qCritical().noquote().nospace()
                    << "acquireSingerSession: ensureModelSet retry failed for " << identifier
                    << ": " << QString::fromUtf8(exp.error().messageWithLocation());
                return std::shared_ptr<ds::session::ModelSetHandle>{};
            }
        }
        return *exp;
    });
}

quint64 InferEngine::publishSingerSelection(const QSet<SingerIdentifier> &identifiers) {
    std::lock_guard selectionLock(m_singerSessionSelectionMutex);
    m_latestSingerSelection = identifiers;
    return m_selectionGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
}

void InferEngine::retainSingerSessions(const QSet<SingerIdentifier> &identifiers) {
    // Publish the selection only. retainOnly() takes the same cache mutex that
    // a background acquire() holds across the whole model factory, so calling
    // it here would stall the GUI behind package loading and model creation.
    publishSingerSelection(identifiers);
    scheduleSessionMaintenance();
}

void InferEngine::scheduleSessionMaintenance() {
    if (m_disposed.load(std::memory_order_acquire))
        return;
    // Coalesce bursts of selection changes: one queued pass consumes the
    // latest generation and schedules another pass if it changed meanwhile.
    if (m_sessionMaintenanceScheduled.exchange(true, std::memory_order_acq_rel))
        return;
    m_singerSessionReleasePool.start([this] { runSessionMaintenance(); });
}

void InferEngine::runSessionMaintenance() {
    m_sessionMaintenanceScheduled.store(false, std::memory_order_release);
    if (m_disposed.load(std::memory_order_acquire))
        return;

    QSet<SingerIdentifier> selection;
    quint64 generation = 0;
    std::size_t capacity = 0;
    std::chrono::seconds idleTimeout{0};
    {
        std::lock_guard lock(m_singerSessionSelectionMutex);
        selection = m_latestSingerSelection;
        generation = m_selectionGeneration.load(std::memory_order_acquire);
        capacity = m_maintenanceCapacity;
        idleTimeout = m_maintenanceIdleTimeout;
    }
    // Cache-side bookkeeping and any waiting on the factory-owned mutex now
    // happen off the GUI thread. The generation is revalidated *inside* the
    // cache lock: a pass blocked behind a model factory must not apply a
    // selection that the GUI has already superseded.
    auto retainResult = m_singerSessions.retainOnly(selection, [this, generation] {
        return generation == m_selectionGeneration.load(std::memory_order_acquire) &&
               !m_disposed.load(std::memory_order_acquire);
    });
    if (!retainResult.applied) {
        // Leave the eviction request pending and re-run with the newest snapshot.
        scheduleSessionMaintenance();
        return;
    }
    const bool evictionRequested =
        m_evictionRequested.exchange(false, std::memory_order_acq_rel);
    if (!m_initialized.load(std::memory_order_acquire) ||
        m_disposed.load(std::memory_order_acquire)) {
        if (retainResult.released > 0) {
            qInfo() << "Singer session retention dropped" << retainResult.released
                    << "deselected resident entries";
        }
        releaseSingerSessionsAsync(std::move(retainResult.handles));
        return;
    }

    if (evictionRequested) {
        auto evictResult = m_singerSessions.evict(capacity, idleTimeout);
        if (evictResult.idle > 0 || evictResult.capacity > 0) {
            qInfo().noquote().nospace()
                << "Singer session retention scan: idleReleased=" << evictResult.idle
                << ", lruReleased=" << evictResult.capacity << ", capacity=" << capacity
                << ", idleTimeoutSeconds=" << idleTimeout.count();
        }
        if (!evictResult.handles.empty())
            releaseSingerSessionsAsync(std::move(evictResult.handles));
    }
    releaseDeselectedSingerSessionsAsync(std::move(retainResult.handles), generation);

    if (generation != m_selectionGeneration.load(std::memory_order_acquire))
        scheduleSessionMaintenance();
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

void InferEngine::releaseDeselectedSingerSessionsAsync(SingerSessionHandleList handles,
                                                        const quint64 generation) {
    const auto releasedSessions = handles.size();
    m_singerSessionReleasePool.start([this, handles = std::move(handles), releasedSessions,
                                      generation]() mutable {
        InferDirectMLSerializationGuard dmlGuard;
        handles.clear();

        // A superseded selection must not submit a fresh unload. Work already
        // started is not forcibly terminated; active handles still protect
        // their users, and a later maintenance pass re-acquires what is needed.
        auto &session = SynthrtEngine::instance().session();
        std::size_t unloadedPackages = 0;
        std::size_t packagesInUse = 0;
        for (const auto &package : session.loadedVoicebanks()) {
            // Re-validate before EVERY new unload admission, not just once
            // before the loop. A selection published while an earlier unload
            // was running may have made this package wanted again.
            if (generation != m_selectionGeneration.load(std::memory_order_acquire) ||
                m_disposed.load(std::memory_order_acquire))
                break;
            QSet<SingerIdentifier> currentSelection;
            {
                std::lock_guard selectionLock(m_singerSessionSelectionMutex);
                currentSelection = m_latestSingerSelection;
            }
            if (generation != m_selectionGeneration.load(std::memory_order_acquire))
                break;
            // The selection mutex is NOT held across unloadVoicebank: that call
            // can load or destroy model resources and must never block the GUI.
            if (packageIsSelected(package, currentSelection)) {
                continue;
            }

            auto result = session.unloadVoicebank(package.packageId, package.version);
            if (result) {
                ++unloadedPackages;
            } else if (result.error().code() == srt::core::ErrorCode::PackageInUse) {
                ++packagesInUse;
            } else if (result.error().code() != srt::core::ErrorCode::RuntimePackageNotLoaded) {
                qWarning().noquote().nospace()
                    << "retainSingerSessions: failed to unload package "
                    << QString::fromStdString(package.packageId) << ": "
                    << QString::fromUtf8(result.error().messageWithLocation());
            }
        }
        if (releasedSessions > 0 || unloadedPackages > 0) {
            qInfo().noquote().nospace()
                << "Singer session retention updated: dropped=" << releasedSessions
                << ", packagesUnloaded=" << unloadedPackages << ", packagesInUse=" << packagesInUse;
        } else if (packagesInUse > 0) {
            qDebug().noquote().nospace()
                << "Singer session package unload deferred: packagesInUse=" << packagesInUse;
        }
    });
}

void InferEngine::evictSingerSessions() {
    // The timer only publishes a policy snapshot and a request flag. Running
    // evict() here would take the cache mutex that a background model factory
    // may hold, freezing the GUI every ten seconds.
    if (!m_initialized.load(std::memory_order_acquire) ||
        m_disposed.load(std::memory_order_acquire)) {
        return;
    }

    const auto option = appOptions->inference();
    const auto capacity = static_cast<std::size_t>(
        InferenceOption::normalizeSingerSessionCacheCapacity(option->singerSessionCacheCapacity));
    const auto idleTimeout =
        std::chrono::seconds(InferenceOption::normalizeSingerSessionIdleTimeoutSeconds(
            option->singerSessionIdleTimeoutSeconds));
    {
        std::lock_guard lock(m_singerSessionSelectionMutex);
        m_maintenanceCapacity = capacity;
        m_maintenanceIdleTimeout = idleTimeout;
    }
    m_evictionRequested.store(true, std::memory_order_release);
    scheduleSessionMaintenance();
}

void InferEngine::dispose() {
    // Close the gate and clear Ready in the same critical section as the
    // initialization publication, so neither order can resurrect a stale state.
    {
        std::lock_guard stateLock(m_stateMutex);
        if (m_disposed.exchange(true, std::memory_order_acq_rel))
            return;
        m_initialized.store(false, std::memory_order_release);
    }
    m_singerSessionEvictionTimer.stop();
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

const srt::core::Runtime &InferEngine::constRuntime() const {
    return SynthrtEngine::instance().runtime();
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
