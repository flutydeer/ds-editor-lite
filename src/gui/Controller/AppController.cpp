//
// Created by FlutyDeer on 2023/12/1.
//

#include <QDebug>
#include <QMessageBox>

#include "AppController.h"

AppController::AppController() {
    // Init views
    m_tracksView = new TracksView;
    m_pianoRollView = new PianoRollGraphicsView;

    auto model = AppModel::instance();
    connect(model, &AppModel::modelChanged, m_tracksView, &TracksView::onModelChanged);
    connect(model, &AppModel::tracksChanged, m_tracksView, &TracksView::onTrackChanged);
    connect(model, &AppModel::modelChanged, m_pianoRollView, &PianoRollGraphicsView::updateView);
    connect(m_tracksView, &TracksView::selectedClipChanged, this, &AppController::onSelectedClipChanged);
    connect(model, &AppModel::selectedClipChanged, m_pianoRollView, &PianoRollGraphicsView::onSelectedClipChanged);
    connect(m_tracksView, &TracksView::trackPropertyChanged, this, &AppController::onTrackPropertyChanged);
    connect(m_tracksView, &TracksView::insertNewTrackTriggered, this, &AppController::onInsertNewTrack);
    connect(m_tracksView, &TracksView::removeTrackTriggerd, this, &AppController::onRemoveTrack);
}
AppController::~AppController() {
}
TracksView *AppController::tracksView() const {
    return m_tracksView;
}
PianoRollGraphicsView *AppController::pianoRollView() const {
    return m_pianoRollView;
}
void AppController::onNewTrack() {
    onInsertNewTrack(AppModel::instance()->tracks().count());
}
void AppController::onInsertNewTrack(int index) {
    DsTrack newTrack;
    AppModel::instance()->insertTrack(newTrack, index);
}
void AppController::onRemoveTrack(int index) {
    QMessageBox msgBox;
    msgBox.setText("Warning");
    msgBox.setInformativeText("Do you want to remove this track?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Yes)
        AppModel::instance()->removeTrack(index);
}
void AppController::openProject(const QString &filePath) {
    AppModel::instance()->loadAProject(filePath);
}
void AppController::addAudioClipToNewTrack(const QString &filePath) {
    auto audioClip = DsAudioClipPtr(new DsAudioClip);
    audioClip->setPath(filePath);
    DsTrack newTrack;
    newTrack.clips.append(audioClip);
    AppModel::instance()->insertTrack(newTrack, AppModel::instance()->tracks().count());
}
void AppController::onSelectedClipChanged(int trackIndex, int clipIndex) {
    AppModel::instance()->onSelectedClipChanged(trackIndex, clipIndex);
}
void AppController::onTrackPropertyChanged(const QString &name, const DsTrackControl &control, int index) {
    auto track = AppModel::instance()->tracks().at(index);
    track.setName(name);
    track.setControl(control);
}