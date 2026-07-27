//
// Created by fluty on 24-11-13.
//

#include "PitchExtractController.h"

#include "ExtractPitchTask.h"
#include "Controller/Actions/AppModel/Param/ParamsActions.h"
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include "Modules/History/HistoryManager.h"
#include <lite/Tasking/TaskManager.h>
#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Toast.h>
#include "UI/Dialogs/Base/TaskDialog.h"
#include <lite/Support/Linq.h>
#include <lite/Support/MathUtils.h>

PitchExtractController::PitchExtractController(QObject *parent) : ModelChangeHandler(parent) {
}

PitchExtractController::~PitchExtractController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(PitchExtractController)

void PitchExtractController::runExtractPitch(const AudioClip *audioClip,
                                             const SingingClip *singingClip) {
    const auto path = audioClip->path();
    const auto &timeline = appModel->timeline();
    const int visibleStartTick = audioClip->start() + audioClip->clipStart();
    const double visibleStartMs = timeline.tickToMs(visibleStartTick);
    const double trimStartMs = audioClip->hasRealTimeAnchor()
                                   ? audioClip->trimStartMs()
                                   : visibleStartMs - timeline.tickToMs(audioClip->start());
    const double visibleLengthMs =
        audioClip->hasRealTimeAnchor()
            ? audioClip->playLengthMs()
            : timeline.tickToMs(visibleStartTick + audioClip->clipLen()) - visibleStartMs;

    ExtractTask::Input input;
    input.singingClipId = singingClip->id();
    input.audioClipId = audioClip->id();
    input.audioPath = path;
    input.timeline = timeline;
    input.singingClipStartTick = singingClip->start();
    input.audioMaterialOriginMs = visibleStartMs - trimStartMs;
    input.audioVisibleStartMs = visibleStartMs;
    input.audioVisibleEndMs = visibleStartMs + visibleLengthMs;
    const auto task = new ExtractPitchTask(std::move(input));
    const auto dlg = new TaskDialog(task, true, true);
    dlg->show();
    connect(task, &Task::finished, this, [=] { onExtractPitchTaskFinished(task); });
    taskManager->addAndStartTask(task);
}

void PitchExtractController::onExtractPitchTaskFinished(ExtractPitchTask *task) {
    taskManager->removeTask(task);
    if (!task->success()) {
        Dialog dialog;
        dialog.setTitle(tr("Task Failed"));
        dialog.setMessage(tr("Failed to extract pitch from audio:\n %1\n\n%2")
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
    const auto singingClip =
        dynamic_cast<SingingClip *>(appModel->findClipById(task->input().singingClipId));
    if (!audioClip || !singingClip) {
        delete task;
        return;
    }
    if (singingClip->start() != task->input().singingClipStartTick ||
        appModel->timeline() != task->input().timeline) {
        qWarning() << "Discarding stale pitch extraction result after timeline/clip movement";
        delete task;
        return;
    }

    QList<Curve *> curves;
    for (const auto &[globalStartTick, values] : task->result) {
        const auto pitchParam = new DrawCurve;
        pitchParam->setLocalStart(globalStartTick - singingClip->start());
        pitchParam->setValues(Linq::selectMany(values, L_PRED(v, static_cast<int>(v * 100))));
        curves.append(pitchParam);
    }

    const auto a = new ParamsActions;
    a->replaceParam(ParamInfo::Pitch, Param::Edited, curves, singingClip);
    a->execute();
    historyManager->record(a);

    delete task;
}
