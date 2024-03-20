//
// Created by fluty on 24-3-19.
//

#include "RunLanguageEngineTask.h"

#include <QApplication>

#include "g2pglobal.h"
#include "Modules/Language/G2pMgr/IG2pManager.h"
#include "Modules/Language/LangMgr/ILanguageManager.h"

#include <QThread>

void RunLanguageEngineTask::runTask() {
    qDebug() << "RunLanguageEngineTask::runTask";
    IKg2p::setDictionaryPath(qApp->applicationDirPath() + "/dict");
    const auto g2pMgr = G2pMgr::IG2pManager::instance();
    const auto langMgr = LangMgr::ILanguageManager::instance();

    QString errorMsg;
    g2pMgr->initialize(errorMsg);

    qDebug() << "G2pMgr: errorMsg" << errorMsg << "initialized:" << g2pMgr->initialized();

    langMgr->initialize(errorMsg);
    qDebug() << "LangMgr: errorMsg" << errorMsg << "initialized:" << langMgr->initialized();

    success = g2pMgr->initialized() && langMgr->initialized();
    errorMessage = errorMsg;

    emit finished(false);
}