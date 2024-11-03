//
// Created by fluty on 24-3-19.
//

#include "LaunchLanguageEngineTask.h"

#include "Modules/Language/LangSetting/ILangSetManager.h"

#include <QApplication>
#include <QThread>
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
    // QThread::sleep(1);
    const auto langMgr = LangMgr::ILanguageManager::instance();
    const auto langSet = LangSetting::ILangSetManager::instance();

    QString errorMsg;

#ifdef Q_OS_MAC
    const QString dictPath = qApp->applicationDirPath() + QStringLiteral("/../Resources/dict");
#else
    const QString dictPath = qApp->applicationDirPath() + QStringLiteral("/dict");
#endif

    auto args = QJsonObject();
    args.insert("pinyinDictPath", dictPath);

    langMgr->initialize(args, errorMsg);
    if (!langMgr->initialized())
        qCritical() << "LangMgr: errorMsg" << errorMsg << "initialized:" << langMgr->initialized();

    success = langMgr->initialized();
    errorMessage = errorMsg;

    if (success)
        qInfo() << "Successfully launched language module";
    else
        qCritical() << "Failed to launch language module: " << errorMessage;
}