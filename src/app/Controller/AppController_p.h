//
// Created by fluty on 24-7-21.
//

#ifndef APPCONTROLLER_P_H
#define APPCONTROLLER_P_H

#include "Global/AppGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/ProjectConverters/DspxProjectConverter.h"
#include "Modules/ProjectConverters/MidiConverter.h"

#include <QObject>
#include <QElapsedTimer>
#include <QStandardPaths>

class AppController;
class IMainWindow;
class IPanel;
class OpenDspxProjectTask;
class ProgressDialog;
class TaskStatus;

class AppControllerPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(AppController)

public:
    enum class ProjectOpenState { Idle, WaitingPackages, Loading, Committing };

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
    OpenDspxProjectTask *m_openProjectTask = nullptr;
    ProjectOpenState m_projectOpenState = ProjectOpenState::Idle;
    quint64 m_openRequestId = 0;
    QElapsedTimer m_projectOpenTimer;

    static void initializeModules();
    static bool isPowerOf2(int num);
    void updateProjectPathAndName(const QString &path);
    void addRecentProjectFile(const QString &path);

    bool openDspxFile(const QString &path, QString &errorMessage);
    bool openMidiFile(const QString &path, QString &errorMessage);
    bool openFileAndActivateFirstClip(const QString &filePath);
    void requestOpenDspxFile(const QString &filePath);
    void waitAndOpenDspxFile();
    void startOpenDspxTask();
    void handleOpenDspxTaskFinished(OpenDspxProjectTask *task);
    void updateOpenProjectDialog(const TaskStatus &status) const;
    void showOpenProjectError(const QString &errorMessage) const;
    void activateFirstClip();
    void cancelPendingOpen();
    void handlePendingOpenPackageStatus(AppStatus::ModuleStatus status);
    void createPendingOpenDialog();
    void clearPendingOpenDialog(bool deleteImmediately = false);
    bool confirmOpenWithoutPackageMetadata();

private:
    AppController *q_ptr = nullptr;
};

#endif // APPCONTROLLER_P_H
