//
// Created by FlutyDeer on 2023/12/1.
//

#include <QDebug>
#include <QApplication>

#include "AppController.h"

#include "g2pglobal.h"
#include "zhg2p.h"

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
    IKg2p::setDictionaryPath(qApp->applicationDirPath() + "/dict");

    auto g2p_man = new IKg2p::ZhG2p("mandarin");

    for (const auto &note : notes) {
        qDebug() << note.lyric() << " -> " << g2p_man->convert(note.lyric(), false, false);
    }
}