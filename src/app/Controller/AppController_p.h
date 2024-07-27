//
// Created by fluty on 24-7-21.
//

#ifndef APPCONTROLLER_P_H
#define APPCONTROLLER_P_H

#include "Global/AppGlobal.h"

#include <QStandardPaths>

class IMainWindow;
class LaunchLanguageEngineTask;
class AppControllerPrivate {
    Q_DECLARE_PUBLIC(AppController)

public:
    explicit AppControllerPrivate(AppController *q) : q_ptr(q) {
    }
    IMainWindow *m_mainWindow = nullptr;
    QString m_lastProjectFolder =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString m_projectPath;
    QString m_projectName;
    bool m_isLanguageEngineReady = false;
    QList<IPanel *> m_panels;
    AppGlobal::PanelType m_activePanel = AppGlobal::TracksEditor;

    bool isPowerOf2(int num);
    void handleRunLanguageEngineTaskFinished(LaunchLanguageEngineTask *task);
    void updateProjectPathAndName(const QString &path);

private:
    AppController *q_ptr = nullptr;
};

#endif // APPCONTROLLER_P_H
