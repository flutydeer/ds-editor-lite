//
// Created by fluty on 2024/1/31.
//

#include "TracksViewController.h"

#include "AppController.h"
#include "Actions/AppModel/Clip/ClipActions.h"
#include "Controller/Actions/AppModel/Track/TrackActions.h"
#include "Model/AppModel.h"
#include "Model/AudioInfoModel.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/DecodeAudioTask.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Views/TrackEditor/GraphicsItem/AudioClipGraphicsItem.h"

void TracksViewController::setParentWidget(QWidget *view) {
    m_parentWidget = view;
}
void TracksViewController::onNewTrack() {
    onInsertNewTrack(appModel->tracks().count());
}
void TracksViewController::onInsertNewTrack(qsizetype index) {
    // bool soloExists = false;
    // auto tracks = appModel->tracks();
    // for (auto dsTrack : tracks) {
    //     auto curControl = dsTrack->control();
    //     if (curControl.solo()) {
    //         soloExists = true;
    //         break;
    //     }
    // }

    auto newTrack = new Track;
    newTrack->setName(tr("New Track"));
    // if (soloExists) {
    //     auto control = newTrack->control();
    //     control.setMute(true);
    //     newTrack->setControl(control);
    // }
    auto a = new TrackActions;
    a->insertTrack(newTrack, index, appModel);
    a->execute();
    historyManager->record(a);
}
void TracksViewController::onAppendTrack(Track *track) {
    auto a = new TrackActions;
    a->insertTrack(track, appModel->tracks().count(), appModel);
    a->execute();
    historyManager->record(a);
}
void TracksViewController::onRemoveTrack(int index) {
    auto trackToRemove = appModel->tracks().at(index);
    QList<Track *> tracks;
    tracks.append(trackToRemove);
    auto a = new TrackActions;
    a->removeTracks(tracks, appModel);
    a->execute();
    historyManager->record(a);
}
void TracksViewController::addAudioClipToNewTrack(const QString &filePath) {
    auto audioClip = new AudioClip;
    audioClip->setPath(filePath);
    auto newTrack = new Track;
    newTrack->insertClip(audioClip);
    auto a = new TrackActions;
    auto index = appModel->tracks().size();
    a->insertTrack(newTrack, index, appModel);
    a->execute();
    historyManager->record(a);
}
void TracksViewController::onSelectedClipChanged(int clipId) {
    appModel->selectClip(clipId);
}
void TracksViewController::onTrackPropertyChanged(const Track::TrackProperties &args) {
    auto tracks = appModel->tracks();
    auto track = tracks.at(args.index);
    auto control = track->control();
    auto a = new TrackActions;
    Track::TrackProperties oldArgs;
    oldArgs.index = args.index;
    oldArgs.name = track->name();
    oldArgs.gain = control.gain();
    oldArgs.pan = control.pan();
    oldArgs.mute = control.mute();
    oldArgs.solo = control.solo();
    a->editTrackProperties(oldArgs, args, track);
    a->execute();
    historyManager->record(a);
}
void TracksViewController::onAddAudioClip(const QString &path, int trackIndex, int tick) {
    auto decodeTask = new DecodeAudioTask;
    decodeTask->path = path;
    decodeTask->trackId = appModel->tracks().at(trackIndex)->id();
    decodeTask->tick = tick;
    auto dlg = new TaskDialog(decodeTask, true, true, m_parentWidget);
    dlg->show();
    connect(decodeTask, &Task::finished, this,
            [=](bool terminate) { handleDecodeAudioTaskFinished(decodeTask, terminate); });
    taskManager->addTask(decodeTask);
    taskManager->startTask(decodeTask);
}
void TracksViewController::onClipPropertyChanged(const Clip::ClipCommonProperties &args) {
    qDebug() << "TracksViewController::onClipPropertyChanged";
    int trackIndex = -1;
    auto clip = appModel->findClipById(args.id, trackIndex);
    auto track = appModel->tracks().at(trackIndex);

    if (clip->type() == Clip::Audio) {
        auto audioClip = dynamic_cast<AudioClip *>(clip);

        auto oldArgs = Clip::ClipCommonProperties::fromClip(audioClip);
        QList<Clip::ClipCommonProperties> oldArgsList;
        oldArgsList.append(oldArgs);
        QList<Clip::ClipCommonProperties> newArgsList;
        newArgsList.append(args);
        QList<AudioClip *> clips;
        clips.append(audioClip);
        QList<Track *> tracks;
        tracks.append(track);

        auto a = new ClipActions;
        a->editAudioClipProperties(oldArgsList, newArgsList, clips, tracks);
        a->execute();
        historyManager->record(a);
    } else if (clip->type() == Clip::Singing) {
        auto singingClip = dynamic_cast<SingingClip *>(clip);

        auto oldArgs = Clip::ClipCommonProperties::fromClip(singingClip);
        // TODO: update singer info?
        QList<Clip::ClipCommonProperties> oldArgsList;
        oldArgsList.append(oldArgs);
        QList<Clip::ClipCommonProperties> newArgsList;
        newArgsList.append(args);
        QList<SingingClip *> clips;
        clips.append(singingClip);
        QList<Track *> tracks;
        tracks.append(track);

        auto a = new ClipActions;
        a->editSingingClipProperties(oldArgsList, newArgsList, clips, tracks);
        a->execute();
        historyManager->record(a);
    }
}
void TracksViewController::onRemoveClip(int clipId) {
    int trackIndex;
    auto result = appModel->findClipById(clipId, trackIndex);
    if (!result)
        return;

    auto a = new ClipActions;
    QList<Clip *> clips;
    clips.append(result);
    a->removeClips(clips, appModel->tracks().at(trackIndex));
    a->execute();
    historyManager->record(a);
}
void TracksViewController::onNewSingingClip(int trackIndex, int tick) {
    auto singingClip = new SingingClip;
    singingClip->setStart(tick);
    singingClip->setClipStart(0);
    singingClip->setLength(1920);
    singingClip->setClipLen(1920);
    auto track = appModel->tracks().at(trackIndex);
    auto a = new ClipActions;
    QList<Clip *> clips;
    clips.append(singingClip);
    a->insertClips(clips, track);
    a->execute();
    historyManager->record(a);
}
void TracksViewController::handleDecodeAudioTaskFinished(DecodeAudioTask *task, bool terminate) {
    taskManager->removeTask(task);
    if (terminate)
        return;
    if (!task->success) {
        // auto clipItem = m_view.findClipItemById(task->id());
        // auto audioClipItem = dynamic_cast<AudioClipGraphicsItem *>(clipItem);
        // audioClipItem->setStatus(AppGlobal::Error);
        auto dlg = new Dialog(m_parentWidget);
        dlg->setWindowTitle(tr("Error"));
        dlg->setTitle(tr("Failed to open audio file:"));
        dlg->setMessage(task->path);
        dlg->setModal(true);

        auto btnClose = new AccentButton(tr("Close"));
        connect(btnClose, &Button::clicked, dlg, &Dialog::accept);
        dlg->setPositiveButton(btnClose);
        dlg->show();
        return;
    }

    auto tick = task->tick;
    auto path = task->path;
    auto trackId = task->trackId;
    auto result = task->result();

    auto sampleRate = result.sampleRate;
    auto tempo = appModel->tempo();
    auto frames = result.frames;
    auto length = frames / (sampleRate * 60 / tempo / 480);

    auto audioClip = new AudioClip;
    audioClip->setStart(tick);
    audioClip->setClipStart(0);
    audioClip->setLength(length);
    audioClip->setClipLen(length);
    audioClip->setPath(path);
    audioClip->setAudioInfo(result);
    int trackIndex = 0;
    auto track = appModel->findTrackById(trackId, trackIndex);
    if (!track) {
        qDebug() << "TracksViewController::handleDecodeAudioTaskFinished track not found";
        return;
    }
    auto a = new ClipActions;
    QList<Clip *> clips;
    clips.append(audioClip);
    a->insertClips(clips, track);
    a->execute();
    historyManager->record(a);
}