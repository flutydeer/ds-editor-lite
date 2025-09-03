//
// Created by fluty on 24-11-13.
//

#include "MidiExtractController.h"

#include "ExtractMidiTask.h"
#include "Controller/TrackController.h"
#include "Controller/Actions/AppModel/Track/TrackActions.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/Note.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Task/TaskManager.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "Utils/Linq.h"

#include <QFileInfo>

void MidiExtractController::runExtractMidi(const AudioClip *audioClip) {
    const auto path = audioClip->path();
    const auto task = new ExtractMidiTask({-1, audioClip->id(), path, appModel->tempo()});
    const auto dlg = new TaskDialog(task, true, true);
    dlg->show();
    connect(task, &Task::finished, this, [=] { onExtractMidiTaskFinished(task); });
    taskManager->addAndStartTask(task);
}

void MidiExtractController::onExtractMidiTaskFinished(ExtractMidiTask *task) {
    taskManager->removeTask(task);
    if (!task->success()) {
        Dialog dialog;
        dialog.setTitle("Task Failed");
        dialog.setMessage(tr("Failed to extract Midi from audio:\n %1\n\n%2")
                              .arg(task->input().audioPath)
                              .arg(task->errorMessage()));
        dialog.setModal(true);

        const auto btnClose = new AccentButton(tr("Close"));
        connect(btnClose, &Button::clicked, &dialog, &Dialog::accept);
        dialog.setPositiveButton(btnClose);
        dialog.exec();
        delete task;
        return;
    }

    const auto audioClip =
        dynamic_cast<AudioClip *>(appModel->findClipById(task->input().audioClipId));
    if (!audioClip) {
        delete task;
        return;
    }

    const auto language = appOptions->general()->defaultSingingLanguage;

    // TODO: Fix start
    QList<Note *> notes;
    const auto defaultLyric = appOptions->general()->defaultLyric;
    const auto audioClipStart = audioClip->start();
    const auto singClipStart = audioClip->start();
    for (const auto &[key, start, duration] : task->result) {
        const auto localStart = start + audioClipStart - singClipStart;
        if (localStart < 0)
            continue;
        const auto note = new Note;
        note->setLocalStart(localStart);
        note->setLength(duration);
        note->setKeyIndex(key);
        note->setLyric(defaultLyric);
        note->setLanguage(language);
        notes.append(note);
    }

    auto singingClip = new SingingClip{notes};
    singingClip->setStart(audioClip->start());
    singingClip->setLength(audioClip->length());
    singingClip->setClipLen(audioClip->length());
    singingClip->setDefaultLanguage(appOptions->general()->defaultSingingLanguage);
    const auto track = new Track(QFileInfo(task->input().audioPath).baseName(), {singingClip});
    track->setDefaultLanguage(language);
    trackController->onAppendTrack(track);

    delete task;
}