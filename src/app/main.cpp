#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/Mcp/EditorMcpController.h"
#include "Automation/Public/PublicAutomationHostAdapter.h"
#include "Bootstrap/AppEnvironment.h"
#include "Bootstrap/CrashHandler.h"
#include "Bootstrap/ExternalOpenRequestQueue.h"
#include "Bootstrap/HeadlessOpenRequestQueue.h"
#include "Bootstrap/HeadlessTerminationHandler.h"
#include "Bootstrap/LoggingBootstrap.h"
#include "Bootstrap/Restarter.h"
#include "Bootstrap/SingleInstanceCoordinator.h"
#include "Bootstrap/StartupArguments.h"
#include "Bootstrap/WindowPlacement.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Model/AppOptions/AppOptions.h"
#include <lite/PackageManager/PackageManager.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/GUI/Theme/ThemeLoader.h>
#include "UI/Window/MainWindow.h"
#include "Utils/UiLanguageManager.h"
#include <lite/ProductMetadata.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMessageBox>
#include <QTextStream>
#include <QUuid>

#include <cstdlib>
#include <memory>

int main(int argc, char *argv[]) {
    QElapsedTimer mstimer;
    mstimer.start();

    const auto hostMode = StartupArguments::preparseHostMode(argc, argv);
    AppEnvironment::preInit(hostMode);
    std::unique_ptr<QCoreApplication> application;
    if (hostMode == AppHostMode::Gui)
        application = std::make_unique<QApplication>(argc, argv);
    else
        application = std::make_unique<QCoreApplication>(argc, argv);
    AppEnvironment::postInit(hostMode);
    const auto parsedArguments = StartupArguments::parseApplicationArguments();
    if (!parsedArguments.isValid()) {
        QTextStream(stderr) << "Invalid startup argument " << parsedArguments.error->option << ": "
                            << parsedArguments.error->message << Qt::endl;
        return EXIT_FAILURE;
    }
    if (parsedArguments.hostMode != hostMode) {
        QTextStream(stderr) << "Startup host mode changed during full argument parsing" << Qt::endl;
        return EXIT_FAILURE;
    }
    const auto &startupPaths = parsedArguments.projectFilePaths;
    const auto nonInteractiveBootstrapErrors =
        hostMode == AppHostMode::Headless || !parsedArguments.automation.isEmpty() ||
        (hostMode == AppHostMode::Gui &&
         QApplication::platformName().compare(QStringLiteral("offscreen"), Qt::CaseInsensitive) ==
             0) ||
        qEnvironmentVariable("QT_QPA_PLATFORM")
                .compare(QStringLiteral("offscreen"), Qt::CaseInsensitive) == 0;
    const auto reportBootstrapError = [&](const QString &error) {
        if (nonInteractiveBootstrapErrors) {
            QTextStream(stderr) << LiteProductMetadata::ProductName << " bootstrap error: " << error
                                << Qt::endl;
            return;
        }
        if (hostMode == AppHostMode::Gui) {
            QMessageBox::critical(nullptr, QString::fromLatin1(LiteProductMetadata::ProductName),
                                  error);
        } else {
            QTextStream(stderr) << LiteProductMetadata::ProductName << " bootstrap error: " << error
                                << Qt::endl;
        }
    };
    SingleInstanceRequest startupRequest{
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        startupPaths.isEmpty() ? SingleInstanceCommand::Activate
                               : SingleInstanceCommand::OpenProjects,
        startupPaths,
    };

    int result = EXIT_FAILURE;
    SingleInstanceCoordinator coordinator(hostMode);
    switch (coordinator.start()) {
        case SingleInstanceCoordinator::StartResult::Secondary: {
            if (!parsedArguments.automation.isEmpty()) {
                reportBootstrapError(QStringLiteral(
                    "Automation command-line options cannot be applied while another editor "
                    "instance is running"));
                return EXIT_FAILURE;
            }
            QString error;
            if (coordinator.forwardRequest(startupRequest, error))
                return EXIT_SUCCESS;
            reportBootstrapError(error);
            return EXIT_FAILURE;
        }
        case SingleInstanceCoordinator::StartResult::Error:
            reportBootstrapError(coordinator.errorString());
            return EXIT_FAILURE;
        case SingleInstanceCoordinator::StartResult::Primary:
            break;
    }

    LoggingBootstrap::init();
    {
        // AppOptions must be constructed (and the UI language applied) before AppContext.
        auto options = std::make_unique<AppOptions>();
        UiLanguageManager uiLanguageManager;
        uiLanguageManager.setPreference(options->general()->uiLanguage);

        // Construct the common composition first. GUI-only singletons are created only in Gui.
        AppContext appContext(std::move(options), hostMode);

        if (hostMode == AppHostMode::Gui) {
            auto initialThemeId = qEnvironmentVariable("DS_EDITOR_THEME").trimmed();
            if (initialThemeId.isEmpty())
                initialThemeId = appOptions->appearance()->themeId;
            if (!ThemeManager::instance()->initialize(initialThemeId)) {
                const auto error = ThemeLoader::lastError();
                const auto fallbackThemeId = ThemeIds::defaultThemeId();
                qWarning("Failed to load initial theme '%s': %s", qPrintable(initialThemeId),
                         qPrintable(error));
                if (initialThemeId == fallbackThemeId ||
                    !ThemeManager::instance()->initialize(fallbackThemeId)) {
                    qFatal("Failed to load fallback theme '%s': %s", qPrintable(fallbackThemeId),
                           qPrintable(ThemeLoader::lastError()));
                }
            }
        }

        packageManager->initialize(appOptions->general()->packageSearchPaths);

        if (hostMode == AppHostMode::Headless) {
            QString error;
            if (!appContext.initializeDefaultDocument(&error)) {
                reportBootstrapError(
                    QStringLiteral("Failed to initialize the headless document: %1").arg(error));
                coordinator.stopAcceptingRequests();
                coordinator.shutdown();
                return EXIT_FAILURE;
            }
        }

        std::unique_ptr<HeadlessTerminationHandler> headlessTerminationHandler;
        bool headlessTerminationAccepted = false;
        if (hostMode == AppHostMode::Headless) {
            headlessTerminationHandler = std::make_unique<HeadlessTerminationHandler>();
            QString error;
            if (!headlessTerminationHandler->start(
                    [&](const HeadlessTerminationSignal signal) {
                        const auto signalName = headlessTerminationSignalName(signal);
                        qInfo().noquote()
                            << QStringLiteral("Received %1; requesting graceful headless exit")
                                   .arg(signalName);
                        const auto termination =
                            appContext.m_coreRuntime->application().requestTermination(
                                {
                                    .source = Automation::InvocationSource::InternalAutomation,
                                    .clientId = QStringLiteral("console-signal"),
                                },
                                Automation::ApplicationTerminationMode::Exit, true);
                        if (!termination) {
                            const auto &terminationError = termination.getError();
                            qWarning().noquote()
                                << QStringLiteral("Graceful headless exit after %1 was rejected: "
                                                  "%2 (%3)")
                                       .arg(signalName, terminationError.message,
                                            Automation::errorCodeName(terminationError.code));
                            return false;
                        }
                        headlessTerminationAccepted = true;
                        return true;
                    },
                    error)) {
                reportBootstrapError(
                    QStringLiteral("Failed to install headless termination handling: %1").arg(error));
                coordinator.stopAcceptingRequests();
                coordinator.shutdown();
                return EXIT_FAILURE;
            }
        }

        auto hostServices = Automation::createPublicAutomationHostServices(
            *appContext.m_coreRuntime, appContext.m_appModel, appContext.m_synthrtEngine);

        if (hostMode == AppHostMode::Gui) {
            MainWindow w;
            Automation::EditorMcpController mcpController(
                *appContext.m_coreRuntime, *appContext.m_appOptions, coordinator, hostMode,
                parsedArguments.automation, hostServices);
            WindowPlacement windowPlacement(w);
            windowPlacement.restoreOrPlace(appOptions->window()->mainWindowGeometry());
            ExternalOpenRequestQueue requestQueue(documentWorkflowController, &w);
            requestQueue.enqueue(startupRequest);
            coordinator.setRequestHandler([&requestQueue](const SingleInstanceRequest &request) {
                requestQueue.enqueue(request);
            });
            w.show();
#if defined(WITH_DIRECT_MANIPULATION)
            w.registerDirectManipulation();
#endif

            const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
            qInfo() << "App launched in" << time << "ms";

            CrashHandler crashHandler;
            result = application->exec();
            coordinator.stopAcceptingRequests();
            const auto saveWindow = appContext.m_coreRuntime->settings().updateWindow(
                {}, {.mainWindowGeometry = windowPlacement.saveGeometry()});
            if (!saveWindow)
                qWarning("Failed to save main-window placement");
        } else {
            HeadlessOpenRequestQueue requestQueue(*appContext.m_coreRuntime,
                                                  hostServices.openDocument);
            requestQueue.enqueue(startupRequest);
            coordinator.setRequestHandler([&requestQueue](const SingleInstanceRequest &request) {
                requestQueue.enqueue(request);
            });
            for (;;) {
                requestQueue.waitUntilIdle();
                coordinator.flushAcknowledgedRequests();
                if (requestQueue.isIdle())
                    break;
            }
            if (headlessTerminationAccepted) {
                headlessTerminationHandler->stop();
                coordinator.stopAcceptingRequests();
                result = EXIT_SUCCESS;
            } else {
                Automation::EditorMcpController mcpController(
                    *appContext.m_coreRuntime, *appContext.m_appOptions, coordinator, hostMode,
                    parsedArguments.automation, hostServices);
                if (mcpController.nativeEndpoint().isEmpty()) {
                    const auto error =
                        mcpController.errorString().isEmpty()
                            ? QStringLiteral("Native automation endpoint failed to start")
                            : mcpController.errorString();
                    reportBootstrapError(error);
                    headlessTerminationHandler->stop();
                    coordinator.stopAcceptingRequests();
                    mcpController.shutdown();
                    coordinator.shutdown();
                    return EXIT_FAILURE;
                }
                const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
                qInfo() << "Headless host launched in" << time << "ms";
                CrashHandler crashHandler;
                result = application->exec();
                headlessTerminationHandler->stop();
                coordinator.stopAcceptingRequests();
            }
        }
    }
    coordinator.shutdown();
    return Restarter(QDir::currentPath()).restartOrExit(result);
}
