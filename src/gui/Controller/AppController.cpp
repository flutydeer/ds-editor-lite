//
// Created by FlutyDeer on 2023/12/1.
//

#include "AppController.h"
#include "Utils/FillLyric/LyricDialog.h"
#include "syllable2p.h"
#include "Controller/History/HistoryManager.h"
#include "Actions/AppModel/Tempo/TempoActions.h"
#include "Actions/AppModel/TimeSignature/TimeSignatureActions.h"

void AppController::onNewProject() {
    AppModel::instance()->newProject();
    HistoryManager::instance()->reset();
    m_lastProjectPath = "";
}
void AppController::openProject(const QString &filePath) {
    AppModel::instance()->loadProject(filePath);
    HistoryManager::instance()->reset();
    m_lastProjectPath = filePath;
}
void AppController::saveProject(const QString &filePath) {
    if (AppModel::instance()->saveProject(filePath)) {
        m_lastProjectPath = filePath;
    }
}
void AppController::importMidiFile(const QString &filePath) {
    AppModel::instance()->importMidiFile(filePath);
}
void AppController::exportMidiFile(const QString &filePath) {
    AppModel::instance()->exportMidiFile(filePath);
}
void AppController::importAproject(const QString &filePath) {
    AppModel::instance()->importAProject(filePath);
    HistoryManager::instance()->reset();
    m_lastProjectPath = "";
}

void AppController::fillLyric() {
    auto model = AppModel::instance();

    auto selectedTrack = model->tracks().at(0);

    auto clips = selectedTrack->clips();
    auto firstClip = clips.at(0);

    auto singingClip = dynamic_cast<SingingClip *>(firstClip);
    if (singingClip == nullptr)
        return;
    auto notes = singingClip->notes();

    QStringList lyrics;
    for (const auto &note : notes) {
        lyrics.append(note->lyric());
    }

    auto lyricDialog = new FillLyric::LyricDialog();
    lyricDialog->setLyrics(lyrics.join(" "));
    lyricDialog->show();

    lyricDialog->exec();
    delete lyricDialog;
}
void AppController::onSetTempo(double tempo) {
    // TODO: validate tempo
    auto model = AppModel::instance();
    auto oldTempo = model->tempo();
    auto newTempo = tempo > 0 ? tempo : model->tempo();
    auto actions = new TempoActions;
    actions->editTempo(oldTempo, newTempo, model);
    actions->execute();
    HistoryManager::instance()->record(actions);
}
void AppController::onSetTimeSignature(int numerator, int denominator) {
    auto model = AppModel::instance();
    auto oldSig = model->timeSignature();
    auto newSig = AppModel::TimeSignature(numerator, denominator);
    auto actions = new TimeSignatureActions;
    if (isPowerOf2(denominator)) {
        actions->editTimeSignature(oldSig, newSig, model);
    } else {
        // QMessageBox msgBox;
        // msgBox.setText("Error");
        // msgBox.setInformativeText("Denominator error.");
        // msgBox.setStandardButtons(QMessageBox::Yes);
        // msgBox.setDefaultButton(QMessageBox::Yes);
        // msgBox.exec();
        actions->editTimeSignature(oldSig, oldSig, model);
    }
    actions->execute();
    HistoryManager::instance()->record(actions);
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

QString AppController::lastProjectPath() const {
    return m_lastProjectPath;
}
