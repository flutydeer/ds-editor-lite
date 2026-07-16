//
// Created by fluty on 2024/1/31.
//

#include "TrackController.h"

#include "Actions/AppModel/Clip/ClipActions.h"
#include "ClipController.h"
#include "Controller/Actions/AppModel/Track/TrackActions.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/AudioInfoModel.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/ComputeAudioHashTask.h"
#include "Tasks/DecodeAudioTask.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Views/TrackEditor/GraphicsItem/AudioClipView.h"
#include "Global/AppGlobal.h"
#include "Global/ControllerGlobal.h"
#include "Utils/DiffscopeAudioWorkspace.h"
#include "Utils/TimelineSnapUtils.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QMimeData>

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
    // bool soloExists = false;
    // auto tracks = appModel->tracks();
    // for (auto dsTrack : tracks) {
    //     auto curControl = dsTrack->control();
    //     if (curControl.solo()) {
    //         soloExists = true;
    //         break;
    //     }
    // }

    const auto newTrack = new Track;
    newTrack->setName(tr("New Track"));
    newTrack->setDefaultLanguage(appOptions->general()->defaultSingingLanguage);
    // if (soloExists) {
    //     auto control = newTrack->control();
    //     control.setMute(true);
    //     newTrack->setControl(control);
    // }

    // TODO: set default singer and speaker here
    // newTrack->setSingerIdentifier({
    //    .singerId = appOptions->general()->defaultSingerId,
    //    .packageId = appOptions->general()->defaultPackageId,
    //    .packageVersion = appOptions->general()->defaultPackageVersion,
    //});
    const auto a = new TrackActions;
    a->insertTrack(newTrack, index, appModel);
    a->execute();
    historyManager->record(a);
}

void TrackController::onAppendTrack(Track *track) {
    const auto a = new TrackActions;
    a->insertTrack(track, appModel->tracks().count(), appModel);
    a->execute();
    historyManager->record(a);
}

void TrackController::onRemoveTrack(const int id) {
    const auto trackToRemove = appModel->findTrackById(id);
    QList<Track *> tracks;
    tracks.append(trackToRemove);
    const auto a = new TrackActions;
    a->removeTracks(tracks, appModel);
    a->execute();
    historyManager->record(a);
}

void TrackController::onMoveTrack(const qsizetype fromIndex, const qsizetype toIndex) {
    if (fromIndex == toIndex)
        return;

    const auto a = new TrackActions;
    a->moveTrack(fromIndex, toIndex, appModel);
    a->execute();
    historyManager->record(a);
}

void TrackController::addAudioClipToNewTrack(const QString &filePath) {
    const auto audioClip = new AudioClip;
    audioClip->setPath(filePath);
    const auto newTrack = new Track;
    newTrack->insertClip(audioClip);
    const auto a = new TrackActions;
    const auto index = appModel->tracks().size();
    a->insertTrack(newTrack, index, appModel);
    a->execute();
    historyManager->record(a);
}

void TrackController::setActiveClip(const int clipId) {
    if (clipId != appStatus->activeClipId) {
        appStatus->selectedNotes = QList<int>();
        appStatus->activeClipId = clipId;
    }
}

void TrackController::changeTrackProperty(const Track::TrackProperties &args) {
    qDebug() << "TrackController::changeTrackProperty" << args.gain << args.pan;
    const auto track = appModel->findTrackById(args.id);
    const auto a = new TrackActions;
    const Track::TrackProperties oldArgs(*track);
    a->editTrackProperties(oldArgs, args, track);
    a->execute();
    historyManager->record(a);
}

void TrackController::onAddAudioClip(const QString &path, talcs::AbstractAudioFormatIO *io,
                                     const QJsonObject &workspace, const int id, const int tick) {
    auto decodeTask = new DecodeAudioTask;
    decodeTask->io = io;
    decodeTask->path = path;
    decodeTask->trackId = id;
    decodeTask->tick = tick;
    decodeTask->workspace = workspace;
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
    int trackIndex = -1;
    const auto clip = appModel->findClipById(clipId, trackIndex);
    if (!clip || clip->clipType() != IClip::Audio) {
        delete io;
        return;
    }
    // io was only used by the file dialog for format probing; path and format info reach the model via the action,
    // re-decoding is done by AudioDecodingController in response to pathChanged
    delete io;

    const auto audioClip = static_cast<AudioClip *>(clip);
    const AudioPathInfo newInfo{DiffscopeAudioWorkspace::relativeDirFor(
                                    path, documentWorkflowController->projectPath()),
                                {}};
    const auto a = new ClipActions;
    a->relocateAudioClip(audioClip, path, newInfo, workspace);
    a->execute();
    historyManager->record(a);
    scheduleHashUpdate(audioClip);
}

void TrackController::onClipPropertyChanged(const Clip::ClipCommonProperties &args) {
    onClipPropertyChanged(args, -1);
}

void TrackController::onClipPropertyChanged(const Clip::ClipCommonProperties &args,
                                            const int newTrackIndex) {
    qDebug() << "TrackController::onClipPropertyChanged";
    int currentTrackIndex = -1;
    const auto clip = appModel->findClipById(args.id, currentTrackIndex);
    const auto oldTrack = appModel->tracks().at(currentTrackIndex);

    const Clip::ClipCommonProperties oldArgs(*clip);

    if (newTrackIndex >= 0 && newTrackIndex != currentTrackIndex) {
        const auto newTrack = appModel->tracks().at(newTrackIndex);
        const auto a = new ClipActions;
        a->moveClipToTrack(oldArgs, args, clip, oldTrack, newTrack);
        a->execute();
        historyManager->record(a);
        if (appStatus->activeClipId == args.id)
            clipController->notifyActiveClipTrackChanged();
        return;
    }

    if (clip->clipType() == Clip::Audio) {
        const auto audioClip = dynamic_cast<AudioClip *>(clip);

        QList<Clip::ClipCommonProperties> oldArgsList;
        oldArgsList.append(oldArgs);
        QList<Clip::ClipCommonProperties> newArgsList;
        newArgsList.append(args);
        QList<AudioClip *> clips;
        clips.append(audioClip);
        QList<Track *> tracks;
        tracks.append(oldTrack);

        const auto a = new ClipActions;
        a->editAudioClipProperties(oldArgsList, newArgsList, clips, tracks);
        a->execute();
        historyManager->record(a);
    } else if (clip->clipType() == Clip::Singing) {
        const auto singingClip = dynamic_cast<SingingClip *>(clip);

        QList<Clip::ClipCommonProperties> oldArgsList;
        oldArgsList.append(oldArgs);
        QList<Clip::ClipCommonProperties> newArgsList;
        newArgsList.append(args);
        QList<SingingClip *> clips;
        clips.append(singingClip);
        QList<Track *> tracks;
        tracks.append(oldTrack);

        const auto a = new ClipActions;
        a->editSingingClipProperties(oldArgsList, newArgsList, clips, tracks);
        a->execute();
        historyManager->record(a);
    }
}

void TrackController::onRemoveClips(const QList<int> &clipsId) {
    if (clipsId.empty())
        return;

    const auto a = new ClipActions;
    QList<Clip *> clips;
    QList<Track *> tracks;
    for (const auto &id : clipsId) {
        auto activeClipId = appStatus->activeClipId;
        if (id == activeClipId)
            setActiveClip(-1);

        Track *track;
        const auto clip = appModel->findClipById(id, track);
        clips.append(clip);
        tracks.append(track);
    }
    a->removeClips(clips, tracks);
    a->execute();
    historyManager->record(a);
}

SingingClip *TrackController::onNewSingingClip(const int trackIndex, const int tick) {
    const auto singingClip = new SingingClip;
    constexpr int bars = 4;
    const auto timeSig = appModel->timeSignature();
    const int length =
        AppGlobal::ticksPerWholeNote * timeSig.numerator / timeSig.denominator * bars;
    singingClip->setName(tr("New Singing Clip"));
    singingClip->setStart(tick);
    singingClip->setClipStart(0);
    singingClip->setLength(length);
    singingClip->setClipLen(length);

    const auto track = appModel->tracks().at(trackIndex);
    singingClip->setDefaultLanguage(track->defaultLanguage());
    singingClip->setTrackVoiceContext(track->singerInfo(), track->speakerInfo(),
                                      track->speakerMixData());
    const auto a = new ClipActions;
    QList<Clip *> clips;
    clips.append(singingClip);
    a->insertClips(clips, track);
    a->execute();
    historyManager->record(a);

    setActiveClip(singingClip->id());
    return singingClip;
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
    if (srcClips.isEmpty())
        return;

    if (trackIndex < 0 || trackIndex >= appModel->tracks().count())
        return;

    const auto quantize = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
    const auto snappedTick = TimelineSnapUtils::snapNearest(tick, quantize);

    int minStart = srcClips.first()->start();
    for (const auto clip : srcClips)
        minStart = qMin(minStart, clip->start());
    const auto offset = snappedTick - minStart;

    QList<Clip *> newClips;
    QList<Track *> targetTracks;

    for (int i = 0; i < srcClips.count(); i++) {
        const auto srcClip = srcClips.at(i);
        int targetTrackIndex = trackIndex + info.trackIndexOffsets.value(i, 0);
        targetTrackIndex = qBound(0, targetTrackIndex, appModel->tracks().count() - 1);

        Clip *newClip = nullptr;
        if (srcClip->clipType() == IClip::Singing) {
            const auto srcSinging = static_cast<SingingClip *>(srcClip);
            auto singingClip = new SingingClip;
            singingClip->setDefaultLanguage(srcSinging->defaultLanguage());

            const auto srcNotes = srcSinging->notes().toList();
            for (const auto srcNote : srcNotes) {
                auto note = new Note;
                note->setLocalStart(srcNote->localStart());
                note->setLength(srcNote->length());
                note->setKeyIndex(srcNote->keyIndex());
                note->setCentShift(srcNote->centShift());
                note->setLyric(srcNote->lyric());
                note->setLanguage(srcNote->language());
                note->setPronunciation(srcNote->pronunciation());
                note->setPronCandidates(srcNote->pronCandidates());
                note->setLineFeed(srcNote->lineFeed());
                Phonemes ph;
                ph.nameSeq.edited = srcNote->phonemeNameSeq().edited;
                note->setPhonemes(ph);
                singingClip->insertNote(note);
            }
            const auto targetTrack = appModel->tracks().at(targetTrackIndex);
            singingClip->setTrackVoiceContext(targetTrack->singerInfo(),
                                              targetTrack->speakerInfo(),
                                              targetTrack->speakerMixData());
            if (!srcSinging->useTrackSingerInfo.get()) {
                singingClip->setOwnSingerAndSpeaker(srcSinging->ownSingerInfo(),
                                                    srcSinging->ownSpeakerInfo());
                singingClip->setOwnSpeakerMixData(srcSinging->ownSpeakerMixData());
            }
            newClip = singingClip;

        } else if (srcClip->clipType() == IClip::Audio) {
            const auto srcAudio = static_cast<AudioClip *>(srcClip);
            auto audioClip = new AudioClip;
            audioClip->setPath(srcAudio->path());
            audioClip->setPathInfo(srcAudio->pathInfo());
            newClip = audioClip;
        }

        if (newClip) {
            newClip->setName(srcClip->name());
            newClip->setStart(srcClip->start() + offset);
            newClip->setLength(srcClip->length());
            newClip->setClipStart(srcClip->clipStart());
            newClip->setClipLen(srcClip->clipLen());
            newClip->setGain(srcClip->gain());
            newClip->setMute(srcClip->mute());
            newClip->workspace() = srcClip->workspace();
            newClips.append(newClip);
            targetTracks.append(appModel->tracks().at(targetTrackIndex));
        }
    }

    if (newClips.isEmpty())
        return;

    const auto a = new ClipActions;
    a->insertClips(newClips, targetTracks);
    a->execute();
    historyManager->record(a);
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
    const auto tempo = appModel->tempo();
    const auto frames = result.frames;
    const auto length = frames / (sampleRate * 60 / tempo / AppGlobal::ticksPerQuarterNote);

    const auto audioClip = new AudioClip;
    audioClip->setName(QFileInfo(path).baseName());
    audioClip->setStart(tick);
    audioClip->setClipStart(0);
    audioClip->setLength(length);
    audioClip->setClipLen(length);
    audioClip->setPath(path);
    audioClip->setAudioInfo(result);
    audioClip->workspace().insert("diffscope.audio.formatData", task->workspace);
    audioClip->setPathInfo({DiffscopeAudioWorkspace::relativeDirFor(
                                path, documentWorkflowController->projectPath()),
                            {}});
    const auto track = appModel->findTrackById(trackId);
    if (!track) {
        qDebug() << "handleDecodeAudioTaskFinished: track not found";
        delete task;
        return;
    }
    const auto a = new ClipActions;
    QList<Clip *> clips;
    clips.append(audioClip);
    a->insertClips(clips, track);
    a->execute();
    historyManager->record(a);
    scheduleHashUpdate(audioClip);
    delete task;
}

void TrackController::scheduleHashUpdate(const AudioClip *clip) {
    if (!clip || clip->path().isEmpty())
        return;
    const auto hashTask = new ComputeAudioHashTask;
    hashTask->clipId = clip->id();
    hashTask->path = clip->path();
    connect(hashTask, &Task::finished, trackController, [hashTask] {
        taskManager->removeTask(hashTask);
        if (hashTask->success && !hashTask->terminated()) {
            // The clip may have been removed or relinked; verify the clipId + path snapshot before writing back
            int trackIndex = -1;
            const auto clip = appModel->findClipById(hashTask->clipId, trackIndex);
            if (clip && clip->clipType() == IClip::Audio) {
                const auto audioClip = static_cast<AudioClip *>(clip);
                if (audioClip->path() == hashTask->path) {
                    auto info = audioClip->pathInfo();
                    info.sha512 = hashTask->resultSha512;
                    audioClip->setPathInfo(info);
                }
            }
        }
        delete hashTask;
    });
    taskManager->addTask(hashTask);
    taskManager->startTask(hashTask);
}
