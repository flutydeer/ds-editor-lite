//
// Created by fluty on 24-3-19.
//

#include "LaunchLanguageEngineTask.h"

#include <QThread>

#include <filesystem>
#include <iostream>
#include <string>

#include <stdcorelib/str.h>
#include <stdcorelib/system.h>

#include <LangCore/Support/Logging.h>
#include <LangCore/Core/Manager.h>
#include <LangCore/Module/Module.h>
#include <LangCore/Task/TaskFactoryPlugin.h>

#include <LangPlugins/Api/Drivers/Onnx/1/OnnxDriverApiL1.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Language/LangSetting/ILangSetManager.h"
#include "Utils/Log.h"
#include "Utils/StringUtils.h"

static void log_report_callback(const int level, const LangCore::LogContext &ctx,
                                const std::string_view &msg) {
    const QString message_qstr = QString::fromUtf8(msg.data(), msg.size());
    switch (level) {
        case LangCore::Logger::Fatal:
            Log::f(ctx.category, message_qstr);
            break;
        case LangCore::Logger::Critical:
            Log::e(ctx.category, message_qstr);
            break;
        case LangCore::Logger::Warning:
            Log::w(ctx.category, message_qstr);
            break;
        case LangCore::Logger::Information:
        case LangCore::Logger::Success:
            Log::i(ctx.category, message_qstr);
            break;
        case LangCore::Logger::Debug:
        default:
            Log::d(ctx.category, message_qstr);
            break;
    }
}

using EP = LangPlugins::Api::Onnx::L1::ExecutionProvider;

std::filesystem::path getPluginRootDirectory() {
#if defined(Q_OS_MAC)
    return MacOSUtils::getMainBundlePath() / _TSTR("Contents/PlugIns");
#elif defined(Q_OS_WIN)
    return stdc::system::application_directory() / _TSTR("plugins");
#else
    return stdc::system::application_directory().parent_path() / _TSTR("lib/plugins");
#endif
}

EP parseExecutionProvider(const std::string &provider) {
    const auto providerLower = stdc::to_lower(provider);
    if (providerLower == "dml" || providerLower == "directml") {
        return EP::DMLExecutionProvider;
    }
    if (providerLower == "cuda") {
        return EP::CUDAExecutionProvider;
    }
    if (providerLower == "coreml") {
        return EP::CoreMLExecutionProvider;
    }
    return EP::CPUExecutionProvider;
}

bool initializeOnnxDriver(const LangCore::Manager *mgr, const std::string &ep,
                          const int deviceIndex, const bool loadFromProgress) {
    const auto onnxDriverPlugin = mgr->plugin<LangCore::DriverFactoryPlugin>("onnx");
    if (!onnxDriverPlugin) {
        std::cerr << "Failed to load ONNX inference driver" << std::endl;
        return false;
    }

    const auto onnxDriver = onnxDriverPlugin->create();
    const auto onnxArgs = LangCore::NO<LangPlugins::Api::Onnx::L1::DriverInitArgs>::create();

    const auto ep_ = parseExecutionProvider(ep);
    onnxArgs->ep = ep_;
    const auto ortParentPath =
        onnxDriverPlugin->path().parent_path() / _TSTR("runtimes") / _TSTR("onnx");
    onnxArgs->runtimePath = ep_ == LangPlugins::Api::Onnx::L1::CUDAExecutionProvider
                                ? ortParentPath / _TSTR("cuda")
                                : ortParentPath / _TSTR("default");

    onnxArgs->loadFromProgress = loadFromProgress;
    onnxArgs->deviceIndex = deviceIndex;

    if (const auto exp = onnxDriver->initialize(onnxArgs); !exp) {
        std::cerr << "Failed to initialize ONNX driver: " << exp.error().message() << std::endl;
        return false;
    }

    auto &driverCategory = *mgr->category("driver");
    driverCategory.addObject("g2pOnnxDriver", onnxDriver);
    return true;
}

LaunchLanguageEngineTask::LaunchLanguageEngineTask(QObject *parent) : Task(parent) {
    TaskStatus status;
    status.title = tr("Launching language module...");
    status.message = "";
    status.isIndetermine = true;
    setStatus(status);
}

void LaunchLanguageEngineTask::runTask() {
    qDebug() << "Launching language module...";
    LangCore::Logger::setLogCallback(log_report_callback);
    const auto langMgr = LangCore::Manager::instance();

    const auto defaultPluginDir = getPluginRootDirectory() / _TSTR("LangPlugins");
    langMgr->addPluginPath("org.openvpi.DriverFactory", defaultPluginDir / _TSTR("Drivers"));
    langMgr->addPluginPath("org.openvpi.TaskFactory", defaultPluginDir / _TSTR("G2ps"));
    langMgr->addPluginPath("org.openvpi.TaskFactory", defaultPluginDir / _TSTR("Taggers"));
    langMgr->addPluginPath("org.openvpi.TaskFactory", defaultPluginDir / _TSTR("Splitters"));

    const std::filesystem::path packagesRootDir =
        stdc::system::application_directory() / _TSTR("G2pPackages");
    langMgr->addPackagePath(packagesRootDir);

    if (const auto onnxDriverInitialized = initializeOnnxDriver(langMgr, "cpu", 0, true);
        !onnxDriverInitialized)
        std::cerr << "Failed to initializeOnnxDriver" << std::endl;

    std::string msg;
    langMgr->initialize(msg);

    if (langMgr->initialized()) {
        qInfo() << "Successfully launched language module";
    } else {
        success = false;
        errorMessage = msg;
    }
    success = true;
}