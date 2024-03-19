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
    G2pMgr::IG2pManager g2pMgr;
    const LangMgr::ILanguageManager langMgr;

    // QThread::sleep(5);
    
    QString errorMsg;
    g2pMgr.initialize(errorMsg);

    qDebug() << "G2pMgr: errorMsg" << errorMsg << "initialized:" << g2pMgr.initialized();

    success = g2pMgr.initialized();
    errorMessage = errorMsg;

    emit finished(false);
}