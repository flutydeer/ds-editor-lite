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
    auto newTrack = new DsTrack;
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
void TracksViewController::onTrackPropertyChanged(const QString &name,
                                                  const DsTrackControl &control, int index) {
    auto track = AppModel::instance()->tracks().at(index);
    track->setName(name);
    track->setControl(control);
}
void TracksViewController::onAddAudioClip(const QString &path, int index) {
    auto audioClip = new DsAudioClip;
    audioClip->setPath(path);
    auto track = AppModel::instance()->tracks().at(index);
    track->insertClip(audioClip);
}