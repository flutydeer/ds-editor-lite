//
// Created by fluty on 2024/1/31.
//

#include <QMessageBox>

#include "TracksViewController.h"
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
        auto a = new TrackActions;
        a->removeTrack(trackToRemove, index, AppModel::instance());
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
void TracksViewController::onTrackPropertyChanged(const DsTrack::TrackPropertyChangedArgs &args) {
    auto tracks = AppModel::instance()->tracks();
    auto track = tracks.at(args.index);
    track->setName(args.name);
    auto control = track->control();
    control.setGain(args.gain);
    control.setPan(args.pan);
    control.setMute(args.mute);
    control.setSolo(args.solo);
    qDebug() << "gain" << control.gain() << "pan" << control.pan();
    track->setControl(control);
}
void TracksViewController::onAddAudioClip(const QString &path, int trackIndex, int tick) {
    auto audioClip = new DsAudioClip;
    audioClip->setStart(tick);
    audioClip->setClipStart(0);
    audioClip->setPath(path);
    auto track = AppModel::instance()->tracks().at(trackIndex);
    track->insertClip(audioClip);
}
void TracksViewController::onClipPropertyChanged(const DsClip::ClipPropertyChangedArgs &args) {
    qDebug() << "TracksViewController::onClipPropertyChanged";
    auto track = AppModel::instance()->tracks().at(args.trackIndex);
    auto clip = track->findClipById(args.id);

    qDebug() << "args.id" << args.id;
    qDebug() << "args.name" << args.name;
    qDebug() << "args.start" << args.start;
    qDebug() << "args.clipStart" << args.clipStart;
    qDebug() << "args.length" << args.length;
    qDebug() << "args.clipLen" << args.clipLen;

    if (clip->type() == DsClip::Audio) {
        auto audioClip = dynamic_cast<DsAudioClip *>(clip);
        qDebug() << "clip path" << audioClip->path();
        auto audioArgs = dynamic_cast<const DsClip::AudioClipPropertyChangedArgs *>(&args);
        qDebug() << "args path" << audioArgs->path;
        // audioClip->setPath(audioArgs->path);
    } else if (clip->type() == DsClip::Singing) {
        auto singingClip = dynamic_cast<DsSingingClip *>(clip);
        auto deltaTime = args.start - clip->start();
        qDebug() << "singing clip moved:" << deltaTime;
        if (deltaTime != 0) {
            // TODO: handle singing clip move: move notes and params
            // for (auto &note : singingClip->notes()) {
            //     note->setStart(note.start() + deltaTime);
            // }
        }
    }
    track->removeClipQuietly(clip);
    clip->setName(args.name);
    clip->setStart(args.start);
    clip->setClipStart(args.clipStart);
    clip->setLength(args.length);
    clip->setClipLen(args.clipLen);
    track->insertClipQuietly(clip);
    track->notityClipPropertyChanged(clip);

    // track->updateClip(clip);
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