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

    int unavailableInferenceProvider(int argc, char **argv) {
        QCoreApplication application(argc, argv);
        AppEnvironment::postInit(AppHostMode::Headless);
        auto options = std::make_unique<AppOptions>();
        options->general()->packageSearchPaths.clear();
        options->inference()->autoStartInfer = false;
        options->inference()->executionProvider = QStringLiteral("CUDA");
        options->inference()->selectedGpuIndex = 3;
        options->inference()->selectedGpuId = QStringLiteral("unavailable-gpu");
        CudaGpuUtils::setNvidiaSmiPath(
            QDir(AppDataPaths::testRoot()).filePath(QStringLiteral("missing-nvidia-smi")));
        AppContext context(std::move(options), AppHostMode::Headless);
        packageManager->initialize({});
        if (!TestSupport::waitUntil(
                [] { return appStatus->inferEngineEnvStatus == AppStatus::ModuleStatus::Ready; },
                5000)) {
            qCritical("The unavailable provider did not fall back to a ready CPU runtime");
            return 1;
        }
        if (!SynthrtEngine::instance().runtimeInitialized() ||
            !SynthrtEngine::instance().initializationDone()) {
            qCritical("CPU fallback must complete runtime initialization");
            return 2;
        }
        if (ExecutionProviderUtils::effective() != ExecutionProvider::Cpu ||
            appOptions->inference()->executionProvider != QStringLiteral("CPU") ||
            appOptions->inference()->selectedGpuIndex != -1 ||
            !appOptions->inference()->selectedGpuId.isEmpty()) {
            qCritical("CPU fallback must clear the unavailable GPU selection");
            return 4;
        }
        if (!TestSupport::waitUntil(
                [] {
                    return taskManager->tasks().isEmpty() &&
                           appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready;
                },
                5000)) {
            qCritical("Package discovery must finish after CPU fallback");
            return 3;
        }
        return 0;
    }

}

int main(int argc, char **argv) {
    if (argc > 1 &&
        QString::fromLocal8Bit(argv[1]) == QStringLiteral("--unavailable-inference-provider"))
        return unavailableInferenceProvider(argc, argv);
    QCoreApplication application(argc, argv);
    if (application.arguments().value(1) == QStringLiteral("proj"))
        return libreSvipProcessFixture(application.arguments());
    ApplicationWorkflowTests tests;
    return QTest::qExec(&tests, argc, argv);
}
