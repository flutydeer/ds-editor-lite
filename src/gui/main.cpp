//
// Created by fluty on 2023/8/27.
//

#include <QApplication>
#include <QStyleFactory>

#include <TalcsFormat/AudioFormatInputSource.h>
#include <TalcsCore/PositionableMixerAudioSource.h>

#include "g2pglobal.h"

#include "Window/MainWindow.h"

#include "Audio/AudioSystem.h"

int main(int argc, char *argv[]) {
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication a(argc, argv);
    QApplication::setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setOrganizationName("OpenVPI");
    QApplication::setApplicationName("DsEditorLite");
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);

    auto style = QStyleFactory::create("fusion");
    QApplication::setStyle(style);

    auto f = QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    QApplication::setFont(f);

    AudioSystem as;
    as.initialize(QApplication::arguments().contains("-vst"));
    // as.openAudioSettings();

    IKg2p::setDictionaryPath(qApp->applicationDirPath() + "/dict");

    auto w = new MainWindow;
    auto scr = QApplication::screenAt(QCursor::pos());
    auto availableRect = scr->availableGeometry();
    auto left = (availableRect.width() - w->width()) / 2;
    auto top = (availableRect.height() - w->height()) / 2;
    w->move(left, top);
    w->show();

    return QApplication::exec();
}