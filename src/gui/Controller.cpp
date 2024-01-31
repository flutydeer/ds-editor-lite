//
// Created by FlutyDeer on 2023/12/1.
//

#include <QDebug>
#include <QMessageBox>

#include "Controller.h"

Controller::Controller() {
    // Init views
    m_tracksView = new TracksView;
    m_pianoRollView = new PianoRollGraphicsView;

    auto model = DsModel::instance();
    connect(model, &DsModel::modelChanged, m_tracksView, &TracksView::onModelChanged);
    connect(model, &DsModel::tracksChanged, m_tracksView, &TracksView::onTrackChanged);
    connect(model, &DsModel::modelChanged, m_pianoRollView, &PianoRollGraphicsView::updateView);
    connect(m_tracksView, &TracksView::selectedClipChanged, this, &Controller::onSelectedClipChanged);
    connect(model, &DsModel::selectedClipChanged, m_pianoRollView, &PianoRollGraphicsView::onSelectedClipChanged);
    connect(m_tracksView, &TracksView::trackPropertyChanged, this, &Controller::onTrackPropertyChanged);
    connect(m_tracksView, &TracksView::insertNewTrackTriggered, this, &Controller::onInsertNewTrack);
    connect(m_tracksView, &TracksView::removeTrackTriggerd, this, &Controller::onRemoveTrack);
}
Controller::~Controller() {
}
TracksView *Controller::tracksView() const {
    return m_tracksView;
}
PianoRollGraphicsView *Controller::pianoRollView() const {
    return m_pianoRollView;
}
void Controller::onNewTrack() {
    onInsertNewTrack(DsModel::instance()->tracks().count());
}
void Controller::onInsertNewTrack(int index) {
    DsTrack newTrack;
    DsModel::instance()->insertTrack(newTrack, index);
}
void Controller::onRemoveTrack(int index) {
    QMessageBox msgBox;
    msgBox.setText("Warning");
    msgBox.setInformativeText("Do you want to remove this track?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Yes)
        DsModel::instance()->removeTrack(index);
}
void Controller::openProject(const QString &filePath) {
    DsModel::instance()->loadAProject(filePath);
}
void Controller::addAudioClipToNewTrack(const QString &filePath) {
    auto audioClip = DsAudioClipPtr(new DsAudioClip);
    audioClip->setPath(filePath);
    DsTrack newTrack;
    newTrack.clips.append(audioClip);
    DsModel::instance()->insertTrack(newTrack, DsModel::instance()->tracks().count());
}
void Controller::onSelectedClipChanged(int trackIndex, int clipIndex) {
    DsModel::instance()->onSelectedClipChanged(trackIndex, clipIndex);
}
void Controller::onTrackPropertyChanged(const QString &name, const DsTrackControl &control, int index) {
    auto track = DsModel::instance()->tracks().at(index);
    track.setName(name);
    track.setControl(control);
}