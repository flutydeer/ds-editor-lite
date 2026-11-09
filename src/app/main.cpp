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
#include "Bootstrap/ShutdownMonitor.h"
#include "Bootstrap/SecondaryHandoff.h"
#include "Bootstrap/SingleInstanceCoordinator.h"
#include "Bootstrap/SingleInstanceIdentity.h"
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
#include <QTimer>
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
    const auto runSecondaryHandoff = [&](const bool allowRecovery) {
        SecondaryHandoffOptions options;
        options.interactive = hostMode == AppHostMode::Gui && !nonInteractiveBootstrapErrors;
        options.allowTerminateRecovery = allowRecovery && options.interactive;
        SecondaryHandoffController controller(
            coordinator.serverName(),
            SingleInstanceIdentity::normalizeDataDirectory({}), hostMode);
        return controller.run(startupRequest, options);
    };
    switch (coordinator.start()) {
        case SingleInstanceCoordinator::StartResult::Secondary: {
            if (!parsedArguments.automation.isEmpty()) {
                reportBootstrapError(QStringLiteral(
                    "Automation command-line options cannot be applied while another editor "
                    "instance is running"));
                return EXIT_FAILURE;
            }
            auto handoff = runSecondaryHandoff(true);
            if (handoff.restartAfterRecovery) {
                // Wait for the old process to leave (done in the recovery step),
                // then re-enter the normal lock race. If another instance won,
                // hand off to it instead of forcing ownership.
                coordinator.shutdown();
                if (coordinator.start() == SingleInstanceCoordinator::StartResult::Primary)
                    break;
                handoff = runSecondaryHandoff(false);
            }
            if (handoff.success()) {
                if (hostMode == AppHostMode::Headless) {
                    QTextStream(stderr)
                        << LiteProductMetadata::ProductName
                        << ": another editor instance is already running; the request was "
                           "taken over and no new Headless instance was started."
                        << Qt::endl;
                } else if (handoff.hostMode.contains(QStringLiteral("headless"),
                                                     Qt::CaseInsensitive)) {
                    QTextStream(stderr)
                        << LiteProductMetadata::ProductName
                        << ": the request was taken over by a background instance. No window "
                           "will be opened."
                        << Qt::endl;
                }
                return EXIT_SUCCESS;
            }
            const auto message = handoff.error.isEmpty()
                                     ? QStringLiteral(
                                           "Could not hand the request to the running instance.")
                                     : handoff.error;
            reportBootstrapError(message + QStringLiteral("\n\n") + handoff.diagnostics);
            return EXIT_FAILURE;
        }
        case SingleInstanceCoordinator::StartResult::Error:
            reportBootstrapError(coordinator.errorString());
            return EXIT_FAILURE;
        case SingleInstanceCoordinator::StartResult::Primary:
            break;
    }

    // Startup phase observability. The log line carries the primary instance
    // UUID and the startup request ID so a stalled phase can be correlated with
    // a secondary's handoff diagnostics. None of these reads take an inference
    // lock, so diagnostics stay available while the GPU is initializing.
    const auto logStage = [&](const QString &stage) {
        qInfo().noquote()
            << QStringLiteral("[startup] instance=%1 request=%2 stage=%3 elapsed_ms=%4")
                   .arg(coordinator.instanceId())
                   .arg(startupRequest.requestId)
                   .arg(stage)
                   .arg(QString::number(mstimer.elapsed()));
    };
    logStage(QStringLiteral("single_instance_resolved"));

    LoggingBootstrap::init();
    coordinator.setPrimaryStage(PrimaryStartupStage::Initializing);
    logStage(QStringLiteral("core_initializing"));

    // The watchdog thread exists from here on, inside the healthiest part of the
    // process, and it is never stopped again: the exit budget has to cover the late
    // Qt and static teardown, which is exactly what a stop-before-return would remove.
    auto &shutdownMonitor = ShutdownMonitor::instance();
    shutdownMonitor.start();

    // The earliest aboutToQuit fallback. Connecting it here puts it ahead of every
    // callback a subsystem registers later (inference disposal, automation stop), so
    // a blocking callback can no longer delay the commit that bounds the exit.
    AppContext *activeContext = nullptr;
    QObject::connect(application.get(), &QCoreApplication::aboutToQuit, application.get(),
                     [&activeContext] {
                         if (activeContext)
                             activeContext->commitShutdown();
                     });

    {
        // AppOptions must be constructed (and the UI language applied) before AppContext.
        auto options = std::make_unique<AppOptions>();
        UiLanguageManager uiLanguageManager;
        uiLanguageManager.setPreference(options->general()->uiLanguage);

        // Construct the common composition first. GUI-only singletons are created only in Gui.
        AppContext appContext(std::move(options), hostMode);
        activeContext = &appContext;
        coordinator.noteMainThreadHeartbeat();
        logStage(QStringLiteral("core_ready"));

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
        coordinator.noteMainThreadHeartbeat();
        logStage(QStringLiteral("packages_ready"));

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
                        // Only after the host committed the exit. Logging first would let
                        // a blocked log writer swallow a termination signal that the
                        // operator expects to end the process.
                        qInfo().noquote()
                            << QStringLiteral("Received %1; requesting graceful headless exit")
                                   .arg(signalName);
                        headlessTerminationAccepted = true;
                        return true;
                    },
                    error)) {
                reportBootstrapError(
                    QStringLiteral("Failed to install headless termination handling: %1")
                        .arg(error));
                coordinator.stopAcceptingRequests();
                coordinator.shutdown();
                return EXIT_FAILURE;
            }
        }

        auto hostServices = Automation::createPublicAutomationHostServices(
            *appContext.m_coreRuntime, appContext.m_appModel, appContext.m_synthrtEngine);

        if (hostMode == AppHostMode::Gui) {
            MainWindow w;
            logStage(QStringLiteral("window_constructed"));
            Automation::EditorMcpController mcpController(
                *appContext.m_coreRuntime, *appContext.m_appOptions, coordinator, hostMode,
                parsedArguments.automation, hostServices);
            WindowPlacement windowPlacement(w);
            windowPlacement.restoreOrPlace(appOptions->window()->mainWindowGeometry());
            ExternalOpenRequestQueue requestQueue(documentWorkflowController, &w);
            [[maybe_unused]] const auto startupAdmission = requestQueue.take(startupRequest);
            coordinator.setAdmissionHandler(
                [&requestQueue](const SingleInstanceRequest &request) {
                    return requestQueue.take(request);
                });
            w.show();
            coordinator.setPrimaryStage(PrimaryStartupStage::WindowShown);
            coordinator.noteMainThreadHeartbeat();
            logStage(QStringLiteral("window_shown"));
            // Open takeover only after the event loop has actually spun. An
            // instance stuck before this point owns the lock and answers IPC,
            // but never admits a request, so a secondary cannot silently exit.
            QTimer::singleShot(0, &coordinator, [&coordinator, &logStage] {
                coordinator.enableAdmission();
                logStage(QStringLiteral("event_loop_ready"));
            });

            const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
            qInfo() << "App launched in" << time << "ms";

            CrashHandler crashHandler;
            result = application->exec();
            qInfo() << "Event loop finished";
            coordinator.stopAcceptingRequests();
            const auto saveWindow = appContext.m_coreRuntime->settings().updateWindow(
                {}, {.mainWindowGeometry = windowPlacement.saveGeometry()});
            if (!saveWindow)
                qWarning("Failed to save main-window placement");
        } else {
            HeadlessOpenRequestQueue requestQueue(*appContext.m_coreRuntime,
                                                  hostServices.openDocument);
            [[maybe_unused]] const auto startupAdmission = requestQueue.take(startupRequest);
            coordinator.setAdmissionHandler(
                [&requestQueue](const SingleInstanceRequest &request) {
                    return requestQueue.take(request);
                });
            // A healthy Headless primary has no window. Admission depends on the
            // queue being usable, not on GUI or MCP readiness.
            coordinator.setPrimaryStage(PrimaryStartupStage::Ready);
            coordinator.enableAdmission();
            coordinator.noteMainThreadHeartbeat();
            logStage(QStringLiteral("headless_admission_open"));
            requestQueue.waitUntilIdle();
            coordinator.pauseRequestDispatchAndFlush();
            requestQueue.waitUntilIdle();
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
                coordinator.resumeRequestDispatch();
                const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
                qInfo() << "Headless host launched in" << time << "ms";
                CrashHandler crashHandler;
                result = application->exec();
                headlessTerminationHandler->stop();
                coordinator.stopAcceptingRequests();
            }
        }
        activeContext = nullptr;
    }
    coordinator.shutdown();
    // The watchdog is not stopped here: Restarter, the outer application object and
    // static teardown all run after this point, and C++ static teardown is one of the
    // phases that can hang. The process-lifetime thread ends with the process.
    return Restarter(QDir::currentPath()).restartOrExit(result);
}
