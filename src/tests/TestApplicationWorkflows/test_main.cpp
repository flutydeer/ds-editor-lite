#include "tst_application_workflows.h"

#include "AppContext.h"
#include "Bootstrap/AppDataPaths.h"
#include "Bootstrap/AppEnvironment.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppOptions/Options/GeneralOption.h"
#include "Model/AppOptions/Options/InferenceOption.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/ExecutionProvider.h"
#include "Modules/Inference/Utils/CudaGpuUtils.h"
#include "../TestSupport/ProcessFixture.h"

#include <lite/PackageManager/PackageManager.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Tasking/TaskManager.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QtTest/QTest>

#include <cstdio>
#include <memory>

namespace {
    int libreSvipProcessFixture(const QStringList &arguments) {
        if (arguments.size() != 5 || arguments[2] != QStringLiteral("convert"))
            return 2;
        QFile answers;
        if (!answers.open(stdin, QIODevice::ReadOnly))
            return 3;
        const auto defaults = answers.readAll();
        if (defaults.isEmpty() || !defaults.trimmed().isEmpty())
            return 3;
        const auto result = qgetenv("DSEL_TEST_LIBRESVIP_RESULT");
        if (result == "error") {
            std::fputs("Fixture conversion rejected the source\n", stderr);
            return 4;
        }
        if (result == "missing")
            return 0;
        QFile output(arguments[4]);
        if (!output.open(QIODevice::WriteOnly))
            return 5;
        if (result == "empty")
            return 0;
        // The external converter is replaced; parsing and importing its DSPX result are real.
        QFile input(arguments[3]);
        if (!input.open(QIODevice::ReadOnly))
            return 6;
        const auto data = input.readAll();
        return output.write(data) == data.size() ? 0 : 7;
    }

    int inferenceProviderStartup(int argc, char **argv, const bool selectedDevice) {
        QCoreApplication application(argc, argv);
        const auto deviceId = application.arguments().value(2);
        bool validIndex = false;
        const auto deviceIndex = application.arguments().value(3).toInt(&validIndex);
        if (selectedDevice && (deviceId.isEmpty() || !validIndex || deviceIndex < 0))
            return 5;
        AppEnvironment::postInit(AppHostMode::Headless);
        auto options = std::make_unique<AppOptions>();
        options->general()->packageSearchPaths.clear();
        options->inference()->autoStartInfer = false;
        options->inference()->executionProvider =
            selectedDevice ? QStringLiteral("DirectML") : QStringLiteral("CUDA");
        options->inference()->selectedGpuIndex = selectedDevice ? deviceIndex : 3;
        options->inference()->selectedGpuId =
            selectedDevice ? deviceId : QStringLiteral("unavailable-gpu");
        if (!selectedDevice) {
            CudaGpuUtils::setNvidiaSmiPath(
                QDir(AppDataPaths::testRoot()).filePath(QStringLiteral("missing-nvidia-smi")));
        }
        AppContext context(std::move(options), AppHostMode::Headless);
        packageManager->initialize({});
        if (!TestSupport::waitUntil(
                [] { return appStatus->inferEngineEnvStatus == AppStatus::ModuleStatus::Ready; },
                5000)) {
            qCritical("Inference startup did not reach a ready runtime");
            return 1;
        }
        if (!SynthrtEngine::instance().runtimeInitialized() ||
            !SynthrtEngine::instance().initializationDone()) {
            qCritical("Inference startup must complete runtime initialization");
            return 2;
        }
        const auto expectedProvider =
            selectedDevice ? ExecutionProvider::DirectML : ExecutionProvider::Cpu;
        if (ExecutionProviderUtils::effective() != expectedProvider ||
            appOptions->inference()->executionProvider !=
                ExecutionProviderUtils::toString(expectedProvider) ||
            appOptions->inference()->selectedGpuIndex != (selectedDevice ? deviceIndex : -1) ||
            appOptions->inference()->selectedGpuId != (selectedDevice ? deviceId : QString{})) {
            qCritical("Inference startup must resolve the provider and saved GPU selection");
            return 4;
        }
        if (!TestSupport::waitUntil(
                [] {
                    return taskManager->tasks().isEmpty() &&
                           appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready;
                },
                5000)) {
            qCritical("Package discovery must finish after inference startup");
            return 3;
        }
        return 0;
    }

}

int main(int argc, char **argv) {
    if (argc > 1 &&
        QString::fromLocal8Bit(argv[1]) == QStringLiteral("--unavailable-inference-provider"))
        return inferenceProviderStartup(argc, argv, false);
    if (argc > 1 &&
        QString::fromLocal8Bit(argv[1]) == QStringLiteral("--selected-inference-device"))
        return inferenceProviderStartup(argc, argv, true);
    QCoreApplication application(argc, argv);
    if (application.arguments().value(1) == QStringLiteral("proj"))
        return libreSvipProcessFixture(application.arguments());
    ApplicationWorkflowTests tests;
    return QTest::qExec(&tests, argc, argv);
}
