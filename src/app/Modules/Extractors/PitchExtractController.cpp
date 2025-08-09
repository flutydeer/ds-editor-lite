//
// Created by fluty on 24-11-13.
//

#include "PitchExtractController.h"

#include "ExtractPitchTask.h"
#include "Controller/Actions/AppModel/Param/ParamsActions.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/DrawCurve.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Task/TaskManager.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"

#include <curve-util/CurveUtil.h>

void PitchExtractController::runExtractPitch(const AudioClip *audioClip,
                                             const SingingClip *singingClip) {
    const auto path = audioClip->path();
    const auto task =
        new ExtractPitchTask({singingClip->id(), audioClip->id(), path, appModel->tempo()});
    const auto dlg = new TaskDialog(task, true, true);
    dlg->show();
    connect(task, &Task::finished, this, [=] { onExtractPitchTaskFinished(task); });
    taskManager->addAndStartTask(task);
}

void PitchExtractController::onExtractPitchTaskFinished(ExtractPitchTask *task) {
    taskManager->removeTask(task);
    if (!task->success()) {
        Dialog dialog;
        dialog.setTitle("Task Failed");
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

    QList<Curve *> curves;
    for (const auto &[offset, values] : task->result) {
        const int rawTick = appModel->msToTick(offset);
        const auto &[alignTick, alignValues] =
            CurveUtil::alignCurve(audioClip->start() - singingClip->start() + rawTick, 5,
                                  {values.begin(), values.end()}, 5);
        const auto pitchParam = new DrawCurve;
        pitchParam->setLocalStart(alignTick);
        pitchParam->setValues(Linq::selectMany(alignValues, L_PRED(v, static_cast<int>(v * 100))));
        curves.append(pitchParam);
    }

    const auto a = new ParamsActions;
    a->replaceParam(ParamInfo::Pitch, Param::Edited, curves, singingClip);
    a->execute();
    historyManager->record(a);

    delete task;
}