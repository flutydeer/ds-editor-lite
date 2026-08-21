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

InferEngine::InferEngine(QObject *parent) : QObject(parent) {
    srt::core::Logger::setLogCallback(log_report_callback);

    // Prevent crash on app exit
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
            &InferEngine::dispose);
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

    const auto ep = appOptions->inference()->executionProvider;
    const auto gpuDeviceList = [&ep]() -> QList<GpuInfo> {
        if (ep == QStringLiteral("DirectML")) {
            return DmlGpuUtils::getGpuList();
        }
        if (ep == QStringLiteral("CUDA")) {
            return CudaGpuUtils::getGpuList();
        }
        return {};
    }();

    if ((ep == QStringLiteral("DirectML") || ep == QStringLiteral("CUDA")) &&
        gpuDeviceList.empty()) {
        qCritical() << "InferEngine: Unable to find GPU device.";
        error = "No available GPU device found.";
        return false;
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
    if (!SynthrtEngine::instance().initialize(packagePathsQt, g2pPackagePaths, ep, index)) {
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
    m_singerSessions.retainOnly(identifiers);
    if (!m_initialized || m_disposed) {
        return;
    }

    auto &session = SynthrtEngine::instance().session();
    for (const auto &package : session.loadedVoicebanks()) {
        if (packageIsSelected(package, identifiers)) {
            continue;
        }

        auto result = session.unloadVoicebank(package.packageId, package.version);
        if (!result && result.error().code() != srt::core::ErrorCode::PackageInUse &&
            result.error().code() != srt::core::ErrorCode::RuntimePackageNotLoaded) {
            qWarning().noquote().nospace()
                << "retainSingerSessions: failed to unload package "
                << QString::fromStdString(package.packageId) << ": "
                << QString::fromUtf8(result.error().messageWithLocation());
        }
    }
}

void InferEngine::dispose() {
    QWriteLocker lock(&m_engineRwLock);
    if (m_disposed) {
        return;
    }
    m_disposed = true;
    m_initialized = false;
    qDebug() << "dispose InferEngine inference sessions";
    m_singerSessions.clear();
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
