#include "InferEngine.h"

#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include <lite/Tasking/TaskManager.h>
#include "Modules/Inference/Models/GenericInferModel.h"
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include "Tasks/InitInferEngineTask.h"

#include <synthrt/Core/Support/Logging.h>

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
        const auto initTask = new InitInferEngineTask;
        connect(initTask, &Task::finished, this, [=] {
            taskManager->removeTask(initTask);
            QWriteLocker lock(&m_engineRwLock);
            if (m_disposed || SynthrtEngine::instance().isAboutToQuit()) {
                delete initTask;
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

    auto ep = appOptions->inference()->executionProvider;
    const auto gpuDeviceList = [&ep]() -> QList<GpuInfo> {
        if (ep == QStringLiteral("DirectML")) {
            return DmlGpuUtils::getGpuList();
        }
        if (ep == QStringLiteral("CUDA")) {
            return CudaGpuUtils::getGpuList();
        }
        return {};
    }();

    // No qualified GPU for the requested provider (e.g. integrated graphics
    // only): degrade to CPU for this session instead of failing the engine.
    // Runtime-only: the stored option is kept so a restored GPU picks the
    // provider back up on next launch.
    if ((ep == QStringLiteral("DirectML") || ep == QStringLiteral("CUDA")) &&
        gpuDeviceList.empty()) {
        qWarning().noquote() << QStringLiteral(
                                    "InferEngine: no qualified GPU found for execution provider "
                                    "'%1'; falling back to CPU for this session.")
                                    .arg(ep);
        ep = QStringLiteral("CPU");
    }

    const auto [index, description, deviceId, memory] = [&ep]() -> GpuInfo {
        if (ep == QStringLiteral("DirectML")) {
            auto selectedGpu_ = DmlGpuUtils::getGpuByPciDeviceVendorIdString(
                appOptions->inference()->selectedGpuId);
            if (selectedGpu_.index < 0) {
                qInfo() << "Auto selecting GPU";
                selectedGpu_ = DmlGpuUtils::getRecommendedGpu();
            } else {
                qInfo() << "Selecting GPU";
            }
            return selectedGpu_;
        }
        if (ep == QStringLiteral("CUDA")) {
            auto selectedGpu_ = CudaGpuUtils::getGpuByUuid(appOptions->inference()->selectedGpuId);
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
    for (const auto &pathQt : appOptions->general()->packageSearchPaths) {
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

    m_paths.singerProvider = StringUtils::path_to_qstr(singerProviderDir);
    m_paths.inferenceDriver = StringUtils::path_to_qstr(inferenceDriverDir);
    m_paths.inferenceInterpreter = StringUtils::path_to_qstr(inferenceInterpreterDir);
    const auto runtimeDir = inferenceDriverDir / "srt-onnxdriver" / "runtimes" / "onnx" /
                            (ep == QStringLiteral("CUDA") ? "cuda" : "default");
    m_paths.inferenceRuntime = StringUtils::path_to_qstr(runtimeDir);

    if (ep == QStringLiteral("DirectML") || ep == QStringLiteral("CUDA")) {
        qInfo().noquote() << QStringLiteral("GPU: %1, Device ID: %2, Memory: %3")
                                 .arg(description)
                                 .arg(deviceId)
                                 .arg(memory);
    }

    m_initialized = true;
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
    const auto releasedSessions = handles.size();
    m_singerSessionReleasePool.start([this, handles = std::move(handles),
                                      releasedSessions]() mutable {
        InferDirectMLSerializationGuard dmlGuard;
        handles.clear();

        auto &session = SynthrtEngine::instance().session();
        std::size_t unloadedPackages = 0;
        std::size_t packagesInUse = 0;
        for (const auto &package : session.loadedVoicebanks()) {
            std::lock_guard selectionLock(m_singerSessionSelectionMutex);
            if (packageIsSelected(package, m_singerSessions.retainedIdentifiers())) {
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
