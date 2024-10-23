//
// Created by fluty on 24-8-30.
//

#include "ProjectStatusController.h"

#include "Model/AppStatus/AppStatus.h"

void ProjectStatusController::handleTrackRemoved(Track *track) {
    ModelChangeHandler::handleTrackRemoved(track);
    bool trackContainsActiveClip = false;
    for (const auto clip : track->clips()) {
        if (clip->id() == appStatus->activeClipId) {
            trackContainsActiveClip = true;
            break;
        }
    }
    if (trackContainsActiveClip) {
        appStatus->selectedNotes = QList<int>();
        appStatus->activeClipId = -1;
    }
}

void ProjectStatusController::handleTempoChanged(double tempo) {
    updateProjectEditableLength();
}

void ProjectStatusController::handleClipInserted(Clip *clip) {
    ModelChangeHandler::handleClipInserted(clip);
    updateProjectEditableLength();
}

void ProjectStatusController::handleClipRemoved(Clip *clip) {
    if (appStatus->activeClipId == clip->id()) {
        appStatus->selectedNotes = QList<int>();
        appStatus->activeClipId = -1;
    }
    updateProjectEditableLength();
}

void ProjectStatusController::handleClipPropertyChanged(Clip *clip) {
    updateProjectEditableLength();
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