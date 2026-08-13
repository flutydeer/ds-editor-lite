#include "AppContext.h"
#include "Bootstrap/AppEnvironment.h"
#include "Bootstrap/CrashHandler.h"
#include "Bootstrap/ExternalOpenRequestQueue.h"
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
#include <QDir>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QUuid>

#include <cstdlib>

int main(int argc, char *argv[]) {
    QElapsedTimer mstimer;
    mstimer.start();

    AppEnvironment::preInit();
    QApplication a(argc, argv);
    AppEnvironment::postInit();
    const auto startupPaths = StartupArguments::projectFilePaths();
    SingleInstanceRequest startupRequest{
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        startupPaths.isEmpty() ? SingleInstanceCommand::Activate
                               : SingleInstanceCommand::OpenProjects,
        startupPaths,
    };

    int result = EXIT_FAILURE;
    SingleInstanceCoordinator coordinator;
    switch (coordinator.start()) {
        case SingleInstanceCoordinator::StartResult::Secondary: {
            QString error;
            if (coordinator.forwardRequest(startupRequest, error))
                return EXIT_SUCCESS;
            QMessageBox::critical(
                nullptr, QString::fromLatin1(LiteProductMetadata::ProductName), error);
            return EXIT_FAILURE;
        }
        case SingleInstanceCoordinator::StartResult::Error:
            QMessageBox::critical(
                nullptr, QString::fromLatin1(LiteProductMetadata::ProductName),
                coordinator.errorString());
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

        auto initialThemeId = qEnvironmentVariable("DS_EDITOR_THEME").trimmed();
        if (initialThemeId.isEmpty())
            initialThemeId = options->appearance()->themeId;

        // Construct AppContext — creates ALL business singletons in dependency order.
        AppContext appContext(std::move(options));

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

        packageManager->initialize(appOptions->general()->packageSearchPaths);

        MainWindow w;
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
        result = a.exec();
        coordinator.stopAcceptingRequests();
        appOptions->window()->setMainWindowGeometry(windowPlacement.saveGeometry());
        if (!appOptions->saveAndNotify(AppOptionsGlobal::Window))
            qWarning("Failed to save main-window placement");
    }
    coordinator.shutdown();
    return Restarter(QDir::currentPath()).restartOrExit(result);
}
