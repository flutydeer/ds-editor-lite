//
// Created by fluty on 24-3-19.
//

#include "LaunchLanguageEngineTask.h"

#include "Model/AppOptions/AppOptions.h"
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

    if (success) {
        qInfo() << "Successfully launched language module";
        const auto options = appOptions->language();
        for (const auto &key : options->langOrder) {
            if (options->g2pConfigs.contains(key)) {
                if (const auto &g2pFactory = langMgr->g2p(key)) {
                    const auto &g2pConfig = options->g2pConfigs.value(key).toObject();

                    if (!g2pConfig.empty()) {
                        g2pFactory->loadG2pConfig(g2pConfig);
                        const auto &langConfig = g2pConfig.value("languageConfig").toObject();
                        if (!langConfig.empty())
                            g2pFactory->loadLanguageConfig(langConfig);
                    }
                }
            }
        }
        qInfo() << "Language module load config Success";
    } else
        qCritical() << "Failed to launch language module: " << errorMessage;
}