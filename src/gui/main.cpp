//
// Created by fluty on 2023/8/27.
//

#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QFile>

#include <TalcsFormat/AudioFormatIO.h>
#include <TalcsFormat/AudioFormatInputSource.h>
#include <TalcsCore/PositionableMixerAudioSource.h>
#include <TalcsCore/BufferingAudioSource.h>

#include "Window/MainWindow.h"

#include "Audio/AudioSystem.h"

#include "Controller/PlaybackController.h"

int main(int argc, char *argv[]) {
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication a(argc, argv);
    QApplication::setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setOrganizationName("OpenVPI");
    QApplication::setApplicationName("DsEditorLite");

    auto f = QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    QApplication::setFont(f);

    AudioSystem as;
    as.initialize(QApplication::arguments().contains("-vst"));
    // as.openAudioSettings();

    auto w = new MainWindow;
    auto scr = QApplication::screenAt(QCursor::pos());
    auto availableRect = scr->availableGeometry();
    auto left = (availableRect.width() - w->width()) / 2;
    auto top = (availableRect.height() - w->height()) / 2;
    w->move(left, top);
    w->show();

//    QFile audioFile(R"(D:\CloudMusic\07.恋染色.flac)");
//    talcs::BufferingAudioSource bufSrc(
//        new talcs::AudioFormatInputSource(new talcs::AudioFormatIO(&audioFile), true),
//        true,
//        2,
//        48000
//    );
//    as.masterTrack()->addSource(&bufSrc);

    return QApplication::exec();
}