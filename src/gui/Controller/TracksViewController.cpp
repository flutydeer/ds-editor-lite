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
void TracksViewController::onAddAudioClip(const QString &path, int index) {
    auto audioClip = new DsAudioClip;
    audioClip->setPath(path);
    auto track = AppModel::instance()->tracks().at(index);
    track->insertClip(audioClip);
}
void TracksViewController::onClipPropertyChanged(const DsClip::ClipPropertyChangedArgs &args) {
    qDebug() << "TracksViewController::onClipPropertyChanged";
    auto track = AppModel::instance()->tracks().at(args.trackIndex);
    auto clip = track->findClipById(args.id);
    track->removeClip(clip);
    clip->setStart(args.start);
    clip->setClipStart(args.clipStart);
    clip->setLength(args.length);
    clip->setClipLen(args.clipLen);
    qDebug() << "ClipPropertyChangedArgs:" << "length" << args.length;
    track->insertClip(clip);
}