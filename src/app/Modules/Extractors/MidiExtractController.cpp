//
// Created by fluty on 24-11-13.
//

#include "MidiExtractController.h"

#include "ExtractMidiTask.h"
#include "Controller/Actions/AppModel/Note/NoteActions.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/Note.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Task/TaskManager.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "Utils/Linq.h"

void MidiExtractController::runExtractMidi(const AudioClip *audioClip,
                                           const SingingClip *singingClip) {
    const auto path = audioClip->path();
    const auto task =
        new ExtractMidiTask({singingClip->id(), audioClip->id(), path, appModel->tempo()});
    const auto dlg = new TaskDialog(task, true, true);
    dlg->show();
    connect(task, &Task::finished, this, [=] { onExtractMidiTaskFinished(task); });
    taskManager->addAndStartTask(task);
}

void MidiExtractController::onExtractMidiTaskFinished(ExtractMidiTask *task) {
    taskManager->removeTask(task);
    if (!task->success) {
        Dialog dialog;
        dialog.setTitle("Task Failed");
        dialog.setMessage(
            tr("Failed to extract Midi from audio:\n %1").arg(task->input().audioPath));
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
    const auto singingClip =
        dynamic_cast<SingingClip *>(appModel->findClipById(task->input().singingClipId));
    if (!audioClip || !singingClip) {
        delete task;
        return;
    }

    // TODO: Fix start
    QList<Note *> notes;
    for (const auto &[key, start, duration] : task->result) {
        const auto note = new Note;
        note->setStart(start);
        note->setLength(duration);
        note->setKeyIndex(key);
        note->setLanguage(singingClip->defaultLanguage);
        note->setG2pId(singingClip->defaultG2pId);
        notes.append(note);
    }

    auto a = new NoteActions;
    a->insertNotes(notes, singingClip);
    a->execute();
    historyManager->record(a);

    delete task;
}