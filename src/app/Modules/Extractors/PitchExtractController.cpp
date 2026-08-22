#include "PitchExtractController.h"

#include "ExtractPitchTask.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/ProjectAutomationDtos.h"
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/Tasking/TaskManager.h>
#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Toast.h>
#include "UI/Dialogs/Base/TaskDialog.h"
#include <lite/Support/Linq.h>
#include <lite/Support/MathUtils.h>

namespace {
Automation::CoreRuntime *automationRuntime() {
    return AppContext::instance<Automation::CoreRuntime>();
}
}

PitchExtractController::PitchExtractController(QObject *parent) : ModelChangeHandler(parent) {
}

PitchExtractController::~PitchExtractController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(PitchExtractController)

void PitchExtractController::runExtractPitch(const AudioClip *audioClip,
                                             const SingingClip *singingClip) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
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
    input.document = runtime->documentVersion();
    const auto task = new ExtractPitchTask(std::move(input));
    const auto automationTask = runtime->automationTasks().createTask(
        QStringLiteral("parameters.replace"), task->input().document,
        Automation::ObjectRef{Automation::ObjectKind::Clip, singingClip->id()},
        [task] { task->terminate(); });
    task->setAutomationTaskId(automationTask.taskId);
    runtime->automationTasks().markRunning(automationTask.taskId);
    const auto dlg = new TaskDialog(task, true, true);
    dlg->show();
    connect(task, &Task::finished, this, [=] { onExtractPitchTaskFinished(task); });
    taskManager->addAndStartTask(task);
}

void PitchExtractController::onExtractPitchTaskFinished(ExtractPitchTask *task) {
    taskManager->removeTask(task);
    auto *runtime = automationRuntime();
    if (!runtime) {
        delete task;
        return;
    }
    if (!task->success()) {
        if (task->errorCode() == ExtractTask::ErrorCode::Terminated ||
            runtime->automationTasks().isCancellationRequested(task->input().automationTaskId)) {
            runtime->automationTasks().cancel(task->input().automationTaskId);
            delete task;
            return;
        } else {
            runtime->automationTasks().fail(
                task->input().automationTaskId,
                Automation::AutomationError{
                    .code = Automation::AutomationErrorCode::InternalError,
                    .message = task->errorMessage(),
                });
        }
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

    if (runtime->automationTasks().isCancellationRequested(task->input().automationTaskId)) {
        runtime->automationTasks().cancel(task->input().automationTaskId);
        delete task;
        return;
    }

    QList<Automation::CurveDraftDto> curves;
    for (const auto &[globalStartTick, values] : task->result) {
        Automation::CurveDraftDto pitchParam;
        pitchParam.type = Automation::CurveDraftDto::Type::Draw;
        pitchParam.localStart = globalStartTick - task->input().singingClipStartTick;
        pitchParam.values = Linq::selectMany(values, L_PRED(v, static_cast<int>(v * 100)));
        curves.append(std::move(pitchParam));
    }

    Automation::CommandContext validationContext{
        .expected = task->input().document,
        .validateOnly = true,
        .source = Automation::InvocationSource::TrustedGui,
    };
    const auto validation = runtime->parameters().replaceParameter(
        validationContext, Automation::ClipId(task->input().singingClipId), ParamInfo::Pitch,
        Param::Edited, curves);
    if (!validation) {
        runtime->automationTasks().fail(task->input().automationTaskId, validation.getError());
        delete task;
        return;
    }
    const auto committing =
        runtime->automationTasks().beginCommitting(task->input().automationTaskId);
    if (!committing || !committing.get()) {
        if (committing)
            runtime->automationTasks().cancel(task->input().automationTaskId);
        delete task;
        return;
    }
    validationContext.validateOnly = false;
    const auto result = runtime->parameters().replaceParameter(
        validationContext, Automation::ClipId(task->input().singingClipId), ParamInfo::Pitch,
        Param::Edited, curves);
    if (result)
        runtime->automationTasks().succeed(task->input().automationTaskId, result.get());
    else
        runtime->automationTasks().fail(task->input().automationTaskId, result.getError());

    delete task;
}
