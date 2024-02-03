//
// Created by FlutyDeer on 2023/12/1.
//

#include <QDebug>

#include "AppController.h"

void AppController::openProject(const QString &filePath) {
    AppModel::instance()->loadAProject(filePath);
}
void AppController::onRunG2p() {
    auto model = AppModel::instance();
    auto firstTrack = model->tracks().first();
    auto firstClip = firstTrack->clips().at(0);
    auto singingClip = dynamic_cast<DsSingingClip *>(firstClip);
    if (singingClip == nullptr)
        return;
    auto notes = singingClip->notes;
    for (const auto &note : notes) {
        qDebug() << note.lyric();
    }
}