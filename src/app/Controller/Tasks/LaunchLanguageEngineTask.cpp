//
// Created by fluty on 24-3-19.
//

#include "LaunchLanguageEngineTask.h"

#include "Modules/Language/LangSetting/ILangSetManager.h"

#include <QApplication>
#include <QThread>
#include <G2pMgr/IG2pManager.h>
#include <LangMgr/ILanguageManager.h>

LaunchLanguageEngineTask::LaunchLanguageEngineTask(QObject *parent) : Task(parent) {
    TaskStatus status;
    status.title = "Launching language engine...";
    status.message = "";
    status.isIndetermine = true;
    setStatus(status);
}

void LaunchLanguageEngineTask::runTask() {
    qDebug() << "RunLanguageEngineTask::runTask";
    const auto g2pMgr = G2pMgr::IG2pManager::instance();
    const auto langMgr = LangMgr::ILanguageManager::instance();
    const auto langSet = LangSetting::ILangSetManager::instance();

    QString errorMsg;
    g2pMgr->initialize(errorMsg);

    if (!g2pMgr->initialized())
        qDebug() << "G2pMgr: errorMsg" << errorMsg << "initialized:" << g2pMgr->initialized();

    langMgr->initialize(errorMsg);
    if (!langMgr->initialized())
        qDebug() << "LangMgr: errorMsg" << errorMsg << "initialized:" << langMgr->initialized();

    success = g2pMgr->initialized() && langMgr->initialized();
    errorMessage = errorMsg;

    // QThread::sleep(5);

    emit finished(false);
}