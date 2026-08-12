#include "ApplicationContext.h"
#include "Bootstrap/AppEnvironment.h"
#include "Bootstrap/CrashHandler.h"
#include "Bootstrap/DocumentManager.h"
#include "Bootstrap/LoggingBootstrap.h"
#include "Bootstrap/Restarter.h"
#include "Bootstrap/SessionAwareApplication.h"
#include "Bootstrap/SingleInstanceCoordinator.h"
#include "Bootstrap/StartupArguments.h"
#include "Model/AppOptions/AppOptions.h"
#include <lite/PackageManager/PackageManager.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/GUI/Theme/ThemeLoader.h>
#include "Utils/UiLanguageManager.h"

#include <QDir>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QUuid>

#include <cstdlib>

int main(int argc, char *argv[]) {
    QElapsedTimer mstimer;
    mstimer.start();

    AppEnvironment::preInit();
    SessionAwareApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);
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
            QMessageBox::critical(nullptr, QObject::tr("DS Editor Lite"), error);
            return EXIT_FAILURE;
        }
        case SingleInstanceCoordinator::StartResult::Error:
            QMessageBox::critical(nullptr, QObject::tr("DS Editor Lite"),
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

        // Application services outlive all document sessions and windows.
        ApplicationContext applicationContext(std::move(options));

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

        DocumentManager documentManager;
        documentManager.handleRequest(startupRequest);
        coordinator.setRequestHandler([&documentManager](const SingleInstanceRequest &request) {
            documentManager.handleRequest(request);
        });

        const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
        qInfo() << "App launched in" << time << "ms";

        CrashHandler crashHandler;
        result = a.exec();
        coordinator.stopAcceptingRequests();
    }
    coordinator.shutdown();
    return Restarter(QDir::currentPath()).restartOrExit(result);
}
