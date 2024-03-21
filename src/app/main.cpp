//
// Created by fluty on 2023/8/27.
//

#include <QApplication>
#include <QStyleFactory>
#include <QScreen>
#include <QTranslator>

#include "UI/Window/MainWindow.h"
#include "Modules/Audio/AudioSystem.h"
#include "Utils/logMessageHandler.h"
#include "Model/AppModel.h"
#include "Model/WorkspaceEditor.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Utils/ThemeManager.h"
#include "UI/Window/TaskWindow.h"

int main(int argc, char *argv[]) {
    // output log to file
    // qInstallMessageHandler(logMessageHandler);
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication a(argc, argv);
    QApplication::setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setOrganizationName("OpenVPI");
    QApplication::setApplicationName("DS Editor Lite");
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);

    // auto style = QStyleFactory::create("fusion");
    // QApplication::setStyle(style);

    auto f = QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setPointSizeF(10);
    QApplication::setFont(f);

    auto translator = new QTranslator;
    auto foundTranslation = translator->load(":translate/translation_zh_CN.qm");
    if (foundTranslation)
        QApplication::installTranslator(translator);

    AudioSystem as;
    as.initialize(QApplication::arguments().contains("-vst"));

    auto taskWindow = new TaskWindow;
    taskWindow->show();

    // 需要存储自定义的信息时，根据唯一名称获取到 editor 对象
    auto editor = AppModel::instance()->workspaceEditor("flutydeer.filllyrics");
    auto workspace = editor->privateWorkspace();
    workspace->insert("version_code", 1);
    workspace->insert("isFirstRun", false);
    workspace->insert("recent_model_path", "D:/Model/test.onnx");
    editor->commit();

    auto globalWorkspace = AppModel::instance()->globalWorkspace();
    // for (const auto &key : globalWorkspace.keys())
    //     qDebug() << "AppModel workspace contains key:" << key;

    // 可根据唯一名称取出存在全局 workspace 中的数据（只读）
    auto privateWorkspace = AppModel::instance()->getPrivateWorkspaceById("flutydeer.testplugin");
    // qDebug() << privateWorkspace.value("recent_model_path").toString();

    QObject::connect(AppOptions::instance(), &AppOptions::optionsChanged, ThemeManager::instance(),
                     &ThemeManager::onAppOptionsChanged);

    auto w = new MainWindow;
    auto scr = QApplication::screenAt(QCursor::pos());
    auto availableRect = scr->availableGeometry();
    auto left = (availableRect.width() - w->width()) / 2;
    auto top = (availableRect.height() - w->height()) / 2;
    w->move(left, top);
    w->show();

    return QApplication::exec();
}