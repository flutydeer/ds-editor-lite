//
// Created by FlutyDeer on 2023/12/1.
//

#include "AppController.h"
#include "Utils/FillLyric/LyricDialog.h"
#include "syllable2p.h"
#include "Controller/History/HistoryManager.h"
#include "Actions/AppModel/Tempo/TempoActions.h"
#include "Actions/AppModel/TimeSignature/TimeSignatureActions.h"
#include "ClipEditorViewController.h"

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
    int selectedTrackIndex;
    auto selectedClipIndex = AppModel::instance()->selectedClipId();
    auto selectedClip = AppModel::instance()->findClipById(selectedClipIndex, selectedTrackIndex);

    QList<Note *> selectedNotes;
    if (selectedClip != nullptr) {
        selectedNotes = dynamic_cast<SingingClip *>(selectedClip)->selectedNotes();
    }

    qDebug() << "fillLyric: "
             << "trackIndex: " << selectedTrackIndex << "clipIndex: " << selectedClipIndex
             << "selectedNotes: " << selectedNotes.size();

    QList<QList<QString>> lyrics;
    auto lineLyrics = QList<QString>();
    for (auto note : selectedNotes) {
        lineLyrics.append(note->lyric());
        if (note->lineFeed()) {
            lyrics.append(lineLyrics);
            lineLyrics.clear();
        }
    }
    if (!lineLyrics.isEmpty()) {
        lyrics.append(lineLyrics);
    }

    qDebug() << "selected lyrics: " << lyrics;

    auto lyricDialog = new FillLyric::LyricDialog();
    lyricDialog->setLyrics(lyrics);
    lyricDialog->show();

    auto result = lyricDialog->exec();
    if (result == QDialog::Accepted) {
        auto phonics = lyricDialog->exportPhonics();
        if (phonics.isEmpty()) {
            return;
        }

        for (int i = 0; i < std::min(phonics.size(), selectedNotes.size()); i++) {
            auto note = selectedNotes[i];
            if (i < phonics.size()) {
                auto phonic = phonics[i];
                note->setLyric(phonic.lyric);
                note->setPronunciation(phonic.syllable);
                note->setPronunciation(phonic.SyllableRevised);
                note->setLineFeed(phonic.lineFeed);
            }
        }
        ClipEditorViewController::instance()->onEditSelectedNotesLyric();
    }
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
