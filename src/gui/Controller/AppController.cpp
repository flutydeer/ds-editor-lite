//
// Created by FlutyDeer on 2023/12/1.
//

#include <QDebug>
#include <QApplication>
#include <QMessageBox>

#include "AppController.h"
#include "g2pglobal.h"
#include "mandarin.h"
#include "syllable2p.h"

void AppController::onNewProject() {
    AppModel::instance()->newProject();
}
void AppController::openProject(const QString &filePath) {
    AppModel::instance()->loadProject(filePath);
}
void AppController::importMidiFile(const QString &filePath) {
    AppModel::instance()->importMidiFile(filePath);
}
void AppController::exportMidiFile(const QString &filePath) {
    AppModel::instance()->exportMidiFile(filePath);
}
void AppController::onRunG2p() {
    auto model = AppModel::instance();
    auto firstTrack = model->tracks().first();
    auto firstClip = firstTrack->clips().at(0);
    auto singingClip = dynamic_cast<DsSingingClip *>(firstClip);
    if (singingClip == nullptr)
        return;
    auto notes = singingClip->notes();
    IKg2p::setDictionaryPath(qApp->applicationDirPath() + "/dict");

    auto g2p_man = new IKg2p::Mandarin();
    auto syllable2p = new IKg2p::Syllable2p(qApp->applicationDirPath() +
                                            "/res/phonemeDict/opencpop-extension.txt");

    QStringList lyrics;
    for (const auto &note : notes) {
        lyrics.append(note->lyric());
    }

    QStringList pinyinRes = g2p_man->hanziToPinyin(lyrics, false, false);
    QList<QStringList> syllableRes = syllable2p->syllableToPhoneme(pinyinRes);
    qDebug() << pinyinRes;
    qDebug() << syllableRes;
}
void AppController::onSetTempo(double tempo) {
    // TODO: validate tempo
    auto model = AppModel::instance();
    model->setTempo(tempo > 0 ? tempo : model->tempo());
}
void AppController::onSetTimeSignature(int numerator, int denominator) {
    auto model = AppModel::instance();
    if (isPowerOf2(denominator)) {
        model->setNumerator(numerator);
        model->setDenominator(denominator);
    } else {
        // QMessageBox msgBox;
        // msgBox.setText("Error");
        // msgBox.setInformativeText("Denominator error.");
        // msgBox.setStandardButtons(QMessageBox::Yes);
        // msgBox.setDefaultButton(QMessageBox::Yes);
        // msgBox.exec();
        model->setNumerator(model->numerator());
        model->setDenominator(model->denominator());
    }
}
void AppController::onSetQuantize(int quantize) {
    AppModel::instance()->setQuantize(quantize);
}
void AppController::onTrackSelectionChanged(int trackIndex) {
    AppModel::instance()->setSelectedTrack(trackIndex);
}
bool AppController::isPowerOf2(int num) {
    return num > 0 && ((num & (num - 1)) == 0);
}