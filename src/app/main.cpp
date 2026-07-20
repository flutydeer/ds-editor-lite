//
// Created by fluty on 2023/8/27.
//

#include "AppContext.h"
#include "Bootstrap/AppEnvironment.h"
#include "Bootstrap/CrashHandler.h"
#include "Bootstrap/LoggingBootstrap.h"
#include "Bootstrap/Restarter.h"
#include "Bootstrap/StartupArguments.h"
#include "Bootstrap/WindowPlacement.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/PackageManager/PackageManager.h"
#include "UI/Utils/ThemeManager.h"
#include "UI/Utils/Theme/ThemeLoader.h"
#include "UI/Window/MainWindow.h"
#include "Utils/UiLanguageManager.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTimer>

int main(int argc, char *argv[]) {
    QElapsedTimer mstimer;
    mstimer.start();

    AppEnvironment::preInit();
    QApplication a(argc, argv);
    AppEnvironment::postInit();
    LoggingBootstrap::init();

    // AppOptions must be constructed (and the UI language applied) before AppContext
    auto options = std::make_unique<AppOptions>();
    UiLanguageManager uiLanguageManager;
    uiLanguageManager.setPreference(options->general()->uiLanguage);

    auto initialThemeId = qEnvironmentVariable("DS_EDITOR_THEME").trimmed();
    if (initialThemeId.isEmpty())
        initialThemeId = options->appearance()->themeId;

    // Construct AppContext — creates ALL business singletons in dependency order
    AppContext appContext(std::move(options));

    // Infrastructure singletons (stays Meyers static)
    // ThemeManager: load theme, apply palette and QSS
    if (!ThemeManager::instance()->initialize(initialThemeId)) {
        const auto error = ThemeLoader::lastError();
        const auto fallbackThemeId = AppearanceOption::defaultThemeId();
        qWarning("Failed to load initial theme '%s': %s", qPrintable(initialThemeId),
                 qPrintable(error));
        if (initialThemeId == fallbackThemeId ||
            !ThemeManager::instance()->initialize(fallbackThemeId)) {
            qFatal("Failed to load fallback theme '%s': %s", qPrintable(fallbackThemeId),
                   qPrintable(ThemeLoader::lastError()));
        }
    }

    packageManager->initialize();

    MainWindow w;
    WindowPlacement::centerOnScreenAtCursor(w);
    w.show();
#if defined(WITH_DIRECT_MANIPULATION)
    w.registerDirectManipulation();
#endif

    if (const auto filePath = StartupArguments::projectFilePath(); !filePath.isEmpty()) {
        QTimer::singleShot(0, documentWorkflowController,
                           [filePath] { documentWorkflowController->requestOpen(filePath); });
    }

    const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
    qInfo() << "App launched in" << time << "ms";

    CrashHandler crashHandler;
    return Restarter(QDir::currentPath()).restartOrExit(a.exec());
}
