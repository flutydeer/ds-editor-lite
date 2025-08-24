//
// Created by fluty on 2024/1/31.
//

#include "TrackController.h"

#include "Actions/AppModel/Clip/ClipActions.h"
#include "Controller/Actions/AppModel/Track/TrackActions.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/AudioInfoModel.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/DecodeAudioTask.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Views/TrackEditor/GraphicsItem/AudioClipView.h"
#include "Utils/G2pUtil.h"

#include <QFileInfo>

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
    // TODO: Temp Use
    newTrack->setDefaultG2pId(defaultG2pId());
    // if (soloExists) {
    //     auto control = newTrack->control();
    //     control.setMute(true);
    //     newTrack->setControl(control);
    // }

    // TODO: set default singer and speaker here
    //newTrack->setSingerIdentifier({
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

void TrackController::onClipPropertyChanged(const Clip::ClipCommonProperties &args) {
    qDebug() << "TrackController::onClipPropertyChanged";
    int trackIndex = -1;
    const auto clip = appModel->findClipById(args.id, trackIndex);
    const auto track = appModel->tracks().at(trackIndex);

    if (clip->clipType() == Clip::Audio) {
        const auto audioClip = dynamic_cast<AudioClip *>(clip);

        const Clip::ClipCommonProperties oldArgs(*audioClip);
        QList<Clip::ClipCommonProperties> oldArgsList;
        oldArgsList.append(oldArgs);
        QList<Clip::ClipCommonProperties> newArgsList;
        newArgsList.append(args);
        QList<AudioClip *> clips;
        clips.append(audioClip);
        QList<Track *> tracks;
        tracks.append(track);

        const auto a = new ClipActions;
        a->editAudioClipProperties(oldArgsList, newArgsList, clips, tracks);
        a->execute();
        historyManager->record(a);
    } else if (clip->clipType() == Clip::Singing) {
        const auto singingClip = dynamic_cast<SingingClip *>(clip);

        Clip::ClipCommonProperties oldArgs(*singingClip);
        // TODO: update singer info?
        QList<Clip::ClipCommonProperties> oldArgsList;
        oldArgsList.append(oldArgs);
        QList<Clip::ClipCommonProperties> newArgsList;
        newArgsList.append(args);
        QList<SingingClip *> clips;
        clips.append(singingClip);
        QList<Track *> tracks;
        tracks.append(track);

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
    const int length = 1920 * timeSig.numerator / timeSig.denominator * bars;
    singingClip->setName(tr("New Singing Clip"));
    singingClip->setStart(tick);
    singingClip->setClipStart(0);
    singingClip->setLength(length);
    singingClip->setClipLen(length);

    const auto track = appModel->tracks().at(trackIndex);
    singingClip->defaultLanguage = track->defaultLanguage();
    // TODO: Temp Use
    singingClip->defaultG2pId = g2pIdFromLanguage(singingClip->defaultLanguage);
    singingClip->trackSingerInfo = track->singerInfo();
    singingClip->trackSpeakerInfo = track->speakerInfo();
    const auto a = new ClipActions;
    QList<Clip *> clips;
    clips.append(singingClip);
    a->insertClips(clips, track);
    a->execute();
    historyManager->record(a);

    setActiveClip(singingClip->id());
    return singingClip;
}

void TrackController::handleDecodeAudioTaskFinished(DecodeAudioTask *task) {
    const auto terminate = task->terminated();
    taskManager->removeTask(task);
    if (terminate) {
        delete task;
        return;
    }
    if (!task->success) {
        // auto clipItem = m_view.findClipItemById(task->id());
        // auto audioClipItem = dynamic_cast<AudioClipGraphicsItem *>(clipItem);
        // audioClipItem->setStatus(AppGlobal::Error);
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
    const auto length = frames / (sampleRate * 60 / tempo / 480);

    const auto audioClip = new AudioClip;
    audioClip->setName(QFileInfo(path).baseName());
    audioClip->setStart(tick);
    audioClip->setClipStart(0);
    audioClip->setLength(length);
    audioClip->setClipLen(length);
    audioClip->setPath(path);
    audioClip->setAudioInfo(result);
    audioClip->workspace().insert("diffscope.audio.formatData", task->workspace);
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
    delete task;
}