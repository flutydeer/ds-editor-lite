//
// Created by fluty on 24-3-19.
//

#include "LaunchLanguageEngineTask.h"

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Language/LangSetting/ILangSetManager.h"

#include "Utils/StringUtils.h"

#include <QApplication>
#include <QThread>

#include <filesystem>
#include <iostream>
#include <string>

#include <stdcorelib/str.h>
#include <stdcorelib/system.h>

#include <LangCore/Core/Manager.h>
#include <LangCore/Module/Module.h>
#include <LangCore/Task/TaskFactoryPlugin.h>

#include <LangPlugins/Api/Drivers/Onnx/1/OnnxDriverApiL1.h>

LaunchLanguageEngineTask::LaunchLanguageEngineTask(QObject *parent) : Task(parent) {
    TaskStatus status;
    status.title = tr("Launching language module...");
    status.message = "";
    status.isIndetermine = true;
    setStatus(status);
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

void LaunchLanguageEngineTask::runTask() {
    qDebug() << "Launching language module...";
    // QThread::sleep(1);
    const auto langMgr = LangCore::Manager::instance();

    const auto defaultPluginDir = getPluginRootDirectory() / _TSTR("LangPlugins");
    langMgr->addPluginPath("org.openvpi.DriverFactory", defaultPluginDir / _TSTR("Drivers"));
    langMgr->addPluginPath("org.openvpi.TaskFactory", defaultPluginDir / _TSTR("G2ps"));
    langMgr->addPluginPath("org.openvpi.TaskFactory", defaultPluginDir / _TSTR("Taggers"));

    const std::filesystem::path packagesRootDir =
        stdc::system::application_directory() / _TSTR("G2pPackages");
    langMgr->addPackagePath(packagesRootDir);

    if (const auto onnxDriverInitialized = initializeOnnxDriver(langMgr, "cpu", 0, true);
        !onnxDriverInitialized)
        std::cerr << "Failed to initializeOnnxDriver" << std::endl;

    langMgr->initialize(errorMessage);

    if (langMgr->initialized()) {
        qInfo() << "Successfully launched language module";
    } else {
        success = false;
        qCritical() << "LangMgr: errorMsg" << errorMessage
                    << "initialized:" << langMgr->initialized();
    }
    success = true;
}