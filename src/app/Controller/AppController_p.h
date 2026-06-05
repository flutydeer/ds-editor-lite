//
// Created by fluty on 24-7-21.
//

#ifndef APPCONTROLLER_P_H
#define APPCONTROLLER_P_H

#include "Global/AppGlobal.h"
#include "Model/AppStatus/AppStatus.h"

#include <QObject>
#include <QStandardPaths>

class AppController;
class IMainWindow;
class IPanel;
class LaunchLanguageEngineTask;
class ProgressDialog;

class AppControllerPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(AppController)

public:
    explicit AppControllerPrivate(AppController *q) : q_ptr(q) {
    }
    ~AppControllerPrivate() override;

    IMainWindow *m_mainWindow = nullptr;
    QString m_lastProjectFolder =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString m_projectPath;
    QString m_projectName;
    QList<IPanel *> m_panels{};
    AppGlobal::PanelType m_activePanel = AppGlobal::TracksEditor;
    QString m_pendingOpenFilePath;
    ProgressDialog *m_pendingOpenDialog = nullptr;
    QMetaObject::Connection m_pendingOpenConnection;

    static void initializeModules();
    static bool isPowerOf2(int num);
    static void onRunLanguageEngineTaskFinished(LaunchLanguageEngineTask *task);
    void updateProjectPathAndName(const QString &path);
    void addRecentProjectFile(const QString &path);

    bool openDspxFile(const QString &path, QString &errorMessage);
    bool openMidiFile(const QString &path, QString &errorMessage);
    bool openFileAndActivateFirstClip(const QString &filePath);
    void waitAndOpenDspxFile(const QString &filePath);
    void cancelPendingOpen();
    void handlePendingOpenPackageStatus(AppStatus::ModuleStatus status);
    QString takePendingOpenFilePath();
    void clearPendingOpenDialog(bool deleteImmediately = false);
    bool confirmOpenWithoutPackageMetadata();

private:
    AppController *q_ptr = nullptr;
};

#endif // APPCONTROLLER_P_H
