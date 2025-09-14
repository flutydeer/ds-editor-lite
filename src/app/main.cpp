//
// Created by fluty on 2023/8/27.
//

#include "Controller/AppController.h"
#include "Controller/ClipController.h"
#include "Controller/ProjectStatusController.h"
#include "Controller/TrackController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/Audio/utils/DeviceTester.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/PackageManager/PackageManager.h"
#include "UI/Dialogs/PackageManager/PackageManagerDialog.h"
#include "UI/Window/MainWindow.h"
#include "UI/Window/TaskWindow.h"
#include "Utils/Log.h"
#include "Utils/SystemUtils.h"

#include <QApplication>
#include <QDir>
#include <QScreen>
#include <QStyleHints>
#include <QStyleFactory>
#include <QTranslator>

#include <QtCore/QProcess>

#ifdef APPLICATION_ENABLE_BREAKPAD
#  include <QBreakpadHandler.h>
#endif

#if defined(WITH_DIRECT_MANIPULATION)
#  include <QWDMHCore/DirectManipulationSystem.h>
#endif

struct AudioSystemContext {
    AudioSystemContext() {
        AudioSystem::outputSystem()->initialize();
        AudioSystem::midiSystem()->initialize();
        // Managed by Qt Object System. No need for manual memory management.
        new DeviceTester(&audioSystem);
        new AudioContext(&audioSystem);
    }

    AudioSystem audioSystem;
};

struct AppContext {
#if defined(WITH_DIRECT_MANIPULATION)
    QWDMH::DirectManipulationSystem directManipSystem;
#endif
    AudioSystemContext audio;
};

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
    // QApplication::setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setOrganizationName("OpenVPI");
    QApplication::setApplicationName("DS Editor Lite");
    QApplication::setApplicationDisplayName("Lite");
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    if (QSysInfo::productType() != "windows")
        QApplication::setStyle(QStyleFactory::create("windows"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#else
    qWarning("setColorScheme is not available in this version of Qt.");
#endif

    // 设置日志等级和过滤器
    QDir appDataDir(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first());
    if (!appDataDir.exists()) {
        if (!appDataDir.mkpath("."))
            qFatal() << "Failed to create app data directory";
    }
    // Log::setLogDirectory(
    //     QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first() + "/Logs");
    Log::setConsoleLogLevel(Log::Debug);
    Log::setConsoleTagFilter(
        {/*"InferDurationTask", "InferPitchTask", "InferVarianceTask", "InferAcousticTask"*/});
    Log::logSystemInfo();
    Log::logGpuInfo();

    auto f = SystemUtils::isWindows() ? QFont("Microsoft Yahei UI") : QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setPixelSize(13);
    QApplication::setFont(f);

    QTranslator translator;
    if (translator.load(":translate/translation_zh_CN.qm"))
        QApplication::installTranslator(&translator);

    AppContext appContext;

    AppController::instance();
    InferEngine::instance();
    PackageManager::instance();

    QObject::connect(inferEngine, &InferEngine::engineInitialized, packageManager,
                     &PackageManager::initialize, Qt::SingleShotConnection);

    // 需要存储自定义的信息时，根据唯一名称获取到 editor 对象
    // auto editor = appModel->workspaceEditor("flutydeer.filllyrics");
    // auto workspace = editor->privateWorkspace();
    // workspace->insert("version_code", 1);
    // workspace->insert("isFirstRun", false);
    // workspace->insert("recent_model_path", "D:/Model/test.onnx");
    // editor->commit();

    // auto globalWorkspace = appModel->globalWorkspace();
    // for (const auto &key : globalWorkspace.keys())
    //     qDebug() << "AppModel workspace contains key:" << key;

    // 可根据唯一名称取出存在全局 workspace 中的数据（只读）
    // auto privateWorkspace = appModel->getPrivateWorkspaceById("flutydeer.testplugin");
    // qDebug() << privateWorkspace.value("recent_model_path").toString();

    MainWindow w;
    trackController->setParentWidget(&w);
    auto scr = QApplication::screenAt(QCursor::pos());
    auto availableRect = scr->availableGeometry();
    auto left = (availableRect.width() - w.width()) / 2;
    auto top = (availableRect.height() - w.height()) / 2;
    w.move(left, top);
    w.show();
#if defined(WITH_DIRECT_MANIPULATION)
    w.registerDirectManipulation();
#endif

    // auto taskWindow = new TaskWindow(&w);
    // taskWindow->move(availableRect.width() - taskWindow->width() - 8,
    //                  availableRect.height() - taskWindow->height() - 8);
    // taskWindow->show();

    auto args = QApplication::arguments();
    if (args.count() == 2) {
        auto filePath = QApplication::arguments().at(1);
        if (!filePath.isEmpty()) {
            QString errorMsg;
            if (appController->openFile(filePath, errorMsg)) {
                auto tracks = appModel->tracks();
                if (!tracks.isEmpty()) {
                    auto clips = tracks.first()->clips();
                    if (clips.count() > 0) {
                        trackController->setActiveClip(clips.toList().first()->id());
                        appController->setActivePanel(AppGlobal::ClipEditor);
                    }
                }
            }
        }
    }

    // trackController->onNewTrack();
    // auto clip = trackController->onNewSingingClip(1, 3840);
    // clipController->setClip(clip);
    // for (int i = 0; i < 10; i++) {
    //     auto note = new Note;
    //     note->setRStart(i * 240);
    //     note->setLength(240);
    //     note->setLyric("啦");
    //     note->setDefaultLanguage("cmn");
    //     clipController->onInsertNote(note);
    // }

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