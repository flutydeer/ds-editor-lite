#include "TrackController.h"
#include <TalcsFormat/AbstractAudioFormatIO.h>

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "EditorViewController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/AudioInfoModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include <lite/Tasking/TaskManager.h>
#include "Tasks/ComputeAudioHashTask.h"
#include "Tasks/DecodeAudioTask.h"
#include <lite/GUI/Controls/AccentButton.h>
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Views/TrackEditor/GraphicsItem/AudioClipView.h"
#include "Global/AppGlobal.h"
#include "Global/ControllerGlobal.h"
#include "Modules/Import/AudioFilePreparer.h"
#include <lite/ProjectModel/Utils/DiffscopeAudioWorkspace.h>

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QMimeData>

namespace {
    Automation::CoreRuntime *automationRuntime() {
        return AppContext::instance<Automation::CoreRuntime>();
    }

    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime) {
        return {.expected = runtime.documentVersion(),
                .source = Automation::InvocationSource::TrustedGui};
    }

    Automation::GuiDocumentCommandContext
        guiDocumentContext(const Automation::CoreRuntime &runtime) {
        return {.expected = runtime.documentVersion(),
                .windowId = runtime.windowId(),
                .source = Automation::InvocationSource::TrustedGui};
    }

    Automation::ClipPropertiesDto clipPropertiesDto(
        const Clip::ClipCommonProperties &properties) {
        return {
            .id = Automation::ClipId(properties.id),
            .name = properties.name,
            .start = properties.start,
            .length = properties.length,
            .clipStart = properties.clipStart,
            .clipLen = properties.clipLen,
            .gain = properties.gain,
            .mute = properties.mute,
            .trimStartMs = properties.trimStartMs,
            .playLengthMs = properties.playLengthMs,
            .materialLengthMs = properties.materialLengthMs,
        };
    }
}

TrackController::TrackController(QObject *parent) : QObject(parent) {
}

TrackController::~TrackController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(TrackController)

void TrackController::setParentWidget(QWidget *view) {
    m_parentWidget = view;
}

void TrackController::onNewTrack() {
    onInsertNewTrack(appModel->tracks().count());
}

void TrackController::onInsertNewTrack(const qsizetype index) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    Automation::TrackDraftDto draft;
    draft.name = tr("New Track");
    draft.defaultLanguage = appOptions->general()->defaultSingingLanguage;
    runtime->project().insertTrack(commandContext(*runtime), index, draft);
}

void TrackController::onAppendTrack(Track *track) {
    if (!track)
        return;
    const auto draft = Automation::trackDraftDto(*track);
    delete track;
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    runtime->project().insertTrack(commandContext(*runtime), appModel->tracks().count(), draft);
}

void TrackController::onRemoveTrack(const int id) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    runtime->project().removeTracks(commandContext(*runtime), {Automation::TrackId(id)});
}

void TrackController::onMoveTrack(const qsizetype fromIndex, const qsizetype toIndex) {
    auto *runtime = automationRuntime();
    if (!runtime || fromIndex < 0 || fromIndex >= appModel->tracks().size())
        return;
    const auto trackId = Automation::TrackId(appModel->tracks().at(fromIndex)->id());
    runtime->project().moveTrack(commandContext(*runtime), trackId, toIndex);
}

void TrackController::addAudioClipToNewTrack(const QString &filePath) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    Automation::TrackDraftDto track;
    Automation::ClipDraftDto clip;
    clip.type = Automation::ClipDraftDto::Type::Audio;
    clip.audioPath = filePath;
    track.clips.append(std::move(clip));
    runtime->project().insertTrack(commandContext(*runtime), appModel->tracks().size(), track);
}

void TrackController::setActiveClip(const int clipId) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    const auto id = clipId >= 0 ? std::optional(Automation::ClipId(clipId)) : std::nullopt;
    runtime->facade().setActiveClip(guiDocumentContext(*runtime), id);
}

void TrackController::setSelectedTrackIndex(const int trackIndex) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    std::optional<Automation::TrackId> trackId;
    if (trackIndex >= 0 && trackIndex < appModel->tracks().size())
        trackId = Automation::TrackId(appModel->tracks().at(trackIndex)->id());
    else if (trackIndex >= 0)
        return;
    runtime->facade().setSelectedTrack(guiDocumentContext(*runtime), trackId);
}

void TrackController::setSelectedClips(const QList<int> &clipIds) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    QList<Automation::ClipId> ids;
    ids.reserve(clipIds.size());
    for (const auto id : clipIds)
        ids.append(Automation::ClipId(id));
    runtime->facade().setSelectedClips(guiDocumentContext(*runtime), ids);
}

void TrackController::changeTrackProperty(const Track::TrackProperties &args) {
    qDebug() << "TrackController::changeTrackProperty" << args.gain << args.pan;
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    runtime->project().setTrackProperties(
        commandContext(*runtime),
        {Automation::TrackId(args.id), args.name, args.gain, args.pan, args.mute, args.solo});
}

void TrackController::changeTrackColor(const int trackId, const int colorIndex) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    runtime->project().setTrackColor(commandContext(*runtime), Automation::TrackId(trackId),
                                     colorIndex);
}

void TrackController::changeTrackDefaultLanguage(const int trackId, const QString &language) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    runtime->project().setTrackDefaultLanguage(commandContext(*runtime),
                                               Automation::TrackId(trackId), language);
}

void TrackController::onAddAudioClip(const QString &path, talcs::AbstractAudioFormatIO *io,
                                     const QJsonObject &workspace, const int id, const int tick) {
    // The decode task takes ownership of `io` (the dialog already probed the
    // format); both dialog and drag-drop paths go through the same preparer.
    auto *decodeTask = AudioFilePreparer::createPrepareTask(path, io, workspace);
    if (const auto *runtime = automationRuntime())
        decodeTask->documentVersion = runtime->documentVersion();
    decodeTask->trackId = id;
    decodeTask->tick = tick;
    const auto dlg = new TaskDialog(decodeTask, true, true, m_parentWidget);
    dlg->show();
    connect(decodeTask, &Task::finished, this,
            [decodeTask, this] { handleDecodeAudioTaskFinished(decodeTask); });
    taskManager->addTask(decodeTask);
    taskManager->startTask(decodeTask);
}

void TrackController::onRelocateAudioClip(const int clipId, const QString &path,
                                          talcs::AbstractAudioFormatIO *io,
                                          const QJsonObject &workspace) {
    delete io;
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    const AudioPathInfo newInfo{DiffscopeAudioWorkspace::relativeDirFor(
                                    path, documentWorkflowController->projectPath()),
                                {}};
    const auto result = runtime->project().relocateAudioClip(
        commandContext(*runtime), Automation::ClipId(clipId), path, newInfo, workspace);
    if (result) {
        const auto *clip = appModel->findClipById(clipId);
        if (clip && clip->clipType() == IClip::Audio)
            scheduleHashUpdate(static_cast<const AudioClip *>(clip));
    }
}

void TrackController::confirmAudioClipPath(const int clipId) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    const auto result = runtime->project().confirmAudioClipPath(
        commandContext(*runtime), Automation::ClipId(clipId));
    if (result) {
        const auto *clip = appModel->findClipById(clipId);
        if (clip && clip->clipType() == IClip::Audio)
            scheduleHashUpdate(static_cast<const AudioClip *>(clip));
    }
}

void TrackController::onClipPropertyChanged(const Clip::ClipCommonProperties &args) {
    onClipPropertyChanged(args, -1);
}

void TrackController::onClipPropertyChanged(const Clip::ClipCommonProperties &args,
                                            const int newTrackIndex) {
    qDebug() << "TrackController::onClipPropertyChanged";
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    std::optional<Automation::TrackId> targetTrackId;
    if (newTrackIndex >= 0 && newTrackIndex < appModel->tracks().size())
        targetTrackId = Automation::TrackId(appModel->tracks().at(newTrackIndex)->id());
    const auto result = runtime->project().setClipProperties(
        commandContext(*runtime), clipPropertiesDto(args), targetTrackId);
    if (result && result.get().changed && targetTrackId && appStatus->activeClipId == args.id)
        editorViewController->refreshActiveClipTrackPresentation();
}

void TrackController::onRemoveClips(const QList<int> &clipsId) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    QList<Automation::ClipId> ids;
    ids.reserve(clipsId.size());
    for (const auto id : clipsId)
        ids.append(Automation::ClipId(id));
    const auto result = runtime->project().removeClips(commandContext(*runtime), ids);
    if (result && result.get().changed && clipsId.contains(appStatus->activeClipId.get()))
        setActiveClip(-1);
}

SingingClip *TrackController::onNewSingingClip(const int trackIndex, const int tick) {
    auto *runtime = automationRuntime();
    if (!runtime || trackIndex < 0 || trackIndex >= appModel->tracks().size())
        return nullptr;
    constexpr int bars = 4;
    const auto &timeline = appModel->timeline();
    const int startBar = timeline.tickToTime(qMax(0, tick)).measure;
    const int length = timeline.barToTick(startBar + bars) - timeline.barToTick(startBar);
    const auto track = appModel->tracks().at(trackIndex);
    Automation::ClipDraftDto draft;
    draft.type = Automation::ClipDraftDto::Type::Singing;
    draft.properties.name = tr("New Singing Clip");
    draft.properties.start = tick;
    draft.properties.length = length;
    draft.properties.clipLen = length;
    draft.defaultLanguage = track->defaultLanguage();
    const auto result = runtime->project().insertClips(
        commandContext(*runtime), {{Automation::TrackId(track->id()), std::move(draft)}});
    if (!result || !result.get().changed || result.get().affectedObjects.isEmpty())
        return nullptr;
    const auto clipId = result.get().affectedObjects.first().value;
    setActiveClip(clipId);
    return static_cast<SingingClip *>(appModel->findClipById(clipId));
}

void TrackController::copySelectedClips() {
    const auto clipIds = appStatus->selectedClips.get();
    if (clipIds.isEmpty())
        return;

    QList<Clip *> clips;
    QList<int> trackIndexOffsets;
    int baseTrackIndex = -1;
    for (const auto id : clipIds) {
        Track *track;
        const auto clip = appModel->findClipById(id, track);
        if (!clip)
            continue;
        const auto trackIndex = appModel->tracks().indexOf(track);
        if (baseTrackIndex < 0)
            baseTrackIndex = trackIndex;
        clips.append(clip);
        trackIndexOffsets.append(trackIndex - baseTrackIndex);
    }

    if (clips.isEmpty())
        return;

    ClipsInfo info;
    info.clips = clips;
    info.trackIndexOffsets = trackIndexOffsets;

    const auto json = ClipsInfo::serializeToJson(info);
    const auto array = QJsonDocument(json).toJson(QJsonDocument::Compact);
    const auto data = new QMimeData;
    data->setData(ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip), array);
    QGuiApplication::clipboard()->setMimeData(data);
}

void TrackController::cutSelectedClips() {
    copySelectedClips();
    onRemoveClips(appStatus->selectedClips.get());
}

void TrackController::pasteClips(const ClipsInfo &info, int tick, int trackIndex) {
    const auto &srcClips = info.clips;
    auto *runtime = automationRuntime();
    if (!runtime || srcClips.isEmpty())
        return;

    if (trackIndex < 0 || trackIndex >= appModel->tracks().count())
        return;

    int minStart = srcClips.first()->start();
    for (const auto clip : srcClips)
        minStart = qMin(minStart, clip->start());
    const auto offset = tick - minStart;

    QList<Automation::ClipInsertDto> inserts;

    for (int i = 0; i < srcClips.count(); i++) {
        const auto srcClip = srcClips.at(i);
        if (!srcClip)
            continue;
        int targetTrackIndex = trackIndex + info.trackIndexOffsets.value(i, 0);
        targetTrackIndex = qBound(0, targetTrackIndex, appModel->tracks().count() - 1);
        auto draft = Automation::clipDraftDto(*srcClip);
        draft.properties.start += offset;
        const auto target = appModel->tracks().at(targetTrackIndex);
        inserts.append({Automation::TrackId(target->id()), std::move(draft)});
    }

    if (inserts.isEmpty())
        return;
    runtime->project().insertClips(commandContext(*runtime), inserts);
}

void TrackController::handleDecodeAudioTaskFinished(DecodeAudioTask *task) {
    const auto terminate = task->terminated();
    taskManager->removeTask(task);
    if (terminate) {
        delete task;
        return;
    }
    if (!task->success) {
        const auto dlg = new Dialog(m_parentWidget);
        dlg->setWindowTitle(tr("Error"));
        dlg->setTitle(tr("Failed to open audio file:"));
        dlg->setMessage(task->path);
        dlg->setModal(true);

        const auto btnClose = new AccentButton(tr("Close"));
        connect(btnClose, &Button::clicked, dlg, &Dialog::accept);
        dlg->setPositiveButton(btnClose);
        dlg->show();

        delete task;
        return;
    }

    const auto tick = task->tick;
    const auto path = task->path;
    const auto trackId = task->trackId;
    const auto result = task->result();

    const auto sampleRate = result.sampleRate;
    const auto frames = result.frames;
    // Realtime truth comes from the file duration; ticks are derived at the
    // drop position under the current tempo map
    const auto &timeline = appModel->timeline();
    const double durationMs =
        sampleRate > 0 ? static_cast<double>(frames) * 1000.0 / sampleRate : 0.0;
    const double posMs = timeline.tickToMs(tick);
    const int length = qMax(1, qRound(timeline.msToTick(posMs + durationMs)) - tick);

    Automation::ClipDraftDto draft;
    draft.type = Automation::ClipDraftDto::Type::Audio;
    draft.properties.name = QFileInfo(path).baseName();
    draft.properties.start = tick;
    draft.properties.length = length;
    draft.properties.clipLen = length;
    draft.properties.trimStartMs = 0;
    draft.properties.playLengthMs = durationMs;
    draft.properties.materialLengthMs = durationMs;
    draft.audioPath = path;
    draft.audioInfo = result;
    draft.audioPathInfo = {DiffscopeAudioWorkspace::relativeDirFor(
                               path, documentWorkflowController->projectPath()),
                           {}};
    draft.hasRealTimeAnchor = true;
    draft.workspace.insert("diffscope.audio.formatData", task->workspace);
    auto *runtime = automationRuntime();
    if (runtime) {
        Automation::CommandContext context{.expected = task->documentVersion,
                                           .source = Automation::InvocationSource::TrustedGui};
        const auto commit = runtime->project().insertClips(
            context, {{Automation::TrackId(trackId), std::move(draft)}});
        if (commit && commit.get().changed && !commit.get().affectedObjects.isEmpty()) {
            const auto clipId = commit.get().affectedObjects.first().value;
            const auto *clip = appModel->findClipById(clipId);
            if (clip && clip->clipType() == IClip::Audio)
                scheduleHashUpdate(static_cast<const AudioClip *>(clip));
        }
    }
    delete task;
}

void TrackController::scheduleHashUpdate(const AudioClip *clip) {
    if (!clip || clip->path().isEmpty())
        return;
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    const auto hashTask = new ComputeAudioHashTask;
    hashTask->clipId = clip->id();
    hashTask->documentVersion = runtime->documentVersion();
    hashTask->path = clip->path();
    const auto automationTask = runtime->automationTasks().createTask(
        Automation::OperationIds::audio_clips::set_hash, hashTask->documentVersion,
        Automation::ObjectRef{Automation::ObjectKind::Clip, hashTask->clipId},
        [hashTask] { hashTask->terminate(); });
    hashTask->automationTaskId = automationTask.taskId;
    runtime->automationTasks().markRunning(automationTask.taskId);
    connect(hashTask, &Task::finished, trackController, [hashTask] {
        taskManager->removeTask(hashTask);
        auto *runtime = automationRuntime();
        if (!runtime) {
            delete hashTask;
            return;
        }
        if (!hashTask->success || hashTask->terminated()) {
            if (hashTask->terminated())
                runtime->automationTasks().cancel(hashTask->automationTaskId);
            else
                runtime->automationTasks().fail(
                    hashTask->automationTaskId,
                    Automation::AutomationError{
                        .code = Automation::AutomationErrorCode::IoError,
                        .message = QStringLiteral("Failed to compute audio hash"),
                    });
            delete hashTask;
            return;
        }
        Automation::CommandContext context{
            .expected = hashTask->documentVersion,
            .validateOnly = true,
            .source = Automation::InvocationSource::TrustedGui,
        };
        const auto validation = runtime->project().setAudioClipHash(
            context, Automation::ClipId(hashTask->clipId), hashTask->path,
            hashTask->resultSha512);
        if (!validation) {
            runtime->automationTasks().fail(hashTask->automationTaskId, validation.getError());
            delete hashTask;
            return;
        }
        const auto committing =
            runtime->automationTasks().beginCommitting(hashTask->automationTaskId);
        if (!committing || !committing.get()) {
            if (committing)
                runtime->automationTasks().cancel(hashTask->automationTaskId);
            delete hashTask;
            return;
        }
        context.validateOnly = false;
        const auto result = runtime->project().setAudioClipHash(
            context, Automation::ClipId(hashTask->clipId), hashTask->path,
            hashTask->resultSha512);
        if (result)
            runtime->automationTasks().succeed(hashTask->automationTaskId, result.get());
        else
            runtime->automationTasks().fail(hashTask->automationTaskId, result.getError());
        delete hashTask;
    });
    taskManager->addTask(hashTask);
    taskManager->startTask(hashTask);
}
