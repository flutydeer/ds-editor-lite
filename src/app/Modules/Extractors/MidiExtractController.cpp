#include "MidiExtractController.h"

#include "ExtractMidiTask.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/ProjectAutomationDtos.h"
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include "Model/AppOptions/AppOptions.h"
#include <lite/Tasking/TaskManager.h>
#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Toast.h>
#include "UI/Dialogs/Base/TaskDialog.h"
#include <lite/Support/Linq.h>

#include <QFileInfo>

namespace {
Automation::CoreRuntime *automationRuntime() {
    return AppContext::instance<Automation::CoreRuntime>();
}
}

MidiExtractController::MidiExtractController(QObject *parent) : ModelChangeHandler(parent) {
}

MidiExtractController::~MidiExtractController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(MidiExtractController)

void MidiExtractController::runExtractMidi(const AudioClip *audioClip) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    const auto path = audioClip->path();
    ExtractTask::Input input;
    input.audioClipId = audioClip->id();
    input.audioPath = path;
    input.timeline = appModel->timeline();
    input.audioClipStartTick = audioClip->start();
    input.audioClipLengthTick = audioClip->length();
    input.document = runtime->documentVersion();
    const auto task = new ExtractMidiTask(std::move(input));
    const auto automationTask = runtime->automationTasks().createTask(
        QStringLiteral("tracks.insert"), task->input().document,
        Automation::ObjectRef{Automation::ObjectKind::Clip, audioClip->id()},
        [task] { task->terminate(); });
    task->setAutomationTaskId(automationTask.taskId);
    runtime->automationTasks().markRunning(automationTask.taskId);
    const auto dlg = new TaskDialog(task, true, true);
    dlg->show();
    connect(task, &Task::finished, this, [=] { onExtractMidiTaskFinished(task); });
    taskManager->addAndStartTask(task);
}

void MidiExtractController::onExtractMidiTaskFinished(ExtractMidiTask *task) {
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

    if (runtime->automationTasks().isCancellationRequested(task->input().automationTaskId)) {
        runtime->automationTasks().cancel(task->input().automationTaskId);
        delete task;
        return;
    }

    const auto language = appOptions->general()->defaultSingingLanguage;
    const auto defaultLyric = appOptions->general()->defaultLyricForLanguage(language);
    Automation::TrackDraftDto track;
    track.name = QFileInfo(task->input().audioPath).baseName();
    track.defaultLanguage = language;
    Automation::ClipDraftDto clip;
    clip.type = Automation::ClipDraftDto::Type::Singing;
    clip.properties.start = task->input().audioClipStartTick;
    clip.properties.length = task->input().audioClipLengthTick;
    clip.properties.clipLen = task->input().audioClipLengthTick;
    clip.defaultLanguage = language;
    for (const auto &[key, start, duration] : task->result) {
        const auto localStart = start;
        if (localStart < 0)
            continue;
        clip.notes.append({
            .localStart = localStart,
            .length = duration,
            .keyIndex = key,
            .lyric = defaultLyric,
            .language = language,
        });
    }
    track.clips.append(std::move(clip));

    const auto project = runtime->project().getProject(task->input().document.documentId);
    if (!project) {
        runtime->automationTasks().fail(task->input().automationTaskId, project.getError());
        delete task;
        return;
    }
    const auto index = project.get().tracks.size();
    Automation::CommandContext context{
        .expected = task->input().document,
        .validateOnly = true,
        .source = Automation::InvocationSource::TrustedGui,
    };
    const auto validation = runtime->project().insertTrack(context, index, track);
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
    context.validateOnly = false;
    const auto result = runtime->project().insertTrack(context, index, track);
    if (result)
        runtime->automationTasks().succeed(task->input().automationTaskId, result.get());
    else
        runtime->automationTasks().fail(task->input().automationTaskId, result.getError());

    delete task;
}
