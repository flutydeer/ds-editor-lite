//
// Created by fluty on 24-3-19.
//

#include "LaunchLanguageEngineTask.h"

#include "Modules/Language/LangSetting/ILangSetManager.h"

#include <QApplication>
#include <QThread>
#include <language-manager/IG2pManager.h>
#include <language-manager/ILanguageManager.h>

LaunchLanguageEngineTask::LaunchLanguageEngineTask(QObject *parent) : Task(parent) {
    TaskStatus status;
    status.title = tr("Launching language module...");
    status.message = "";
    status.isIndetermine = true;
    setStatus(status);
}

void LaunchLanguageEngineTask::runTask() {
    qDebug() << "Launching language module...";
    QThread::sleep(1);
    const auto g2pMgr = LangMgr::IG2pManager::instance();
    const auto langMgr = LangMgr::ILanguageManager::instance();
    const auto langSet = LangSetting::ILangSetManager::instance();

    QString errorMsg;
    g2pMgr->initialize(errorMsg);

    if (!g2pMgr->initialized())
        qCritical() << "G2pMgr: errorMsg" << errorMsg << "initialized:" << g2pMgr->initialized();

    langMgr->initialize(errorMsg);
    if (!langMgr->initialized())
        qCritical() << "LangMgr: errorMsg" << errorMsg << "initialized:" << langMgr->initialized();

    success = g2pMgr->initialized() && langMgr->initialized();
    errorMessage = errorMsg;

    if (success)
        qInfo() << "Successfully launched language module";
    else
        qCritical() << "Failed to launch language module: " << errorMessage;
    emit finished(false);
}