//
// Created by fluty on 2023/8/27.
//

#include "AppContext.h"
#include "Controller/AppController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/ClipController.h"
#include "Controller/ProjectStatusController.h"
#include "Controller/TrackController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/PackageManager/PackageManager.h"
#include "UI/Dialogs/PackageManager/PackageManagerDialog.h"
#include "UI/Window/MainWindow.h"
#include "UI/Window/TaskWindow.h"
#include "Utils/FontManager.h"
#include "Utils/Log.h"
#include "Utils/SystemUtils.h"
#include "UI/Utils/AppColorPalette.h"

#include <QMWidgets/ccombobox.h>
#include <QMWidgets/cmenu.h>

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QScreen>
#include <QStyleHints>
#include <QStyleFactory>
#include <QTimer>
#include <QTranslator>

#include <QtCore/QProcess>

#ifdef APPLICATION_ENABLE_BREAKPAD
#  include <QBreakpadHandler.h>
#endif

#if defined(WITH_DIRECT_MANIPULATION)
#  include <QWDMHCore/DirectManipulationSystem.h>
#endif

class Restarter {
public:
    explicit Restarter(const QString &workingDir) : m_workingDir(workingDir) {
    }

    int restartOrExit(int exitCode) const {
        return qApp->property("restart").toBool() ? restart(exitCode) : exitCode;
    }

    int restart(int exitCode) const {
        qDebug() << "Restarting application...";
        QProcess::startDetached(QApplication::applicationFilePath(), QApplication::arguments(),
                                m_workingDir);
        return exitCode;
    }

private:
    QString m_workingDir;
};

int main(int argc, char *argv[]) {
    QElapsedTimer mstimer;
    mstimer.start();
    // output log to file
    qInstallMessageHandler(Log::handler);
    qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    if (QSysInfo::productType() == "windows")
        QGuiApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    QApplication a(argc, argv);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setOrganizationName("OpenVPI");
    QApplication::setApplicationName("DS Editor Lite");
    QApplication::setApplicationDisplayName("Lite");
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    if (QSysInfo::productType() != "windows")
        QApplication::setStyle(QStyleFactory::create("windows"));
    else
        QApplication::setStyle(QStyleFactory::create("windowsvista"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#else
    qWarning("setColorScheme is not available in this version of Qt.");
#endif

    CMenu::setDefaultCornerPreference(CMenu::Round);
    CComboBox::setDefaultCornerPreference(CComboBox::Round);

    // 设置日志等级和过滤器
    QDir appDataDir(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first());
    if (!appDataDir.exists()) {
        if (!appDataDir.mkpath("."))
            qFatal() << "Failed to create app data directory";
    }
    // Log::setLogDirectory(
    //     QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first() + "/Logs");
    Log::setConsoleLogLevel(Log::Debug);
    // Log::setConsoleTagFilter({"InferPipeline"});
    Log::logSystemInfo();
    Log::logGpuInfo();

    auto f = SystemUtils::isWindows() ? QFont("Microsoft Yahei UI") : QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setPixelSize(13);
    QApplication::setFont(f);

    // Initialize FontManager to load custom fonts early (stays Meyers static)
    FontManager::instance();

    QTranslator translator;
    if (translator.load(":translate/translation_zh_CN.qm"))
        QApplication::installTranslator(&translator);

    // Construct AppContext — creates ALL business singletons in dependency order
    AppContext appContext;

    // Infrastructure singletons (stays Meyers static)
    AppColorPalette::instance()->load(":/theme/lite-dark/app-color-palette.json");

    QObject::connect(inferEngine, &InferEngine::engineInitialized, appController,
                     &AppController::initializeLanguageEngine, Qt::SingleShotConnection);
    if (inferEngine->initialized())
        appController->initializeLanguageEngine();
    packageManager->initialize();

    MainWindow w;
    trackController->setParentWidget(&w);
    auto scr = QApplication::screenAt(QCursor::pos());
    if (!scr) {
        scr = QApplication::primaryScreen();
    }
    if (scr) {
        auto availableRect = scr->availableGeometry();
        auto left = (availableRect.width() - w.width()) / 2;
        auto top = (availableRect.height() - w.height()) / 2;
        w.move(left, top);
    }
    w.show();
#if defined(WITH_DIRECT_MANIPULATION)
    w.registerDirectManipulation();
#endif

    auto args = QApplication::arguments();
    if (args.count() == 2) {
        auto filePath = QApplication::arguments().at(1);
        if (!filePath.isEmpty()) {
            QTimer::singleShot(0, documentWorkflowController,
                               [filePath] { documentWorkflowController->requestOpen(filePath); });
        }
    }

    const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
    qInfo() << "App launched in" << time << "ms";

#ifdef APPLICATION_ENABLE_BREAKPAD
    QBreakpadHandler handler;
    handler.setDumpPath(a.applicationDirPath() + "/dumps");

    QBreakpadHandler::UniqueExtraHandler = []() {
        // Do something when crash occurs.
    };
#endif

    return Restarter(QDir::currentPath()).restartOrExit(a.exec());
}
