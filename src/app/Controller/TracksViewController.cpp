//
// Created by fluty on 2024/1/31.
//

#include "TracksViewController.h"

#include <QMessageBox>

#include "Actions/AppModel/Clip/ClipActions.h"
#include "Model/AppModel.h"
#include "Controller/Actions/AppModel/Track/TrackActions.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/DecodeAudioTask.h"
#include "Model/AudioInfoModel.h"
#include "UI/Views/TracksEditor/GraphicsItem/AudioClipGraphicsItem.h"

void TracksViewController::onNewTrack() {
    onInsertNewTrack(AppModel::instance()->tracks().count());
}
void TracksViewController::onInsertNewTrack(qsizetype index) {
    // bool soloExists = false;
    // auto tracks = AppModel::instance()->tracks();
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
    a->insertTrack(newTrack, index, AppModel::instance());
    a->execute();
    HistoryManager::instance()->record(a);
}
void TracksViewController::onAppendTrack(Track *track) {
    auto a = new TrackActions;
    a->insertTrack(track, AppModel::instance()->tracks().count(), AppModel::instance());
    a->execute();
    HistoryManager::instance()->record(a);
}
void TracksViewController::onRemoveTrack(int index) {
    QMessageBox msgBox;
    msgBox.setText(tr("Warning"));
    msgBox.setInformativeText(tr("Do you want to remove this track?"));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Yes) {
        auto trackToRemove = AppModel::instance()->tracks().at(index);
        QList<Track *> tracks;
        tracks.append(trackToRemove);
        auto a = new TrackActions;
        a->removeTracks(tracks, AppModel::instance());
        a->execute();
        HistoryManager::instance()->record(a);
    }
}
void TracksViewController::addAudioClipToNewTrack(const QString &filePath) {
    auto audioClip = new AudioClip;
    audioClip->setPath(filePath);
    auto newTrack = new Track;
    newTrack->insertClip(audioClip);
    auto a = new TrackActions;
    auto index = AppModel::instance()->tracks().size();
    a->insertTrack(newTrack, index, AppModel::instance());
    a->execute();
    HistoryManager::instance()->record(a);
}
void TracksViewController::onSelectedClipChanged(int clipId) {
    AppModel::instance()->onSelectedClipChanged(clipId);
}
void TracksViewController::onTrackPropertyChanged(const Track::TrackProperties &args) {
    auto tracks = AppModel::instance()->tracks();
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
    HistoryManager::instance()->record(a);
}
void TracksViewController::onAddAudioClip(const QString &path, int trackIndex, int tick) {
    auto decodeTask = new DecodeAudioTask;
    decodeTask->path = path;
    decodeTask->trackId = AppModel::instance()->tracks().at(trackIndex)->id();
    decodeTask->tick = tick;
    connect(decodeTask, &ITask::finished, this,
            [=](bool terminate) { handleDecodeAudioTaskFinished(decodeTask, terminate); });
    TaskManager::instance()->addTask(decodeTask);
    TaskManager::instance()->startTask(decodeTask);
}
void TracksViewController::onClipPropertyChanged(const Clip::ClipCommonProperties &args) {
    qDebug() << "TracksViewController::onClipPropertyChanged";
    auto track = AppModel::instance()->tracks().at(args.trackIndex);
    auto clip = track->findClipById(args.id);

    // qDebug() << "args.id" << args.id;
    // qDebug() << "args.name" << args.name;
    // qDebug() << "args.start" << args.start;
    // qDebug() << "args.clipStart" << args.clipStart;
    // qDebug() << "args.length" << args.length;
    // qDebug() << "args.clipLen" << args.clipLen;

    if (clip->type() == Clip::Audio) {
        auto audioClip = dynamic_cast<AudioClip *>(clip);
        // qDebug() << "clip path" << audioClip->path();
        // auto audioArgs = dynamic_cast<const DsClip::AudioClipPropertyChangedArgs *>(&args);
        // qDebug() << "args path" << audioArgs->path;
        // audioClip->setPath(audioArgs->path);

        Clip::ClipCommonProperties oldArgs;
        oldArgs.name = audioClip->name();
        oldArgs.id = audioClip->id();
        oldArgs.start = audioClip->start();
        oldArgs.clipStart = audioClip->clipStart();
        oldArgs.length = audioClip->length();
        oldArgs.clipLen = audioClip->clipLen();
        oldArgs.gain = audioClip->gain();
        oldArgs.mute = audioClip->mute();
        oldArgs.trackIndex = args.trackIndex;

        QList<Clip::ClipCommonProperties> oldArgsList;
        oldArgsList.append(oldArgs);
        QList<Clip::ClipCommonProperties> newArgsList;
        newArgsList.append(args);
        QList<AudioClip *> clips;
        clips.append(audioClip);

        auto a = new ClipActions;
        a->editAudioClipProperties(oldArgsList, newArgsList, clips, track);
        a->execute();
        HistoryManager::instance()->record(a);
    } else if (clip->type() == Clip::Singing) {
        auto singingClip = dynamic_cast<SingingClip *>(clip);

        Clip::ClipCommonProperties oldArgs;
        oldArgs.name = singingClip->name();
        oldArgs.id = singingClip->id();
        oldArgs.start = singingClip->start();
        oldArgs.clipStart = singingClip->clipStart();
        oldArgs.length = singingClip->length();
        oldArgs.clipLen = singingClip->clipLen();
        oldArgs.gain = singingClip->gain();
        oldArgs.mute = singingClip->mute();
        oldArgs.trackIndex = args.trackIndex;

        QList<Clip::ClipCommonProperties> oldArgsList;
        oldArgsList.append(oldArgs);
        QList<Clip::ClipCommonProperties> newArgsList;
        newArgsList.append(args);
        QList<SingingClip *> clips;
        clips.append(singingClip);

        auto a = new ClipActions;
        a->editSingingClipProperties(oldArgsList, newArgsList, clips, track);
        a->execute();
        HistoryManager::instance()->record(a);
    }
}
void TracksViewController::onRemoveClip(int clipId) {
    QMessageBox msgBox;
    msgBox.setText(tr("Warning"));
    msgBox.setInformativeText(tr("Do you want to remove this clip?"));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Yes) {
        for (const auto &track : AppModel::instance()->tracks()) {
            auto result = track->findClipById(clipId);
            if (result != nullptr) {
                auto a = new ClipActions;
                QList<Clip *> clips;
                clips.append(result);
                a->removeClips(clips, track);
                a->execute();
                HistoryManager::instance()->record(a);
            }
        }
    }
}
void TracksViewController::onNewSingingClip(int trackIndex, int tick) {
    auto singingClip = new SingingClip;
    singingClip->setStart(tick);
    singingClip->setClipStart(0);
    singingClip->setLength(1920);
    singingClip->setClipLen(1920);
    auto track = AppModel::instance()->tracks().at(trackIndex);
    auto a = new ClipActions;
    QList<Clip *> clips;
    clips.append(singingClip);
    a->insertClips(clips, track);
    a->execute();
    HistoryManager::instance()->record(a);
}
void TracksViewController::handleDecodeAudioTaskFinished(DecodeAudioTask *task, bool terminate) {
    TaskManager::instance()->removeTask(task);
    if (terminate)
        return;
    if (!task->success) {
        // auto clipItem = m_view.findClipItemById(task->id());
        // auto audioClipItem = dynamic_cast<AudioClipGraphicsItem *>(clipItem);
        // audioClipItem->setStatus(AppGlobal::Error);
        QMessageBox msgBox;
        msgBox.setText(tr("Error"));
        msgBox.setInformativeText(tr("Open file error:") + task->errorMessage);
        msgBox.setStandardButtons(QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.exec();
        return;
    }

    auto tick = task->tick;
    auto path = task->path;
    auto trackId = task->trackId;
    auto result = task->result();

    auto sampleRate = result.sampleRate;
    auto tempo = AppModel::instance()->tempo();
    auto frames = result.frames;
    auto length = frames / (sampleRate * 60 / tempo / 480);

    auto audioClip = new AudioClip;
    audioClip->setStart(tick);
    audioClip->setClipStart(0);
    audioClip->setLength(length);
    audioClip->setClipLen(length);
    audioClip->setPath(path);
    audioClip->info = result;
    int trackIndex = 0;
    auto track = AppModel::instance()->findTrackById(trackId, trackIndex);
    if(!track) {
        qDebug() << "TracksViewController::handleDecodeAudioTaskFinished track not found";
        return;
    }
    auto a = new ClipActions;
    QList<Clip *> clips;
    clips.append(audioClip);
    a->insertClips(clips, track);
    a->execute();
    HistoryManager::instance()->record(a);
}