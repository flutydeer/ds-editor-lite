//
// Created by fluty on 2024/1/31.
//

#include <QMessageBox>

#include "TracksViewController.h"
#include "Model/AppModel.h"
#include "Utils/IdGenerator.h"

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
    AppModel::instance()->insertTrack(newTrack, index);
}
void TracksViewController::onRemoveTrack(int index) {
    QMessageBox msgBox;
    msgBox.setText("Warning");
    msgBox.setInformativeText("Do you want to remove this track?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Yes)
        AppModel::instance()->removeTrack(index);
}
void TracksViewController::addAudioClipToNewTrack(const QString &filePath) {
    auto audioClip = new DsAudioClip;
    audioClip->setPath(filePath);
    auto newTrack = new DsTrack;
    newTrack->insertClip(audioClip);
    AppModel::instance()->insertTrack(newTrack, AppModel::instance()->tracks().size());
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

    if (clip->type() == DsClip::Audio) {
        auto audioClip = dynamic_cast<DsAudioClip *>(clip);
        qDebug() << "clip path" << audioClip->path();
        auto audioArgs = dynamic_cast<const DsClip::AudioClipPropertyChangedArgs *>(&args);
        qDebug() << "args path" << audioArgs->path;
        audioClip->setPath(audioArgs->path);
    } else if (clip->type() == DsClip::Singing) {
        auto singingClip = dynamic_cast<DsSingingClip *>(clip);
        auto deltaTime = args.start - clip->start();
        qDebug() << "singing clip moved:" << deltaTime;
        if (deltaTime != 0) {
            for (auto &note : singingClip->notes) {
                note.setStart(note.start() + deltaTime);
            }
        }
    }
    track->removeClipQuietly(clip);
    clip->setStart(args.start);
    clip->setClipStart(args.clipStart);
    clip->setLength(args.length);
    clip->setClipLen(args.clipLen);
    track->insertClipQuietly(clip);
    track->notityClipUpdated(clip);

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