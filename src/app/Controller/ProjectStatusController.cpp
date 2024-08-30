//
// Created by fluty on 24-8-30.
//

#include "ProjectStatusController.h"

#include "Model/AppStatus/AppStatus.h"

ProjectStatusController::ProjectStatusController() {
    connect(appModel, &AppModel::modelChanged, this, &ProjectStatusController::onModelChanged);
}

void ProjectStatusController::onModelChanged() {
    // qDebug() << "onModelChanged";
    for (const auto track : m_tracks)
        onTrackChanged(AppModel::Remove, -1, track);

    for (const auto track : appModel->tracks())
        onTrackChanged(AppModel::Insert, -1, track);
}

void ProjectStatusController::onTrackChanged(AppModel::TrackChangeType type, qsizetype index,
                                             Track *track) {
    if (type == AppModel::Insert) {
        // qDebug() << "onTrackChanged" << "Insert";
        m_tracks.append(track);
        for (const auto clip : track->clips())
            handleClipInserted(clip);
        connect(track, &Track::clipChanged, this, &ProjectStatusController::onClipChanged);
    } else if (type == AppModel::Remove) {
        // qDebug() << "onTrackChanged" << "Remove";
        m_tracks.removeOne(track);
        for (const auto clip : track->clips())
            handleClipInserted(clip);
        disconnect(track, &Track::clipChanged, this, &ProjectStatusController::onClipChanged);
    }
}

void ProjectStatusController::onClipChanged(Track::ClipChangeType type, Clip *clip) {
    if (type == Track::Inserted) {
        // qDebug() << "onClipChanged" << "Inserted";
        handleClipInserted(clip);
    } else if (type == Track::Removed) {
        // qDebug() << "onClipChanged" << "Removed";
        handleClipRemoved(clip);
    }
    updateProjectEditableLength();
}

void ProjectStatusController::handleClipInserted(Clip *clip) {
    connect(clip, &Clip::propertyChanged, this,
            &ProjectStatusController::updateProjectEditableLength);
}

void ProjectStatusController::handleClipRemoved(Clip *clip) const {
    disconnect(clip, nullptr, this, nullptr);
}

void ProjectStatusController::updateProjectEditableLength() {
    int maxEndTick = 1920 * 100;
    int tailLength = 3840; // 在编辑区域的尾部留下至少 2 小节（4/4）的空白
    for (const auto track : appModel->tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->endTick() + tailLength > maxEndTick)
                maxEndTick = clip->endTick() + tailLength;
        }
    }
    appStatus->projectEditableLength = maxEndTick;
    qDebug() << "Update project editable length:" << maxEndTick;
}