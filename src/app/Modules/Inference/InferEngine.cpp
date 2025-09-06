//
// Created by fluty on 24-9-25.
//

#include "InferEngine.h"

#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Task/TaskManager.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/PackageManager/PackageManager.h"
#include "Tasks/InitInferEngineTask.h"

#include <stdcorelib/str.h>
#include <stdcorelib/system.h>
#include <stdcorelib/path.h>

#include <synthrt/Core/PackageRef.h>
#include <synthrt/Support/Logging.h>
#include <synthrt/SVS/SingerContrib.h>
#include <synthrt/SVS/InferenceContrib.h>
#include <synthrt/SVS/Inference.h>

#include <dsinfer/Inference/InferenceDriver.h>
#include <dsinfer/Inference/InferenceDriverPlugin.h>
#include <dsinfer/Api/Drivers/Onnx/OnnxDriverApi.h>
#include <dsinfer/Support/PackageListConfig.h>

#include "Utils/DmlGpuUtils.h"
#include "Utils/Log.h"
#include "Utils/Expected.h"
#include "Utils/StringUtils.h"
#include "Utils/VersionUtils.h"

#include <QDebug>
#include <QDir>
#include <QReadWriteLock>
#include <QStandardPaths>
#include <QString>

#include "Tasks/InferTaskCommon.h"
#include "Utils/CudaGpuUtils.h"
#if defined(Q_OS_MAC)
#  include "Utils/MacOSUtils.h"
#endif

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

static srt::Expected<void> checkPath(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) {
        return srt::Error(srt::Error::FileNotFound,
                          "Path does not exist: " + stdc::path::to_utf8(path));
    }
    if (!std::filesystem::is_directory(path)) {
        return srt::Error(srt::Error::InvalidArgument,
                          "Path is not a directory: " + stdc::path::to_utf8(path));
    }
    return srt::Expected<void>();
}

static srt::Expected<void> initializeSU(srt::SynthUnit &su, ds::Api::Onnx::ExecutionProvider ep,
                                        int deviceIndex, InferEnginePaths &outPaths) {
    // Get basic directories
    auto pluginRootDir =
#if defined(Q_OS_MAC)
        MacOSUtils::getMainBundlePath() / _TSTR("Contents/PlugIns");
#else
        stdc::system::application_directory() / _TSTR("plugins");
#endif
    auto defaultPluginDir = pluginRootDir / _TSTR("dsinfer");

    // Set default plugin directories
    auto singerProviderDir = defaultPluginDir / _TSTR("singerproviders");
    auto inferenceDriverDir = defaultPluginDir / _TSTR("inferencedrivers");
    auto inferenceInterpreterDir = defaultPluginDir / _TSTR("inferenceinterpreters");

    auto singerProviderDirString = StringUtils::path_to_qstr(singerProviderDir);
    auto inferenceDriverDirString = StringUtils::path_to_qstr(inferenceDriverDir);
    auto inferenceInterpreterDirString = StringUtils::path_to_qstr(inferenceInterpreterDir);
    qDebug().noquote().nospace() << "Singer provider plugin path: " << singerProviderDirString;
    qDebug().noquote().nospace() << "Inference driver plugin path: " << inferenceDriverDirString;
    qDebug().noquote().nospace() << "Inference interpreter plugin path: "
                                 << inferenceInterpreterDirString;
    if (auto exp = checkPath(singerProviderDir); !exp) {
        return exp.takeError();
    }
    if (auto exp = checkPath(inferenceDriverDir); !exp) {
        return exp.takeError();
    }
    if (auto exp = checkPath(inferenceInterpreterDir); !exp) {
        return exp.takeError();
    }
    su.addPluginPath("org.openvpi.SingerProvider", singerProviderDir);
    su.addPluginPath("org.openvpi.InferenceDriver", inferenceDriverDir);
    su.addPluginPath("org.openvpi.InferenceInterpreter", inferenceInterpreterDir);

    // Load driver
    auto plugin = su.plugin<ds::InferenceDriverPlugin>("onnx");
    if (!plugin) {
        return srt::Error(srt::Error::FileNotOpen, "failed to load inference driver");
    }

    auto onnxDriver = plugin->create();
    auto onnxArgs = srt::NO<ds::Api::Onnx::DriverInitArgs>::create();

    onnxArgs->ep = ep;
    auto ortParentPath = plugin->path().parent_path() / _TSTR("runtimes") / _TSTR("onnx");
    if (ep == ds::Api::Onnx::CUDAExecutionProvider) {
        onnxArgs->runtimePath = ortParentPath / _TSTR("cuda");
    } else {
        onnxArgs->runtimePath = ortParentPath / _TSTR("default");
    }
    onnxArgs->deviceIndex = deviceIndex;

    if (auto exp = onnxDriver->initialize(onnxArgs); !exp) {
        return srt::Error(srt::Error::FileNotOpen,
                          "failed to initialize onnx driver: " + exp.error().message());
    }

    // Add driver
    auto &ic = *su.category("inference");
    ic.addObject("dsdriver", onnxDriver);

    outPaths.singerProvider = singerProviderDirString;
    outPaths.inferenceDriver = inferenceDriverDirString;
    outPaths.inferenceRuntime = StringUtils::path_to_qstr(onnxArgs->runtimePath);
    outPaths.inferenceInterpreter = inferenceInterpreterDirString;

    return srt::Expected<void>();
}

InferEngine::InferEngine(QObject *parent) : QObject(parent) {
    srt::Logger::setLogCallback(log_report_callback);

    const auto initTask = new InitInferEngineTask;
    connect(initTask, &Task::finished, this, [=] {
        taskManager->removeTask(initTask);
        if (initTask->success)
            appStatus->inferEngineEnvStatus = AppStatus::ModuleStatus::Ready;
        else
            appStatus->inferEngineEnvStatus = AppStatus::ModuleStatus::Error;
        delete initTask;
    });
    taskManager->addAndStartTask(initTask);
    appStatus->inferEngineEnvStatus = AppStatus::ModuleStatus::Loading;

    // Prevent crash on app exit
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
            &InferEngine::dispose);
}

InferEngine::~InferEngine() {
    dispose();
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(InferEngine)

bool InferEngine::initialized() {
    QMutexLocker lock(&m_mutex);
    return m_initialized;
}

// void InferenceEngine::loadConfig(const QString &path) {
//     m_configLoaded = false;
//     auto task = new LoadInferConfigTask(path);
//     connect(task, &Task::finished, this, [=] {
//         taskManager->removeTask(task);
//         if (task->success) {
//             m_configLoaded = true;
//             m_paths.config = path;
//         }
//         delete task;
//     });
//     taskManager->addAndStartTask(task);
// }

bool InferEngine::initialize(QString &error) {
    QMutexLocker lock(&m_mutex);
    if (m_initialized) {
        qDebug() << "InferEngine already initialized";
        return true;
    }

    using EP = ds::Api::Onnx::ExecutionProvider;
    auto ep = EP::CPUExecutionProvider;
    if (appOptions->inference()->executionProvider == "DirectML") {
        ep = EP::DMLExecutionProvider;
    } else if (appOptions->inference()->executionProvider == "CUDA") {
        ep = EP::CUDAExecutionProvider;
    } else if (appOptions->inference()->executionProvider == "CoreML") {
        ep = EP::CoreMLExecutionProvider;
    }

    const auto gpuDeviceList = [](const EP ep_) -> QList<GpuInfo> {
        switch (ep_) {
            case EP::DMLExecutionProvider:
                return DmlGpuUtils::getGpuList();
            case EP::CUDAExecutionProvider:
                return CudaGpuUtils::getGpuList();
            default:
                return {};
        }
    }(ep);

    if (ep != EP::CPUExecutionProvider && gpuDeviceList.empty()) {
        qCritical() << "InferEngine: Unable to find GPU device.";
    }

    const auto [index, description, deviceId, memory] = [](const EP ep_) -> GpuInfo {
        switch (ep_) {
            case EP::DMLExecutionProvider: {
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
            case EP::CUDAExecutionProvider: {
                auto selectedGpu_ =
                    CudaGpuUtils::getGpuByUuid(appOptions->inference()->selectedGpuId);
                if (selectedGpu_.index < 0) {
                    qInfo() << "Auto selecting GPU";
                    selectedGpu_ = CudaGpuUtils::getRecommendedGpu();
                } else {
                    qInfo() << "Selecting GPU";
                }
                return selectedGpu_;
            }
            default:
                return {};
        }
    }(ep);


    // Initialize SynthUnit (must do this before inference)
    if (auto exp = initializeSU(m_su, ep, index, m_paths); !exp) {
        error = QString::fromUtf8(exp.error().message());
        return false;
    }


    //const auto homeDir = StringUtils::qstr_to_path(QDir::toNativeSeparators(
    //    QStandardPaths::writableLocation(QStandardPaths::HomeLocation)));

    //const std::filesystem::path paths = {homeDir / ".diffsinger/packages"};
    const auto packagePathsQt = appOptions->general()->packageSearchPaths;
    std::vector<std::filesystem::path> packagePaths;
    packagePaths.reserve(packagePathsQt.size());
    for (const auto &path : std::as_const(packagePathsQt)) {
        packagePaths.emplace_back(StringUtils::qstr_to_path(path));
    }
    m_su.setPackagePaths(packagePaths);

    qInfo().noquote() << QStringLiteral("GPU: %1, Device ID: %2, Memory: %3")
                             .arg(description)
                             .arg(deviceId)
                             .arg(memory);

    m_initialized = true;
    qInfo().noquote() << "Successfully loaded environment. Execution provider:"
                      << appOptions->inference()->executionProvider;
    return true;
}

bool InferEngine::loadPackage(const QString &packagePath, const bool noLoad,
                              srt::PackageRef &outPackage) {
    return loadPackage(StringUtils::qstr_to_path(packagePath), noLoad, outPackage);
}

bool InferEngine::loadPackageAndAllSingers(const QString &packagePath,
                                           srt::PackageRef &outPackage) {
    srt::PackageRef pkg;
    if (!loadPackage(packagePath, false, pkg)) {
        return false;
    }
    loadAllSingersFromPackage(pkg);
    outPackage = pkg;
    return true;
}

void InferEngine::loadAllSingersFromPackage(const srt::PackageRef &package) {
    const auto singers = package.contributes("singer");
    std::vector<std::shared_ptr<InferenceLoader>> loaders;
    loaders.reserve(singers.size());

    for (const auto singer : singers) {
        // NOLINTNEXTLINE(*-pro-type-static-cast-downcast)
        const auto singerSpec = static_cast<srt::SingerSpec *>(singer);
        auto loader = std::make_shared<InferenceLoader>(singerSpec);
        loader->loadInferenceSpecs();
        loaders.push_back(std::move(loader));
    }

    {
        QWriteLocker wrLock(&m_loaderRwLock);  // 这一行进行不下去了
        for (auto &loader : loaders) {
            m_loaders[loader->singerIdentifier()] = std::move(loader);
        }
    }
    qDebug() << "Successfully loaded all singers from inference package";
}

srt::SingerSpec *InferEngine::findSingerForPackage(const srt::PackageRef &package,
                                                   const QString &singerId) {
    const auto singerIdString = singerId.toStdString();
    return findSingerForPackage(package, singerIdString);
}

srt::SingerSpec *InferEngine::findSingerForPackage(const srt::PackageRef &package,
                                                   const std::string_view singerId) {
    // Find singer
    const auto singers = package.contributes("singer");
    srt::SingerSpec *singerSpec = nullptr;
    for (const auto singer : singers) {
        if (singer->id() == singerId) {
            // NOLINTNEXTLINE(*-pro-type-static-cast-downcast)
            singerSpec = static_cast<srt::SingerSpec *>(singer);
            break;
        }
    }
    if (!singerSpec) {
        qCritical().noquote().nospace() << "singer \"" << singerId << "\" not found in package";
        return nullptr;
    }
    return singerSpec;
}

std::shared_ptr<InferenceLoader>
    InferEngine::findLoaderForSinger(const SingerIdentifier &identifier) const {

    std::shared_ptr<InferenceLoader> loader;
    // Looking for the inference loader
    {
        QReadLocker rdLock(&m_loaderRwLock);
        const auto it = m_loaders.constFind(identifier);
        if (it != m_loaders.constEnd()) {
            loader = *it;
        }
    }
    return loader;
}

bool InferEngine::loadPackage(const std::filesystem::path &packagePath, const bool noLoad,
                              srt::PackageRef &outPackage) {
    if (!m_initialized) {
        qCritical() << "loadPackage: SynthUnit is not initialized!";
        return false;
    }

    srt::PackageRef pkg;

    const auto packagePathString = StringUtils::path_to_qstr(packagePath);
    // Load package
    if (auto exp = m_su.open(packagePath, noLoad); !exp) {
        qCritical().noquote().nospace() << "loadPackage: failed to open package \""
                                        << packagePathString << "\": " << exp.error().message();
        return false;
    } else {
        pkg = exp.take();
    }
    if (!noLoad && !pkg.isLoaded()) {
        qCritical().noquote().nospace() << "loadPackage: failed to load package \""
                                        << packagePathString << "\": " << pkg.error().message();
        return false;
    }
    outPackage = pkg;
    return true;
}

#if false
bool InferEngine::loadInferences(const QString &path) {
    if (path.isNull() || path.isEmpty()) {
        qWarning() << "Package path is null or empty";
        return false;
    }
    QMutexLocker lock(&m_mutex);
    if (path == m_paths.config) {
        qInfo() << "Already loaded config";
    }

    if (!m_initialized) {
        qCritical() << "loadInferences: SynthUnit is not initialized!";
        return false;
    }

    // Load models
    const auto packagePath = StringUtils::qstr_to_path(path);

    srt::PackageRef pkg;
    // Load package
    if (!loadPackage(packagePath, false, pkg)) {
        qCritical() << "Failed to load package" << path;
        return false;
    }

    loadAllSingersFromPackage(pkg);

    SingerIdentifier identifier;
    identifier.packageId = QString::fromUtf8(pkg.id());
    identifier.packageVersion = VersionUtils::stdc_to_qt(pkg.version());
    identifier.singerId = appOptions->general()->defaultSingerId;

    return loadInferencesForSinger(identifier);
}
#endif

bool InferEngine::loadInferencesForSinger(const SingerIdentifier &identifier) {
    const auto packageId = identifier.packageId.toStdString();

    auto loader = findLoaderForSinger(identifier);

    // If not found, try loading the package
    if (!loader) {
        qDebug() << "loadInferencesForSinger: "
                    "singer" << identifier << "not loaded, try loading now";
        const auto packageInfo = packageManager->findPackageByIdentifier(identifier);
        if (packageInfo.isEmpty()) {
            qCritical() << "loadInferencesForSinger: "
                           "package for singer" << identifier << "not found";
            return false;
        }
        srt::PackageRef pkg;
        if (!loadPackageAndAllSingers(packageInfo.path(), pkg)) {
            qCritical() << "loadInferencesForSinger: "
                           "failed to load package and singers" << identifier;
            return false;
        }
        Q_UNUSED(pkg)
        // Looking for the inference loader again
        {
            QReadLocker rdLock(&m_loaderRwLock);
            const auto it = m_loaders.constFind(identifier);
            if (it != m_loaders.constEnd()) {
                loader = *it;
            }
        }
        if (!loader) {
            qCritical() << "loadInferencesForSinger: "
                           "loader still not found after loading package"
                        << identifier;
            return false;
        }
    }
    {
        QReadLocker rdLock(&m_inferenceRwLock);
        const auto it = m_inferences.constFind(identifier);
        if (it != m_inferences.constEnd()) {
            qDebug() << "loadInferencesForSinger: "
                        "inferences already loaded for" << identifier;
            return true;
        }
    }

    InferenceSet inference;
    bool allLoaded = true;
    if (auto exp = loader->loadInferenceSpecs(); !exp) {
        qCritical().noquote().nospace() << "Failed to load inference specs: " << exp.getError();
        return false;
    } else {
        auto flags = exp.get();
        if (flags.has(InferenceFlag::Duration)) {
            if (auto exp2 = loader->createDuration(); !exp2) {
                allLoaded = false;
                qCritical().noquote().nospace()
                    << "Failed to create duration inference: " << exp2.getError();
            } else {
                inference.duration = std::move(exp2.get());
            }
        } else {
            allLoaded = false;
            qCritical().noquote().nospace() << "Missing duration inference";
        }
        if (flags.has(InferenceFlag::Pitch)) {
            if (auto exp2 = loader->createPitch(); !exp2) {
                allLoaded = false;
                qCritical().noquote().nospace()
                    << "Failed to create pitch inference: " << exp2.getError();
            } else {
                inference.pitch = std::move(exp2.get());
            }
        } else {
            allLoaded = false;
            qCritical().noquote().nospace() << "Missing pitch inference";
        }
        if (flags.has(InferenceFlag::Variance)) {
            if (auto exp2 = loader->createVariance(); !exp2) {
                allLoaded = false;
                qCritical().noquote().nospace()
                    << "Failed to create variance inference: " << exp2.getError();
            } else {
                inference.variance = std::move(exp2.get());
            }
        } else {
            allLoaded = false;
            qCritical().noquote().nospace() << "Missing variance inference";
        }
        if (flags.has(InferenceFlag::Acoustic)) {
            if (auto exp2 = loader->createAcoustic(); !exp2) {
                allLoaded = false;
                qCritical().noquote().nospace()
                    << "Failed to create acoustic inference: " << exp2.getError();
            } else {
                inference.acoustic = std::move(exp2.get());
            }
        } else {
            allLoaded = false;
            qCritical().noquote().nospace() << "Missing acoustic inference";
        }
        if (flags.has(InferenceFlag::Vocoder)) {
            if (auto exp2 = loader->createVocoder(); !exp2) {
                allLoaded = false;
                qCritical().noquote().nospace()
                    << "Failed to create vocoder inference: " << exp2.getError();
            } else {
                inference.vocoder = std::move(exp2.get());
            }
        } else {
            allLoaded = false;
            qCritical().noquote().nospace() << "Missing vocoder inference";
        }
    }

    if (!allLoaded) {
        return false;
    }
    {
        QWriteLocker wrLock(&m_inferenceRwLock);
        m_inferences[identifier] = std::move(inference);
    }

    //m_paths.config = StringUtils::path_to_qstr(singerSpec->parent().path());
    qInfo() << "loadInferences success";

    return true;
}

void InferEngine::terminateInferDurationAll() const {
    qInfo() << "terminateInferDurationAsync";
    QWriteLocker wrLock(&m_inferenceRwLock);
    for (const auto &inference : std::as_const(m_inferences)) {
        if (inference.duration) {
            inference.duration->stop();
        }
    }
}

void InferEngine::terminateInferPitchAll() const {
    qInfo() << "terminateInferPitchAsync";
    QWriteLocker wrLock(&m_inferenceRwLock);
    for (const auto &inference : std::as_const(m_inferences)) {
        if (inference.pitch) {
            inference.pitch->stop();
        }
    }
}

void InferEngine::terminateInferVarianceAll() const {
    qInfo() << "terminateInferVarianceAsync";
    QWriteLocker wrLock(&m_inferenceRwLock);
    for (const auto &inference : std::as_const(m_inferences)) {
        if (inference.variance) {
            inference.variance->stop();
        }
    }
}

void InferEngine::terminateInferAcousticAll() const {
    qInfo() << "terminateInferAcousticAsync";
    QWriteLocker wrLock(&m_inferenceRwLock);
    for (const auto &inference : std::as_const(m_inferences)) {
        if (inference.acoustic) {
            inference.acoustic->stop();
        }
        if (inference.vocoder) {
            inference.vocoder->stop();
        }
    }
}

void InferEngine::dispose() {
    qDebug() << "dispose InferEngine inference sessions";
    terminateInferDurationAll();
    terminateInferPitchAll();
    terminateInferVarianceAll();
    terminateInferAcousticAll();
    {
        QWriteLocker wrLock(&m_inferenceRwLock);
        m_inferences.clear();
    }
    auto packages = m_su.packages();
    for (auto &package : packages) {
        while (package.isLoaded()) {
            package.close();
        }
    }
}

srt::SynthUnit &InferEngine::synthUnit() {
    return m_su;
}

const srt::SynthUnit &InferEngine::constSynthUnit() const {
    return m_su;
}

QString InferEngine::configPath() {
    QMutexLocker lock(&m_mutex);
    return m_paths.config;
}

QString InferEngine::singerProviderPath() {
    QMutexLocker lock(&m_mutex);
    return m_paths.singerProvider;
}

QString InferEngine::inferenceDriverPath() {
    QMutexLocker lock(&m_mutex);
    return m_paths.inferenceDriver;
}

QString InferEngine::inferenceRuntimePath() {
    QMutexLocker lock(&m_mutex);
    return m_paths.inferenceRuntime;
}

QString InferEngine::inferenceInterpreterPath() {
    QMutexLocker lock(&m_mutex);
    return m_paths.inferenceInterpreter;
}
