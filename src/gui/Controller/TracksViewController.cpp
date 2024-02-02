//
// Created by fluty on 2024/1/31.
//

#include <QMessageBox>

#include "TracksViewController.h"
#include "Model/AppModel.h"

void TracksViewController::onNewTrack() {
    onInsertNewTrack(AppModel::instance()->tracks().count());
}
void TracksViewController::onInsertNewTrack(int index) {
    bool soloExists = false;
    auto tracks = AppModel::instance()->tracks();
    for (auto dsTrack : tracks) {
        auto curControl = dsTrack->control();
        if (curControl.solo()) {
            soloExists = true;
            break;
        }
    }

    auto newTrack = new DsTrack;
    newTrack->setName("New Track");
    if (soloExists) {
        auto control = newTrack->control();
        control.setMute(true);
        newTrack->setControl(control);
    }
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
void TracksViewController::onSelectedClipChanged(int trackIndex, int clipIndex) {
    AppModel::instance()->onSelectedClipChanged(trackIndex, clipIndex);
}
void TracksViewController::onTrackPropertyChanged(const DsTrack::TrackPropertyChangedArgs &args) {
    auto tracks = AppModel::instance()->tracks();
    auto track = tracks.at(args.index);
    track->setName(args.name);
    auto control = track->control();
    control.setGain(args.gain);
    control.setPan(args.pan);
    track->setControl(control);

    // find whether the mute or solo was clicked
    bool isSoloClicked = false;
    if (args.solo != track->control().solo())
        isSoloClicked = true;

    bool isMuteClicked = false;
    if (args.mute != track->control().mute())
        isMuteClicked = true;

    if (isSoloClicked) {
        if (args.solo == true) { // turn on solo
            for (auto dsTrack : tracks) {
                auto curControl = dsTrack->control();
                if (dsTrack == track) {
                    curControl.setMute(false);
                    curControl.setSolo(true);
                    track->setControl(curControl);
                    continue;
                }

                // mute all unmuted tracks
                if (!curControl.mute() && !curControl.solo()) {
                    curControl.setMute(true);
                    dsTrack->setControl(curControl);
                }
            }
        } else { // turn off solo
            control = track->control();
            control.setSolo(false);
            track->setControl(control);

            int soloTracks = 0;
            for (auto dsTrack : tracks) {
                auto curControl = dsTrack->control();
                if (curControl.solo())
                    soloTracks++;
            }
            if (soloTracks > 0) {
                control.setMute(true);
                track->setControl(control);
            } else if (soloTracks == 0) { // unmute all muted tracks
                for (auto dsTrack : tracks) {
                    auto curControl = dsTrack->control();
                    curControl.setMute(false);
                    dsTrack->setControl(curControl);
                }
            }
        }
    } else if (isMuteClicked) {  // is mute clicked
        if (args.mute == true) { // turn on mute
            control = track->control();
            control.setSolo(false);
            control.setMute(true);
        } else { // turn off mute
            bool soloExists = false;

            if (!soloExists) {
                control.setMute(false);
            }
        }
        track->setControl(control);
    }
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
    auto clip = track->clips().at(args.clipIndex);
    track->removeClip(clip);
    clip->setStart(args.start);
    clip->setClipStart(args.clipStart);
    clip->setLength(args.length);
    clip->setClipLen(args.clipLen);
    track->insertClip(clip);
}