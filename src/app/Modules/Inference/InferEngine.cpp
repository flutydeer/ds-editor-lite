//
// Created by fluty on 24-9-25.
//

#include "InferEngine.h"

#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Task/TaskManager.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Tasks/InitInferEngineTask.h"

#include <stdcorelib/str.h>
#include <stdcorelib/system.h>
#include <stdcorelib/path.h>

#include <synthrt/Core/PackageRef.h>
#include <synthrt/Support/JSON.h>
#include <synthrt/Support/Logging.h>
#include <synthrt/SVS/SingerContrib.h>
#include <synthrt/SVS/InferenceContrib.h>
#include <synthrt/SVS/Inference.h>

#include <dsinfer/Inference/InferenceDriver.h>
#include <dsinfer/Inference/InferenceDriverPlugin.h>
#include <dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>
#include <dsinfer/Api/Inferences/Duration/1/DurationApiL1.h>
#include <dsinfer/Api/Inferences/Pitch/1/PitchApiL1.h>
#include <dsinfer/Api/Inferences/Variance/1/VarianceApiL1.h>
#include <dsinfer/Api/Inferences/Vocoder/1/VocoderApiL1.h>
#include <dsinfer/Api/Drivers/Onnx/OnnxDriverApi.h>
#include <dsinfer/Support/PackageListConfig.h>

#include <sndfile.hh>

#include "Utils/BasePitchCurve.h"
#include "Utils/DmlGpuUtils.h"
#include "Utils/Log.h"
#include "Utils/Expected.h"

#include <QDebug>
#include <QDir>

#include "Tasks/InferTaskCommon.h"
#include "Utils/CudaGpuUtils.h"

namespace Co = ds::Api::Common::L1;
namespace Ac = ds::Api::Acoustic::L1;
namespace Dur = ds::Api::Duration::L1;
namespace Pit = ds::Api::Pitch::L1;
namespace Var = ds::Api::Variance::L1;
namespace Vo = ds::Api::Vocoder::L1;

static void log_report_callback(int level, const srt::LogContext &ctx, const std::string_view &msg) {
    auto message_qstr = QString::fromUtf8(msg.data(), msg.size());
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

static srt::Expected<void> initializeSU(srt::SynthUnit &su, ds::Api::Onnx::ExecutionProvider ep, int deviceIndex) {
    // Get basic directories
    auto appDir = stdc::system::application_directory();
    auto defaultPluginDir = appDir / _TSTR("plugins") / _TSTR("dsinfer");

    // Set default plugin directories
    auto singerProviderDir = defaultPluginDir / _TSTR("singerproviders");
    auto inferenceDriverDir = defaultPluginDir / _TSTR("inferencedrivers");
    auto inferenceInterpreterDir = defaultPluginDir / _TSTR("inferenceinterpreters");
    qDebug().noquote().nospace()
        << "Singer provider plugin path: " << QDir(singerProviderDir).path();
    qDebug().noquote().nospace()
        << "Inference driver plugin path: " << QDir(inferenceDriverDir).path();
    qDebug().noquote().nospace()
        << "Inference interpreter plugin path: " << QDir(inferenceInterpreterDir).path();
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
    return srt::Expected<void>();
}

InferEngine::InferEngine() {
    srt::Logger::setLogCallback(log_report_callback);

    auto initTask = new InitInferEngineTask;
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
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &InferEngine::dispose);
}

InferEngine::~InferEngine() {
    dispose();
}

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
//             m_configPath = path;
//         }
//         delete task;
//     });
//     taskManager->addAndStartTask(task);
// }

bool InferEngine::initialize(QString &error) {
    using EP = ds::Api::Onnx::ExecutionProvider;
    auto ep = EP::CPUExecutionProvider;
    if (appOptions->inference()->executionProvider == "DirectML") {
        ep = EP::DMLExecutionProvider;
    } else if (appOptions->inference()->executionProvider == "CUDA") {
        ep = EP::CUDAExecutionProvider;
    } else if (appOptions->inference()->executionProvider == "CoreML") {
        ep = EP::CoreMLExecutionProvider;
    }

    const auto gpuDeviceList = [](EP ep_) -> QList<GpuInfo> {
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

    const auto selectedGpu = [](EP ep_) -> GpuInfo {
        switch (ep_) {
            case EP::DMLExecutionProvider: {
                auto selectedGpu = DmlGpuUtils::getGpuByPciDeviceVendorIdString(appOptions->inference()->selectedGpuId);
                if (selectedGpu.index < 0) {
                    qInfo() << "Auto selecting GPU";
                    selectedGpu = DmlGpuUtils::getRecommendedGpu();
                } else {
                    qInfo() << "Selecting GPU";
                }
                return selectedGpu;
            }
            case EP::CUDAExecutionProvider: {
                auto selectedGpu = CudaGpuUtils::getGpuByUuid(appOptions->inference()->selectedGpuId);
                if (selectedGpu.index < 0) {
                    qInfo() << "Auto selecting GPU";
                    selectedGpu = CudaGpuUtils::getRecommendedGpu();
                } else {
                    qInfo() << "Selecting GPU";
                }
                return selectedGpu;
            }
            default:
                return {};
        }
    }(ep);


    // Initialize SynthUnit (must do this before inference)
    if (auto exp = initializeSU(m_su, ep, selectedGpu.index); !exp) {
        error = QString::fromUtf8(exp.error().message());
        return false;
    }

    qInfo().noquote() << QStringLiteral("GPU: %1, Device ID: %2, Memory: %3")
                            .arg(selectedGpu.description)
                            .arg(selectedGpu.deviceId)
                            .arg(selectedGpu.memory);

    m_initialized = true;
    qInfo().noquote() << "Successfully loaded environment. Execution provider:"
                      << appOptions->inference()->executionProvider;
    return true;
}


static inline std::filesystem::path to_filesystem_path(const QString &s) {
#ifdef _WIN32
    return s.toStdWString();
#else
    return s.toStdString();
#endif
}


bool InferEngine::loadPackage(const QString &packagePath, bool metadataOnly, srt::PackageRef &outPackage) {
    return loadPackage(to_filesystem_path(packagePath), metadataOnly, outPackage);
}

bool InferEngine::loadPackage(const std::filesystem::path &packagePath, bool metadataOnly, srt::PackageRef &outPackage) {
    if (!m_initialized) {
        qCritical() << "runLoadConfig: SynthUnit is not initialized!";
        return false;
    }

    srt::PackageRef pkg;

    // Load package
    if (auto exp = m_su.open(packagePath, metadataOnly); !exp) {
        qCritical().noquote() << stdc::formatN(R"(failed to open package "%1": %2)", packagePath,
                                               exp.error().message());
        return false;
    } else {
        pkg = exp.take();
    }
    if (!pkg.isLoaded()) {
        qCritical().noquote() << stdc::formatN(R"(failed to load package "%1": %2)", packagePath,
                                               pkg.error().message());
        return false;
    }
    outPackage = pkg;
    return true;
}

bool InferEngine::runLoadConfig(const QString &path) {
    if (path.isNull() || path.isEmpty()) {
        qWarning() << "Config path is null or empty";
        return false;
    }
    QMutexLocker lock(&m_mutex);
    if (path == m_configPath) {
        qInfo() << "Already loaded config";
        return m_configLoaded;
    }

    if (!m_initialized) {
        qCritical() << "runLoadConfig: SynthUnit is not initialized!";
        return false;
    }

    // Load models
    m_configLoaded = false;

    std::filesystem::path packagePath = to_filesystem_path(path);

    m_su.addPackagePath(packagePath.parent_path());

    std::string inputSinger = appOptions->general()->defaultSingerId.toStdString();

    // Load package
    if (!loadPackage(packagePath, false, m_pkg)) {
        qCritical() << "Failed to load package" << path;
        return false;
    }

    // Find singer
    auto &sc = *m_su.category("singer")->as<srt::SingerCategory>();
    const auto &singers = sc.singers();
    const srt::SingerSpec *singerSpec = nullptr;
    for (const auto &singer : singers) {
        if (singer->id() == inputSinger) {
            singerSpec = singer;
            break;
        }
    }
    if (!singerSpec) {
        qCritical().noquote() << stdc::formatN(R"(singer "%1" not found in package)", inputSinger);
        return false;
    }

    struct ImportEntry {
        std::string_view className;
        std::string_view apiName;
        Inference *data;
    };

    ImportEntry imports[] = {
        {Dur::API_CLASS, Dur::API_NAME, &m_duration},
        {Pit::API_CLASS, Pit::API_NAME, &m_pitch   },
        {Var::API_CLASS, Var::API_NAME, &m_variance},
        {Ac::API_CLASS,  Ac::API_NAME,  &m_acoustic},
        {Vo::API_CLASS,  Vo::API_NAME,  &m_vocoder },
    };

    // Assign imports
    for (const auto &imp : singerSpec->imports()) {
        const auto &cls = imp.inference()->className();
        for (auto &entry : imports) {
            if (cls == entry.className) {
                *entry.data = {imp.options(), imp.inference()};
                break;
            }
        }
    }

    // Check for missing inferences
    for (const auto &entry : imports) {
        if (!entry.data->spec) {
            qCritical().noquote() << stdc::formatN(R"(%1 inference not found for singer "%2")",
                                                   entry.apiName, inputSinger);
            return false;
        }
    }

    // Initialize duration inference
    if (auto exp = m_duration.spec->createInference(
        m_duration.options, srt::NO<Dur::DurationRuntimeOptions>::create());
    !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to create duration inference for singer "%1": %2)",
                          inputSinger, exp.error().message()));
        return false;
    } else {
        m_duration.session = exp.take();
    }
    if (auto exp = m_duration.session->initialize(srt::NO<Dur::DurationInitArgs>::create()); !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to initialize duration inference for singer "%1": %2)",
                          inputSinger, exp.error().message()));
        return false;
    }

    // Initialize pitch inference
    if (auto exp = m_pitch.spec->createInference(
            m_pitch.options, srt::NO<Pit::PitchRuntimeOptions>::create());
        !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to create pitch inference for singer "%1": %2)",
                          inputSinger, exp.error().message()));
        return false;
    } else {
        m_pitch.session = exp.take();
    }
    if (auto exp = m_pitch.session->initialize(srt::NO<Pit::PitchInitArgs>::create()); !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to initialize pitch inference for singer "%1": %2)",
                          inputSinger, exp.error().message()));
        return false;
    }

    // Initialize variance inference
     if (auto exp = m_variance.spec->createInference(
            m_variance.options, srt::NO<Var::VarianceRuntimeOptions>::create());
        !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to create variance inference for singer "%1": %2)",
                          inputSinger, exp.error().message()));
        return false;
    } else {
        m_variance.session = exp.take();
    }
    if (auto exp = m_variance.session->initialize(srt::NO<Var::VarianceInitArgs>::create()); !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to initialize variance inference for singer "%1": %2)",
                          inputSinger, exp.error().message()));
        return false;
    }

    // Initialize acoustic inference
    if (auto exp = m_acoustic.spec->createInference(
            m_acoustic.options, srt::NO<Ac::AcousticRuntimeOptions>::create());
        !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to create acoustic inference for singer "%1": %2)",
                          inputSinger, exp.error().message()));
        return false;
    } else {
        m_acoustic.session = exp.take();
    }
    if (auto exp = m_acoustic.session->initialize(srt::NO<Ac::AcousticInitArgs>::create()); !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to initialize acoustic inference for singer "%1": %2)",
                          inputSinger, exp.error().message()));
        return false;
    }

    // Initialize vocoder inference
    if (auto exp = m_vocoder.spec->createInference(
            m_vocoder.options, srt::NO<Vo::VocoderRuntimeOptions>::create());
        !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to create vocoder inference for singer "%1": %2)",
                          inputSinger, exp.error().message()));
        return false;
    } else {
        m_vocoder.session = exp.take();
    }
    if (auto exp = m_vocoder.session->initialize(srt::NO<Vo::VocoderInitArgs>::create()); !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to initialize vocoder inference for singer "%1": %2)",
                          inputSinger, exp.error().message()));
        return false;
    }
    m_configLoaded = true;
    m_configPath = path;
    qInfo() << "runLoadConfig success";

    return true;
}

bool InferEngine::inferDuration(const GenericInferModel &model, std::vector<double> &outDuration, QString &error) const {
    if (!m_initialized) {
        qCritical().noquote() << "inferDuration: Environment is not initialized";
        return false;
    }
    if (!m_configLoaded) {
        qCritical().noquote() << "inferDuration: Config is not loaded";
        return false;
    }

    std::string singer = model.singer.toStdString();
    std::string speakerName = model.speaker.toStdString();
    const auto input = srt::NO<Dur::DurationStartInput>::create();
    input->words = convertInputWords(model.words, std::move(speakerName));

    // Run duration
    srt::NO<Dur::DurationResult> result;
    // Start inference
    if (auto exp = m_duration.session->start(input); !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to start duration inference for singer "%1": %2)",
                          singer, exp.error().message()));
        return false;
    } else {
        result = exp.take().as<Dur::DurationResult>();
    }

    if (m_duration.session->state() == srt::ITask::Failed) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to run duration inference for singer "%1": %2)",
                          singer, result->error.message()));
        return false;
    }

    outDuration = std::move(result->durations);

    return true;
}

bool InferEngine::inferPitch(const GenericInferModel &model, InferParam &outPitch, QString &error) const {
    if (!m_initialized) {
        qCritical().noquote() << "inferPitch: Environment is not initialized";
        return false;
    }
    if (!m_configLoaded) {
        qCritical().noquote() << "inferPitch: Config is not loaded";
        return false;
    }

    std::string singer = model.singer.toStdString();
    std::string speakerName = model.speaker.toStdString();
    const auto input = srt::NO<Pit::PitchStartInput>::create();
    input->words = convertInputWords(model.words, speakerName);
    input->parameters = convertInputParams(model.params);
    input->speakers = std::vector{createStaticSpeaker(std::move(speakerName))};
    input->steps = appOptions->inference()->samplingSteps;

    // Run pitch
    srt::NO<Pit::PitchResult> result;
    // Start inference
    if (auto exp = m_pitch.session->start(input); !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to start pitch inference for singer "%1": %2)",
                          singer, exp.error().message()));
        return false;
    } else {
        result = exp.take().as<Pit::PitchResult>();
    }

    if (m_pitch.session->state() == srt::ITask::Failed) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to run pitch inference for singer "%1": %2)", singer,
                          result->error.message()));
        return false;
    }

    outPitch.tag = "pitch";
    outPitch.interval = result->interval;
    outPitch.values.assign(result->pitch.begin(), result->pitch.end());

    return true;
}

bool InferEngine::inferVariance(const GenericInferModel &model, QList<InferParam> &outParams,
                                QString &error) const {
    if (!m_initialized) {
        qCritical().noquote() << "inferVariance: Environment is not initialized";
        return false;
    }
    if (!m_configLoaded) {
        qCritical().noquote() << "inferVariance: Config is not loaded";
        return false;
    }

    std::string singer = model.singer.toStdString();
    std::string speakerName = model.speaker.toStdString();
    const auto input = srt::NO<Var::VarianceStartInput>::create();
    input->words = convertInputWords(model.words, speakerName);
    input->parameters = convertInputParams(model.params);
    input->speakers = std::vector{createStaticSpeaker(std::move(speakerName))};
    input->steps = appOptions->inference()->samplingSteps;

    // Run variance
    srt::NO<Var::VarianceResult> result;
    // Start inference
    if (auto exp = m_variance.session->start(input); !exp) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to start variance inference for singer "%1": %2)",
                          singer, exp.error().message()));
        return false;
    } else {
        result = exp.take().as<Var::VarianceResult>();
    }

    if (m_variance.session->state() == srt::ITask::Failed) {
        qCritical().noquote() << (
            stdc::formatN(R"(failed to run variance inference for singer "%1": %2)",
                          singer, result->error.message()));
        return false;
    }
    outParams.reserve(result->predictions.size());
    for (const auto &param : result->predictions) {
        InferParam inferParam;
        inferParam.tag.assign(param.tag.name());
        inferParam.interval = param.interval;
        inferParam.values.assign(param.values.begin(), param.values.end());
        inferParam.dynamic = true;
        outParams.emplace_back(std::move(inferParam));
    }
    return true;
}

bool InferEngine::inferAcoustic(const GenericInferModel &model, const QString &outputPath,
                                QString &error) const {
    if (!m_initialized) {
        qCritical().noquote() << "inferAcoustic: Environment is not initialized";
        return false;
    }
    if (!m_configLoaded) {
        qCritical().noquote() << "inferAcoustic: Config is not loaded";
        return false;
    }

    std::string singer = model.singer.toStdString();
    std::string speakerName = model.speaker.toStdString();
    const auto input = srt::NO<Ac::AcousticStartInput>::create();
    input->words = convertInputWords(model.words, speakerName);
    input->parameters = convertInputParams(model.params);
    input->speakers = std::vector{createStaticSpeaker(std::move(speakerName))};
    input->depth = appOptions->inference()->depth;
    input->steps = appOptions->inference()->samplingSteps;

    // Infer acoustic
    srt::NO<ds::ITensor> mel;
    srt::NO<ds::ITensor> f0;
    {
        srt::NO<Ac::AcousticResult> result;
        // Start inference
        if (auto exp = m_acoustic.session->start(input); !exp) {
            qCritical().noquote() << (
                stdc::formatN(R"(failed to start acoustic inference for singer "%1": %2)",
                              singer, exp.error().message()));
            return false;
        } else {
            result = exp.take().as<Ac::AcousticResult>();
        }

        if (m_acoustic.session->state() == srt::ITask::Failed) {
            qCritical().noquote() << (
                stdc::formatN(R"(failed to run acoustic inference for singer "%1": %2)",
                              singer, result->error.message()));
            return false;
        }
        mel = result->mel;
        f0 = result->f0;
    }
    // Run vocoder
    {
        auto vocoderInput = srt::NO<Vo::VocoderStartInput>::create();
        vocoderInput->mel = mel;
        vocoderInput->f0 = f0;

        srt::NO<Vo::VocoderResult> result;
        // Start inference
        if (auto exp = m_vocoder.session->start(vocoderInput); !exp) {
            qCritical().noquote() << (
                stdc::formatN(R"(failed to start vocoder inference for singer "%1": %2)",
                              singer, exp.error().message()));
            return false;
        } else {
            result = exp.take().as<Vo::VocoderResult>();
        }

        if (m_vocoder.session->state() == srt::ITask::Failed) {
            qCritical().noquote() << (
                stdc::formatN(R"(failed to run vocoder inference for singer "%1": %2)",
                              singer, result->error.message()));
            return false;
        }
        const auto &audioRawData = result->audioData;

        auto outputPathStr = outputPath
#ifdef _WIN32
            .toStdWString();
#else
            .toStdString();
#endif

        SndfileHandle audioFile(outputPathStr.c_str(), SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_FLOAT, 1, 44100);
        if (audioFile.error() != SF_ERR_NO_ERROR) {
            qDebug() << "Failed to run acoustic inference: " << audioFile.strError() << '\n';
            return false;
        }
        auto audioData = reinterpret_cast<const float *>(audioRawData.data());
        auto audioSize = static_cast<sf_count_t>(audioRawData.size() / sizeof(float));
        if (audioFile.write(audioData, audioSize) != audioSize) {
            qDebug() << "Failed to run acoustic inference: " << audioFile.strError() << '\n';
            return false;
        }
    }
    return true;
}

void InferEngine::terminateInferDurationAsync() const {
    qInfo() << "terminateInferDurationAsync";
    if (m_duration.session) {
        m_duration.session->stop();
    }
}

void InferEngine::terminateInferPitchAsync() const {
    qInfo() << "terminateInferPitchAsync";
    if (m_pitch.session) {
        m_pitch.session->stop();
    }
}

void InferEngine::terminateInferVarianceAsync() const {
    qInfo() << "terminateInferVarianceAsync";
    if (m_variance.session) {
        m_variance.session->stop();
    }
}

void InferEngine::terminateInferAcousticAsync() const {
    qInfo() << "terminateInferAcousticAsync";
    if (m_acoustic.session) {
        m_acoustic.session->stop();
    }
    if (m_vocoder.session) {
        m_vocoder.session->stop();
    }
}

void InferEngine::dispose() {
    m_duration.session.reset();
    m_pitch.session.reset();
    m_variance.session.reset();
    m_acoustic.session.reset();
    m_vocoder.session.reset();
}

srt::SynthUnit *InferEngine::synthUnit() {
    return &m_su;
}

QString InferEngine::configPath() {
    QMutexLocker lock(&m_mutex);
    return m_configPath;
}

bool InferEngine::configLoaded() {
    QMutexLocker lock(&m_mutex);
    return m_configLoaded;
}
