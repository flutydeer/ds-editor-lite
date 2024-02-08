//
// Created by fluty on 2024/1/31.
//

#include <QMessageBox>

#include "TracksViewController.h"

#include "Actions/AppModel/Clip/ClipActions.h"
#include "Model/AppModel.h"
#include "Utils/IdGenerator.h"
#include "Controller/Actions/AppModel/Track/TrackActions.h"
#include "History/HistoryManager.h"

void TracksViewController::onNewTrack() {
    onInsertNewTrack(AppModel::instance()->tracks().count());
}
void TracksViewController::onInsertNewTrack(int index) {
    // bool soloExists = false;
    // auto tracks = AppModel::instance()->tracks();
    // for (auto dsTrack : tracks) {
    //     auto curControl = dsTrack->control();
    //     if (curControl.solo()) {
    //         soloExists = true;
    //         break;
    //     }
    // }

    auto newTrack = new DsTrack;
    newTrack->setName("New Track");
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
void TracksViewController::onRemoveTrack(int index) {
    QMessageBox msgBox;
    msgBox.setText("Warning");
    msgBox.setInformativeText("Do you want to remove this track?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Yes) {
        auto trackToRemove = AppModel::instance()->tracks().at(index);
        QList<DsTrack *> tracks;
        tracks.append(trackToRemove);
        auto a = new TrackActions;
        a->removeTracks(tracks, AppModel::instance());
        a->execute();
        HistoryManager::instance()->record(a);
    }
}
void TracksViewController::addAudioClipToNewTrack(const QString &filePath) {
    auto audioClip = new DsAudioClip;
    audioClip->setPath(filePath);
    auto newTrack = new DsTrack;
    newTrack->insertClip(audioClip);
    auto a = new TrackActions;
    auto index = AppModel::instance()->tracks().size();
    a->insertTrack(newTrack, index, AppModel::instance());
    a->execute();
    HistoryManager::instance()->record(a);
}
void TracksViewController::onSelectedClipChanged(int trackIndex, int clipId) {
    AppModel::instance()->onSelectedClipChanged(trackIndex, clipId);
}
void TracksViewController::onTrackPropertyChanged(const DsTrack::TrackProperties &args) {
    auto tracks = AppModel::instance()->tracks();
    auto track = tracks.at(args.index);
    auto control = track->control();
    auto a = new TrackActions;
    DsTrack::TrackProperties oldArgs;
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
    auto audioClip = new DsAudioClip;
    audioClip->setStart(tick);
    audioClip->setClipStart(0);
    audioClip->setPath(path);
    auto track = AppModel::instance()->tracks().at(trackIndex);
    track->insertClip(audioClip);
}
void TracksViewController::onClipPropertyChanged(const DsClip::ClipCommonProperties &args) {
    qDebug() << "TracksViewController::onClipPropertyChanged";
    auto track = AppModel::instance()->tracks().at(args.trackIndex);
    auto clip = track->findClipById(args.id);

    // qDebug() << "args.id" << args.id;
    // qDebug() << "args.name" << args.name;
    // qDebug() << "args.start" << args.start;
    // qDebug() << "args.clipStart" << args.clipStart;
    // qDebug() << "args.length" << args.length;
    // qDebug() << "args.clipLen" << args.clipLen;

    if (clip->type() == DsClip::Audio) {
        auto audioClip = dynamic_cast<DsAudioClip *>(clip);
        // qDebug() << "clip path" << audioClip->path();
        // auto audioArgs = dynamic_cast<const DsClip::AudioClipPropertyChangedArgs *>(&args);
        // qDebug() << "args path" << audioArgs->path;
        // audioClip->setPath(audioArgs->path);

        DsClip::ClipCommonProperties oldArgs;
        oldArgs.name = audioClip->name();
        oldArgs.id = audioClip->id();
        oldArgs.start = audioClip->start();
        oldArgs.clipStart = audioClip->clipStart();
        oldArgs.length = audioClip->length();
        oldArgs.clipLen = audioClip->clipLen();
        oldArgs.gain = audioClip->gain();
        oldArgs.mute = audioClip->mute();
        oldArgs.trackIndex = args.trackIndex;

        QList<DsClip::ClipCommonProperties> oldArgsList;
        oldArgsList.append(oldArgs);
        QList<DsClip::ClipCommonProperties> newArgsList;
        newArgsList.append(args);
        QList<DsAudioClip *> clips;
        clips.append(audioClip);

        auto a = new ClipActions;
        a->editAudioClipProperties(oldArgsList, newArgsList, clips, track);
        a->execute();
        HistoryManager::instance()->record(a);
    } else if (clip->type() == DsClip::Singing) {
        auto singingClip = dynamic_cast<DsSingingClip *>(clip);

        DsClip::ClipCommonProperties oldArgs;
        oldArgs.name = singingClip->name();
        oldArgs.id = singingClip->id();
        oldArgs.start = singingClip->start();
        oldArgs.clipStart = singingClip->clipStart();
        oldArgs.length = singingClip->length();
        oldArgs.clipLen = singingClip->clipLen();
        oldArgs.gain = singingClip->gain();
        oldArgs.mute = singingClip->mute();
        oldArgs.trackIndex = args.trackIndex;

        QList<DsClip::ClipCommonProperties> oldArgsList;
        oldArgsList.append(oldArgs);
        QList<DsClip::ClipCommonProperties> newArgsList;
        newArgsList.append(args);
        QList<DsSingingClip *> clips;
        clips.append(singingClip);

        auto a = new ClipActions;
        a->editSingingClipProperties(oldArgsList, newArgsList, clips, track);
        a->execute();
        HistoryManager::instance()->record(a);
    }
}
void TracksViewController::onRemoveClip(int clipId) {
    QMessageBox msgBox;
    msgBox.setText("Warning");
    msgBox.setInformativeText("Do you want to remove this clip?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Yes) {
        for (const auto &track : AppModel::instance()->tracks()) {
            auto result = track->findClipById(clipId);
            if (result != nullptr)
                track->removeClip(result);
        }
    }
}
void TracksViewController::onNewSingingClip(int trackIndex, int tick) {
    auto singingClip = new DsSingingClip;
    singingClip->setStart(tick);
    singingClip->setClipStart(0);
    singingClip->setLength(1920);
    singingClip->setClipLen(1920);
    auto track = AppModel::instance()->tracks().at(trackIndex);
    track->insertClip(singingClip);
}